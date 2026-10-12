#pragma once

#include "Foundation/Types.h"
#include "Sound/SpuAdpcm.h"
#include "Sound/SpuEnvelope.h"

#include <array>
#include <optional>
#include <span>

namespace Weltall
{
	/** A volume register of the SPU: set directly, or sweeping up or down by the envelope operation. */
	struct FSpuVolume
	{
		int16 Level = 0;

		void Write(uint16 Value);
		/** One sample of the sweep, if the volume sweeps. */
		void Tick();

	private:
		bool bSweep = false;
		FSpuEnvelopeRate Rate;
		uint32 Counter = 0;
	};

	/** The SPU's noise generator, a shift register every noise voice plays. */
	class FSpuNoise
	{
	public:
		int16 Level() const { return static_cast<int16>(Shifter); }

		/** One sample, at the frequency the control register's noise shift and step give. */
		void Tick(uint16 Clock);

	private:
		uint16 Shifter = 0;
		int32 Timer = 0;
	};

	/** One of the SPU's 24 voices: its registers, its ADPCM playback and its envelope. */
	struct FSpuVoice
	{
		FSpuVolume Left;
		FSpuVolume Right;
		/** The envelope's level (ENVX). */
		int16 Level = 0;
		/** The loop address, in 8-byte units, which a block's loop-start flag sets. */
		uint16 Repeat = 0;

		/** A write to one of the voice's registers, 0 to 0x0E. */
		void Write(uint32 Register, uint16 Value);
		void KeyOn();
		void KeyOff();

		/**
		 * One sample: the voice's output after its envelope, then its envelope and its playback move
		 * on. Noise gives the noise's level for a noise voice; Modulator the voice before's output when
		 * it modulates this one's pitch. Reaching a loop end sets Bit in Ended.
		 */
		int32 Sample(std::span<const uint8> Ram, std::optional<int16> Noise, std::optional<int32> Modulator, uint32& Ended, uint32 Bit);

	private:
		enum class EPhase : uint8
		{
			Attack,
			Decay,
			Sustain,
			Release,
		};

		void EnterPhase(EPhase Next);
		void TickEnvelope();
		void LoadBlock(std::span<const uint8> Ram);

		uint16 Pitch = 0;
		uint16 Start = 0;
		uint16 Adsr1 = 0;
		uint16 Adsr2 = 0;

		/** The block playing, in 8-byte units, and the pitch counter within it (bits 12 and up the sample). */
		uint32 Address = 0;
		uint32 Counter = 0;
		bool bHasBlock = false;
		uint8 Flags = 0;
		/** The three last samples of the block before, then the block's 28, for the interpolation. */
		std::array<int16, 3 + SpuBlockSamples> Samples{};
		int16 Old = 0;
		int16 Older = 0;

		/** A voice never keyed on rests in release at level 0. */
		EPhase Phase = EPhase::Release;
		FSpuEnvelopeRate Rate{false, true};
		uint32 EnvelopeCounter = 0;
		int32 Target = 0;
	};
}
