#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Gears/GearChannels.h"
#include "Gears/GearSkeleton.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Weltall
{
	class FPsxRandom;
	struct FGearMotionPack;
	struct FGteArctangents;

	/**
	 * What the gears' skeletal engine runs on, whichever program hosts it (the field's and the gear
	 * shop's module, Battle's objects): the channel pool its models share, the first reason it cannot
	 * go on as the original does, and the arctangent table and random generator its steps use. The
	 * engine's steps read the motion packs the host keeps; a place in a pack the host no longer holds
	 * faults.
	 */
	class FGearEngine
	{
	public:
		/** Name heads every fault's message. */
		WELTALLCORE_API explicit FGearEngine(const char* Name);
		WELTALLCORE_API virtual ~FGearEngine();
		FGearEngine(const FGearEngine&) = delete;
		FGearEngine& operator=(const FGearEngine&) = delete;

		/** Records the first reason the engine cannot go on; every later one is dropped. */
		WELTALLCORE_API void Fail(std::string Reason);
		bool Faulted() const { return Fault.has_value(); }

		/** Whether Pack is still in the host's memory. */
		virtual bool HoldsPack(const FGearMotionPack* Pack) const = 0;

		FGearChannelPool Channels;
		std::optional<FError> Fault;
		/** The game's random numbers, which one script instruction draws, and libgte's arctangents. */
		FPsxRandom* Random = nullptr;
		const FGteArctangents* Arctangents = nullptr;

	private:
		const char* Name;
	};

	/**
	 * The part of a model the engine's steps move: its skeleton, its scale (4.12), the channel tag its
	 * scripts' waits watch (FFFFh none) and the point its turns aim at.
	 */
	struct FGearBody
	{
		std::vector<FGearBone> Bones;
		int16 Scale = 0;
		uint16 WatchTag = 0xFFFF;
		std::array<int16, 3> Target{};
	};
}
