#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FSoundDriverTables;

	/** The sound driver's arithmetic: pitches, a voice's volumes from its levels and pan, envelope registers. */
	namespace SoundDriverMath
	{
		/** A pitch at which the SPU's step reaches 4000h, past its register's 14 bits: key 84. */
		inline constexpr int32 FirstPitchPastSpuRange = 0x5400;

		/**
		 * A pitch in 1/256 semitone (key 60 with no tuning, 3C00h, plays the sample at its own rate,
		 * a step of 1000h) as the SPU's step, before its register's 14-bit mask.
		 */
		WELTALLCORE_API int16 PitchToStep(const FSoundDriverTables& Tables, int16 Pitch);

		/**
		 * The PITCH register for a pitch: the step masked to 14 bits as the original does, so that
		 * notes from key 84 up wrap to low pitches, or with bHighNotesStayHigh the highest step instead.
		 */
		WELTALLCORE_API uint16 PitchRegister(const FSoundDriverTables& Tables, int16 Pitch, bool bHighNotesStayHigh);

		/** What a voice's volumes come from. */
		struct FVoiceMix
		{
			/** 8.24, 7F000000h full. */
			int32 SequenceVolume = 0;
			/** Added to the pan in its high half. */
			int32 SequencePan = 0;
			/** 8.24. */
			int32 TrackVolume = 0;
			/** A note's velocity << 8, or an effect's volume. */
			uint16 Expression = 0;
			/** 0 left, 4000h centre, 7F00h right. */
			uint16 Pan = 0;
			/** The tremolo and auto-pan LFOs' current values. */
			int16 LfoVolume = 0;
			int16 LfoPan = 0;
		};

		struct FVoiceVolumes
		{
			uint16 Left = 0;
			uint16 Right = 0;
		};

		/** VOLL and VOLR: in mono both from the level alone, in stereo through the driver's piecewise-linear pan law. */
		WELTALLCORE_API FVoiceVolumes VoiceVolumes(const FVoiceMix& Mix, bool bStereo);

		/**
		 * The envelope registers, field by field as the driver rewrites them: each keeps the register's
		 * other bits and adds its own (so an out-of-range value carries into its neighbours, as it does
		 * in the original). Modes are the instruments' 1, 3, 5, 7 (linear or exponential, up or down).
		 */
		WELTALLCORE_API uint16 WithAttack(uint16 Adsr1, uint8 Rate, uint8 Mode);
		WELTALLCORE_API uint16 WithDecay(uint16 Adsr1, uint8 Rate);
		WELTALLCORE_API uint16 WithSustainLevel(uint16 Adsr1, uint8 Level);
		WELTALLCORE_API uint16 WithSustainRate(uint16 Adsr2, uint8 Rate, uint8 Mode);
		WELTALLCORE_API uint16 WithRelease(uint16 Adsr2, uint8 Rate, uint8 Mode);
	}
}
