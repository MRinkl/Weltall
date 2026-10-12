#include "Support/BattleDigest.h"

#include <cstddef>

namespace Weltall::Testing
{
	namespace
	{
		/** A visitor that digests each member at its width. */
		struct FDigestVisitor
		{
			FOracleDigest& Digest;

			template <typename T>
			void operator()(std::size_t, const T& Member) const
			{
				if constexpr (sizeof(T) == 1)
				{
					Digest.U8(Member);
				}
				else if constexpr (sizeof(T) == 2)
				{
					Digest.I16(Member);
				}
				else
				{
					Digest.I32(Member);
				}
			}
		};
	}

	void DigestCombatant(FOracleDigest& Digest, const FBattleCombatant& Combatant)
	{
		VisitBattleCombatantLayout(Combatant, FDigestVisitor{Digest});
	}

	void DigestCommand(FOracleDigest& Digest, const FBattleCommand& Command)
	{
		VisitBattleCommandLayout(Command, FDigestVisitor{Digest});
	}

	void DigestItem(FOracleDigest& Digest, const FBattleItem& Item)
	{
		VisitBattleItemLayout(Item, FDigestVisitor{Digest});
	}

	void DigestCommands(FOracleDigest& Digest, const FBattleContext& Context)
	{
		for (int32 Index = 0; Index < FBattleContext::CommandRecordCount; ++Index)
		{
			if (FBattleContext::IsLoadedCommand(Index))
			{
				DigestCommand(Digest, Context.Commands[static_cast<std::size_t>(Index)]);
			}
		}
	}
}
