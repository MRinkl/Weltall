#include "Sound/SoundDriverIntegers.h"
#include "Sound/SoundDriverState.h"

namespace Weltall
{
	using namespace SoundInteger;

	namespace
	{
		constexpr uint8 Pulse = 0;
		constexpr uint8 Square = 1;
		constexpr uint8 TriangleUp = 2;
		constexpr uint8 Triangle = 3;
		constexpr uint8 SawUp = 4;
		constexpr uint8 Saw = 5;
		constexpr uint8 RandomUp = 6;
		constexpr uint8 Off = 8;

		constexpr int16 FullFade = 0x400;
	}

	int32 FSoundDriver::FState::Random()
	{
		uint32 X = RandomState;
		X ^= X << 17;
		X ^= static_cast<uint32>(static_cast<int32>(X) >> 15);
		RandomState = X;
		return static_cast<int32>(X & 0x7FFF);
	}

	int32 FSoundDriver::FState::RunWaveform(FSoundLfo& Lfo)
	{
		const uint8 Waveform = Lfo.Waveform;
		if (Waveform >= Off)
		{
			// The "off" waveform switches its LFO off and returns what it leaves in its result register.
			Lfo.Flags &= static_cast<uint16>(~FSoundLfo::On);
			return Lfo.Flags;
		}
		if (Waveform == RandomUp)
		{
			Random();
		}
		const bool bExpired = --Lfo.Countdown == 0;
		switch (Waveform)
		{
		case Pulse:
			if (bExpired)
			{
				Lfo.Countdown = Lfo.Period;
				Lfo.Value = Lfo.Value == 0 ? Lfo.Size : 0;
			}
			break;
		case Square:
			if (bExpired)
			{
				Lfo.Countdown = Lfo.Period;
				Lfo.Value = (Lfo.Flags & FSoundLfo::GoingDown) ? Negate(Lfo.Size) : Lfo.Size;
				Lfo.Flags ^= FSoundLfo::GoingDown;
			}
			break;
		case TriangleUp:
			if (bExpired)
			{
				Lfo.Countdown = Lfo.Period;
				Lfo.Step = (Lfo.Flags & FSoundLfo::GoingDown) ? Negate(Lfo.Size) : Lfo.Size;
				Lfo.Flags ^= FSoundLfo::GoingDown;
			}
			Lfo.Value = Add(Lfo.Value, Lfo.Step);
			break;
		case Triangle:
			if (bExpired)
			{
				// After the first quarter from 0, each run goes from one extreme to the other.
				Lfo.Countdown = static_cast<uint16>((Lfo.Flags & FSoundLfo::FirstHalfDone) ? Lfo.Period << 1 : Lfo.Period);
				Lfo.Step = (Lfo.Flags & FSoundLfo::GoingDown) ? Negate(Lfo.Size) : Lfo.Size;
				Lfo.Flags = static_cast<uint16>((Lfo.Flags | FSoundLfo::FirstHalfDone) ^ FSoundLfo::GoingDown);
			}
			Lfo.Value = Add(Lfo.Value, Lfo.Step);
			break;
		case SawUp:
		case Saw:
			if (bExpired)
			{
				Lfo.Countdown = Lfo.Period;
				Lfo.Value = 0;
			}
			else
			{
				Lfo.Value = Add(Lfo.Value, Lfo.Size);
			}
			break;
		case RandomUp:
			if (bExpired)
			{
				Lfo.Countdown = Lfo.Period;
				Lfo.Value = static_cast<int32>(static_cast<int64>(Lfo.Size >> 15) * Random());
			}
			break;
		default:
			if (bExpired)
			{
				Lfo.Countdown = Lfo.Period;
				Lfo.Value = static_cast<int32>(static_cast<int64>(Lfo.Size >> 14) * Random() - Lfo.Size);
			}
			break;
		}
		return Lfo.Value;
	}

	void FSoundDriver::FState::StepLfos(FSoundSequence& Sequence)
	{
		for (FSoundTrack& Track : Sequence.Used())
		{
			if (Track.Flags == 0)
			{
				continue;
			}
			Track.LfoPitch = 0;
			Track.LfoVolume = 0;
			Track.LfoPan = 0;
			if (Track.ActiveLfos == 0)
			{
				continue;
			}
			uint16 Requests = Track.Requests;
			for (FSoundLfo& Lfo : Track.Lfos)
			{
				if (!(Lfo.Flags & FSoundLfo::On))
				{
					continue;
				}
				if (Lfo.DelayLeft != 0)
				{
					--Lfo.DelayLeft;
					continue;
				}
				int32 Value = RunWaveform(Lfo);
				if (Lfo.Fade < FullFade)
				{
					Value = static_cast<int32>(static_cast<int64>(Value >> 10) * Lfo.Fade);
					Lfo.Fade = static_cast<int16>(Lfo.Fade + Lfo.FadeStep);
				}
				const int16 Delta = static_cast<int16>(Value >> 16);
				switch (Lfo.Target)
				{
				case 0:
					Requests |= TrackRequest::Pitch;
					Track.LfoPitch = static_cast<int16>(Track.LfoPitch + Delta);
					break;
				case 1:
					Requests |= TrackRequest::Volume;
					Track.LfoVolume = static_cast<int16>(Track.LfoVolume + Delta);
					break;
				case 2:
					Requests |= TrackRequest::Volume;
					Track.LfoPan = static_cast<int16>(Track.LfoPan + Delta);
					break;
				default:
					break;
				}
			}
			Track.Requests = Requests;
		}
	}
}
