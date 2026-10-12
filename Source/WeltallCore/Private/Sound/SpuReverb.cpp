#include "Sound/SpuReverb.h"

#include "Sound/Spu.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The half-band filter that brings the reverb's input down to 22,050 Hz and its output back up. */
		constexpr std::array<int32, 39> HalfBand = {
			-0x0001, 0, 0x0002, 0, -0x000A, 0, 0x0023, 0, -0x0067, 0, 0x010A, 0, -0x0268, 0, 0x0534, 0, -0x0B90, 0, 0x2806, 0x4000,
			0x2806, 0, -0x0B90, 0, 0x0534, 0, -0x0268, 0, 0x010A, 0, -0x0067, 0, 0x0023, 0, -0x000A, 0, 0x0002, 0, -0x0001};

		enum ERegister : int32
		{
			ApfOffset1 = 0,
			ApfOffset2 = 1,
			ReflectionVolume1 = 2,
			CombVolume1 = 3,
			CombVolume2 = 4,
			CombVolume3 = 5,
			CombVolume4 = 6,
			ReflectionVolume2 = 7,
			ApfVolume1 = 8,
			ApfVolume2 = 9,
			SameSide1 = 10,
			Comb1 = 12,
			Comb2 = 14,
			SameSide2 = 16,
			DifferentSide1 = 18,
			Comb3 = 20,
			Comb4 = 22,
			DifferentSide2 = 24,
			Apf1 = 26,
			Apf2 = 28,
			InputVolume = 30,
		};

		int32 Saturate(int32 Value)
		{
			return std::clamp(Value, -0x8000, 0x7FFF);
		}

		int32 Filter(const int16* Window)
		{
			int32 Sum = 0;
			for (std::size_t Tap = 0; Tap < HalfBand.size(); ++Tap)
			{
				Sum += HalfBand[Tap] * Window[Tap];
			}
			return Sum;
		}
	}

	void FSpuReverb::SetStart(uint16 Start)
	{
		StartUnits = Start;
		Current = static_cast<uint32>(Start) * 8;
	}

	int16 FSpuReverb::Read(std::span<const uint8> Ram, int32 Offset) const
	{
		const uint32 Base = static_cast<uint32>(StartUnits) * 8;
		const int64 Size = static_cast<int64>(FSpu::RamSize) - Base;
		int64 Relative = (static_cast<int64>(Current) - Base + Offset) % Size;
		Relative += Relative < 0 ? Size : 0;
		const std::size_t At = static_cast<std::size_t>((Base + Relative) & ~int64{1});
		return static_cast<int16>(Ram[At] | Ram[At + 1] << 8);
	}

	void FSpuReverb::Write(std::span<uint8> Ram, int32 Offset, int32 Value) const
	{
		const uint32 Base = static_cast<uint32>(StartUnits) * 8;
		const int64 Size = static_cast<int64>(FSpu::RamSize) - Base;
		int64 Relative = (static_cast<int64>(Current) - Base + Offset) % Size;
		Relative += Relative < 0 ? Size : 0;
		const std::size_t At = static_cast<std::size_t>((Base + Relative) & ~int64{1});
		const uint16 Sample = static_cast<uint16>(Saturate(Value));
		Ram[At] = static_cast<uint8>(Sample);
		Ram[At + 1] = static_cast<uint8>(Sample >> 8);
	}

	int32 FSpuReverb::RunSide(int32 Side, int32 Input, std::span<uint8> Ram, bool bWrites)
	{
		const auto Volume = [this](int32 Index) { return static_cast<int32>(static_cast<int16>(Registers[static_cast<std::size_t>(Index)])); };
		const auto Address = [this](int32 Index) { return static_cast<int32>(Registers[static_cast<std::size_t>(Index)]) * 8; };
		const int32 Other = 1 - Side;

		const int32 In = Saturate(Input * Volume(InputVolume + Side) >> 15);
		// The reflections: each a one-pole filter fed by the input and a tap of the buffer, on its own side and across.
		const auto Reflect = [&](int32 Destination, int32 Source) {
			const int32 Previous = Read(Ram, Address(Destination) - 2);
			const int32 Fed = Saturate(Saturate(In + (Read(Ram, Address(Source)) * Volume(ReflectionVolume2) >> 15)) - Previous);
			if (bWrites)
			{
				Write(Ram, Address(Destination), (Fed * Volume(ReflectionVolume1) >> 15) + Previous);
			}
		};
		Reflect(SameSide1 + Side, SameSide2 + Side);
		Reflect(DifferentSide1 + Side, DifferentSide2 + Other);

		int32 Out = Saturate((Volume(CombVolume1) * Read(Ram, Address(Comb1 + Side)) + Volume(CombVolume2) * Read(Ram, Address(Comb2 + Side))
			+ Volume(CombVolume3) * Read(Ram, Address(Comb3 + Side)) + Volume(CombVolume4) * Read(Ram, Address(Comb4 + Side))) >> 15);
		// Two all-pass stages.
		const auto AllPass = [&](int32 Stage, int32 Offset, int32 StageVolume) {
			const int32 Delayed = Read(Ram, Address(Stage + Side) - Address(Offset));
			const int32 Fed = Saturate(Out - (Volume(StageVolume) * Delayed >> 15));
			if (bWrites)
			{
				Write(Ram, Address(Stage + Side), Fed);
			}
			Out = Saturate((Fed * Volume(StageVolume) >> 15) + Delayed);
		};
		AllPass(Apf1, ApfOffset1, ApfVolume1);
		AllPass(Apf2, ApfOffset2, ApfVolume2);
		return Out;
	}

	std::array<int32, 2> FSpuReverb::Process(std::array<int32, 2> Input, std::span<uint8> Ram, bool bWrites)
	{
		const std::size_t Here = static_cast<std::size_t>(Position);
		for (std::size_t Side = 0; Side < 2; ++Side)
		{
			const int16 Sample = static_cast<int16>(Saturate(Input[Side]));
			Downsample[Side][Here] = Downsample[Side][Here + HistorySize] = Sample;
			Upsample[Side][Here] = Upsample[Side][Here + HistorySize] = 0;
		}
		const std::size_t WindowStart = Here + HistorySize - (HalfBand.size() - 1);
		if (Position & 1)
		{
			for (std::size_t Side = 0; Side < 2; ++Side)
			{
				const int32 Slow = Saturate(Filter(&Downsample[Side][WindowStart]) >> 15);
				const int16 Result = static_cast<int16>(RunSide(static_cast<int32>(Side), Slow, Ram, bWrites));
				Upsample[Side][Here] = Upsample[Side][Here + HistorySize] = Result;
			}
			const uint32 Base = static_cast<uint32>(StartUnits) * 8;
			Current = std::max(Base, (Current + 2) & (FSpu::RamSize - 2));
		}
		std::array<int32, 2> Output{};
		for (std::size_t Side = 0; Side < 2; ++Side)
		{
			// The stuffed zeros halve the level: the filter's output is doubled back.
			Output[Side] = Saturate(Filter(&Upsample[Side][WindowStart]) >> 14);
		}
		Position = (Position + 1) % HistorySize;
		return Output;
	}
}
