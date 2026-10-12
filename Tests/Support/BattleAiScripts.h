#pragma once

#include "Foundation/Types.h"
#include "Support/XorShift32.h"

#include <span>
#include <vector>

namespace Weltall::Testing
{
	/**
	 * A random script for enemy Enemy, appended to Scene, as the oracle's generator
	 * draws it: blocks of conditions and actions within the queue's and the copies' budgets.
	 */
	void DrawAiScript(FXorShift32& G, uint32 Enemy, bool bEventWork, std::vector<uint8>& Scene);

	/** A random queue entry with the fields its type needs, put in the queue's entry Entry, as draw_queue draws one. */
	void DrawAiQueueEntry(FXorShift32& G, std::span<uint8> Queue, uint32 Entry);
}
