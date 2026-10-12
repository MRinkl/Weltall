#pragma once

#include "Field/FieldPartySprites.h"
#include "Field/FieldRuntime.h"
#include "Game/GameState.h"
#include "Game/GameStateNames.h"
#include "Game/PsxRandom.h"
#include "Input/KernelPad.h"
#include "Input/PadState.h"
#include "Support/FieldOracleMap.h"
#include "Support/FieldSongsModel.h"
#include "Text/MesResident.h"
#include "Timing/PlayClock.h"

#include <string>
#include <vector>

namespace Weltall::Testing
{
	/** A tick of the pad's script: the buttons held, and those pressed at its first vertical blank only. */
	struct FOraclePadStep
	{
		uint16 Held = 0;
		uint16 Pressed = 0;
	};

	/**
	 * The pad's script the tick oracles play on a map, as their generator draws it from
	 * XorShift32 (seed 0x71C00000 + the map's number): runs of 4 to 23 ticks, each holding a direction
	 * or none, sometimes running, and pressing the jump, talk or turn buttons now and then on its first.
	 */
	std::vector<FOraclePadStep> OraclePadScript(int32 Map, int32 Ticks);

	/** A step's buttons at each of a tick's vertical blanks, as the pad reports them. */
	std::vector<FPadState> OracleVblanks(const FOraclePadStep& Step, int32 Vblanks);

	/** A story variable a map starts with in the tick oracles, by its byte address. */
	struct FOracleMapVariable
	{
		uint32 Address = 0;
		uint16 Value = 0;
	};

	/**
	 * The story variables the tick oracles start Map with where a new game's leave its scripts reading
	 * past their code: the place pickers 305-307,
	 * with the places the ship's chart on map 290 marks before it changes to them and the lowest story
	 * variable 0 that reaches each.
	 */
	std::vector<FOracleMapVariable> OracleMapVariables(int32 Map);

	/** The index of the first tick whose digests (space-separated) differ, -1 when they agree. */
	int32 FirstDifference(const std::string& Actual, const std::string& Original);

	/**
	 * What the tick oracles' field runs with: a new game (with, for a map whose scripts read past their
	 * code on a new game's variables, the story variables that reach it: OracleMapVariables), the
	 * harness's random seed, the songs' model, the party's files, the pad and the play clock.
	 */
	struct FOracleTickSession
	{
		static constexpr uint32 Seed = 0x1D5C6A31;

		FGameState Game;
		FPsxRandom Random{Seed};
		FFieldSongsModel Songs;
		FPartySpriteFiles Party;
		FKernelPad Pad;
		FPlayClock Clock;
		FGameStateNames Names{Game};
		/** The cockpit's instruments as the Field program lays them out, which the screen's digest draws. */
		FFieldCockpitHudLayout CockpitHud;

		static constexpr int32 AnyMap = -1;

		/** Map: the map the session enters first, whose story variables it takes; AnyMap for a new game's alone. */
		explicit FOracleTickSession(const FOracleFieldFiles& Files, int32 Map = AnyMap);
		FOracleTickSession(const FOracleTickSession&) = delete;
		FOracleTickSession& operator=(const FOracleTickSession&) = delete;

		/** The field's services over the session, the oracles' files and the resident text; all must outlive the runtime. */
		FFieldServices Services(const FOracleFieldFiles& Files, const FMesResident& Resident);
	};
}
