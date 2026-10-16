#include "Formats/Wave.h"
#include "Sound/SoundDriverIntegers.h"
#include "Sound/SoundDriverState.h"

namespace Weltall
{
	using namespace SoundInteger;

	namespace
	{
		constexpr int32 TickUnit = 0x10000;
	}

	void FSoundFade::Run()
	{
		const int16 Before = Count;
		Count = static_cast<int16>(Before - 1);
		Value = Before == 1 ? static_cast<int32>(static_cast<uint32>(static_cast<uint16>(Target)) << 16) : Add(Value, Step);
	}

	void FSoundDriver::FState::RunInterrupt()
	{
		if (Flags & DriverFlag::Paused)
		{
			return;
		}
		const uint32 Previous = InterruptCount++;
		// The main and CD volume fades step on every other interrupt.
		if (Previous & 1)
		{
			StepCommonFades();
		}
		FlushVoices();
		for (int32 Index = 0; Index < SequenceCount; ++Index)
		{
			FSoundSequence& Sequence = *Sequences[static_cast<std::size_t>(Index)];
			if (Sequence.IsPlaying())
			{
				Advance(Sequence);
			}
		}
		for (int32 Index = 0; Index < SequenceCount; ++Index)
		{
			FSoundSequence& Sequence = *Sequences[static_cast<std::size_t>(Index)];
			if (Sequence.IsPlaying() && Sequence.TrackCount != 0)
			{
				StepLfos(Sequence);
				UpdateVoices(Sequence);
			}
		}
		KeyOffVoices();
	}

	void FSoundDriver::FState::StepCommonFades()
	{
		if (MasterFade.Count != 0)
		{
			MasterFade.Run();
			MasterVolume = High(MasterFade.Value);
			const std::array<int16, 2> Pair = OutputPair(MasterVolume, false);
			Common.MainLeft = Pair[0];
			Common.MainRight = Pair[1];
			Common.Mask |= FSpuCommonAttributes::MainLeftBit | FSpuCommonAttributes::MainRightBit;
		}
		if (CdFade.Count != 0)
		{
			CdFade.Run();
			CdVolume = static_cast<uint16>(High(CdFade.Value));
			Common.CdLeft = static_cast<int16>(CdVolume);
			Common.CdRight = static_cast<int16>(CdVolume);
			Common.Mask |= FSpuCommonAttributes::CdLeftBit | FSpuCommonAttributes::CdRightBit;
		}
		if (Common.Mask != 0)
		{
			Library.SetCommonAttributes(Common);
			Common.Mask = 0;
		}
	}

	void FSoundDriver::FState::Advance(FSoundSequence& Sequence)
	{
		if (Sequence.Scale.Count != 0)
		{
			Sequence.Scale.Run();
			Sequence.TempoStep = High(Sequence.Tempo) * High(Sequence.Scale.Value);
		}
		if (Sequence.Volume.Count != 0)
		{
			Sequence.Volume.Run();
			MarkTracks(TrackRequest::Volume, Sequence);
		}
		if (Sequence.Pitch.Count != 0)
		{
			Sequence.Pitch.Run();
			MarkTracks(TrackRequest::Pitch, Sequence);
		}
		if (Sequence.Pan.Count != 0)
		{
			Sequence.Pan.Run();
			MarkTracks(TrackRequest::Volume, Sequence);
		}
		// The accumulator loses tempo * scale every interrupt and runs a tick each time it drops below 0.
		Sequence.TempoAccumulator = Subtract(Sequence.TempoAccumulator, Sequence.TempoStep);
		while (Sequence.TempoAccumulator < 0)
		{
			Sequence.TempoAccumulator = Add(Sequence.TempoAccumulator, TickUnit);
			if (--Sequence.BeatTicksLeft == 0)
			{
				Sequence.BeatTicksLeft = Sequence.TicksPerBeat;
				++Sequence.Beat;
				if (Sequence.BeatsPerMeasure < Sequence.Beat)
				{
					Sequence.Beat = 1;
					++Sequence.Measure;
				}
			}
			if (Sequence.TrackCount != 0)
			{
				StepTracks(Sequence);
				ReadTracks(Sequence);
			}
			if (Sequence.Active == 0)
			{
				Sequence.Flags &= static_cast<uint16>(~SequenceFlag::Playing);
				Sequence.bWaveTail = Sequence.File != nullptr && Sequence.File->Replacement && !Sequence.File->Replacement->Loop;
				return;
			}
			// A fade to silence ends in a pause, which fading back up ends.
			if (Sequence.Volume.Value == 0)
			{
				StopSequence(Sequence);
				Sequence.Flags |= SequenceFlag::WaitsForVolume;
			}
			if (Sequence.Measure == Sequence.SeekMeasure)
			{
				Sequence.Flags &= static_cast<uint16>(~SequenceFlag::Seeking);
				SetTempoScale(Sequence, 0, 0);
				Sequence.SeekMeasure = 0;
			}
		}
	}

	void FSoundDriver::FState::StepTracks(FSoundSequence& Sequence)
	{
		const int16 SlideCount = Sequence.TempoSlideCount;
		if (SlideCount != 0)
		{
			Sequence.Tempo = SlideCount == 1 ? static_cast<int32>(static_cast<uint32>(Sequence.TempoSlideTarget) << 16) : Add(Sequence.Tempo, Sequence.TempoSlideStep);
			Sequence.TempoSlideCount = static_cast<int16>(SlideCount - 1);
			Sequence.TempoStep = High(Sequence.Tempo) * High(Sequence.Scale.Value);
		}
		for (FSoundTrack& Track : Sequence.Used())
		{
			if (Track.Flags == 0 || Track.Length == 0)
			{
				continue;
			}
			uint16 Requests = Track.Requests;
			uint16 Slides = Track.Slides;
			if (Slides & TrackSlide::Volume)
			{
				Requests |= TrackRequest::Volume;
				if (--Track.VolumeSlideTicks == 0)
				{
					Slides &= static_cast<uint16>(~TrackSlide::Volume);
				}
				Track.Volume = Add(Track.Volume, Track.VolumeSlideStep);
			}
			if (Slides & TrackSlide::Pitch)
			{
				Requests |= TrackRequest::Pitch;
				if (!(Slides & TrackSlide::HoldPitch) && --Track.PitchSlideTicks == 0)
				{
					Slides &= static_cast<uint16>(~TrackSlide::Pitch);
				}
				Track.Pitch = Add(Track.Pitch, Track.PitchSlideStep);
			}
			if (Slides & TrackSlide::Pan)
			{
				if (--Track.PanSlideTicks == 0)
				{
					Track.Pan = Track.PanSlideTarget;
					Slides &= static_cast<uint16>(~TrackSlide::Pan);
				}
				else
				{
					Track.Pan = static_cast<uint16>(Track.Pan + Track.PanSlideStep);
				}
				Requests |= TrackRequest::Volume;
			}
			if (Slides & TrackSlide::Expression)
			{
				if (--Track.ExpressionSlideTicks == 0)
				{
					Track.Expression = Track.ExpressionSlideTarget;
					Slides &= static_cast<uint16>(~TrackSlide::Expression);
				}
				else
				{
					Track.Expression = static_cast<uint16>(Track.Expression + Track.ExpressionSlideStep);
				}
				Requests |= TrackRequest::Volume;
			}
			Track.Slides = Slides;
			// Length and gate count down as one 32-bit word, so a gate already at 0 wraps to FFFFh and never fires.
			const uint32 Word = (static_cast<uint32>(Track.Gate) << 16 | Track.Length) - 0x10001u;
			const uint16 Length = static_cast<uint16>(Word);
			const uint16 Gate = static_cast<uint16>(Word >> 16);
			if (Length == 1 && (Track.Flags & TrackFlag::NoteAhead))
			{
				Track.ReleaseRate = 6;
				Track.Rewrite |= VoiceRewrite::Release;
			}
			if (Gate == 0)
			{
				Requests |= TrackRequest::KeyOff;
				Track.Flags |= TrackFlag::Released;
			}
			Track.Length = Length;
			Track.Gate = Gate;
			Track.Requests = Requests;
		}
	}
}
