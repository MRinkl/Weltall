#include "Formats/Wave.h"

#include <limits>
#include <string_view>

namespace Weltall
{
	namespace
	{
		uint16 Read16(std::span<const uint8> Bytes, std::size_t At)
		{
			return static_cast<uint16>(Bytes[At] | static_cast<uint16>(Bytes[At + 1]) << 8);
		}

		uint32 Read32(std::span<const uint8> Bytes, std::size_t At)
		{
			return static_cast<uint32>(Read16(Bytes, At)) | static_cast<uint32>(Read16(Bytes, At + 2)) << 16;
		}

		bool Is(std::span<const uint8> Bytes, std::size_t At, std::string_view Name)
		{
			return At <= Bytes.size() && Name.size() <= Bytes.size() - At
				&& std::string_view(reinterpret_cast<const char*>(Bytes.data() + At), Name.size()) == Name;
		}

		void Put16(std::vector<uint8>& Bytes, uint16 Value)
		{
			Bytes.push_back(static_cast<uint8>(Value));
			Bytes.push_back(static_cast<uint8>(Value >> 8));
		}

		void Put32(std::vector<uint8>& Bytes, uint32 Value)
		{
			Put16(Bytes, static_cast<uint16>(Value));
			Put16(Bytes, static_cast<uint16>(Value >> 16));
		}

		void PutName(std::vector<uint8>& Bytes, std::string_view Name)
		{
			Bytes.insert(Bytes.end(), Name.begin(), Name.end());
		}

		bool ValidFormat(uint32 Rate, uint16 Channels)
		{
			return Rate >= 8000 && Rate <= 192000 && (Channels == 1 || Channels == 2);
		}
	}

	TResult<FWave> FWave::Parse(std::span<const uint8> Bytes)
	{
		if (Bytes.size() < 12 || !Is(Bytes, 0, "RIFF") || !Is(Bytes, 8, "WAVE"))
		{
			return MakeError("Music replacement is not a RIFF WAVE file.");
		}
		const uint64 End = static_cast<uint64>(Read32(Bytes, 4)) + 8;
		if (End < 12 || End > Bytes.size())
		{
			return MakeError("WAVE file ends before its RIFF data.");
		}
		FWave Wave;
		bool bFormat = false;
		std::optional<std::span<const uint8>> Data;
		for (std::size_t At = 12; At < End;)
		{
			if (End - At < 8)
			{
				return MakeError("WAVE file has an incomplete chunk header.");
			}
			const uint32 Size = Read32(Bytes, At + 4);
			const std::size_t Begin = At + 8;
			if (Size > End - Begin || static_cast<uint64>(Size) + (Size & 1u) > End - Begin)
			{
				return MakeError("WAVE chunk extends beyond its RIFF data.");
			}
			if (Is(Bytes, At, "fmt "))
			{
				if (bFormat || Size < 16 || Read16(Bytes, Begin) != 1 || Read16(Bytes, Begin + 14) != 16)
				{
					return MakeError("Music replacements require 16-bit integer PCM WAVE.");
				}
				Wave.Channels = Read16(Bytes, Begin + 2);
				Wave.SampleRate = Read32(Bytes, Begin + 4);
				if (!ValidFormat(Wave.SampleRate, Wave.Channels) || Read16(Bytes, Begin + 12) != Wave.Channels * 2
					|| Read32(Bytes, Begin + 8) != Wave.SampleRate * Wave.Channels * 2)
				{
					return MakeError("WAVE music must be mono or stereo at 8,000 to 192,000 Hz with valid PCM alignment.");
				}
				bFormat = true;
			}
			else if (Is(Bytes, At, "data"))
			{
				if (Data)
				{
					return MakeError("WAVE file has more than one sample data chunk.");
				}
				Data = Bytes.subspan(Begin, Size);
			}
			else if (Is(Bytes, At, "smpl"))
			{
				if (Size < 36 || Read32(Bytes, Begin + 28) > (Size - 36) / 24)
				{
					return MakeError("WAVE sampler loop table is incomplete.");
				}
				const uint32 Loops = Read32(Bytes, Begin + 28);
				if (Loops > 1 || (Loops == 1 && (Read32(Bytes, Begin + 40) != 0 || Read32(Bytes, Begin + 52) != 0 || Read32(Bytes, Begin + 56) != 0)))
				{
					return MakeError("WAVE music supports one infinite forward loop without a fractional endpoint.");
				}
				if (Loops == 1)
				{
					const uint32 Last = Read32(Bytes, Begin + 48);
					if (Last == std::numeric_limits<uint32>::max())
					{
						return MakeError("WAVE sampler loop ends beyond the sample data.");
					}
					Wave.Loop = FWaveLoop{Read32(Bytes, Begin + 44), Last + 1};
				}
			}
			At = Begin + Size + (Size & 1u);
		}
		if (!bFormat || !Data || Data->empty() || Data->size() % (Wave.Channels * 2) != 0)
		{
			return MakeError("WAVE music lacks a format or complete sample frames.");
		}
		Wave.Samples.resize(Data->size() / 2);
		for (std::size_t Index = 0; Index < Wave.Samples.size(); ++Index)
		{
			Wave.Samples[Index] = static_cast<int16>(Read16(*Data, Index * 2));
		}
		if (Wave.Loop && (Wave.Loop->FirstFrame >= Wave.Loop->EndFrame || Wave.Loop->EndFrame > Wave.Frames()))
		{
			return MakeError("WAVE sampler loop lies outside the sample data.");
		}
		return Wave;
	}

	TResult<std::vector<uint8>> FWave::Serialize() const
	{
		if (!ValidFormat(SampleRate, Channels) || Samples.empty() || Samples.size() % Channels != 0
			|| Samples.size() > (std::numeric_limits<uint32>::max() - 104u) / 2
			|| (Loop && (Loop->FirstFrame >= Loop->EndFrame || Loop->EndFrame > Frames())))
		{
			return MakeError("Cannot write WAVE music with an invalid format, sample data or sampler loop.");
		}
		std::vector<uint8> Bytes;
		const uint32 DataSize = static_cast<uint32>(Samples.size() * 2);
		Bytes.reserve(static_cast<std::size_t>(DataSize) + 44 + (Loop ? 68 : 0));
		PutName(Bytes, "RIFF");
		Put32(Bytes, 36 + DataSize + (Loop ? 68 : 0));
		PutName(Bytes, "WAVEfmt ");
		Put32(Bytes, 16);
		Put16(Bytes, 1);
		Put16(Bytes, Channels);
		Put32(Bytes, SampleRate);
		Put32(Bytes, SampleRate * Channels * 2);
		Put16(Bytes, static_cast<uint16>(Channels * 2));
		Put16(Bytes, 16);
		PutName(Bytes, "data");
		Put32(Bytes, DataSize);
		for (const int16 Sample : Samples)
		{
			Put16(Bytes, static_cast<uint16>(Sample));
		}
		if (Loop)
		{
			PutName(Bytes, "smpl");
			Put32(Bytes, 60);
			Put32(Bytes, 0);
			Put32(Bytes, 0);
			Put32(Bytes, 1000000000u / SampleRate);
			Put32(Bytes, 60);
			for (int32 Index = 0; Index < 3; ++Index) Put32(Bytes, 0);
			Put32(Bytes, 1);
			Put32(Bytes, 0);
			Put32(Bytes, 0);
			Put32(Bytes, 0);
			Put32(Bytes, Loop->FirstFrame);
			Put32(Bytes, Loop->EndFrame - 1);
			Put32(Bytes, 0);
			Put32(Bytes, 0);
		}
		return Bytes;
	}
}
