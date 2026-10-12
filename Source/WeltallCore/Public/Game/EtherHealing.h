#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/** The equipment's marks the Ethers' costs and healing follow: Ether Doubler (ether buffs), Economether (equipment flags). */
	namespace EtherMarks
	{
		constexpr uint16 EtherDoubler = 0x2000;
		constexpr uint16 Economether = 0x4000;
	}

	/**
	 * What a healing Ether restores, as battle heals: the healer's Ether (its equipment's bonus
	 * included) times the ability's power, doubled by Ether Doubler, then three tenths less or more by
	 * the target's element guard (bits 100h and 200h).
	 */
	WELTALLCORE_API int16 EtherHealing(uint8 Ether, uint8 Power, uint16 HealerEtherBuffs, uint16 TargetGuard);

	/** An Ether's EP cost as battle charges it: doubled by Ether Doubler, then halved (rounding up) by Economether. */
	WELTALLCORE_API uint8 EtherCost(uint8 Cost, uint16 EtherBuffs, uint16 EquipmentFlags);
}
