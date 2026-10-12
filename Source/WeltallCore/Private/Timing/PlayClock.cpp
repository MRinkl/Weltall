#include "Timing/PlayClock.h"

namespace Weltall
{
	void FPlayClock::Vblank()
	{
		if (bStopped)
		{
			return;
		}
		constexpr uint8 PerUnit = 60;
		if (++Frames == PerUnit)
		{
			Frames = 0;
			++Seconds;
		}
		if (Seconds == PerUnit)
		{
			Seconds = 0;
			++Minutes;
		}
		if (Minutes == PerUnit)
		{
			Minutes = 0;
			++Hours;
		}
		if (Hours == StopHours)
		{
			bStopped = true;
		}
	}
}
