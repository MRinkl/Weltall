#include "Sound/SoundDriverIntegers.h"
#include "Sound/SoundDriverState.h"

namespace Weltall
{
	using namespace SoundInteger;

	namespace
	{
		constexpr uint8 Rest = 0x80;
		constexpr uint8 Tie = 0x81;
		constexpr uint8 End = 0x90;
		constexpr uint8 RepeatEnd = 0x99;
		constexpr uint8 RepeatBreak = 0x9A;
		constexpr uint8 LegatoOn = 0xB0;
		constexpr uint8 LegatoOff = 0xB1;

		constexpr uint16 GateLengthLessOne = 0x0F;
		constexpr uint16 GateFullLength = 0x10;
		constexpr uint16 GateHeld = 0x7FFF;

		/**
		 * Commands a single step may read before a note, rest or tie. The original loops forever on
		 * data that never gets there (a loop point right before its end); the port ends the track.
		 */
		constexpr int32 CommandsPerStep = 0x10000;
		/** Likewise for reading ahead, which the original can loop in forever on such data. */
		constexpr int32 LookAheadSteps = 0x10000;

		int32 NotePitch(int32 Key, const FSoundTrack& Track)
		{
			return ShiftLeft(Key * 0x100 + Track.Detune + Track.Tuning, 16);
		}
	}

	void FSoundDriver::FState::EndTrack(FSoundTrack& Track)
	{
		Track.Requests &= static_cast<uint16>(~(TrackRequest::KeyOn | TrackRequest::KeyOff));
		ReleaseVoice(Track, Track.Voice);
		Track.Flags = 0;
	}

	void FSoundDriver::FState::ReadTracks(FSoundSequence& Sequence)
	{
		for (FSoundTrack& Track : Sequence.Used())
		{
			if (Track.Flags == 0 || Track.Length != 0)
			{
				continue;
			}
			const uint16 Before = Track.Flags;
			FSoundCursor At = Track.Current;
			Track.Flags &= static_cast<uint16>(~(TrackFlag::StepDone | TrackFlag::Sustained | TrackFlag::Released));
			bool bPlayed = false;
			for (int32 Read = 0; Track.Flags != 0 && !(Track.Flags & (TrackFlag::StepDone | TrackFlag::Released)); ++Read)
			{
				if (Read == CommandsPerStep)
				{
					EndTrack(Track);
					break;
				}
				const uint8 Code = At.Byte();
				if (Code < 0x80)
				{
					At = PlayNote(Sequence, Track, At, Before);
					bPlayed = true;
				}
				else
				{
					At = RunCommand(Code, Sequence, Track, At.At(1));
				}
			}
			Track.Current = At;
			if (Track.Flags == 0)
			{
				Sequence.Active &= ~(1u << Track.Number);
				continue;
			}
			LookAhead(Track, bPlayed);
		}
	}

	FSoundCursor FSoundDriver::FState::PlayNote(FSoundSequence& Sequence, FSoundTrack& Track, FSoundCursor At, uint16 FlagsBefore)
	{
		if (!(Track.Flags & TrackFlag::IgnoreVelocity))
		{
			Track.Expression = static_cast<uint16>(At.Byte() << 8);
		}
		Track.Requests |= TrackRequest::Volume;
		const uint8 NoteByte = At.Byte(1);
		const uint8 Key = static_cast<uint8>(Track.Octave + NoteByte / SoundFormat::NoteLengths.size());
		Track.Key = Key;
		uint16 Length = SoundFormat::NoteLengths[NoteByte % SoundFormat::NoteLengths.size()];
		At = At.At(2);
		if (Length == 0)
		{
			Length = At.Byte();
			At = At.At(1);
		}
		Track.Length = Length;
		Track.ReleaseRate = Track.InstrumentRelease;
		Track.Rewrite |= VoiceRewrite::Release;
		if (Track.Flags & TrackFlag::Percussion)
		{
			PlayPercussion(Sequence, Track, Key);
		}
		else
		{
			Track.Pitch = NotePitch(Key, Track);
		}
		Track.Requests |= TrackRequest::Pitch;
		Track.Flags |= TrackFlag::StepDone | TrackFlag::NotePlayed;
		// A note keys on only after a release (a rest, or its gate); otherwise it slurs from the last.
		if (FlagsBefore & TrackFlag::Released)
		{
			Track.Requests |= TrackRequest::KeyOn;
		}
		if (Track.Flags & TrackFlag::InstrumentChanged)
		{
			Track.Flags &= static_cast<uint16>(~TrackFlag::InstrumentChanged);
			Track.Rewrite = VoiceRewrite::All;
			Track.Requests |= TrackRequest::Volume | TrackRequest::Pitch;
		}
		return At;
	}

	void FSoundDriver::FState::PlayPercussion(const FSoundSequence& Sequence, FSoundTrack& Track, uint8 Key)
	{
		// Keys past the table read what follows it in the original; the port reads an empty entry.
		const FSoundPercussion Entry = Sequence.File != nullptr && Key < FSoundSequenceFile::PercussionKeys
			? Sequence.File->Percussion(Key) : FSoundPercussion{};
		SetInstrument(Entry.Instrument, Track);
		Track.Pitch = NotePitch(Entry.Key, Track);
		Track.Requests |= TrackRequest::Volume;
		Track.Pan = static_cast<uint16>(Entry.Pan << 8);
	}

	void FSoundDriver::FState::LookAhead(FSoundTrack& Track, bool bPlayed)
	{
		if (Track.Flags & TrackFlag::Legato)
		{
			Track.Flags |= TrackFlag::Sustained;
		}
		// Whether a note comes next decides the gate: look past loops and repeats, and over other
		// commands by the driver's own length table (which disagrees with two handlers).
		FSoundCursor At = Track.Current;
		int32 Level = Track.RepeatLevel;
		const auto RepeatAt = [&Track](int32 Depth) {
			return Depth >= 0 && Depth < FSoundTrack::RepeatDepth ? Track.Repeats[static_cast<std::size_t>(Depth)] : FSoundRepeat{};
		};
		bool bNote = false;
		for (int32 Step = 0; Step < LookAheadSteps && At.IsSet() && At.Holds(0); ++Step)
		{
			const uint8 Code = At.Byte();
			if (Code < 0x80)
			{
				bNote = true;
				break;
			}
			if (Code == End)
			{
				if (!Track.LoopPoint.IsSet())
				{
					break;
				}
				At = Track.LoopPoint;
			}
			else if (Code == Rest || Code == LegatoOn || Code == LegatoOff)
			{
				Track.Flags &= static_cast<uint16>(~TrackFlag::Sustained);
				break;
			}
			else if (Code == Tie)
			{
				Track.Flags |= TrackFlag::Sustained;
				break;
			}
			else
			{
				FSoundRepeat Repeat = RepeatAt(Level);
				if (Code == RepeatEnd && Repeat.Count != 0)
				{
					At = Repeat.Start;
				}
				else
				{
					if (Code == RepeatEnd)
					{
						Repeat = RepeatAt(--Level);
					}
					if (Code == RepeatBreak && Repeat.Count == 0)
					{
						At = Repeat.Exit;
						--Level;
					}
					else
					{
						At = At.At(Tables.CommandLengths[Code - 0x80]);
					}
				}
			}
		}
		if (bNote)
		{
			Track.Flags |= TrackFlag::NoteAhead;
		}
		else
		{
			Track.Flags &= static_cast<uint16>(~TrackFlag::NoteAhead);
		}

		int32 Total = static_cast<int32>(Track.LengthAdjust) + Track.Length;
		if (static_cast<int16>(Total) < 1)
		{
			Total = Track.Length + Total;
			Track.LengthAdjust = static_cast<int8>(Track.LengthAdjust + Track.Length);
		}
		int32 Gate = GateHeld;
		if (!(Track.Flags & (TrackFlag::Sustained | TrackFlag::Released)))
		{
			const uint16 Ratio = Track.GateRatio;
			if (Ratio == GateLengthLessOne)
			{
				Gate = Total - 1;
			}
			else if (Ratio == GateFullLength)
			{
				Gate = Total;
			}
			else
			{
				Gate = static_cast<int16>(Total) * Ratio >> 4;
			}
			if (Ratio != GateFullLength && (Gate & 0xFFFF) == 0)
			{
				Gate = 1;
			}
		}
		const uint32 Word = static_cast<uint32>(static_cast<int32>(static_cast<int16>(Total))) + (static_cast<uint32>(Gate) << 16);
		Track.Length = static_cast<uint16>(Word);
		Track.Gate = static_cast<uint16>(Word >> 16);

		if (!bPlayed)
		{
			return;
		}
		if (Track.Slides & TrackSlide::Portamento)
		{
			const int32 Difference = ShiftLeft(Track.Key - Track.PreviousKey, 24);
			if (Difference != 0)
			{
				Track.Slides |= TrackSlide::Pitch;
				Track.PitchSlideTicks = Track.Portamento;
				Track.Pitch = NotePitch(Track.PreviousKey, Track);
				Track.PitchSlideStep = Divide(Difference, Track.Portamento);
			}
		}
		Track.PreviousKey = Track.Key;
		if (Track.Slides & TrackSlide::Sweep)
		{
			Track.VolumeSlideTicks = Track.SweepTicks;
			Track.VolumeSlideStep = Track.SweepStep;
			Track.Volume = ShiftLeft(static_cast<uint16>(Track.SweepStart), 16);
			Track.Slides |= TrackSlide::Volume;
		}
		for (FSoundLfo& Lfo : Track.Lfos)
		{
			if ((Lfo.Flags & (FSoundLfo::On | FSoundLfo::RestartsAtNotes)) == (FSoundLfo::On | FSoundLfo::RestartsAtNotes))
			{
				Lfo.Value = 0;
				Lfo.Countdown = 1;
				Lfo.DelayLeft = Lfo.Delay;
				Lfo.Fade = Lfo.FadeStep;
				Track.Requests |= TrackRequest::Volume;
				Lfo.Flags &= static_cast<uint16>(~(FSoundLfo::FirstHalfDone | FSoundLfo::GoingDown));
			}
		}
	}
}
