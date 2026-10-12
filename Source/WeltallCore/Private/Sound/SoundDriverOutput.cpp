#include "Sound/SoundDriverIntegers.h"
#include "Sound/SoundDriverState.h"

namespace Weltall
{
	using namespace SoundInteger;

	namespace
	{
		constexpr uint32 ReservedForErrors = 0x10000;
		constexpr uint32 ReservedForErrorsBytes = 0x2000;
		constexpr int32 StartingReverbType = 4;
		constexpr int16 StartingMasterVolume = 0x3FFF;
		constexpr int16 StartingCdVolume = 0x7FFF;
		constexpr int32 KeepReverbType = -1;
		constexpr int32 IgnoreReverb = -2;
		constexpr uint32 ReverbClearChunk = 0x800;
		/** The last chunk may be this long rather than leave a short one. */
		constexpr uint32 ReverbClearLastChunk = 0x840;
		constexpr uint32 TransferAddressMask = 0x7FFF8;

		const std::array<uint8, ReverbClearLastChunk> Silence{};

		constexpr uint16 PausedAdsr2 = 0x1FDF;
		constexpr uint16 PausedAttack = 0x7F00;
		constexpr uint16 ResumeRewrite = 0x1F5;
	}

	void FSoundDriver::FState::Start()
	{
		Library.Start();
		Library.DisableInterrupt();
		Ram.ReserveAt(ReservedForErrorsBytes, ReservedForErrors);
		Flags = static_cast<uint16>((Flags & ~DriverFlag::OutputMode) | DriverFlag::Stereo);
		ApplyVolumes();
		Library.SetReverbDepth(ReverbVolume[0], ReverbVolume[1]);
		Common.Mask |= FSpuCommonAttributes::CdReverbBit | FSpuCommonAttributes::CdMixBit;
		Common.bCdReverb = false;
		Common.bCdMix = true;
		Library.SetCommonAttributes(Common);

		MasterFade = {StartingMasterVolume << 16, 0, 0, StartingMasterVolume};
		MasterVolume = StartingMasterVolume;
		const std::array<int16, 2> Main = OutputPair(MasterVolume, false);
		Common.MainLeft = Main[0];
		Common.MainRight = Main[1];
		CdFade = {StartingCdVolume << 16, 0, 0, StartingCdVolume};
		CdVolume = StartingCdVolume;
		Common.CdLeft = StartingCdVolume;
		Common.CdRight = StartingCdVolume;
		Common.Mask |= FSpuCommonAttributes::MainLeftBit | FSpuCommonAttributes::MainRightBit | FSpuCommonAttributes::CdLeftBit | FSpuCommonAttributes::CdRightBit;

		// The effect player: a sequence of 16 tracks without a file, track n on voice n + 8.
		EffectPlayer.Flags = 0x0002;
		EffectPlayer.TrackCount = EffectTracks;
		ResetSequenceState(EffectPlayer);
		for (int32 Number = 0; Number < EffectTracks; ++Number)
		{
			FSoundTrack& Track = EffectPlayer.Tracks[static_cast<std::size_t>(Number)];
			Track.Number = static_cast<uint8>(Number);
			Track.Voice = static_cast<uint8>(VoiceCount - EffectTracks + Number);
		}
		Link(EffectPlayer);

		SetReverb(StartingReverbType, 0, 0, 0);
		Library.EnableReverb();
		RunTransfers();
	}

	std::array<int16, 2> FSoundDriver::FState::OutputPair(int16 Volume, bool bReverb) const
	{
		// The inverted modes turn one side upside down: of the main output and the reverb, opposite sides.
		const int16 Inverted = static_cast<int16>(-Volume);
		if (Flags & DriverFlag::Inverted)
		{
			const bool bMainRightInverted = (Flags & DriverFlag::InvertMainRight) != 0;
			return bMainRightInverted != bReverb ? std::array<int16, 2>{Volume, Inverted} : std::array<int16, 2>{Inverted, Volume};
		}
		return {Volume, Volume};
	}

	void FSoundDriver::FState::ApplyVolumes()
	{
		const std::array<int16, 2> Main = OutputPair(MasterVolume, false);
		Common.MainLeft = Main[0];
		Common.MainRight = Main[1];
		Common.CdLeft = static_cast<int16>(CdVolume);
		Common.CdRight = static_cast<int16>(CdVolume);
		ReverbVolume = OutputPair(ReverbDepth, true);
		Common.Mask |= FSpuCommonAttributes::MainLeftBit | FSpuCommonAttributes::MainRightBit | FSpuCommonAttributes::CdLeftBit | FSpuCommonAttributes::CdRightBit;
	}

	void FSoundDriver::FState::SetReverb(int32 Type, int16 Depth, int32 Delay, int32 Feedback)
	{
		if (Type == IgnoreReverb)
		{
			return;
		}
		if (Type == 0)
		{
			Feedback = 0;
			Delay = 0;
			Depth = 0;
		}
		else if (Type == KeepReverbType)
		{
			Type = ReverbType;
		}
		if (Type < 0 || Type >= FSoundDriverTables::ReverbTypes)
		{
			return;
		}
		// A new type takes a new work area at the top of sound RAM, which is cleared before the reverb
		// is heard; the area cleared is the one asked for, even when it could not be reserved.
		const bool bNewType = Library.ReverbType() != Type || Type == 0;
		const uint32 AreaBytes = Tables.ReverbAreaSizes[static_cast<std::size_t>(Type)];
		const uint32 AreaStart = FSoundRam::Size - AreaBytes;
		if (bNewType)
		{
			if (ReverbArea != -1)
			{
				Ram.Free(static_cast<uint32>(ReverbArea));
			}
			ReverbArea = static_cast<int32>(Ram.ReserveAt(AreaBytes, AreaStart));
			if (ReverbArea == 0)
			{
				Type = 0;
				Feedback = 0;
				Delay = 0;
				Depth = 0;
			}
		}
		ReverbType = static_cast<uint8>(Type);
		ReverbDelay = static_cast<uint8>(Delay);
		ReverbFeedback = static_cast<uint8>(Feedback);
		ReverbDepth = Depth;
		ApplyVolumes();
		if (bNewType)
		{
			Library.SetReverbDepth(0, 0);
			Library.SetReverbType(Type);
			ClearAddress = AreaStart;
			ClearRemaining = AreaBytes;
			Flags |= DriverFlag::ClearingReverb;
			ClearReverbStep();
		}
		else
		{
			Library.SetReverbDepth(ReverbVolume[0], ReverbVolume[1]);
			Library.SetReverbDelay(Delay);
			Library.SetReverbFeedback(Feedback);
		}
	}

	void FSoundDriver::FState::ClearReverbStep()
	{
		if (ClearRemaining == 0)
		{
			Library.SetReverbDepth(ReverbVolume[0], ReverbVolume[1]);
			Library.SetReverbDelay(ReverbDelay);
			Library.SetReverbFeedback(ReverbFeedback);
			Flags &= static_cast<uint16>(~DriverFlag::ClearingReverb);
			return;
		}
		const uint32 Bytes = ClearRemaining <= ReverbClearLastChunk ? ClearRemaining : ReverbClearChunk;
		const uint32 Address = ClearAddress;
		ClearRemaining -= Bytes;
		ClearAddress += Bytes;
		QueueWrite(Address, std::span<const uint8>(Silence).first(Bytes), true);
	}

	void FSoundDriver::FState::QueueWrite(uint32 Address, std::span<const uint8> Data, bool bClearsReverb)
	{
		Transfers[static_cast<std::size_t>(TransferCount++)] = {Address & TransferAddressMask, Data, bClearsReverb};
	}

	void FSoundDriver::FState::RunTransfers()
	{
		// Transfers complete in order once the call that queued them returns; a completed step of
		// the reverb's clearing queues the next one.
		while (TransferCount != 0)
		{
			const FSoundTransfer Transfer = Transfers[0];
			for (int32 Index = 1; Index < TransferCount; ++Index)
			{
				Transfers[static_cast<std::size_t>(Index - 1)] = Transfers[static_cast<std::size_t>(Index)];
			}
			--TransferCount;
			Library.Write(Transfer.Address, Transfer.Data);
			if (Transfer.bClearsReverb)
			{
				ClearReverbStep();
			}
		}
	}

	void FSoundDriver::SetMasterVolume(int32 Volume, int32 Steps)
	{
		FState& S = *State;
		S.MasterFade.Target = static_cast<int16>(Volume);
		if (Steps == 0)
		{
			S.MasterFade.Value = ShiftLeft(Volume, 16);
			S.MasterFade.Count = 0;
			S.MasterVolume = static_cast<int16>(Volume);
			const std::array<int16, 2> Main = S.OutputPair(S.MasterVolume, false);
			S.Common.MainLeft = Main[0];
			S.Common.MainRight = Main[1];
			S.Common.Mask |= FSpuCommonAttributes::MainLeftBit | FSpuCommonAttributes::MainRightBit;
			return;
		}
		const int32 Difference = Subtract(Volume * 0x100, S.MasterFade.Value >> 8);
		if (Difference != 0)
		{
			S.MasterFade.Count = static_cast<int16>(Steps);
			S.MasterFade.Step = ShiftLeft(Divide(Difference, Steps), 8);
		}
	}

	void FSoundDriver::SetCdVolume(int32 Volume, int32 Steps)
	{
		FState& S = *State;
		S.CdFade.Target = static_cast<int16>(Volume);
		if (Steps == 0)
		{
			S.CdFade.Value = ShiftLeft(Volume, 16);
			S.CdFade.Count = 0;
			S.Common.Mask |= FSpuCommonAttributes::CdLeftBit | FSpuCommonAttributes::CdRightBit;
			S.Common.CdLeft = static_cast<int16>(Volume);
			S.Common.CdRight = static_cast<int16>(Volume);
			S.CdVolume = static_cast<uint16>(Volume);
			return;
		}
		const int32 Difference = Subtract(Volume * 0x100, S.CdFade.Value >> 8);
		if (Difference != 0)
		{
			S.CdFade.Count = static_cast<int16>(Steps);
			S.CdFade.Step = ShiftLeft(Divide(Difference, Steps), 8);
		}
	}

	void FSoundDriver::SetCdAttributes(bool bReverb, bool bMix)
	{
		FState& S = *State;
		S.Common.Mask |= FSpuCommonAttributes::CdReverbBit | FSpuCommonAttributes::CdMixBit;
		S.Common.bCdReverb = bReverb;
		S.Common.bCdMix = bMix;
		S.Library.SetCommonAttributes(S.Common);
	}

	void FSoundDriver::SetOutputMode(int32 Mode)
	{
		FState& S = *State;
		S.Flags &= static_cast<uint16>(~DriverFlag::OutputMode);
		switch (Mode)
		{
		case 1:
			S.Flags |= DriverFlag::Stereo;
			break;
		case 2:
			S.Flags |= DriverFlag::Stereo | DriverFlag::InvertMainRight;
			break;
		case 3:
			S.Flags |= DriverFlag::Stereo | DriverFlag::InvertMainLeft;
			break;
		default:
			break;
		}
		S.ApplyVolumes();
		S.Library.SetReverbDepth(S.ReverbVolume[0], S.ReverbVolume[1]);
		for (int32 Index = 0; Index < S.SequenceCount; ++Index)
		{
			S.MarkTracks(TrackRequest::Volume, *S.Sequences[static_cast<std::size_t>(Index)]);
		}
	}

	int32 FSoundDriver::OutputMode() const
	{
		if ((State->Flags & DriverFlag::OutputMode) == 0)
		{
			return 0;
		}
		return (State->Flags & DriverFlag::Inverted) != 0 ? 2 : 1;
	}

	void FSoundDriver::SetOptions(const FSoundDriverOptions& Options)
	{
		State->Options = Options;
	}

	void FSoundDriver::SetReverb(int32 Type, int32 Depth, int32 Delay, int32 Feedback)
	{
		State->SetReverb(Type, static_cast<int16>(Depth), Delay, Feedback);
		State->RunTransfers();
	}

	void FSoundDriver::Pause()
	{
		using namespace SpuRegister;
		FState& S = *State;
		S.Flags |= DriverFlag::Paused;
		for (int32 Voice = 0; Voice < VoiceCount; ++Voice)
		{
			// Silent, not advancing, the envelope frozen at its slowest rates.
			S.Spu.Write(OfVoice(Voice, VolumeLeft), 0);
			S.Spu.Write(OfVoice(Voice, VolumeRight), 0);
			S.Spu.Write(OfVoice(Voice, SpuRegister::Pitch), 0);
			S.Spu.Write(OfVoice(Voice, Adsr2), PausedAdsr2);
			S.Spu.Write(OfVoice(Voice, Adsr1), static_cast<uint16>((S.Spu.Read(OfVoice(Voice, Adsr1)) & 0xFF) + PausedAttack));
		}
	}

	void FSoundDriver::Resume()
	{
		for (FSoundTrack* Track : State->Owners)
		{
			if (Track != nullptr)
			{
				Track->Rewrite |= ResumeRewrite;
			}
		}
		State->Flags &= static_cast<uint16>(~DriverFlag::Paused);
	}
}
