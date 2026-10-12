#pragma once

#include "WorldMap/WorldMapCutscenes.h"

namespace Weltall::WorldMapOps
{
	/** What the directors of scenes 13, 15 and 16 share: a script of steps, each taken after a wait. */
	namespace DirectorStep
	{
		/** Nothing more happens. */
		inline constexpr int16 Done = 0;
		/** The wait before the script's next step. */
		inline constexpr int16 Wait = 1;
		/** The scene ends. */
		inline constexpr int16 Leave = 0x40;
	}

	/** The messages the fade's task takes, and its blends: adding (to white) and subtracting (to black). */
	namespace DirectorFade
	{
		inline constexpr int16 In = 0xC;
		inline constexpr int16 Out = 0xD;
		inline constexpr int32 White = 1;
		inline constexpr int32 Black = 2;
	}

	/** A director's words: where its script stands (+50h). */
	inline constexpr int32 DirectorScriptIndex = 0;
	/** A camera task's word: how far its shake reaches (+7Ch, 20.12). */
	inline constexpr int32 CameraShakeReach = 11;
	inline constexpr int32 CameraShakeStill = 0x1000;

	/** The script's entry at the director's index into its step and timer; a script that ended before is a fault. */
	void TakeDirectorStep(FWorldMapContext& Context, FWorldMapTask& Task, const FWorldMapDirectorScript& Script);

	/** The director's wait: its timer run down a tick, and once below zero the script's next step and wait taken, the index moved on. */
	void WaitDirector(FWorldMapContext& Context, FWorldMapTask& Task, const FWorldMapDirectorScript& Script);

	/** The scene ends: the loop stops after this pass and leaves for the field. */
	void LeaveCutscene(FWorldMapContext& Context, FWorldMapTask& Task);

	/** A message to the fade's task, with the blend and step it then draws and moves by. */
	void SendFade(FWorldMapContext& Context, int16 Message, int32 Blend, int32 Step);

	/**
	 * A camera task's shake, every tick: one rand() within its reach about 0, added to the heights of the orbit's eye
	 * and target (nothing while the reach is one unit, though the draw is made).
	 */
	void ShakeCamera(FWorldMapContext& Context, const FWorldMapTask& Task);

	/**
	 * A camera task as scenes 13, 15 and 16 start theirs: turning by its angles, its shake still, the screen's centre
	 * at 120, the target and the controlled place at the scene's start, and a first message (1) to itself, which its
	 * first update takes.
	 */
	void InitDirectedCamera(FWorldMapContext& Context, FWorldMapTask& Task);

	/**
	 * A camera easing in: the words it keeps (the distance it heads for +50h; the pitch it heads for and its pitch
	 * +54h, +58h; the yaw it heads for and its yaw +5Ch, +60h; all 20.12), and how each goal moves a tick and where
	 * it stops.
	 */
	struct FCameraEase
	{
		int32 DistanceStep;
		int32 DistanceEnd;
		int32 PitchStep;
		int32 PitchEnd;
		int32 YawStep;
		int32 YawEnd;
	};

	/** The ease begun: the camera cut to a distance (20.12), pitch and yaw, its goals there too. */
	void StartCameraEase(FWorldMapContext& Context, FWorldMapTask& Task, int32 Distance, int16 Pitch, int16 Yaw);

	/** The ease's tick: each goal a step on, the distance following by a 32nd of what is left, the pitch by a 16th, the yaw by a 32nd. */
	void EaseCamera(FWorldMapContext& Context, FWorldMapTask& Task, const FCameraEase& Ease);

	/** A place in whole units, as the particle effects take it. */
	std::array<int16, 4> EffectPlace(const FWorldMapVector& Position);

	/**
	 * A ring of light on the ground, two glowing discs (the models First and First + 1) at the task's place: as it
	 * starts, its words (the first disc's width +50h and the second's +54h, 4.12, the second from half a width turned
	 * about; their brightness +58h) and the discs' look; the task then waits for a message.
	 */
	FResult InitGlowRing(FWorldMapContext& Context, FWorldMapTask& Task, int32 First);

	/**
	 * The ring's tick: the discs placed and widened, the packets of the buffer being drawn dimmed to the brightness
	 * (the other buffer keeps the tick before's), which then falls by Dimming; below zero the task waits again, its
	 * discs left as they are.
	 */
	FResult UpdateGlowRing(FWorldMapContext& Context, FWorldMapTask& Task, int32 First, int32 Dimming);

	/** The field's song stopped, as the scenes with a song of their own start. */
	void StopFieldSong(FWorldMapContext& Context);

	/** The scene's bank loaded, its effects registered, its song loaded and played at full volume. */
	void StartCutsceneSong(FWorldMapContext& Context);
}
