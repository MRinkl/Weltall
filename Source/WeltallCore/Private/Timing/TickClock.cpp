#include "Timing/TickClock.h"

#include <algorithm>
#include <cmath>

namespace Weltall
{
	int32 FTickClock::Advance(double Seconds, double Speed)
	{
		if (!(Seconds > 0.0) || !(Speed > 0.0))
		{
			return 0;
		}
		const double Total = Elapsed + Seconds * Speed;
		double Due = std::floor(Total / TickSeconds);
		double Remainder = Total - Due * TickSeconds;
		// The division can round across a tick boundary either way; the remainder says which.
		if (Remainder >= TickSeconds)
		{
			Due += 1.0;
			Remainder -= TickSeconds;
		}
		else if (Remainder < 0.0)
		{
			Due -= 1.0;
			Remainder += TickSeconds;
		}
		Elapsed = Remainder;
		return static_cast<int32>(std::min(Due, std::ceil(MaxTicksPerAdvance * Speed)));
	}
}
