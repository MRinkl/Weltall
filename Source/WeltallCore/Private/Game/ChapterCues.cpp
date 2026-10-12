#include "Game/ChapterCues.h"

#include "Field/EventContext.h"
#include "Field/EventScripts.h"

#include <algorithm>
#include <array>

namespace Weltall
{
	namespace
	{
		/** The chapters the story passes inside scenes whose cards begin with the story's value: the second disc's narrated ones. */
		constexpr std::array<int32, 6> StoryChapters = {57, 61, 62, 63, 65, 67};

		/** A place of a map's script that asks for a chapter's card: where a script waits, known by the bytes there. */
		struct FSceneCue
		{
			int32 Chapter = 0;
			int32 Map = 0;
			uint16 Offset = 0;
			std::array<uint8, 3> Instruction{};
		};

		constexpr std::array<FSceneCue, 5> SceneCues = {{
			// Lahan burning: Fei runs off to look for the villagers and the scene cuts to the gears among the houses.
			{5, 2, 0x0449, {0x09, 0x01, 0x6A}},
			// Solaris's dock goes to black after the red gear's rampage, before the Yggdrasil's bridge watches the city fall.
			{53, 610, 0x016B, {0x5B, 0x36, 0x04}},
			// Shevat's throne room fades out on Citan's account of Fei, before the council that decides what to do with him.
			{54, 453, 0x0145, {0x26, 0x78, 0x80}},
			// The cage fades out with Fei set free, before the two of them make for the gear hangar.
			{55, 467, 0x00CE, {0x26, 0x50, 0x80}},
			// Lacan's memory fades to white, before the voice that calls Fei: its scene's lines come where the title stands.
			{66, 682, 0x03FE, {0x26, 0x5A, 0x80}},
		}};

		bool IsAt(const FEventContext& Context, const FSceneCue& Cue)
		{
			const FEventScripts& Scripts = *Context.Scripts;
			if (Cue.Offset + Cue.Instruction.size() > Scripts.CodeSize() || !std::equal(Cue.Instruction.begin(), Cue.Instruction.end(), Scripts.Code() + Cue.Offset))
			{
				return false;
			}
			for (const FEventActorState& State : Context.States)
			{
				for (const FEventThread& Thread : State.Threads)
				{
					if (!Thread.IsFree() && Thread.Pc == Cue.Offset)
					{
						return true;
					}
				}
			}
			return false;
		}
	}

	bool ChapterCues::BeginsWithStory(int32 Chapter)
	{
		return std::find(StoryChapters.begin(), StoryChapters.end(), Chapter) != StoryChapters.end();
	}

	int32 ChapterCues::AskedByScene(const FEventContext& Context)
	{
		if (Context.Scripts == nullptr || !Context.bFieldRunning)
		{
			return 0;
		}
		for (const FSceneCue& Cue : SceneCues)
		{
			if (Cue.Map == Context.RunningMap && IsAt(Context, Cue))
			{
				return Cue.Chapter;
			}
		}
		return 0;
	}
}
