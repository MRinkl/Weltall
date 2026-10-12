#pragma once

#include "Field/FieldFrame.h"
#include "Field/FieldPlay.h"
#include "Field/FieldSilentSongs.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"

#include <functional>
#include <memory>

namespace Weltall
{
	class FIso9660;
}

namespace Weltall::Testing
{
	/** A new game's field played on a real map with nobody at the controller, each tick's frame kept. */
	struct FFieldPlaySession
	{
		std::unique_ptr<FPsxExecutable> Executable;
		std::unique_ptr<FGameFileIndex> Index;
		FFieldSilentSongs Songs;
		std::unique_ptr<FFieldPlay> Play;
		FFieldFrame Frame;

		/** Runs Ticks ticks, failing the test at the first error. */
		void Run(int32 Ticks, EFieldCulling Culling = EFieldCulling::InRange);
	};

	/** A new game entering Map, failing the test when it cannot. */
	std::unique_ptr<FFieldPlaySession> EnterFieldMap(const FIso9660& Disc, int32 Map);

	/** The same, the new game's state staged by Stage before the map is entered. */
	std::unique_ptr<FFieldPlaySession> EnterFieldMap(const FIso9660& Disc, int32 Map, const std::function<void(FGameState&)>& Stage);

	/** The ordering-table entry of an entry of a frame's painter's order. */
	int32 FieldDrawEntry(const FFieldFrame& Frame, const FFieldDrawItem& Item);

	/** Within an entry, what is drawn first: the gears (linked last), the particles, the characters, the children's models, then the scenery. */
	int32 FieldDrawRank(const FFieldDrawItem& Item);
}
