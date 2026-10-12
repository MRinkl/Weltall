#include "Field/EventContext.h"

namespace Weltall
{
	void FFieldClock::Tick(FEventVariables& Variables, const FEventScripts& Scripts)
	{
		++Ticks;
		if (Ticks <= 30)
		{
			return;
		}
		Ticks = 0;
		if ((Mode & Stopped) != 0)
		{
			return;
		}
		const int32 Value = Variables.Get(Address, Scripts);
		int32 Seconds = Value & 0xFF;
		int32 Minutes = Value >> 8 & 0xFF;
		if ((Mode & CountsDown) == 0)
		{
			// Seconds reach 60 before they wrap: a minute counts 61 of them.
			if (++Seconds > 60)
			{
				Seconds = 0;
				++Minutes;
			}
		}
		else if (Seconds == 0)
		{
			if (Minutes != 0)
			{
				Seconds = 59;
				--Minutes;
			}
		}
		else
		{
			--Seconds;
		}
		Variables.Set(Address, Minutes << 8 | (Seconds & 0xFF));
	}
}
