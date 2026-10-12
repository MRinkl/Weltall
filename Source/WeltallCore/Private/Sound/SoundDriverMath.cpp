#include "Sound/SoundDriverMath.h"

#include "Sound/SoundDriverTables.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 HighestStep = 0x3FFF;
		constexpr int32 PanCentre = 0x4000;
		constexpr int32 PanRight = 0x7F00;
		/** The pan law's gain at the centre (-3 dB) and its drop across each half, in 7F00h full scale. */
		constexpr int32 CentreGain = 0x5A00;
		constexpr int32 NearSideRise = 0x2500;
	}

	int16 SoundDriverMath::PitchToStep(const FSoundDriverTables& Tables, int16 Pitch)
	{
		const int32 Bits = static_cast<uint16>(Pitch);
		const uint8 OctaveAndNote = Tables.Octaves[static_cast<std::size_t>((Bits & 0x7FFF) >> 8)];
		const int32 Value = Tables.Pitches[static_cast<std::size_t>((Bits & 0xFF) + (OctaveAndNote & 0xF) * 256)];
		const int32 Shift = 6 - (OctaveAndNote >> 4);
		return static_cast<int16>(Shift >= 0 ? Value >> Shift : Value << -Shift);
	}

	uint16 SoundDriverMath::PitchRegister(const FSoundDriverTables& Tables, int16 Pitch, bool bHighNotesStayHigh)
	{
		if (bHighNotesStayHigh && Pitch >= FirstPitchPastSpuRange)
		{
			return HighestStep;
		}
		return static_cast<uint16>(PitchToStep(Tables, Pitch) & HighestStep);
	}

	SoundDriverMath::FVoiceVolumes SoundDriverMath::VoiceVolumes(const FVoiceMix& Mix, bool bStereo)
	{
		const int32 TrackLevel = static_cast<int16>(static_cast<uint32>(Mix.TrackVolume) >> 16);
		// A tremolo only ever lowers the level.
		const int32 Level = std::clamp(TrackLevel - (TrackLevel * Mix.LfoVolume >> 15), 0, 0x7FFF);
		const int32 Pan = std::clamp(static_cast<int16>(Mix.Pan) + Mix.LfoPan + static_cast<int16>(static_cast<uint32>(Mix.SequencePan) >> 16), 0, PanRight);
		const int32 Scaled = static_cast<int16>(static_cast<uint32>(Mix.SequenceVolume) >> 16) * (static_cast<int16>(Mix.Expression) * Level >> 15) >> 16;
		if (!bStereo)
		{
			const uint16 Both = static_cast<uint16>(Scaled * CentreGain >> 15);
			return {Both, Both};
		}
		int32 LeftGain;
		int32 RightGain;
		if (Pan < PanCentre)
		{
			RightGain = Pan * CentreGain >> 14;
			LeftGain = PanRight - (Pan * NearSideRise >> 14);
		}
		else
		{
			LeftGain = (0x8000 - Pan) * CentreGain >> 14;
			RightGain = PanRight - ((0x8000 - Pan) * NearSideRise >> 14);
		}
		return {static_cast<uint16>(LeftGain * Scaled >> 15), static_cast<uint16>(RightGain * Scaled >> 15)};
	}

	uint16 SoundDriverMath::WithAttack(uint16 Adsr1, uint8 Rate, uint8 Mode)
	{
		return static_cast<uint16>((Adsr1 & 0xFF) + Rate * 0x100 + (Mode >> 2) * 0x8000);
	}

	uint16 SoundDriverMath::WithDecay(uint16 Adsr1, uint8 Rate)
	{
		return static_cast<uint16>((Adsr1 & 0xFF0F) + Rate * 0x10);
	}

	uint16 SoundDriverMath::WithSustainLevel(uint16 Adsr1, uint8 Level)
	{
		return static_cast<uint16>(Level + (Adsr1 & 0xFFF0));
	}

	uint16 SoundDriverMath::WithSustainRate(uint16 Adsr2, uint8 Rate, uint8 Mode)
	{
		return static_cast<uint16>((Adsr2 & 0x3F) + Rate * 0x40 + (Mode >> 1) * 0x4000);
	}

	uint16 SoundDriverMath::WithRelease(uint16 Adsr2, uint8 Rate, uint8 Mode)
	{
		return static_cast<uint16>((Adsr2 & 0xFFC0) + Rate + (Mode >> 2) * 0x20);
	}
}
