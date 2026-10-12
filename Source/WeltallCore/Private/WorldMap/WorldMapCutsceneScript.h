#pragma once

#include "WorldMap/WorldMapCutscenes.h"
#include "WorldMap/WorldMapScriptTables.h"

namespace Weltall::WorldMapOps
{
	/** The script task's word: how far into its script it has come, in bytes. */
	inline constexpr int32 ScriptAt = 0;

	/**
	 * A scene's script: the one the task's init named, run from where it stands until an instruction waits (a
	 * wait counting its ticks down in the task's timer, or the end, which stops the world map for the field). Any
	 * number of instructions run in a tick.
	 */
	FResult RunCutsceneScript(FWorldMapContext& Context, int32 Slot);

	/**
	 * A scripted camera's words: the target it shows in the task's position and the angles it has in its direction
	 * (pitch, yaw, roll, 20.12), the angles and the distance it eases toward, and the reach of its shake (20.12).
	 */
	namespace ScriptCameraWord
	{
		inline constexpr int32 GoalPitch = 0;
		inline constexpr int32 GoalYaw = 1;
		inline constexpr int32 GoalRoll = 2;
		inline constexpr int32 GoalDistance = 3;
		inline constexpr int32 Shake = 11;
	}

	/** A scripted camera as it starts: at the start point, by its angles, no shake, the screen's centre 120 lines down. */
	void InitScriptCamera(FWorldMapContext& Context, FWorldMapTask& Task, int32 Distance, int16 Pitch, int16 Yaw);

	/** A cut: the camera's distance and angles set at once, where they are and where they head. */
	void CutScriptCamera(FWorldMapContext& Context, FWorldMapTask& Task, int32 Distance, int16 Pitch, int16 Yaw);

	/** Value moved by Step, or to Goal once nearer to it than a step (the step's sign is the caller's). */
	int32 StepTowards(int32 Value, int32 Goal, int32 Step);

	/**
	 * What a scripted camera's update ends with: its angles and distance eased an eighth of the way to their goals,
	 * its target to where the scene's place of interest stands, then the orbit's eye and target shaken up or down
	 * by a draw within the shake's reach.
	 */
	void FollowScriptCamera(FWorldMapContext& Context, FWorldMapTask& Task);
}
