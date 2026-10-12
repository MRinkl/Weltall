#pragma once

#include "WorldMap/WorldMapOps.h"

#include <array>
#include <optional>
#include <span>

namespace Weltall::WorldMapOps
{
	/** A cutscene task's routine. */
	struct FCutsceneRoutine
	{
		EWorldMapRoutine Routine;
		FResult (*Run)(FWorldMapContext& Context, int32 Slot);
	};

	/**
	 * A cutscene scene (8-18) as the program plays it: the scene numbers that play it, its start's fade, its own start
	 * (after the one every cutscene shares: its start point, models and clouds, its sound, its tasks), its end (its sound,
	 * and the field's map, entrance and camera yaw it hands over) and the routines of its tasks. Each scene keeps its own
	 * in its own file, and the table in WorldMapCutscenes.cpp lists them.
	 */
	struct FCutscene
	{
		std::array<int16, 2> Scenes;
		int32 FadeBlend = 0;
		int32 FadeStep = 0;
		void (*Start)(FWorldMapContext& Context) = nullptr;
		void (*End)(FWorldMapContext& Context) = nullptr;
		std::span<const FCutsceneRoutine> Routines;
	};

	/** The cutscene that plays a scene, none where the port does not play it yet. */
	const FCutscene* FindCutscene(int32 Scene);

	/** A cutscene task's routine run, none where no cutscene the port plays has it. */
	std::optional<FResult> RunCutsceneRoutine(FWorldMapContext& Context, int32 Slot, EWorldMapRoutine Routine);

	/** The start every cutscene shares (the display's distance, the fade's blend and step, no menu, the camera by its angles), then the scene's own. */
	void StartCutscene(FWorldMapContext& Context, const FCutscene& Cutscene);

	/** The start point (20.12), then the models and the clouds (their rand()), as every cutscene's start places them. */
	void StartCutsceneWorld(FWorldMapContext& Context, const FWorldMapVector& Start);

	/** The scene's effects registered with the sound driver (its group's effect file), as scenes 9-18 start. */
	void RegisterCutsceneEffects(FWorldMapContext& Context);

	/** Every effect stopped and the scene's effects unregistered, as the ends of scenes 9-18 begin. */
	void StopCutsceneEffects(FWorldMapContext& Context);

	/** What every cutscene's end hands the field: the map and entrance and its own camera yaw (the place's record is then not read). */
	void EndCutscene(FWorldMapContext& Context, uint16 Map, uint16 Entrance);

	/** The scene's effect played, its volume set and faded (over the driver's ticks). */
	void PlayCutsceneEffect(FWorldMapContext& Context, int32 Effect);
	/** The scene's effect played where the effects after it cannot take its channels. */
	void PlayProtectedCutsceneEffect(FWorldMapContext& Context, int32 Effect);
	void SetCutsceneEffectVolume(FWorldMapContext& Context, int32 Effect, int32 Volume);
	void FadeCutsceneEffect(FWorldMapContext& Context, int32 Effect, int32 Volume, int32 Ticks);

	/**
	 * A glow: a model's packets drawn semi-transparent (the background and a quarter of them) with the
	 * particles' 8-bit page and CLUT, its texture modulated by mid grey, in both packet buffers.
	 */
	void SetGlowLook(FWorldMapObject& Object);

	/**
	 * A director's wait between its steps: its timer run down a tick and, once below zero, its next step and wait taken
	 * from its lists at its index (the task's first word), which moves on. A fault where the lists have ended.
	 */
	void WaitForCutsceneStep(FWorldMapContext& Context, FWorldMapTask& Task, std::span<const int16> Steps, std::span<const int16> Waits);

	/** The scenes. */
	extern const FCutscene TelescopeCutscene;
	extern const FCutscene Scene9Cutscene;
	extern const FCutscene Scene10Cutscene;
	extern const FCutscene Scene12Cutscene;
	extern const FCutscene Scene14Cutscene;
	extern const FCutscene Scene13Cutscene;
	extern const FCutscene Scene15Cutscene;
	extern const FCutscene Scene16Cutscene;
	extern const FCutscene Scene17Cutscene;
	extern const FCutscene Scene18Cutscene;
}
