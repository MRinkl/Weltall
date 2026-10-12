#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FEventContext;

	/**
	 * Where a chapter's title card begins when the story passes the chapter inside scenes and battles, with no moment
	 * of the player's control to begin it.
	 *
	 * Such a chapter's card begins as soon as the story is in the chapter, or at a place of a scene's script chosen
	 * for it, where the title belongs to what the scenes show rather than to where the story's progress is counted:
	 * the attack on Lahan is under way long before its chapter's value is set, and Shevat's council sits two minutes
	 * after its own is. The places are the ends of scenes, where the screen goes to black and the title stands alone.
	 * No card begins while a movie plays: it waits for the scene to come back.
	 */
	namespace ChapterCues
	{
		/**
		 * Whether Chapter's (numbered from 1) card begins as soon as the story is in the chapter: the story passes it
		 * without giving the player the party, and no scene has a place for it.
		 */
		WELTALLCORE_API bool BeginsWithStory(int32 Chapter);

		/** The chapter whose card a scene of the running map asks for now: one of its scripts waits at the scene's place. 0 for none. */
		WELTALLCORE_API int32 AskedByScene(const FEventContext& Context);
	}
}
