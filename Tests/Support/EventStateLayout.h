#pragma once

#include "Field/EventActorState.h"

#include <array>
#include <span>

namespace Weltall::Testing
{
	/** The 0x138 bytes the original keeps a script actor's state in. */
	inline constexpr std::size_t EventStateBytes = 0x138;

	/** A state laid out as the original lays it out; the bytes of links to other systems stay zero. */
	std::array<uint8, EventStateBytes> ToOriginalLayout(const FEventActorState& State);

	FEventActorState FromOriginalLayout(std::span<const uint8, EventStateBytes> Bytes);
}
