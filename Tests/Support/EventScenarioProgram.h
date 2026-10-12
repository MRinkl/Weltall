#pragma once

#include "Foundation/Types.h"
#include "Support/XorShift32.h"

#include <array>
#include <vector>

namespace Weltall::Testing
{
	/** The scenarios' random numbers: XorShift32's, and a number below a count as the remainder. */
	class FScenarioRandom
	{
	public:
		explicit FScenarioRandom(uint32 Seed) : Random(Seed) {}

		uint32 Next() { return Random.Next(); }
		uint32 Below(uint32 Count) { return Next() % Count; }

	private:
		FXorShift32 Random;
	};

	namespace EventScenarioShape
	{
		inline constexpr uint32 Zones = 3;
		/** Walking layers; the camera's follows them. */
		inline constexpr uint32 WalkingLayers = 2;
		inline constexpr std::array<uint8, 4> ClockModes = {0xFF, 0x00, 0x04, 0x84};
	}

	/** Which opcodes a scenario's program draws from: the machine's, or the walks and glides with their helpers. */
	enum class EEventScenarioFamily : uint8
	{
		Machine,
		Moves,
		/** The jumps and the party's walks, with stand-in sprites as the moves'. */
		Jumps,
	};

	/**
	 * A random program of Planned instructions of the family's opcodes, each with random operands
	 * that stay inside the scenario's data (a jump table brings its three jumps), ended by a jump to
	 * its start (and for moves an End the inits run): the bytecode, with each instruction's offset in
	 * Offsets.
	 */
	std::vector<uint8> GenerateEventProgram(FScenarioRandom& Random, uint32 Actors, uint32 Planned, std::vector<uint32>& Offsets, EEventScenarioFamily Family);
}
