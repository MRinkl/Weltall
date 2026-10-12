#include "Battle/BattleContext.h"

namespace Weltall
{
	bool FBattleContext::IsLoadedCommand(int32 Index)
	{
		if (Index < 0 || Index >= CommandRecordCount)
		{
			return false;
		}
		if (Index < FirstGearCommand)
		{
			return true;
		}
		if (Index < FirstEnemyCommand)
		{
			return (Index - FirstGearCommand) % GearTableSize < FBattleTables::GearCommandCount;
		}
		return Index - FirstEnemyCommand < FBattleTables::EnemyCommandCount;
	}
}
