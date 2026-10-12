#pragma once

#include "Foundation/Types.h"
#include "WorldMap/WorldMapTerrain.h"

#include <array>

namespace Weltall
{
	/**
	 * The world map's task routines: each slot runs an init, then an update every tick. Numbered as
	 * the playable scenes list them (the base list, then each era's), then the inits the return from
	 * a battle sets, then the cutscene scenes' own, scene by scene in slot order (a routine two scenes
	 * share listed once, where it first comes).
	 */
	enum class EWorldMapRoutine : uint8
	{
		None = 0,
		InitFade,
		UpdateFade,
		InitLeader,
		UpdateLeader,
		InitMember2,
		UpdateMember,
		InitMember3,
		InitGear1,
		UpdateGear1,
		InitGear2,
		UpdateGear,
		InitGear3,
		InitVehicle,
		UpdateVehicle,
		InitVehicleParts,
		UpdateVehicleParts,
		InitCameraTarget,
		UpdateCameraTarget,
		InitCameraDistance,
		UpdateCameraDistance,
		InitScreenCentre,
		UpdateScreenCentre,
		InitPlaceName,
		UpdatePlaceName,
		InitNotice,
		UpdateNotice,
		InitFrame,
		UpdateFrame,
		InitSpinner,
		UpdateSpinner,
		InitThames,
		UpdateThames,
		InitShevat,
		UpdateShevat,
		InitFastSpinnerPair,
		UpdateFastSpinnerPair,
		InitSpinnerPair,
		UpdateSpinnerPair,
		InitStoryObject,
		UpdateStoryObject,
		InitLandmarkColliderC,
		UpdateLandmarkColliderC,
		InitEmptyEraSlot,
		UpdateEmptyEraSlot,
		InitLandmarkColliderA,
		UpdateLandmarkColliderA,
		InitLandmarkColliderB,
		UpdateLandmarkColliderB,
		RestoreLeader,
		RestoreMember2,
		RestoreMember3,
		RestoreGear1,
		RestoreGear2,
		RestoreGear3,
		RestoreVehicle,
		RestoreVehicleParts,
		RestoreThames,
		RestoreShevat,
		RestoreFastSpinnerPair,
		RestoreStoryObject,
		// Scenes 8 and 11, the telescope.
		InitTelescopeCamera,
		UpdateTelescopeCamera,
		// Scene 9.
		InitScene9Camera,
		UpdateScene9Camera,
		InitScene9Craft,
		UpdateScene9Craft,
		InitCutsceneFrame,
		UpdateCutsceneFrame,
		// Scene 10.
		InitScene10Director,
		UpdateScene10Director,
		InitScene10Craft,
		UpdateScene10Craft,
		InitScene10Rings,
		UpdateScene10Rings,
		InitScene10Trail,
		UpdateScene10Trail,
		InitIdleTask,
		UpdateScene10Burst,
		InitScene10SeaObject,
		UpdateScene10SeaObject,
		// Scene 12.
		InitScene12Director,
		UpdateScene12Director,
		InitScene12Camera,
		UpdateScene12Camera,
		InitScene12Lead,
		UpdateScene12Lead,
		InitScene12Ship5,
		UpdateScene12Ship5,
		InitScene12Ship9,
		UpdateScene12Ship9,
		InitScene12Ship10,
		UpdateScene12Ship10,
		InitScene12Ship12,
		UpdateScene12Ship12,
		InitScene12Ship13,
		UpdateScene12Ship13,
		InitScene12LateShip,
		UpdateScene12LateShip,
		// Scene 13.
		InitScene13Director,
		UpdateScene13Director,
		InitScene13Camera,
		UpdateScene13Camera,
		InitScene13Effects,
		UpdateScene13Effects,
		InitScene13Ring,
		UpdateScene13Ring,
		InitCutsceneFrameTrees,
		UpdateCutsceneFrameTrees,
		// Scene 14.
		InitScene14Director,
		UpdateScene14Director,
		InitScene14Camera,
		UpdateScene14Camera,
		InitScene14Rings1,
		UpdateScene14Rings1,
		InitScene14Rings2,
		UpdateScene14Rings2,
		InitScene14Streak,
		UpdateScene14Streak,
		InitScene14Flyer,
		UpdateScene14Flyer,
		// Scene 15.
		InitScene15Director,
		UpdateScene15Director,
		InitScene15Camera,
		UpdateScene15Camera,
		InitScene15Shot,
		UpdateScene15Shot,
		InitScene15Ring,
		UpdateScene15Ring,
		InitScene15LightRing,
		UpdateScene15LightRing,
		// Scene 16.
		InitScene16Director,
		UpdateScene16Director,
		InitScene16Camera,
		UpdateScene16Camera,
		InitScene16Core,
		UpdateScene16Core,
		InitScene16Pillar,
		UpdateScene16Pillar,
		InitScreenWaver,
		UpdateScreenWaver,
		InitWaverControl,
		UpdateWaverControl,
		// Scene 17.
		InitScene17Script,
		RunScript,
		InitScene17Camera,
		UpdateScene17Camera,
		InitRibbon,
		UpdateRibbon,
		InitRestream,
		UpdateRestream,
		// Scene 18.
		InitScene18Script,
		InitScene18Camera,
		UpdateScene18Camera,
		InitMahanon,
		UpdateMahanon,
	};

	/** What a slot does each tick. */
	namespace WorldMapTaskState
	{
		inline constexpr int16 Init = 0;
		inline constexpr int16 Update = 1;
		/** The countdown runs down, then the update. */
		inline constexpr int16 Waiting = 2;
		inline constexpr int16 Idle = 3;
		inline constexpr int16 FreeSprite = 4;
	}

	/**
	 * A task slot: the words every task keeps (its state, messages, step and timer, whether it is
	 * hidden, its place, direction, heading and speed, its sprite) and twelve words each kind names
	 * (WorldMapTaskWord). The original's slots are 0x80 bytes; a word here is the word at the same
	 * place there, so one kind reading another's word reads what the original reads.
	 */
	struct FWorldMapTask
	{
		static constexpr int32 WordCount = 12;
		static constexpr int32 NoSprite = -1;

		int16 State = 0;
		int16 Countdown = 0;
		/** A message another task sent (0 none), and its argument. */
		int16 Message = 0;
		int16 MessageArgument = 0;
		EWorldMapRoutine Init = EWorldMapRoutine::None;
		EWorldMapRoutine Update = EWorldMapRoutine::None;
		/** The kind's own state machine's step, and a counter some kinds keep. */
		int16 Step = 0;
		int16 Timer = 0;
		int16 Hidden = 0;
		FWorldMapVector Position;
		/** 4.12, the way it moves. */
		FWorldMapVector Direction;
		int16 Heading = 0;
		/** Whole units a tick. */
		int16 Speed = 0;
		/** Its sprite among the runtime's, or NoSprite. */
		int32 Sprite = NoSprite;
		std::array<int32, WordCount> Words{};

		int32& Word(int32 Index) { return Words[static_cast<std::size_t>(Index)]; }
		int32 Word(int32 Index) const { return Words[static_cast<std::size_t>(Index)]; }
	};

	/** The kinds' own words (FWorldMapTask::Words): the same index names different things in different kinds. */
	namespace WorldMapTaskWord
	{
		/** The party's members and gears: the point a scripted walk heads for (whole units). */
		inline constexpr int32 WalkX = 0;
		inline constexpr int32 WalkZ = 1;
		/** The members and gears 2 and 3: how far behind on the trail they stand; the leader and gear 1: the members aboard so far. */
		inline constexpr int32 Follow = 2;
		/** Every task with a sprite: the facing its sprite turns toward its heading, at most 100h a tick. */
		inline constexpr int32 DrawnFacing = 3;

		/** The fade: its level (0-255). */
		inline constexpr int32 FadeLevel = 0;

		/** The camera's target and yaw: the yaw it heads for, its turn a tick, the yaw in 20.12, where a turn ends, whether the target catches up. */
		inline constexpr int32 GoalYaw = 0;
		inline constexpr int32 TurnStep = 1;
		inline constexpr int32 SmoothYaw = 2;
		inline constexpr int32 TurnEnd = 3;
		inline constexpr int32 CatchingUp = 4;

		/** The camera's distance and pitch: the preset, the distance and pitch it heads for, the pitch in 20.12, and which presets (0 on the ground, 1 flying). */
		inline constexpr int32 Preset = 0;
		inline constexpr int32 GoalDistance = 1;
		inline constexpr int32 GoalPitch = 2;
		inline constexpr int32 SmoothPitch = 4;
		inline constexpr int32 PitchTable = 5;
		inline constexpr int32 ClearanceTable = 6;

		/** The screen centre: its height in 20.12. */
		inline constexpr int32 CentreY = 0;

		/** The place name and the notice: the message shown. */
		inline constexpr int32 Shown = 0;

		/** The Yggdrasil's parts: the two parts' angles and their spins a tick. */
		inline constexpr int32 PartAngleA = 0;
		inline constexpr int32 PartAngleB = 1;
		inline constexpr int32 PartSpinA = 2;
		inline constexpr int32 PartSpinB = 3;

		/**
		 * The Yggdrasil: its yaw in 20.12 and yaw rate while it flies, its throttle, whether its flight's
		 * controls have started, its draft or target height, its pitch's state and its pitch (20.12), the
		 * members aboard, the direction it lands the party in, the voyage's waypoint.
		 */
		inline constexpr int32 FlightYaw = 2;
		inline constexpr int32 YawRate = 3;
		inline constexpr int32 Throttle = 4;
		inline constexpr int32 FlightStarted = 5;
		inline constexpr int32 Draft = 6;
		inline constexpr int32 PitchState = 7;
		inline constexpr int32 Pitch = 8;
		inline constexpr int32 Aboard = 9;
		inline constexpr int32 LandingDirection = 10;
		inline constexpr int32 Waypoint = 11;

		/** The spinners: an angle and its turn a tick. */
		inline constexpr int32 SpinAngle = 0;
		inline constexpr int32 SpinStep = 1;

		/** Thames: the waypoint it heads for, and where its wake is written and read. */
		inline constexpr int32 ThamesWaypoint = 0;
		inline constexpr int32 ThamesWakeWrite = 1;
		inline constexpr int32 ThamesWakeRead = 2;

		/** Shevat: its spinning part's angle and turn a tick, its ring's. */
		inline constexpr int32 ShevatSpin = 0;
		inline constexpr int32 ShevatSpinStep = 1;
		inline constexpr int32 ShevatTurn = 2;
		inline constexpr int32 ShevatTurnStep = 3;
	}
}
