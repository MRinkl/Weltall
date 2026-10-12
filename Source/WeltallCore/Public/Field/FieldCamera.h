#pragma once

#include "Foundation/Types.h"
#include "Graphics/AffineBlend.h"
#include "Graphics/Gte.h"
#include "Graphics/GteVectors.h"

#include <array>

namespace Weltall
{
	/** A point as the field camera keeps it: 16.16 world units, y growing downward. */
	using FFieldPoint = std::array<int32, 3>;

	/** The screen's center, where the field's projection puts the view axis (320 x 224 pixels). */
	inline constexpr int32 FieldScreenCenterX = 160;
	inline constexpr int32 FieldScreenCenterY = 112;

	/** Bits of FFieldCamera::Flags. */
	namespace FieldCameraFlags
	{
		constexpr uint32 ZoomRamp = 0x1;
		constexpr uint32 PitchRamp = 0x8;
		constexpr uint32 ProjectionRamp = 0x10;
		/** The eye may go below the camera layer's plane. */
		constexpr uint32 EyeMayGoBelowCameraLayer = 0x4000;
		/** L1 and R1 do not turn the camera. */
		constexpr uint32 NoButtonTurn = 0x8000;
	}

	/** A value a script moves by a step per tick for a count of ticks: 16.16. */
	struct FFieldCameraRamp
	{
		int16 Ticks = 0;
		int32 Value = 0;
		int32 Step = 0;
	};

	/** One of the camera's scripted moves: ticks left, where the point is and its step per tick (16.16). */
	struct FFieldCameraMove
	{
		int16 Ticks = 0;
		FFieldPoint At{};
		FFieldPoint Step{};
	};

	/** Bits of FFieldCamera::PathMoves: the scripted moves under way. */
	namespace FieldCameraPathMoves
	{
		constexpr uint16 Target = 0x1;
		constexpr uint16 Eye = 0x2;
	}

	/**
	 * The scale the field's view carries (4.12): three view units to a world unit. What is sized in view units (a
	 * sprite's parts) is a third as large in the world.
	 */
	inline constexpr int32 FieldWorldScale = 0x3000;

	/** What the field's drawing takes from the camera each frame, as the GTE's matrices. */
	struct FFieldView
	{
		/** World to view: the look-at, turned by the screen's rotation, times the world scale 3. */
		FGteMatrix View;
		/** View with the world's rotation: what the scenery's placements are composed with. */
		FGteMatrix Scene;
		/** The screen's rotation alone (for models kept facing the screen). */
		FGteMatrix ScreenRotation;
		/** The camera's pitch alone, times the world scale: what models kept facing the screen are turned by. */
		FGteMatrix Billboard;
		/** The look-at turned by the screen's rotation, and the camera's pitch alone, before the world scale: the compass's. */
		FGteMatrix Rotation;
		FGteMatrix Pitch;
		uint16 ProjectionDistance = 0;
	};

	/**
	 * The field camera's state: where it is, and what scripts set about it, at the values a map
	 * starts with. The camera's tick (UpdateFieldCamera) runs the follow, the scripted moves, the
	 * easing and the shake on it; FollowTicks, ReturnTicks and ShakeTicks are the ones a map's load
	 * leaves as they were.
	 */
	struct FFieldCamera
	{
		FFieldPoint Eye{};
		FFieldPoint Target{};
		/** Where the follow puts the eye and the target: they ease toward these. */
		FFieldPoint EyeGoal{};
		FFieldPoint TargetGoal{};
		/** The scripted path: where the target and the eye start and end. */
		FFieldPoint TargetFrom{};
		FFieldPoint TargetTo{};
		FFieldPoint EyeFrom{};
		FFieldPoint EyeTo{};
		/** FieldCameraPathMoves, and the moves along the path in script mode. */
		uint16 PathMoves = 0;
		FFieldCameraMove TargetMove;
		FFieldCameraMove EyeMove;
		/** Degrees the eye sits above the target. */
		int16 Pitch = 30;
		/** H, the projection distance: the eye sits 2 * H * DistanceScale world units from the target. */
		int32 ProjectionDistance = 512;
		/** 4.12. */
		int16 DistanceScale = 0x1000;
		/** The turn of the eye about the target (x, yaw, z; 4096 is a turn): the yaw goes in steps of 0x200. */
		FGteVector Rotation = {0, 0x800, 0};
		/** The yaw as it turns, 16.16: Rotation's yaw is its whole part. */
		int32 TurningYaw = 0x8000000;
		/** The yaw the camera turns to. */
		int32 YawGoal = 0x800;
		/** How far TurningYaw moves each tick of a turn (16.16), and the ticks the turn has left. */
		int32 TurnStep = 0x400000;
		int16 TurnTicks = 0;
		/**
		 * Whether L1 or R1 started the turn under way, which nothing of the original's tells from a
		 * script's: the player's camera keeps its offsets through the player's turns only.
		 */
		bool bTurnByButtons = false;
		/** The actor the camera follows. */
		uint8 FollowedActor = 0;
		/** The rotation of the screen a script can set (a tilt of the whole view). */
		FGteVector ScreenAngles{};
		/** The rotation of the world a script can set, applied to the scenery only. */
		FGteVector WorldAngles{};
		/**
		 * WorldAngles' matrix as the map's load and each tick's camera step last built it: the actors'
		 * update before that step (the platforms' test) still places the scenery by the tick before's.
		 */
		FGteMatrix WorldRotation = RotationXyz({});
		/** FieldCameraFlags. */
		uint32 Flags = 0;
		/** Scripted changes under way: the distance scale's, the pitch's and the projection distance's. */
		FFieldCameraRamp ZoomRamp;
		FFieldCameraRamp PitchRamp;
		FFieldCameraRamp ProjectionRamp;
		/** Ticks the camera snaps to its goals instead of easing toward them (the follow divisors are 1); a map starts with two. */
		int32 SnapTicks = 2;
		/** Directions the camera may not stay in or turn to, and those it leaves by itself: bit {0x10, 0x20, 0x40, 0x80, 1, 2, 4, 8}[d] for direction d. */
		uint8 ForbiddenDirections = 0;
		uint8 LeftDirections = 0;
		/** 0 follows the followed actor, 1 moves along the scripts' paths, 2 eases back to following; any other only eases. */
		int16 Mode = 0;
		/** The target and the eye move toward their goals by 1/divisor of the way each tick. */
		int32 TargetDivisor = 8;
		int32 EyeDivisor = 8;
		/** The scripted camera's yaw, pitch and distance, and the scale of its orbits (4.12). */
		int16 ScriptYaw = 0;
		int16 ScriptPitch = 0;
		int32 ScriptDistance = 0;
		int16 ScriptScale = 0x1000;
		/**
		 * The rectangle whose nearest point to the followed actor the way into the camera's area
		 * starts from: x, z, width and minus the depth; 1 each at a map's start.
		 */
		std::array<int16, 4> Bounds{1, 1, 1, 1};
		/**
		 * Outside its area, non-zero: the target keeps following the followed actor's height; 0: it
		 * keeps the height of the actor's floor where it left. Inside, it always follows the actor.
		 */
		uint8 FollowHeight = 0;
		/** The target keeps the floor's height it took when the followed actor left the area. */
		bool bHeightKept = false;
		/** Ticks spent following since the camera last left the other modes: the divisors ease back to 8 every fourth. */
		int32 FollowTicks = 0;
		/** Ticks spent easing back to following. */
		int32 ReturnTicks = 0;
		/**
		 * The shake: while on, its amplitudes (16.16) grow by their growth for its ticks, and each
		 * tick puts the eye and the target a random 0 to 32767 times the amplitude's whole part off
		 * along each axis; when its ticks have run out a shake asked to stop ends.
		 */
		bool bShaking = false;
		int16 ShakeTicks = 0;
		bool bShakeStops = false;
		FFieldPoint ShakeAmplitude{};
		FFieldPoint ShakeGrowth{};
		/** This tick's shake, added to the eye and the target the view is built from. */
		FFieldPoint Shake{};
		/**
		 * The view's direction across the ground less a quarter turn (4096 a turn), taken each tick
		 * before the camera moves: from the eye to the target, which the sprites face by, and from the
		 * eye's goal to the target's, which the controls and the scripts' camera-relative directions
		 * are measured from.
		 */
		int16 ViewYaw = 0;
		int16 GoalViewYaw = 0;
		/** The view's angle from straight down (4096 a turn), taken with the view yaws: effects turn by it. */
		int32 ViewPitch = 0;
		/** The look-at of this tick's shaken eye and target, which the next tick's frame is drawn with. */
		FGteMatrix LookAt = FGteMatrix::Identity();
		/** The matrices this tick's frame is drawn with. */
		FFieldView View;

		/** The yaw step (0 to 7) the camera's rotation sets: 45 degrees each. */
		static constexpr int16 YawStep = 0x200;
	};

	/** The follow camera's target for an actor standing at Position inside the camera's area: 32 units above it. */
	WELTALLCORE_API FFieldPoint FollowTarget(const FFieldPoint& Position);

	/**
	 * Where the follow camera puts its eye for a target: 2 * H * DistanceScale away, Pitch degrees
	 * above it, turned about it by the camera's rotation; in the original's integer arithmetic.
	 */
	WELTALLCORE_API FFieldPoint FollowEye(const FFieldCamera& Camera, const FFieldPoint& Target);

	/**
	 * The camera's look-at: rows right, down and forward from Eye to Target (up is -y), and the
	 * translation -R * (3 * Eye), from the whole parts of the points.
	 */
	WELTALLCORE_API FGteMatrix FieldLookAt(const FFieldPoint& Eye, const FFieldPoint& Target);

	/** The field's length of a horizontal vector: the square root of the sum of squares, from the library's table. */
	WELTALLCORE_API int32 FieldLength2D(int32 X, int32 Z);

	/**
	 * The matrices the field draws a frame with for a look-at: the screen's and the world's rotations
	 * the camera holds, and the pitch-only matrix of its eye and target as they are.
	 */
	WELTALLCORE_API FFieldView BuildFieldView(const FFieldCamera& Camera, const FGteMatrix& LookAt);

	/** The matrices for a camera placed outright: the look-at of its eye and target as they are. */
	WELTALLCORE_API FFieldView BuildFieldView(const FFieldCamera& Camera);

	/**
	 * BuildFieldView's View of the look-at of Eye and Target without the whole steps FieldLookAt takes: the points as
	 * they are (16.16), the rows exact, turned by the camera's screen rotation and scaled by the world's.
	 */
	WELTALLCORE_API FAffine UnroundedFieldView(const FFieldCamera& Camera, const FFieldPoint& Eye, const FFieldPoint& Target);
}
