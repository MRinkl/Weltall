#pragma once

#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	/** How the field shows a movie the scripts play. */
	enum class EFieldMovieView : uint8
	{
		/** Full screen, the field stopped. */
		FullScreen = 0,
		/** Full screen, with the first actor's scripts and the message windows running over it. */
		OverField = 1,
		/** In the scene, as a texture the map's models show, the field running on. */
		InScene = 2,
	};

	/** A movie the scripts ask the field to play (FE 60, FE 67, FE A0), as the field hands it to the movie player. */
	struct FFieldMovieRequest
	{
		static constexpr uint16 NoCues = 0xFF;
		/** Flags: the player may skip it; the movie's own sound is not played (as with a set of cues). */
		static constexpr uint8 Skippable = 0x80;
		static constexpr uint8 OwnSoundOff = 0x40;

		uint16 Movie = 0;
		uint16 StartSector = 0;
		uint16 StartFrame = 0;
		uint16 EndFrame = 0;
		/** The flags operand's low nibble, which FE A0 keeps from the movie before. */
		uint16 Kind = 0;
		/** Where the player decodes its two pictures in VRAM (x, y each), and their size. */
		std::array<int16, 4> Buffers{};
		int16 Width = 0;
		int16 Height = 0;
		/** Decoded in 24-bit colour. */
		bool bTrueColour = false;
		/** The set of sound cues FE A0 plays along, NoCues for none. */
		uint16 Cues = NoCues;
		/** It starts again from its first frame at its end, until the scripts stop it. */
		bool bRewinds = false;
		uint8 Flags = 0;
	};

	/** What the movie player tells the field's scripts of the movie it plays. */
	struct FFieldMoviePlayback
	{
		/** Set when a movie starts playing; FE 61 waits for it and clears it. */
		bool bStarted = false;
		/** FE CB's requests to stop a movie shown in the scene. */
		int32 StopRequests = 0;
		/** The frame the movie shows (FE 85). */
		int32 Frame = 0;
	};

	/**
	 * What a tick's scripts asked the field to do. The scripted battle, the world map and the map
	 * change stop every script at once; the field's main loop takes them all up once its tick has run.
	 */
	struct FEventRequests
	{
		bool bScriptedBattle = false;
		bool bWorldMap = false;
		/** The next map is the context's MapNumber, entered by the entrance variable 2 holds. */
		bool bMapChange = false;
		/** How the screen goes from this map to the next (0 to 6), and over how many ticks. */
		int32 MapTransition = 2;
		int32 MapTransitionTicks = 0x20;
		/** Another program (FE 83): bits 0-6 its number, bit 7 a new game's state loaded first. */
		bool bProgram = false;
		int32 Program = 0;
		/** The Battling arena (FE BF), and the six bytes its match starts from. */
		bool bBattling = false;
		std::array<uint8, 6> BattlingParameters{};
		/** How the battle asked for starts: a script's takes the scripted battles' kind when it asks, an encounter's the encounters' when the field leaves. */
		uint8 BattleKind = 0;
		/** A battle or the arena was asked for: FE 7F waits until the field is left. */
		bool bLeavingField = false;
		/** FE 84's battle names the map the field goes on with after it, which then starts afresh. */
		bool bBattleNamesNextMap = false;
		/** A movie is to play (FE 60, FE 67, FE A0): the map changes wait for it. */
		bool bMovie = false;
		FFieldMovieRequest Movie;
		/** A dissolve of the screen the main loop runs after the tick (FE 9B-9D: modes 1 to 3), and its ticks. */
		uint8 Dissolve = 0;
		int32 DissolveTicks = 0;

		bool StopScripts() const { return bScriptedBattle || bWorldMap || bMapChange; }
	};

	/** What the field's main loop leaves to the game around it. */
	enum class EFieldRequest : uint8
	{
		None,
		/** The Battle program: a random encounter's battle or a script's. */
		Battle,
		/** The world map program, at the place the game state holds. */
		WorldMap,
		/** The Battling arena's program. */
		Battling,
		/** A program the scripts name (FE 83). */
		Program,
		/** The menu, which the field runs inside itself. */
		Menu,
		/** A movie, which the field plays inside itself. */
		Movie,
		/** L1, R1, Start and Select held together: the console starts again. */
		SoftReset,
	};

	/**
	 * The request the field stops at, with its parameters as the field hands them to the kernel. For
	 * the programs the field has already left its map: its place, variables and song are in the game
	 * state, as the next program reads them.
	 */
	struct FFieldRequest
	{
		EFieldRequest Kind = EFieldRequest::None;
		/** The program the kernel starts next: Battle 2, the world map 3, Battling 4, or the scripts' own. */
		uint8 NextProgram = 0;
		/** The scripts' program starts from a new game's state. */
		bool bNewGame = false;
		/** The field comes back to its map as it left it when the next program ends (a battle, the arena). */
		bool bFieldKept = false;
		/** The battle's formation (of the map's encounter section) and how it starts. */
		uint8 Formation = 0;
		uint8 BattleKind = 0;
		/** A random encounter's battle, which plays the encounters' song. */
		bool bEncounter = false;
		uint16 BattleSong = 0;
		std::array<uint8, 6> BattlingParameters{};
		/** The menu's mode (0 Normal Menu, 1 Member Change, 2 Load Game, 3 Enter Name, 4 Shop, 5 Robo, 6 CD Change), its parameter, and whether the player's button opened it. */
		uint8 MenuMode = 0;
		uint8 MenuParam = 0;
		bool bMenuByButton = false;
		/** The gears cannot be boarded or left from the menu. */
		bool bGearBoardingLocked = false;
		FFieldMovieRequest Movie;
		EFieldMovieView MovieView = EFieldMovieView::FullScreen;
	};
}
