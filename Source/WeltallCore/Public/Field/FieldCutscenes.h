#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FEventContext;

	/**
	 * Whether the field is playing one of the story's cutscenes, for the drawing that keeps the original look there.
	 * One begins when the scripts hold the party (PlayerLock) for no talk the player began, and the scene shows itself
	 * as a staged one: the screen is black or not yet drawn (a scene the map opens on, or one set up behind a fade), or
	 * the camera, which followed the party until the scripts took it, takes the scripts' paths. The scripts take the
	 * party for less, too, which shows neither: a talk, a lift's ride, the party gathering at a door, a message over
	 * the field. A cutscene goes on while the scripts hold the party and ends once the player has had it for EndTicks,
	 * so a scene told in pieces stays one; behind a black screen it ends at once. A map with no actor for the pad to
	 * walk (the party's leader is not on it: a memory, a view of somewhere else) is a cutscene for as long as it runs,
	 * though its scripts hold nobody.
	 */
	class FFieldCutscenes
	{
	public:
		static constexpr int32 EndTicks = 8;

		/** Takes the field's state as a tick's events left it. */
		WELTALLCORE_API void Watch(const FEventContext& Context);
		/** A map's load, or the field's return from elsewhere: no cutscene is known yet. */
		void Reset() { *this = FFieldCutscenes(); }
		bool InCutscene() const { return bInCutscene; }

	private:
		bool bInCutscene = false;
		/** Ticks the player has had the party since the scripts last held it. */
		int32 FreeTicks = 0;
		/** The camera followed the party when the player last had it (some maps keep it on the scripts' paths as they play). */
		bool bCameraFollowed = false;
	};
}
