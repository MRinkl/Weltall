#include "Game/GamePace.h"

#include "Battle/BattleRuntime.h"
#include "Game/BattleGameProgram.h"
#include "Game/GameLoop.h"
#include "Game/PlayerSettings.h"

namespace Weltall
{
	bool IsBattleFought(const FGameLoop& Loop)
	{
		const FBattleGameProgram* Battle = Loop.Stage() == EGameLoopStage::Program ? Loop.Battle() : nullptr;
		return Battle != nullptr && Battle->Runtime().IsFought();
	}

	int32 GamePace(const FGameLoop& Loop, const FPlayerSettings& Settings)
	{
		return IsBattleFought(Loop) ? BattleSpeedRate(Settings.BattleSpeed) : 1;
	}
}
