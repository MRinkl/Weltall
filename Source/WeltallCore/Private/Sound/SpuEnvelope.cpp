#include "Sound/SpuEnvelope.h"

#include <algorithm>

namespace Weltall
{
	int16 StepSpuEnvelope(int16 Level, const FSpuEnvelopeRate& Rate, uint32& Counter)
	{
		const int32 Shift = Rate.Shift;
		int32 Step = 7 - Rate.Step;
		if (Rate.bDecreasing != Rate.bNegativePhase)
		{
			// +7, +6, +5, +4 become -8, -7, -6, -5.
			Step = ~Step;
		}
		Step *= 1 << std::max(0, 11 - Shift);
		int32 Increment = 0x8000 >> std::max(0, Shift - 11);
		if (Rate.bExponential && !Rate.bDecreasing && Level > 0x6000)
		{
			// The exponential increase is a slower linear one near the top.
			if (Shift < 10)
			{
				Step >>= 2;
			}
			else if (Shift >= 11)
			{
				Increment >>= 2;
			}
			else
			{
				Step >>= 1;
				Increment >>= 1;
			}
		}
		else if (Rate.bExponential && Rate.bDecreasing)
		{
			Step = Step * Level >> 15;
		}
		if (!Rate.bFrozen)
		{
			Increment = std::max(Increment, 1);
		}

		Counter += static_cast<uint32>(Increment);
		if ((Counter & 0x8000) == 0)
		{
			return Level;
		}
		Counter = 0;
		int32 Next = Level + Step;
		if (!Rate.bDecreasing)
		{
			Next = std::clamp(Next, -0x8000, 0x7FFF);
		}
		else if (Rate.bNegativePhase)
		{
			Next = std::clamp(Next, -0x8000, 0);
		}
		else
		{
			Next = std::max(Next, 0);
		}
		return static_cast<int16>(Next);
	}
}
