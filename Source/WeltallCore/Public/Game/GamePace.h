#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	class FGameLoop;
	struct FPlayerSettings;

	/**
	 * Whether a battle is being fought: the Battle program from its opening transition to the last frame of
	 * its ending, before its result's screen.
	 */
	WELTALLCORE_API bool IsBattleFought(const FGameLoop& Loop);

	/**
	 * How many times its own pace the game runs at this moment: the settings' battle speed while a battle is
	 * fought, its own pace (1) everywhere else.
	 */
	WELTALLCORE_API int32 GamePace(const FGameLoop& Loop, const FPlayerSettings& Settings);
}
