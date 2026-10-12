#pragma once

#include "Foundation/Types.h"
#include "Sound/SoundDriverTables.h"
#include "Sound/SpuBus.h"

#include <span>

namespace Weltall
{
	/** The SPU's registers the driver and the sound library program, at offsets from 1F801C00. */
	namespace SpuRegister
	{
		inline constexpr uint32 VoiceStride = 0x10;
		inline constexpr uint32 VolumeLeft = 0x0;
		inline constexpr uint32 VolumeRight = 0x2;
		inline constexpr uint32 Pitch = 0x4;
		inline constexpr uint32 StartAddress = 0x6;
		inline constexpr uint32 Adsr1 = 0x8;
		inline constexpr uint32 Adsr2 = 0xA;
		inline constexpr uint32 Envelope = 0xC;
		inline constexpr uint32 LoopAddress = 0xE;
		inline constexpr uint32 MainLeft = 0x180;
		inline constexpr uint32 MainRight = 0x182;
		inline constexpr uint32 ReverbLeft = 0x184;
		inline constexpr uint32 ReverbRight = 0x186;
		inline constexpr uint32 KeyOn = 0x188;
		inline constexpr uint32 KeyOff = 0x18C;
		inline constexpr uint32 PitchModulation = 0x190;
		inline constexpr uint32 Noise = 0x194;
		inline constexpr uint32 Reverb = 0x198;
		inline constexpr uint32 ReverbStart = 0x1A2;
		inline constexpr uint32 TransferAddress = 0x1A6;
		inline constexpr uint32 TransferFifo = 0x1A8;
		inline constexpr uint32 Control = 0x1AA;
		inline constexpr uint32 TransferControl = 0x1AC;
		inline constexpr uint32 CdLeft = 0x1B0;
		inline constexpr uint32 CdRight = 0x1B2;
		inline constexpr uint32 ExternalLeft = 0x1B4;
		inline constexpr uint32 ExternalRight = 0x1B6;
		inline constexpr uint32 ReverbConfiguration = 0x1C0;

		constexpr uint32 OfVoice(int32 Voice, uint32 Register) { return static_cast<uint32>(Voice) * VoiceStride + Register; }
	}

	/** The common attributes the driver keeps for the sound library to apply: main and CD volumes, CD reverb and mix. */
	struct FSpuCommonAttributes
	{
		static constexpr uint32 MainLeftBit = 0x001;
		static constexpr uint32 MainRightBit = 0x002;
		static constexpr uint32 CdLeftBit = 0x040;
		static constexpr uint32 CdRightBit = 0x080;
		static constexpr uint32 CdReverbBit = 0x100;
		static constexpr uint32 CdMixBit = 0x200;

		uint32 Mask = 0;
		int16 MainLeft = 0;
		int16 MainRight = 0;
		int16 CdLeft = 0;
		int16 CdRight = 0;
		bool bCdReverb = false;
		bool bCdMix = false;
	};

	/**
	 * The PlayStation sound library's calls the driver makes, writing the SPU's registers as the
	 * library does: its start-up, the common attributes, the reverb's presets, depth, delay and
	 * feedback, the noise clock, and DMA transfers into sound RAM.
	 */
	class FSpuLibrary
	{
	public:
		FSpuLibrary(ISpuBus& InSpu, const FSoundDriverTables& InTables);

		/** The library's start-up: every register reset, and every voice left playing a silent block at 1000h. */
		void Start();
		/** Stops the SPU's interrupt (the driver never uses it). */
		void DisableInterrupt();
		void SetCommonAttributes(const FSpuCommonAttributes& Attributes);
		/** Programs reverb preset Type (0-9): the output muted, the 32 registers, the work area's start. */
		void SetReverbType(int32 Type);
		int32 ReverbType() const { return Type; }
		void SetReverbDepth(int16 Left, int16 Right);
		/** The echo and delay presets' delay time and feedback (0-127); other presets have none. */
		void SetReverbDelay(int32 Delay);
		void SetReverbFeedback(int32 Feedback);
		void EnableReverb();
		void SetNoiseClock(int32 Clock);
		/** A DMA write of Data into sound RAM at Address. */
		void Write(uint32 Address, std::span<const uint8> Data);
		/** A voice's envelope level, as its register reads. */
		int16 Envelope(int32 Voice) const;

	private:
		void Set(uint32 Register, uint16 Value) { Spu.Write(Register, Value); }
		uint16 Get(uint32 Register) const { return Spu.Read(Register); }
		bool HasDelay() const { return Type == 7 || Type == 8; }

		ISpuBus& Spu;
		const FSoundDriverTables& Tables;
		int32 Type = 0;
	};
}
