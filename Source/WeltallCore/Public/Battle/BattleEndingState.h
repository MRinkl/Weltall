#pragma once

#include "Battle/BattleEventState.h"
#include "Foundation/Types.h"

#include <optional>

namespace Weltall
{
	/** What the battle's end hands the show. */
	enum class EBattleEndCue : uint8
	{
		/** The disc's reads given up and the sprites' loads finished (the game waits: frames while the show blocks). */
		FinishLoads,
		/** The party's end pose (Arg an EBattleEndPose), then the enemies released (frames while the show blocks). */
		Pose,
		/** The scene dimmed behind the result screen. */
		Dim,
		/** The battle's music fades out. */
		FadeMusic,
		/** A sound of the result screen (Arg the battle's common effect): the EXP count's tick, the fanfare's three. */
		ResultSound,
		/** The result screen closes: every effect still playing stops. */
		StopSounds,
	};

	/** The pose the party takes as the battle ends. */
	enum class EBattleEndPose : uint8
	{
		/** The camera on everyone, the standing members in their victory pose. */
		Victory = 0,
		None = 1,
		/** The members run, the screen fades to black. */
		Escape = 2,
	};

	/** The battle's end under way: the game's steps from the battle's loop to the result. */
	struct FBattleEndingWork
	{
		uint8 Pc = 0;
		/** How the end was sorted: 0 won, 1 escaped, 2 lost, 3 a scripted defeat, above that none of them. */
		uint8 Kind = 0;
		EBattleEndPose Pose = EBattleEndPose::None;
		/** The result, put aside while the event's run after the battle sees none. */
		uint8 SavedEnd = 0;
		/** The event's own music was loaded when the event ended. */
		bool bEventMusic = false;
		bool bRunning = false;
	};

	/**
	 * What the battle leaves the programs after it, beside what it changed in the game state (the party,
	 * the items, the rewards, the mode's result, the acting turns, the next map an event set).
	 */
	struct FBattleOutcome
	{
		/** 1 won or ended by the event, 0x40 escaped, 0x81 lost (BattleEnd). */
		uint8 Result = 0;
		/** The battle the event chained to this one. */
		std::optional<FBattleChain> Chain;
		/** The movie the event asked for. */
		std::optional<FBattleMovieRequest> Movie;
		/** The field starts afresh at the map the event set. */
		bool bFieldStartsFresh = false;
		/** The field's return flag the event set. */
		std::optional<uint8> FieldReturn;
		/** The result screen played its fanfare. */
		bool bFanfarePlayed = false;
	};
}
