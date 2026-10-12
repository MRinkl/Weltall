#pragma once

#include "Field/EventContext.h"
#include "Support/OracleDigest.h"

#include <vector>

namespace Weltall::Testing
{
	/**
	 * The camera's words as the camera's oracle reads them from the original each tick:
	 * the points, the view's angles, the turn, the settings, the ramps, the divisors and counters, the
	 * path's moves, the shake, the look-at and the view matrix.
	 */
	void DigestFieldCamera(FOracleDigest& Digest, const FFieldCamera& Camera);

	/**
	 * The four message windows and their shared words, laid out as the messages' oracle
	 * reads them from the original's window blocks: each window's state, its drawn frame and cursors,
	 * its kernel window's typing state; the open mask, the opening's length, the slow motion, the
	 * cursors' animation and the faces' cache.
	 */
	std::vector<uint8> MessageWindowBytes(const FEventContext& Context);
}
