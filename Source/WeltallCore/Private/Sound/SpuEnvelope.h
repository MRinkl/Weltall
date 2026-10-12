#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/** How one of the SPU's envelopes moves: an ADSR phase's, or a volume's sweep. */
	struct FSpuEnvelopeRate
	{
		bool bExponential = false;
		bool bDecreasing = false;
		/** A negative sweep: its steps run the other way, and a decrease stops at -0x8000 rather than 0. */
		bool bNegativePhase = false;
		uint8 Shift = 0;
		/** The step field, 0-3 for +7 to +4, or -8 to -5 when decreasing. */
		uint8 Step = 0;
		/** Every bit of the rate's field is set: the counter may stay at 0, and the level then never moves. */
		bool bFrozen = false;
	};

	/**
	 * One sample of the SPU's envelope operation: the counter adds its increment, and once it carries
	 * into bit 15 it starts over and the level takes a step, clamped as the direction allows.
	 */
	int16 StepSpuEnvelope(int16 Level, const FSpuEnvelopeRate& Rate, uint32& Counter);
}
