#include "Sound/SpuVoice.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The interpolation's weights (psx-spx's "gaussian" table: a four-term cosine window). */
		constexpr std::array<int32, 512> Interpolation = {
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 3, 3,
		3, 4, 4, 5, 5, 6, 7, 7, 8, 9, 9, 10, 11, 12, 13, 14,
		15, 16, 17, 18, 19, 21, 22, 24, 25, 27, 28, 30, 32, 33, 35, 37,
		39, 41, 44, 46, 48, 51, 53, 56, 58, 61, 64, 67, 70, 73, 77, 80,
		84, 87, 91, 95, 99, 103, 107, 111, 116, 120, 125, 130, 135, 140, 145, 150,
		156, 161, 167, 173, 179, 186, 192, 199, 205, 212, 219, 227, 234, 242, 250, 257,
		266, 274, 283, 291, 300, 309, 319, 328, 338, 348, 358, 369, 379, 390, 401, 412,
		424, 436, 448, 460, 473, 485, 498, 512, 525, 539, 553, 567, 582, 597, 612, 627,
		643, 659, 675, 692, 708, 726, 743, 761, 779, 797, 816, 835, 854, 874, 894, 914,
		935, 956, 977, 999, 1020, 1043, 1066, 1089, 1112, 1136, 1160, 1184, 1209, 1234, 1260, 1286,
		1312, 1339, 1366, 1394, 1422, 1450, 1479, 1508, 1537, 1567, 1598, 1628, 1660, 1691, 1723, 1756,
		1789, 1822, 1856, 1890, 1924, 1959, 1995, 2031, 2067, 2104, 2141, 2179, 2217, 2256, 2295, 2334,
		2374, 2415, 2456, 2497, 2539, 2582, 2624, 2668, 2712, 2756, 2801, 2846, 2892, 2938, 2985, 3032,
		3079, 3128, 3176, 3225, 3275, 3325, 3376, 3427, 3479, 3531, 3584, 3637, 3691, 3745, 3799, 3855,
		3910, 3967, 4023, 4081, 4138, 4197, 4255, 4315, 4374, 4435, 4495, 4557, 4619, 4681, 4744, 4807,
		4871, 4935, 5000, 5065, 5131, 5197, 5264, 5332, 5399, 5468, 5536, 5606, 5676, 5746, 5817, 5888,
		5959, 6032, 6104, 6177, 6251, 6325, 6400, 6475, 6550, 6626, 6702, 6779, 6856, 6934, 7012, 7091,
		7170, 7249, 7329, 7409, 7490, 7571, 7653, 7735, 7817, 7900, 7983, 8066, 8150, 8234, 8319, 8404,
		8489, 8575, 8661, 8748, 8834, 8922, 9009, 9097, 9185, 9273, 9362, 9451, 9541, 9630, 9720, 9811,
		9901, 9992, 10083, 10174, 10266, 10358, 10450, 10542, 10635, 10727, 10820, 10913, 11007, 11100, 11194, 11288,
		11382, 11476, 11571, 11665, 11760, 11855, 11950, 12045, 12140, 12236, 12331, 12427, 12522, 12618, 12714, 12809,
		12905, 13001, 13097, 13193, 13289, 13385, 13481, 13577, 13673, 13769, 13865, 13961, 14056, 14152, 14248, 14343,
		14439, 14534, 14630, 14725, 14820, 14915, 15010, 15104, 15199, 15293, 15387, 15481, 15575, 15669, 15762, 15855,
		15948, 16041, 16133, 16226, 16317, 16409, 16500, 16592, 16682, 16773, 16863, 16953, 17042, 17131, 17220, 17308,
		17396, 17484, 17571, 17658, 17744, 17830, 17916, 18001, 18086, 18170, 18254, 18337, 18420, 18502, 18584, 18665,
		18746, 18826, 18905, 18985, 19063, 19141, 19219, 19295, 19372, 19447, 19522, 19597, 19671, 19744, 19816, 19888,
		19959, 20030, 20100, 20169, 20238, 20306, 20373, 20439, 20505, 20570, 20634, 20698, 20760, 20822, 20884, 20944,
		21004, 21063, 21121, 21178, 21235, 21290, 21345, 21399, 21452, 21505, 21556, 21607, 21657, 21706, 21754, 21801,
		21848, 21893, 21938, 21982, 22025, 22066, 22107, 22148, 22187, 22225, 22262, 22299, 22334, 22369, 22402, 22435,
		22467, 22498, 22527, 22556, 22584, 22611, 22637, 22662, 22686, 22709, 22731, 22752, 22772, 22791, 22809, 22826,
		22842, 22857, 22872, 22885, 22897, 22908, 22918, 22927, 22935, 22942, 22948, 22953, 22957, 22960, 22962, 22963,
		};

		constexpr int32 LargestStep = 0x4000;
		constexpr int32 SampleShift = 12;
		constexpr uint32 SampleMask = 0xFFF;
		/** A block is 16 bytes: two of the addresses' 8-byte units. */
		constexpr uint32 BlockUnits = 2;
		constexpr uint32 AddressMask = 0xFFFF;
		constexpr int32 TopLevel = 0x7FFF;
		constexpr int32 SustainStep = 0x800;

		FSpuEnvelopeRate RateOf(bool bExponential, bool bDecreasing, uint32 Shift, uint32 Step, bool bFrozen)
		{
			FSpuEnvelopeRate Rate;
			Rate.bExponential = bExponential;
			Rate.bDecreasing = bDecreasing;
			Rate.Shift = static_cast<uint8>(Shift);
			Rate.Step = static_cast<uint8>(Step);
			Rate.bFrozen = bFrozen;
			return Rate;
		}
	}

	void FSpuVolume::Write(uint16 Value)
	{
		bSweep = (Value & 0x8000) != 0;
		if (!bSweep)
		{
			// Bits 0-14 are half the volume, signed.
			Level = static_cast<int16>(Value << 1);
			return;
		}
		Rate = RateOf((Value & 0x4000) != 0, (Value & 0x2000) != 0, (Value >> 2) & 0x1F, Value & 3, (Value & 0x7F) == 0x7F);
		Rate.bNegativePhase = (Value & 0x1000) != 0;
		Counter = 0;
	}

	void FSpuVolume::Tick()
	{
		if (bSweep)
		{
			Level = StepSpuEnvelope(Level, Rate, Counter);
		}
	}

	void FSpuNoise::Tick(uint16 Clock)
	{
		const int32 Shift = (Clock >> 2) & 0x0F;
		const int32 Step = (Clock & 3) + 4;
		Timer -= Step;
		if (Timer >= 0)
		{
			return;
		}
		const uint16 Parity = static_cast<uint16>(((Shifter >> 15) ^ (Shifter >> 12) ^ (Shifter >> 11) ^ (Shifter >> 10) ^ 1) & 1);
		Shifter = static_cast<uint16>(Shifter << 1 | Parity);
		Timer += 0x20000 >> Shift;
		if (Timer < 0)
		{
			Timer += 0x20000 >> Shift;
		}
	}

	void FSpuVoice::Write(uint32 Register, uint16 Value)
	{
		switch (Register)
		{
		case 0x00:
			Left.Write(Value);
			break;
		case 0x02:
			Right.Write(Value);
			break;
		case 0x04:
			Pitch = Value;
			break;
		case 0x06:
			Start = Value;
			break;
		case 0x08:
			Adsr1 = Value;
			break;
		case 0x0A:
			Adsr2 = Value;
			break;
		case 0x0C:
			Level = static_cast<int16>(Value);
			break;
		default:
			Repeat = Value;
			break;
		}
	}

	void FSpuVoice::KeyOn()
	{
		Address = Start;
		Counter = 0;
		bHasBlock = false;
		Samples.fill(0);
		Old = 0;
		Older = 0;
		Level = 0;
		EnterPhase(EPhase::Attack);
	}

	void FSpuVoice::KeyOff()
	{
		if (Phase != EPhase::Release)
		{
			EnterPhase(EPhase::Release);
		}
	}

	void FSpuVoice::EnterPhase(EPhase Next)
	{
		Phase = Next;
		EnvelopeCounter = 0;
		switch (Next)
		{
		case EPhase::Attack:
			Rate = RateOf((Adsr1 & 0x8000) != 0, false, (Adsr1 >> 10) & 0x1F, (Adsr1 >> 8) & 3, ((Adsr1 >> 8) & 0x7F) == 0x7F);
			Target = TopLevel;
			break;
		case EPhase::Decay:
			Rate = RateOf(true, true, (Adsr1 >> 4) & 0x0F, 0, false);
			Target = ((Adsr1 & 0x0F) + 1) * SustainStep;
			break;
		case EPhase::Sustain:
			Rate = RateOf((Adsr2 & 0x8000) != 0, (Adsr2 & 0x4000) != 0, (Adsr2 >> 8) & 0x1F, (Adsr2 >> 6) & 3, ((Adsr2 >> 6) & 0x7F) == 0x7F);
			break;
		case EPhase::Release:
			Rate = RateOf((Adsr2 & 0x20) != 0, true, Adsr2 & 0x1F, 0, (Adsr2 & 0x1F) == 0x1F);
			Target = 0;
			break;
		}
	}

	void FSpuVoice::TickEnvelope()
	{
		Level = StepSpuEnvelope(Level, Rate, EnvelopeCounter);
		if (Phase == EPhase::Attack && Level >= Target)
		{
			EnterPhase(EPhase::Decay);
		}
		else if (Phase == EPhase::Decay && Level <= Target)
		{
			EnterPhase(EPhase::Sustain);
		}
	}

	void FSpuVoice::LoadBlock(std::span<const uint8> Ram)
	{
		// The block before's last three samples stay for the interpolation.
		std::copy(Samples.end() - 3, Samples.end(), Samples.begin());
		const std::size_t At = static_cast<std::size_t>(Address & AddressMask) * 8;
		const std::span<const uint8> Block = Ram.subspan(At, SpuBlockBytes);
		Flags = Block[1];
		if (Flags & SpuLoopStart)
		{
			Repeat = static_cast<uint16>(Address);
		}
		DecodeSpuAdpcmBlock(Block, std::span<int16>(Samples).subspan(3), Old, Older);
		bHasBlock = true;
	}

	int32 FSpuVoice::Sample(std::span<const uint8> Ram, std::optional<int16> Noise, std::optional<int32> Modulator, uint32& Ended, uint32 Bit)
	{
		if (!bHasBlock)
		{
			LoadBlock(Ram);
		}
		int32 Input = 0;
		if (Noise)
		{
			Input = *Noise;
		}
		else
		{
			const std::size_t Index = Counter >> SampleShift;
			const std::size_t Weight = (Counter >> 4) & 0xFF;
			Input = (Interpolation[0xFF - Weight] * Samples[Index] >> 15) + (Interpolation[0x1FF - Weight] * Samples[Index + 1] >> 15)
				+ (Interpolation[0x100 + Weight] * Samples[Index + 2] >> 15) + (Interpolation[Weight] * Samples[Index + 3] >> 15);
		}
		const int32 Output = Input * Level >> 15;
		TickEnvelope();

		int32 Step = Pitch;
		if (Modulator)
		{
			// The pitch is taken as signed, scaled by 0 to 1.99, and the sign dropped again.
			Step = (static_cast<int16>(Pitch) * (*Modulator + 0x8000) >> 15) & 0xFFFF;
		}
		Counter += static_cast<uint32>(std::min(Step, LargestStep));
		if ((Counter >> SampleShift) >= static_cast<uint32>(SpuBlockSamples))
		{
			Counter = (Counter & SampleMask) | (((Counter >> SampleShift) - SpuBlockSamples) << SampleShift);
			bHasBlock = false;
			Address = (Address + BlockUnits) & AddressMask;
			if (Flags & SpuLoopEnd)
			{
				Ended |= Bit;
				Address = Repeat;
				if ((Flags & SpuLoopRepeat) == 0 && !Noise)
				{
					EnterPhase(EPhase::Release);
					Level = 0;
				}
			}
		}
		return Output;
	}
}
