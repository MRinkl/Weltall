#include "Sound/SoundDriverIntegers.h"
#include "Sound/SoundDriverState.h"

namespace Weltall
{
	using namespace SoundInteger;

	namespace
	{
		constexpr int32 TicksPerWholeNote = 0xC0;
		constexpr uint8 NoiseClockMask = 0x3F;
		/** AA's operand for "no voice". */
		constexpr uint8 VoiceNone = 24;
		constexpr int32 EffectVolumeForMusic = 0x7F;
		constexpr int32 EffectPanForMusic = 0x40;

		uint16 EffectNumber(FSoundCursor At)
		{
			return static_cast<uint16>(At.Byte() | At.Byte(1) << 8);
		}

		/** The repeat at the track's current level, or null when the level is off the stack. */
		FSoundRepeat* CurrentRepeat(FSoundTrack& Track)
		{
			return Track.RepeatLevel >= 0 && Track.RepeatLevel < FSoundTrack::RepeatDepth ? &Track.Repeats[static_cast<std::size_t>(Track.RepeatLevel)] : nullptr;
		}
	}

	FSoundCursor FSoundDriver::FState::RunCommand(uint8 Code, FSoundSequence& Sequence, FSoundTrack& Track, FSoundCursor At)
	{
		switch (Code)
		{
		case 0x80: // Rest: the note sounding is released.
			Track.Length = At.Byte();
			Track.Requests |= TrackRequest::KeyOff;
			Track.Flags |= TrackFlag::Released;
			return At.At(1);
		case 0x81: // Tie: the note sounding goes on.
			Track.Flags |= TrackFlag::StepDone;
			Track.Length = At.Byte();
			return At.At(1);
		case 0x8A:
		case 0x8F:
		case 0xBD:
		case 0xBE:
		case 0xF5:
			return At;
		case 0x8E:
		case 0xBC:
			return At.At(3);
		case 0x8D: // Loop point, if the sequence's variable matches.
			if (At.Byte() == Sequence.VariableB)
			{
				if (Track.Loops == 0) Track.FirstLoopFrame = Sequence.PlaybackFrames;
				Track.LoopPoint = At.At(1);
				Track.LoopOctave = static_cast<uint8>(Track.Octave);
			}
			return At.At(1);
		case 0x90: // End: back to the loop point, or the track ends.
			if (Track.LoopPoint.IsSet())
			{
				if (Track.Loops == 0) Track.FirstLoopEndFrame = Sequence.PlaybackFrames;
				++Track.Loops;
				Track.Octave = Track.LoopOctave;
				return Track.LoopPoint;
			}
			EndTrack(Track);
			return At;
		case 0x91:
			if (Track.Loops == 0) Track.FirstLoopFrame = Sequence.PlaybackFrames;
			Track.LoopPoint = At;
			Track.LoopOctave = static_cast<uint8>(Track.Octave);
			return At;
		case 0x94:
			Track.Octave = static_cast<uint16>(At.Byte() * 12);
			return At.At(1);
		case 0x95:
			Track.Octave = static_cast<uint16>(Track.Octave + 12);
			return At;
		case 0x96:
			Track.Octave = static_cast<uint16>(Track.Octave - 12);
			return At;
		case 0x97: // Time signature: beats per measure, and the beat's note value.
			Sequence.TicksPerBeat = static_cast<uint16>(TicksPerWholeNote / At.Byte(1));
			Sequence.BeatUnit = At.Byte(1);
			Sequence.BeatsPerMeasure = At.Byte();
			Sequence.BeatTicksLeft = Sequence.TicksPerBeat;
			return At.At(2);
		case 0xF9: // Position: measure and beat.
			Sequence.Measure = At.Byte();
			Sequence.BeatTicksLeft = Sequence.TicksPerBeat;
			Sequence.Beat = At.Byte(1);
			return At.At(2);
		case 0xA4:
			Sequence.VariableA = At.Byte();
			return At.At(1);
		case 0xA5:
			Sequence.VariableA = static_cast<uint8>(Sequence.VariableA + At.Byte());
			return At.At(1);
		case 0x98: // Repeat start: n passes.
		{
			++Track.RepeatLevel;
			FSoundRepeat* Repeat = CurrentRepeat(Track);
			if (Repeat == nullptr)
			{
				// Deeper than the stack: the original writes past it.
				EndTrack(Track);
				return At;
			}
			Repeat->Count = static_cast<uint8>(At.Byte() - 1);
			Repeat->Start = At.At(1);
			Repeat->Octave = static_cast<uint8>(Track.Octave);
			return At.At(1);
		}
		case 0x99: // Repeat end: back to the start until the passes are done.
		{
			FSoundRepeat* Repeat = CurrentRepeat(Track);
			if (Repeat == nullptr || !Repeat->Start.IsSet())
			{
				EndTrack(Track);
				return At;
			}
			if (--Repeat->Count != 0xFF)
			{
				Repeat->Exit = At;
				Repeat->EndOctave = static_cast<uint8>(Track.Octave);
				Track.Octave = Repeat->Octave;
				return Repeat->Start;
			}
			--Track.RepeatLevel;
			return At;
		}
		case 0x9A: // Repeat break: on the last pass, straight out of the repeat.
		{
			FSoundRepeat* Repeat = CurrentRepeat(Track);
			if (Repeat == nullptr)
			{
				EndTrack(Track);
				return At;
			}
			if (Repeat->Count == 0)
			{
				if (!Repeat->Exit.IsSet())
				{
					EndTrack(Track);
					return At;
				}
				Track.Octave = Repeat->EndOctave;
				--Track.RepeatLevel;
				return Repeat->Exit;
			}
			return At;
		}
		case 0x9C: // An effect of the first effect file, at the music's volume and centred.
			PlayEffectVolumePan(EffectNumber(At), EffectVolumeForMusic, EffectPanForMusic);
			return At.At(3);
		case 0x9D:
			StopEffect(EffectNumber(At));
			return At.At(2);
		case 0x9E: // Go on in an effect's commands (the original, without one, reads its operands as commands).
		{
			const int16 FileId = static_cast<int16>(Track.EffectId >> 16);
			const FSoundEffectFile* File = FileId == 0 ? (EffectFileCount != 0 ? EffectFiles[0] : nullptr) : (FileId > 0 ? EffectFile(static_cast<uint16>(FileId)) : nullptr);
			const int32 Index = At.Byte(2) + static_cast<int16>(EffectNumber(At)) * 2;
			if (File == nullptr || Index < 0 || Index >= File->EffectCount() * 2 || !File->CanContinue(Index))
			{
				EndTrack(Track);
				return At;
			}
			return FSoundCursor::In(File->Bytes(), File->ChannelOffset(Index) + 3u);
		}
		case 0xA0: // Tempo.
			Sequence.Tempo = ShiftLeft(At.Byte(), 16);
			Sequence.TempoStep = At.Byte() * High(Sequence.Scale.Value);
			return At.At(1);
		case 0xA1: // Tempo change: it also zeroes the step, so the sequence stops until the tempo is set again.
			Sequence.TempoStep = 0;
			Sequence.Tempo = Add(ShiftLeft(static_cast<int8>(At.Byte()), 16), Sequence.Tempo);
			return At.At(1);
		case 0xA2: // Tempo slide over n ticks.
		{
			const uint8 Ticks = At.Byte();
			const uint8 Target = At.Byte(1);
			Sequence.TempoSlideTarget = Target;
			const int32 Difference = Subtract(ShiftLeft(Target, 16), Sequence.Tempo);
			if (Ticks != 0 && Difference != 0)
			{
				Sequence.TempoSlideCount = Ticks;
				Sequence.TempoSlideStep = Divide(Difference, Ticks);
			}
			return At.At(2);
		}
		case 0xA6: // Sequence volume, at once.
			Sequence.Volume.Value = ShiftLeft(At.Byte(), 24);
			MarkTracks(TrackRequest::Volume, Sequence);
			return At.At(1);
		case 0xA7: // Sequence volume fade over n * 32 interrupts.
		{
			const int32 Interrupts = At.Byte() << 5;
			const uint8 Volume = At.Byte(1);
			const int32 Difference = Subtract(ShiftLeft(Volume, 24), Sequence.Volume.Value);
			if (Interrupts != 0 && Difference != 0)
			{
				Sequence.Volume.Count = static_cast<int16>(Interrupts);
				Sequence.Volume.Target = static_cast<int16>(Volume << 8);
				Sequence.Volume.Step = Divide(Difference, Interrupts);
			}
			return At.At(2);
		}
		case 0xA9:
			Track.GateRatio = At.Byte();
			return At.At(1);
		case 0xAA: // Move to another voice.
			if (At.Byte() <= VoiceNone)
			{
				ReleaseVoice(Track, Track.Voice);
				Track.Voice = At.Byte();
				ClaimVoice(Track, Track.Voice);
			}
			return At.At(1);
		case 0xAD:
			Track.LengthAdjust = At.Byte() != 0 ? static_cast<int8>(At.Byte() + Track.LengthAdjust) : 0;
			return At.At(1);
		case 0xAE:
			if (Sequence.File != nullptr && Sequence.File->HasPercussion())
			{
				Track.Flags |= TrackFlag::Percussion;
			}
			return At;
		case 0xAF:
			Track.Flags &= static_cast<uint16>(~TrackFlag::Percussion);
			return At;
		case 0xB0:
			Track.Flags |= TrackFlag::Legato;
			return At;
		case 0xB1:
			Track.Flags &= static_cast<uint16>(~TrackFlag::Legato);
			return At;
		case 0xB4:
		case 0xB5: // Noise clock (set, or added to), and noise on.
			Sequence.NoiseClock = Code == 0xB4 ? At.Byte() : static_cast<uint16>((At.Byte() + Sequence.NoiseClock) & NoiseClockMask);
			Library.SetNoiseClock(Sequence.NoiseClock);
			Track.Rewrite |= VoiceRewrite::Noise;
			Track.Switches |= VoiceSwitch::Noise;
			return At.At(1);
		case 0xB6:
			Track.Rewrite |= VoiceRewrite::Noise;
			Track.Switches |= VoiceSwitch::Noise;
			return At;
		case 0xB7:
			Track.Rewrite |= VoiceRewrite::Noise;
			Track.Switches &= static_cast<uint16>(~VoiceSwitch::Noise);
			return At;
		case 0xB8: // Reverb depth, delay and feedback of the current type.
			Sequence.ReverbDepth = static_cast<int16>(At.Byte() << 8);
			Sequence.ReverbDelay = At.Byte(1);
			Sequence.ReverbFeedback = At.Byte(2);
			SetReverb(-1, static_cast<int16>(static_cast<int8>(At.Byte()) * 0x100), static_cast<int8>(At.Byte(1)), static_cast<int8>(At.Byte(2)));
			return At.At(3);
		case 0xBA:
			if ((Sequence.Flags & SequenceFlag::ReverbRestricted) == 0 || ((Flags & DriverFlag::ReverbOnFileTracks) && !(Track.Flags & TrackFlag::FromEffectFile)))
			{
				Track.Rewrite |= VoiceRewrite::Reverb;
				Track.Switches |= VoiceSwitch::Reverb;
			}
			return At;
		case 0xBB:
			Track.Rewrite |= VoiceRewrite::Reverb;
			Track.Switches &= static_cast<uint16>(~VoiceSwitch::Reverb);
			return At;
		case 0xFD: // Tempo scale (0: unchanged).
			if (At.Byte() != 0)
			{
				Sequence.Scale.Value = ShiftLeft(At.Byte(), 24);
				Sequence.TempoStep = High(Sequence.Tempo) * (At.Byte() << 8);
			}
			return At.At(1);
		case 0xFF: // End once the voice has faded out.
			if ((Track.Voice < VoiceCount ? Library.Envelope(Track.Voice) : 0) == 0)
			{
				EndTrack(Track);
			}
			return At;
		default:
			return RunVoiceCommand(Code, Sequence, Track, At);
		}
	}
}
