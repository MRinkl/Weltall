#pragma once

#include "Foundation/Types.h"

#include <span>
#include <vector>

namespace Weltall
{
	struct FGearBone;
	struct FGearMotionPack;

	/**
	 * How a gear's battle model stands before its battle places it, as its sequence archive's first sequence sets it
	 * up: which of its bones show (a gear's other pair of hands and its stowed weapons are bones that start hidden)
	 * and which turn toward the camera. Read from the sequence's instructions up to the one that places the gear,
	 * for whatever shows the model outside a battle.
	 */
	struct FBattleGearOpening
	{
		struct FShownBone
		{
			int16 Bone = 0;
			bool bShown = false;
			/** The bones under it too. */
			bool bWithChildren = false;
		};

		struct FBillboard
		{
			int16 Bone = 0;
			int16 Mode = 0;
		};

		/** In the order the sequence gives them: a later one overrides an earlier. */
		std::vector<FShownBone> Shown;
		std::vector<FBillboard> Billboards;
		/** The sequence was read as far as the instruction that places the gear. */
		bool bComplete = false;

		WELTALLCORE_API static FBattleGearOpening Read(const FGearMotionPack& Sequences);

		/** Shows, hides and turns the bones of a skeleton made for the gear; bones the skeleton lacks are passed over. */
		WELTALLCORE_API void Apply(std::span<FGearBone> Bones) const;
	};
}
