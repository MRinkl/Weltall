#include "Sound/SoundDriverIntegers.h"
#include "Sound/SoundDriverState.h"

#include <cstdlib>

namespace Weltall
{
	using namespace SoundInteger;

	namespace
	{
		constexpr uint8 WaveformTriangleUp = 2;
		constexpr uint8 WaveformTriangle = 3;
		constexpr uint8 WaveformSawUp = 4;
		constexpr uint8 WaveformOff = 8;
		constexpr int16 FullFade = 0x400;
		constexpr int32 VibratoSlot = 0;
		constexpr int32 TremoloSlot = 1;
		constexpr int32 AutoPanSlot = 2;

		/** An LFO's period for a rate byte: rate + rate² / 64 steps. */
		int32 Period(uint8 Rate)
		{
			return Rate + (Rate * Rate >> 6);
		}

		/** The step a waveform takes to reach Depth over its period: the triangles and the rising saw divide it. */
		int32 LfoStep(int32 Depth, int32 PeriodSteps, int32 Wave)
		{
			const int16 Steps = static_cast<int16>(PeriodSteps);
			if (Depth != 0 && Steps != 0 && Wave > 1)
			{
				if (Wave < WaveformSawUp)
				{
					return Divide(Depth, Steps);
				}
				if (Wave == WaveformSawUp && Steps != 1)
				{
					return Divide(Depth, Steps - 1);
				}
			}
			return Depth;
		}

		/** Vibrato depths grow with the square of the operand; the others linearly. */
		int32 LfoDepth(int8 Depth, bool bSquared)
		{
			return bSquared ? ShiftLeft(Depth * std::abs(Depth), 14) : ShiftLeft(Depth, 24);
		}

		void ResetLfo(FSoundLfo& Lfo)
		{
			Lfo.Countdown = 1;
			Lfo.Value = 0;
			Lfo.Flags &= static_cast<uint16>(~(FSoundLfo::FirstHalfDone | FSoundLfo::GoingDown));
			Lfo.DelayLeft = Lfo.Delay;
			Lfo.Fade = Lfo.FadeStep;
		}

		void SetWave(FSoundLfo& Lfo, uint8 Wave)
		{
			Lfo.Wave = Wave;
			Lfo.Waveform = Wave < WaveformOff ? Wave : WaveformOff;
		}

		void StartLfo(FSoundTrack& Track, int32 Slot, uint8 Wave, uint8 Target, uint16 Flags, int32 Delay, int32 Size, int32 PeriodSteps)
		{
			FSoundLfo& Lfo = Track.Lfos[static_cast<std::size_t>(Slot)];
			Lfo.Size = Size;
			Lfo.Period = static_cast<uint16>(PeriodSteps);
			Lfo.FadeStep = FullFade;
			SetWave(Lfo, Wave);
			Lfo.Target = Target;
			Lfo.Flags = Flags;
			Lfo.Delay = static_cast<uint16>(Delay);
			Track.ActiveLfos |= static_cast<uint16>(1 << Slot);
			ResetLfo(Lfo);
		}

		/** D8, E4, EC: a fixed waveform restarting at each note, with a delay. */
		FSoundCursor LfoWithDelay(FSoundTrack& Track, FSoundCursor At, int32 Slot, uint8 Wave, bool bSquared)
		{
			const uint8 Rate = At.Byte();
			const int8 Depth = static_cast<int8>(At.Byte(1));
			if (Depth != 0 && Rate != 0)
			{
				const int32 Steps = static_cast<int16>(Period(Rate));
				const uint16 Flags = FSoundLfo::On | FSoundLfo::RestartsAtNotes;
				StartLfo(Track, Slot, Wave, static_cast<uint8>(Slot), Flags, At.Byte(2) * 4, LfoStep(LfoDepth(Depth, bSquared), Steps, Wave), Steps);
			}
			return At.At(3);
		}

		/** D9, E5, ED: the waveform from the third operand, restarting at notes unless its bit 4 is set. */
		FSoundCursor LfoWithWave(FSoundTrack& Track, FSoundCursor At, int32 Slot, bool bSquared)
		{
			const uint8 Rate = At.Byte();
			const int8 Depth = static_cast<int8>(At.Byte(1));
			const uint8 Option = At.Byte(2);
			if (Depth != 0 && Rate != 0)
			{
				const int32 Steps = static_cast<int16>(Period(Rate));
				const uint8 Wave = Option & 0xF;
				const uint16 Flags = static_cast<uint16>(FSoundLfo::On | ((Option & 0x10) ? 0 : FSoundLfo::RestartsAtNotes));
				StartLfo(Track, Slot, Wave, static_cast<uint8>(Slot), Flags, 0, LfoStep(LfoDepth(Depth, bSquared), Steps, Wave), Steps);
			}
			return At.At(3);
		}

		FSoundCursor LfoFadeIn(FSoundTrack& Track, FSoundCursor At, int32 Slot)
		{
			const uint8 Steps = static_cast<uint8>(At.Byte() + 1);
			if (Steps != 0)
			{
				FSoundLfo& Lfo = Track.Lfos[static_cast<std::size_t>(Slot)];
				Lfo.FadeStep = static_cast<int16>(FullFade / (Steps * 4));
				Lfo.Fade = Lfo.FadeStep;
			}
			return At.At(1);
		}

		FSoundCursor SwitchLfo(FSoundTrack& Track, FSoundCursor At, int32 Slot, bool bOn)
		{
			FSoundLfo& Lfo = Track.Lfos[static_cast<std::size_t>(Slot)];
			if (bOn)
			{
				Track.ActiveLfos |= static_cast<uint16>(1 << Slot);
				Lfo.Flags |= FSoundLfo::On;
			}
			else
			{
				Track.ActiveLfos &= static_cast<uint16>(~(1 << Slot));
				Lfo.Flags &= static_cast<uint16>(~FSoundLfo::On);
			}
			return At;
		}

		/** The LFO an operand or the track's selection names; null past the four (the original writes past them). */
		FSoundLfo* Slot(FSoundTrack& Track, uint32 Index)
		{
			return Index < FSoundTrack::LfoCount ? &Track.Lfos[Index] : nullptr;
		}

		FSoundCursor SetField(uint8& Field, FSoundTrack& Track, FSoundCursor At, uint16 Rewrite)
		{
			Field = At.Byte();
			Track.Rewrite |= Rewrite;
			return At.At(1);
		}

		FSoundCursor Detune(FSoundTrack& Track, FSoundCursor At, int32 Amount, bool bAdd)
		{
			Track.Detune = static_cast<int16>((bAdd ? Track.Detune : 0) + Amount);
			Track.Requests |= TrackRequest::Pitch;
			return At.At(1);
		}
	}

	FSoundCursor FSoundDriver::FState::RunVoiceCommand(uint8 Code, FSoundSequence& Sequence, FSoundTrack& Track, FSoundCursor At)
	{
		(void)Sequence;
		switch (Code)
		{
		case 0xAC: // Instrument.
			SetInstrument(At.Byte(), Track);
			return At.At(1);
		case 0xFC: // Bank and instrument.
			Track.BankId = At.Byte();
			Track.Bank = BankOrFirst(Track.BankId);
			SetInstrument(At.Byte(1), Track);
			return At.At(2);
		case 0xFE:
			Track.BankId = At.Byte();
			Track.Bank = BankOrFirst(Track.BankId);
			return At.At(1);
		case 0xB2: // Pitch modulation by the voice before, on odd voices only.
		case 0xB3:
			if (Track.Voice & 1)
			{
				Track.Rewrite |= VoiceRewrite::PitchModulation;
				Track.Switches = static_cast<uint16>(Code == 0xB2 ? Track.Switches | VoiceSwitch::PitchModulation : Track.Switches & ~VoiceSwitch::PitchModulation);
			}
			return At;
		case 0xC0: // The instrument's own envelope again.
			SetInstrument(Track.Instrument, Track);
			return At;
		case 0xC1:
			Track.AttackMode = At.Byte();
			Track.SustainMode = At.Byte(1);
			Track.ReleaseMode = At.Byte(2);
			Track.Rewrite |= VoiceRewrite::Envelope;
			return At.At(3);
		case 0xC2:
			return SetField(Track.AttackRate, Track, At, VoiceRewrite::Attack);
		case 0xC3:
			return SetField(Track.DecayRate, Track, At, VoiceRewrite::Decay);
		case 0xC4:
			return SetField(Track.SustainRate, Track, At, VoiceRewrite::SustainRate);
		case 0xC5: // Release rate, which each note restores.
			Track.InstrumentRelease = At.Byte();
			return SetField(Track.ReleaseRate, Track, At, VoiceRewrite::Release);
		case 0xC6:
			return SetField(Track.SustainLevel, Track, At, VoiceRewrite::SustainLevel);
		case 0xC7:
			Track.DecayRate = At.Byte();
			Track.SustainLevel = At.Byte(1);
			Track.Rewrite |= VoiceRewrite::Decay | VoiceRewrite::SustainLevel;
			return At.At(2);
		case 0xC8:
			return SetField(Track.AttackMode, Track, At, VoiceRewrite::Attack);
		case 0xC9:
			return SetField(Track.SustainMode, Track, At, VoiceRewrite::SustainRate);
		case 0xCA:
			return SetField(Track.ReleaseMode, Track, At, VoiceRewrite::Release);
		case 0xD0: // Detune in 1/8 semitones, set or added; in 1/32; in semitones and 1/256.
			return Detune(Track, At, static_cast<int8>(At.Byte()) * 32, false);
		case 0xD1:
			return Detune(Track, At, static_cast<int8>(At.Byte()) * 32, true);
		case 0xD2:
			return Detune(Track, At, static_cast<int8>(At.Byte()) * 8, true);
		case 0xD3:
			Detune(Track, At, static_cast<int8>(At.Byte()) * 256 + At.Byte(1), true);
			return At.At(2);
		case 0xD4: // Pitch slide by s semitones over n ticks.
		{
			const uint8 Ticks = At.Byte();
			const int8 Semitones = static_cast<int8>(At.Byte(1));
			if (Ticks != 0 && Semitones != 0)
			{
				Track.PitchSlideStep = Divide(ShiftLeft(Semitones, 24), Ticks);
				Track.PitchSlideTicks = Ticks;
				Track.Slides |= TrackSlide::Pitch;
			}
			else
			{
				Track.Slides &= static_cast<uint16>(~TrackSlide::Pitch);
			}
			return At.At(2);
		}
		case 0xD5:
			Track.Slides ^= TrackSlide::HoldPitch;
			return At;
		case 0xDC:
			Track.Slides &= static_cast<uint16>(~TrackSlide::Pitch);
			return At;
		case 0xD6: // Portamento over n ticks (0: off).
			Track.Portamento = At.Byte();
			Track.Slides = static_cast<uint16>(Track.Portamento != 0 ? Track.Slides | TrackSlide::Portamento : Track.Slides & ~TrackSlide::Portamento);
			return At.At(1);
		case 0xD7:
			return LfoFadeIn(Track, At, VibratoSlot);
		case 0xD8:
			return LfoWithDelay(Track, At, VibratoSlot, WaveformTriangle, true);
		case 0xD9:
			return LfoWithWave(Track, At, VibratoSlot, true);
		case 0xDA:
		case 0xDB:
			return SwitchLfo(Track, At, VibratoSlot, Code == 0xDA);
		case 0xE3:
			return LfoFadeIn(Track, At, TremoloSlot);
		case 0xE4:
			return LfoWithDelay(Track, At, TremoloSlot, WaveformTriangleUp, false);
		case 0xE5:
			return LfoWithWave(Track, At, TremoloSlot, false);
		case 0xE6:
		case 0xE7:
			return SwitchLfo(Track, At, TremoloSlot, Code == 0xE6);
		case 0xEB:
			return LfoFadeIn(Track, At, AutoPanSlot);
		case 0xEC:
			return LfoWithDelay(Track, At, AutoPanSlot, WaveformTriangle, false);
		case 0xED:
			return LfoWithWave(Track, At, AutoPanSlot, false);
		case 0xEE:
		case 0xEF:
			return SwitchLfo(Track, At, AutoPanSlot, Code == 0xEE);
		case 0xF0: // Select an LFO: its waveform (bit 4: no restart at notes) and target; off until F6.
			Track.SelectedLfo = At.Byte();
			if (FSoundLfo* Lfo = Slot(Track, Track.SelectedLfo))
			{
				SetWave(*Lfo, At.Byte(1) & 0xF);
				Lfo->Flags = (At.Byte(1) & 0x10) ? 0 : FSoundLfo::RestartsAtNotes;
				Lfo->FadeStep = FullFade;
				Lfo->Delay = 0;
				Lfo->Target = At.Byte(2);
			}
			return At.At(3);
		case 0xF1: // The selected LFO's period and depth.
			if (FSoundLfo* Lfo = Slot(Track, Track.SelectedLfo))
			{
				const int32 Steps = Period(At.Byte());
				const int32 Depth = static_cast<int32>(static_cast<uint32>(ShiftLeft(static_cast<int8>(At.Byte(1)), 24)) | static_cast<uint32>(At.Byte(2)) << 16);
				Lfo->Size = LfoStep(Depth, Steps, Lfo->Wave);
				Lfo->Period = static_cast<uint16>(Steps);
			}
			return At.At(3);
		case 0xF2: // The selected LFO's delay and fade-in.
		{
			const uint8 Steps = static_cast<uint8>(At.Byte(1) + 1);
			FSoundLfo* Lfo = Slot(Track, Track.SelectedLfo);
			if (Lfo != nullptr && Steps != 0)
			{
				Lfo->Delay = static_cast<uint16>(At.Byte() * 4);
				Lfo->FadeStep = static_cast<int16>(FullFade / (Steps * 4));
				Lfo->Fade = Lfo->FadeStep;
			}
			return At.At(2);
		}
		case 0xF6:
			if (FSoundLfo* Lfo = Slot(Track, At.Byte()))
			{
				ResetLfo(*Lfo);
				Lfo->Flags |= FSoundLfo::On;
				Track.ActiveLfos |= static_cast<uint16>(1 << At.Byte());
			}
			return At.At(1);
		case 0xF7:
			if (FSoundLfo* Lfo = Slot(Track, At.Byte()))
			{
				Lfo->Flags &= static_cast<uint16>(~FSoundLfo::On);
				Track.ActiveLfos &= static_cast<uint16>(~(1 << At.Byte()));
			}
			return At.At(1);
		case 0xE0: // Track volume, which ends its slides.
			Track.Volume = ShiftLeft(At.Byte(), 24);
			Track.Slides &= static_cast<uint16>(~(TrackSlide::Sweep | TrackSlide::Volume));
			Track.Requests |= TrackRequest::Volume;
			return At.At(1);
		case 0xE1:
			Track.Volume = static_cast<int32>(static_cast<uint32>(Add(ShiftLeft(static_cast<int8>(At.Byte()), 24), Track.Volume)) & 0x7FFFFFFF);
			Track.Requests |= TrackRequest::Volume;
			Track.Slides &= static_cast<uint16>(~(TrackSlide::Sweep | TrackSlide::Volume));
			return At.At(1);
		case 0xE2: // Track volume slide over n ticks.
		{
			const uint8 Ticks = At.Byte();
			const int32 Difference = Subtract(ShiftLeft(static_cast<int8>(At.Byte(1)), 24), Track.Volume);
			if (Ticks != 0 && Difference != 0)
			{
				Track.VolumeSlideStep = Divide(Difference, Ticks);
				Track.VolumeSlideTicks = Ticks;
				Track.Slides = static_cast<uint16>((Track.Slides | TrackSlide::Volume) & ~TrackSlide::Sweep);
			}
			return At.At(2);
		}
		case 0xF8: // A volume sweep at every note, from a to b over n ticks.
		{
			const uint8 From = At.Byte();
			const uint8 Ticks = At.Byte(1);
			const int32 Difference = Subtract(ShiftLeft(At.Byte(2), 24), ShiftLeft(From, 24));
			if (Difference != 0 && Ticks != 0)
			{
				Track.SweepStep = Divide(Difference, Ticks);
				Track.SweepStart = static_cast<int16>(static_cast<int8>(From) * 0x100);
				Track.SweepTicks = Ticks;
				Track.Slides = static_cast<uint16>((Track.Slides | TrackSlide::Sweep) & ~TrackSlide::Volume);
			}
			else
			{
				Track.Slides &= static_cast<uint16>(~TrackSlide::Sweep);
			}
			return At.At(3);
		}
		case 0xE8:
			Track.Pan = static_cast<uint16>(At.Byte() << 8);
			Track.Requests |= TrackRequest::Volume;
			return At.At(1);
		case 0xE9:
			Track.Pan = static_cast<uint16>((Track.Pan + static_cast<int8>(At.Byte()) * 0x100) & 0x7FFF);
			Track.Requests |= TrackRequest::Volume;
			return At.At(1);
		case 0xEA: // Pan slide to p over n ticks.
		{
			const uint8 Ticks = At.Byte();
			const int32 Target = static_cast<int8>(At.Byte(1));
			const int32 Difference = Target - (static_cast<int16>(Track.Pan) >> 8);
			if (Ticks != 0 && Difference != 0)
			{
				// The original stores the slide's difference where its end belongs, so the pan jumps
				// there when the slide ends.
				Track.PanSlideTarget = static_cast<uint16>((Options.bPanSlidesEndOnTarget ? Target : Difference) << 8);
				Track.PanSlideTicks = Ticks;
				Track.Slides |= TrackSlide::Pan;
				Track.PanSlideStep = static_cast<int16>(Divide(Difference << 8, Ticks));
			}
			return At.At(2);
		}
		default:
			// Bytes that are no command never reach here: the files were checked.
			return At;
		}
	}
}
