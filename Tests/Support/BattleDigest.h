#pragma once

#include "Battle/BattleContext.h"
#include "Support/OracleDigest.h"

namespace Weltall::Testing
{
	/**
	 * A battle record's members at the widths the original keeps them, in its layout visitor's order:
	 * what the oracle scripts take of the original's memory.
	 */
	void DigestCombatant(FOracleDigest& Digest, const FBattleCombatant& Combatant);
	void DigestCommand(FOracleDigest& Digest, const FBattleCommand& Command);
	void DigestItem(FOracleDigest& Digest, const FBattleItem& Item);

	/** Every command record the tables fill, in the context's order. */
	void DigestCommands(FOracleDigest& Digest, const FBattleContext& Context);

	/** A record pointer of the rules as the oracle scripts write it: the slot, or FFh for none. */
	inline uint8 RecordSlot(uint8 Index)
	{
		return Index < FBattleContext::SlotCount ? Index : 0xFF;
	}
}
