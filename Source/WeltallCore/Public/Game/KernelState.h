#pragma once

#include "Field/FieldKernelState.h"
#include "Game/GameProgram.h"
#include "Graphics/GteMatrix.h"

#include <array>
#include <memory>
#include <optional>

namespace Weltall
{
	class FGameState;
	struct FWorldMapBattleKeep;

	/** The movie the kernel asks the Movie program for. */
	struct FMovieProgramRequest
	{
		/** The type that asks for no movie: the program shows its debug menu instead. */
		static constexpr uint8 NoMovie = 0xFF;
		/** In the type: the movie plays to EndFrame rather than to frame 233. */
		static constexpr uint8 EndFrameGiven = 0x80;
		static constexpr uint8 KindBits = 0x7F;

		/** Bits 0-6: 0 its picture alone, 1 its picture and sound, 2 its sound alone; bit 7 EndFrameGiven. */
		uint8 Type = 0;
		uint8 Number = 0;
		/** The program after the movie. */
		EGameProgram NextProgram = EGameProgram::DebugMenu;
		/** Nonzero: the player cannot skip it. */
		uint8 NoSkip = 0;
		uint16 EndFrame = 0;
	};

	/** Whose song the kernel's current sequence is: the one the field's music or the world map's loaded last. */
	enum class EKernelMusic : uint8
	{
		Field,
		WorldMap,
	};

	/** What the Battle program leaves the kernel as it returns, which picks the program after it. */
	struct FBattleProgramEnd
	{
		/** The battle's result: 1 won (or ended by its event), 0x40 escaped, 0x81 lost. */
		uint8 Result = 0;
		/** The battle's event asked for a movie to follow. */
		bool bMovie = false;
	};

	/**
	 * The kernel's globals the game's programs hand each other, as the console keeps them from power-on:
	 * the program to run next, the movie the Movie program plays, the battle the Battle program sets up,
	 * what the field keeps of itself between its runs, the menu's mode, the arena's match, and what a new
	 * game sets. The result a program leaves the next (1 after a lost battle, 2 after a game loaded) is
	 * the game state's ProgramResult.
	 */
	struct FKernelState
	{
		static constexpr std::size_t FormationBytes = 0x20;
		static constexpr int32 FormationCount = 16;
		/** The map a lost battle returns to: the title's. */
		static constexpr uint16 TitleMap = 0x1EA;
		static constexpr uint8 BattleWon = 1;
		static constexpr uint8 BattleEscaped = 0x40;
		static constexpr uint8 BattleLost = 0x81;
		/** A result the kernel tests for as a battle's end, though no battle leaves it. */
		static constexpr uint8 BattleResultUnused = 0x21;

		/** The program the kernel runs when the running one ends: the debug menu unless it names another. */
		EGameProgram NextProgram = EGameProgram::DebugMenu;
		FMovieProgramRequest Movie;

		/**
		 * The disc in the drive, whose tables the kernel read (its directory table's last word, less 1): 0
		 * the first, 1 the second. The Menu takes another when the game asks for it.
		 */
		int32 Disc = 0;

		/**
		 * The GTE's light matrix, which nothing but a lit drawing loads: zeros from power-on, then the one the last
		 * relit model, gear part or lit sprite model was drawn with, in whichever program drew it. A program's lit
		 * drawings that load none of their own (a battle effect's models) take it as the program before left it.
		 */
		FGteMatrix LightMatrix;

		// The battle.
		/** The kind of start the battle makes (its sounds, its transition, the party's entrance). */
		uint8 BattleKind = 0;
		/** Set at rest: a battle would open its scene select; every battle the game starts clears it. */
		bool bBattleDebug = false;
		/** The battle's event asked for another battle straight after: its formation + 1, 0 for none. */
		uint8 ChainedBattle = 0;
		/**
		 * A result screen has played its fanfare since a chained battle last began: the next chained battle, however
		 * much later, puts the music back at full volume as it starts.
		 */
		bool bResultFanfare = false;
		/** The song the battle plays: the one the program that started it (the field or the world map) left playing. */
		EKernelMusic Music = EKernelMusic::Field;
		/**
		 * The formations the battle is set up from: the last field map's or world map's table, 16 of
		 * FormationBytes. Their weights are the field's (Field.EncounterWeights).
		 */
		std::array<uint8, FormationBytes * FormationCount> Formations{};

		// The field.
		/** What the field reads and writes of the kernel, from one run to the next; the battle's formation among it. */
		FFieldKernelState Field;
		/** The field has entered a map since power-on: a later run starts with the song the game state keeps. */
		bool bFieldEntered = false;
		/** The field's trips to a battle or the arena, which it comes back from as it left, and to the arena alone. */
		int32 FieldTrips = 0;
		int32 ArenaTrips = 0;
		/** The next map comes in with the battle event's own fade. */
		bool bEventFadeIn = false;

		// The menu, when a program runs it.
		/** Mode 0 Normal Menu, 1 Member Change, 2 Load Game, 3 Enter Name, 4 Shop, 5 Robo, 6 CD Change. */
		uint8 MenuMode = 0;
		/** The menu runs as a program of its own (the debug menu's way), not inside the field or the world map. */
		bool bMenuStandalone = false;

		// The arena.
		std::array<uint8, 6> BattlingParameters{};
		/** Which of the arena's task functions it runs: there is one. */
		uint8 BattlingTask = 0;

		// The world map.
		/** What the world map kept of itself for a battle's return (its task slots, the party's trail, the camera); none before one. */
		std::shared_ptr<const FWorldMapBattleKeep> WorldMapKeep;

		// What a new game sets.
		/** The colour the field's and the battle's message windows are drawn in. */
		std::array<uint8, 3> WindowColour{};
		/** The blend mode of the battle's windows. */
		uint8 BattleWindowBlend = 0;

		/**
		 * The globals as the kernel's start-up leaves them before it shows its notice, the movie request
		 * among them: the opening's logos (disc 1's movie 16, disc 2's movie 7), then the field.
		 * DiscNumber: 0 for the first disc.
		 */
		WELTALLCORE_API static FKernelState PowerOn(int32 DiscNumber);

		/** What a new game sets beside its state (the kernel's new game, after the state is loaded). */
		WELTALLCORE_API void InitNewGame();

		/**
		 * The Battle program returns: the program after it from its end (the movie it asked for, a
		 * chained battle, the world map or the field by the game state's place), and after a lost battle
		 * the title's map with the field starting afresh; the battle debug set at rest unless a battle
		 * follows.
		 */
		WELTALLCORE_API void EndBattle(const FBattleProgramEnd& End, FGameState& Game);

		/** Formation Index's record in the table, when the table holds it. */
		WELTALLCORE_API std::optional<std::array<uint8, FormationBytes>> Formation(int32 Index) const;
	};

	/** The program a loaded game goes on in, as the title's map sends one: the Field program on a field map, the world map's from place 400h up. */
	WELTALLCORE_API EGameProgram LoadedGameProgram(const FGameState& Game);
}
