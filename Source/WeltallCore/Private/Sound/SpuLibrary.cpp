#include "Sound/SpuLibrary.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 VoiceCount = 24;
		constexpr uint16 ControlTransferMode = 0x0030;
		constexpr uint16 ControlManualWrite = 0x0010;
		constexpr uint16 ControlDmaWrite = 0x0020;
		constexpr uint16 ControlInterrupt = 0x0040;
		constexpr uint16 ControlReverb = 0x0080;
		constexpr uint16 ControlCdReverb = 0x0004;
		constexpr uint16 ControlCdAudio = 0x0001;
		constexpr uint16 ControlNoiseClock = 0x3F00;
		constexpr uint16 ControlStarted = 0xC000;
		/** The silent block's place in sound RAM, in 8-byte units. */
		constexpr uint16 SilentBlockUnits = 0x200;
		constexpr uint16 HighestPitch = 0x3FFF;
		/** Largest single DMA transfer the library allows. */
		constexpr std::size_t MaxTransfer = 0x7EFF0;
		constexpr int32 MaxNoiseClock = 0x3F;
		constexpr int32 MaxDelay = 0x7F;
	}

	FSpuLibrary::FSpuLibrary(ISpuBus& InSpu, const FSoundDriverTables& InTables)
		: Spu(InSpu), Tables(InTables)
	{
	}

	void FSpuLibrary::Start()
	{
		using namespace SpuRegister;
		Set(MainLeft, 0);
		Set(MainRight, 0);
		Set(Control, 0);
		Set(MainLeft, 0);
		Set(MainRight, 0);
		Set(TransferControl, 4);
		Set(ReverbLeft, 0);
		Set(ReverbRight, 0);
		Set(KeyOff, 0xFFFF);
		Set(KeyOff + 2, 0xFFFF);
		Set(Reverb, 0);
		Set(Reverb + 2, 0);
		Set(PitchModulation, 0);
		Set(PitchModulation + 2, 0);
		Set(Noise, 0);
		Set(Noise + 2, 0);
		Set(CdLeft, 0);
		Set(CdRight, 0);
		Set(ExternalLeft, 0);
		Set(ExternalRight, 0);
		Set(TransferAddress, SilentBlockUnits);
		for (std::size_t Byte = 0; Byte < Tables.SilentBlock.size(); Byte += 2)
		{
			Set(TransferFifo, static_cast<uint16>(Tables.SilentBlock[Byte] | Tables.SilentBlock[Byte + 1] << 8));
		}
		Set(Control, static_cast<uint16>((Get(Control) & ~ControlTransferMode) | ControlManualWrite));
		Set(Control, static_cast<uint16>(Get(Control) & ~ControlTransferMode));
		for (int32 Voice = 0; Voice < VoiceCount; ++Voice)
		{
			Set(OfVoice(Voice, VolumeLeft), 0);
			Set(OfVoice(Voice, VolumeRight), 0);
			Set(OfVoice(Voice, SpuRegister::Pitch), HighestPitch);
			Set(OfVoice(Voice, StartAddress), SilentBlockUnits);
			Set(OfVoice(Voice, Adsr1), 0);
			Set(OfVoice(Voice, Adsr2), 0);
		}
		Set(KeyOn, 0xFFFF);
		Set(KeyOn + 2, 0x00FF);
		Set(KeyOff, 0xFFFF);
		Set(KeyOff + 2, 0x00FF);
		Set(Control, ControlStarted);
		Set(ReverbStart, Tables.ReverbPresets[0].Start);
		Type = 0;
	}

	void FSpuLibrary::DisableInterrupt()
	{
		Set(SpuRegister::Control, static_cast<uint16>(Get(SpuRegister::Control) & ~ControlInterrupt));
	}

	void FSpuLibrary::SetCommonAttributes(const FSpuCommonAttributes& Attributes)
	{
		using namespace SpuRegister;
		const uint32 Mask = Attributes.Mask;
		if (Mask & FSpuCommonAttributes::MainLeftBit)
		{
			Set(MainLeft, static_cast<uint16>(Attributes.MainLeft & 0x7FFF));
		}
		if (Mask & FSpuCommonAttributes::MainRightBit)
		{
			Set(MainRight, static_cast<uint16>(Attributes.MainRight & 0x7FFF));
		}
		if (Mask & FSpuCommonAttributes::CdLeftBit)
		{
			Set(CdLeft, static_cast<uint16>(Attributes.CdLeft));
		}
		if (Mask & FSpuCommonAttributes::CdRightBit)
		{
			Set(CdRight, static_cast<uint16>(Attributes.CdRight));
		}
		if (Mask & FSpuCommonAttributes::CdReverbBit)
		{
			Set(Control, static_cast<uint16>(Attributes.bCdReverb ? Get(Control) | ControlCdReverb : Get(Control) & ~ControlCdReverb));
		}
		if (Mask & FSpuCommonAttributes::CdMixBit)
		{
			Set(Control, static_cast<uint16>(Attributes.bCdMix ? Get(Control) | ControlCdAudio : Get(Control) & ~ControlCdAudio));
		}
	}

	void FSpuLibrary::SetReverbType(int32 NewType)
	{
		using namespace SpuRegister;
		// The reverb is switched off while its registers change and back on afterwards.
		const bool bOn = (Get(Control) & ControlReverb) != 0;
		if (bOn)
		{
			Set(Control, static_cast<uint16>(Get(Control) & ~ControlReverb));
		}
		Set(ReverbLeft, 0);
		Set(ReverbRight, 0);
		const FSpuReverbPreset& Preset = Tables.ReverbPresets[static_cast<std::size_t>(NewType)];
		for (std::size_t Register = 0; Register < Preset.Registers.size(); ++Register)
		{
			Set(ReverbConfiguration + static_cast<uint32>(Register) * 2, Preset.Registers[Register]);
		}
		Set(ReverbStart, Preset.Start);
		if (bOn)
		{
			Set(Control, static_cast<uint16>(Get(Control) | ControlReverb));
		}
		Type = NewType;
	}

	void FSpuLibrary::SetReverbDepth(int16 Left, int16 Right)
	{
		Set(SpuRegister::ReverbLeft, static_cast<uint16>(Left));
		Set(SpuRegister::ReverbRight, static_cast<uint16>(Right));
	}

	void FSpuLibrary::SetReverbDelay(int32 Delay)
	{
		if (!HasDelay())
		{
			return;
		}
		// The echo and delay presets move their reflections and combs with the delay: whole spans
		// of 2000h and 1000h units for a delay of 127, taken from the preset's own offsets.
		const auto& Preset = Tables.ReverbPresets[static_cast<std::size_t>(Type)].Registers;
		const int16 Long = static_cast<int16>(Delay * 0x2000 / MaxDelay);
		const int16 Short = static_cast<int16>(Delay * 0x1000 / MaxDelay);
		const auto Register = [](int32 Index) { return SpuRegister::ReverbConfiguration + static_cast<uint32>(Index) * 2; };
		Set(Register(10), static_cast<uint16>(Long - static_cast<int16>(Preset[0])));
		Set(Register(11), static_cast<uint16>(Short - static_cast<int16>(Preset[1])));
		Set(Register(12), static_cast<uint16>(Preset[13] + Short));
		Set(Register(16), static_cast<uint16>(Preset[17] + Short));
		Set(Register(26), static_cast<uint16>(Preset[28] + Short));
		Set(Register(27), static_cast<uint16>(Preset[29] + Short));
	}

	void FSpuLibrary::SetReverbFeedback(int32 Feedback)
	{
		if (HasDelay())
		{
			Set(SpuRegister::ReverbConfiguration + 7 * 2, static_cast<uint16>(Feedback * 0x8100 / MaxDelay));
		}
	}

	void FSpuLibrary::EnableReverb()
	{
		Set(SpuRegister::Control, static_cast<uint16>(Get(SpuRegister::Control) | ControlReverb));
	}

	void FSpuLibrary::SetNoiseClock(int32 Clock)
	{
		const int32 Clamped = std::clamp(Clock, 0, MaxNoiseClock);
		Set(SpuRegister::Control, static_cast<uint16>((Get(SpuRegister::Control) & ~ControlNoiseClock) | Clamped << 8));
	}

	void FSpuLibrary::Write(uint32 Address, std::span<const uint8> Data)
	{
		using namespace SpuRegister;
		Set(TransferAddress, static_cast<uint16>(Address >> 3));
		Set(Control, static_cast<uint16>((Get(Control) & ~ControlTransferMode) | ControlDmaWrite));
		Spu.WriteDma(Data.first(std::min(Data.size(), MaxTransfer)));
		Set(Control, static_cast<uint16>(Get(Control) & ~ControlTransferMode));
	}

	int16 FSpuLibrary::Envelope(int32 Voice) const
	{
		return static_cast<int16>(Get(SpuRegister::OfVoice(Voice, SpuRegister::Envelope)));
	}
}
