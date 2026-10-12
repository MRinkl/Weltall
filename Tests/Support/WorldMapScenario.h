#pragma once

#include "Foundation/Result.h"
#include "Game/GameState.h"
#include "Game/GameStateNames.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteMath.h"
#include "Input/KernelPad.h"
#include "Input/PadState.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/FieldTickSession.h"
#include "Text/MesResident.h"
#include "Timing/PlayClock.h"
#include "WorldMap/WorldMapRuntime.h"
#include "WorldMap/WorldMapTables.h"

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Weltall
{
	class FIso9660;
}

namespace Weltall::Testing
{
	/**
	 * A game state the world map oracles start the
	 * world map from: a party with their gears, who rides, the story's value, the place, entrance and camera
	 * yaw the field hands over, the scene, the Yggdrasil's place and flags; and how long it runs, with what
	 * the pad's script adds.
	 */
	struct FWorldMapScenario
	{
		const char* Name;
		uint16 Story;
		uint16 Entrance;
		uint16 Location;
		uint16 Yaw = 0xC00;
		std::array<uint8, 3> Party{0, 2, 3};
		std::array<uint8, 3> Riding{0, 0, 0};
		int32 Ticks = 256;
		/** Buttons pressed at given frames besides the script's. */
		std::array<std::pair<int32, uint16>, 4> Presses{};
		/** The Yggdrasil: its place and heading (whole units), and the flags word (the type, 2000h start at it, 4000h aboard). */
		std::array<int16, 4> Vehicle{};
		uint16 VehicleFlags = 0;
		/** Buttons held from one frame to (not including) another besides the script's, and the first frames with none at all. */
		std::array<std::array<int32, 3>, 7> Holds{};
		int32 Quiet = 0;
		/** The scene word: 1 the map, 2-5 the Yggdrasil's scripted voyages, 8-18 the cutscenes. */
		uint16 Scene = 1;
		/** The places the minimap shows, a bit each. */
		uint32 MinimapDots = 0;
		/** The disc it runs from. */
		EDiscRelease Disc = EDiscRelease::UsaDisc1;
	};

	/** The oracles' scenarios, in their order: a scenario's pad script is seeded by its place in it. */
	std::span<const FWorldMapScenario> WorldMapScenarios();

	/** The scenario of that name, and its place among them. */
	std::pair<const FWorldMapScenario*, std::size_t> FindWorldMapScenario(const std::string& Name);

	/** Physical buttons in the controller's report word, as the scenarios name them. */
	namespace WorldMapButtons
	{
		constexpr uint16 L2 = 0x01, R2 = 0x02, L1 = 0x04, R1 = 0x08, Triangle = 0x10, Circle = 0x20, Cross = 0x40, Square = 0x80, Select = 0x100,
			Start = 0x800;
		constexpr uint16 Up = 0x1000, Right = 0x2000, Down = 0x4000, Left = 0x8000;
	}

	/** What the world map oracles read from the disc: its files, the program's tables and the resident text. */
	struct FWorldMapOracleFiles
	{
		const FIso9660* Disc = nullptr;
		std::optional<FPsxExecutable> Executable;
		std::optional<FGameFileIndex> Index;
		std::optional<FGteArctangents> Arctangents;
		std::optional<FWorldMapTables> Tables;
		std::optional<FMesResident> Resident;

		/** Reads them from a release's disc; the reason when it cannot. */
		std::optional<FError> Read(const FIso9660& InDisc, EDiscRelease Release = EDiscRelease::UsaDisc1);
	};

	/** What a scenario's world map runs with: its game state, the harness's random seed, the pad and the play clock. */
	struct FWorldMapOracleSession
	{
		static constexpr uint32 RandomSeed = 0x1D5C6A31;

		FGameState Game;
		FPsxRandom Random{RandomSeed};
		FKernelPad Pad;
		FPlayClock Clock;
		FGameStateNames Names{Game};
		/** What the world map's sound calls go to; none: silent. */
		IWorldMapSound* Sound = nullptr;
		/** The fixes the world map runs with: none, as the oracles check the original. */
		FWorldMapOptions Options = FWorldMapOptions::Original();

		explicit FWorldMapOracleSession(const FWorldMapScenario& Scenario);
		FWorldMapOracleSession(const FWorldMapOracleSession&) = delete;
		FWorldMapOracleSession& operator=(const FWorldMapOracleSession&) = delete;

		/** The world map's services over the session and the files; both must outlive the runtime. */
		FWorldMapServices Services(const FWorldMapOracleFiles& Files);
	};

	/** What a scenario's run does besides running the world map: after each entry, for each frame, after each tick of the loop. */
	struct FWorldMapOracleHooks
	{
		/** After the world map is entered, and entered again after a battle. */
		std::function<std::optional<FError>(FWorldMapRuntime&)> Entered;
		/** One frame of the world map with the buttons of its vertical blanks; the runtime's own Tick when not given. */
		std::function<std::optional<FError>(FWorldMapRuntime&, std::span<const FPadState>)> Frame;
		/** After each tick of the loop (not a frame of the pause), numbered from 0. */
		std::function<void(FWorldMapRuntime&, int32)> Ticked;
	};

	/**
	 * A scenario's world map as the oracles run it: entered from its game state, then frame after frame
	 * with the pad's script until Ticks ticks have passed or it leaves for the field (after a battle it
	 * starts again from what it kept; the menu closes at once). Returns why it stopped early, empty when
	 * it did not.
	 */
	std::string RunWorldMapScenario(const FWorldMapOracleFiles& Files, std::size_t Number, int32 Ticks, FWorldMapOracleSession& Session,
		FWorldMapOracleHooks& Hooks);
}
