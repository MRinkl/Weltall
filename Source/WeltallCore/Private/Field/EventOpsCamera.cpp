#include "EventOps.h"

#include "Field/FieldCamera.h"
#include "Graphics/Gte.h"
#include "Graphics/GteVectors.h"

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint32 UnusedCameraFlag = 0x2000;
		/** What 9A keeps of the camera's flags: all of the lower half but the lock on the player's turns. */
		constexpr uint32 KeptWithButtonTurn = 0x7FFF;
		constexpr int16 FollowMode = 0;
		constexpr int16 ScriptMode = 1;
		constexpr int16 ReturnMode = 2;
		/** A change asked to happen in no ticks snaps the camera for two. */
		constexpr int32 SnapTicks = 2;

		/**
		 * Starts moving a camera value to Target over Ticks: the value in 16.16 and a step that brings
		 * it there; the step is the kernel's 32-bit product divided toward zero.
		 */
		void StartRamp(FFieldCameraRamp& Ramp, int32 Current, int32 Target, int32 Ticks)
		{
			Ramp.Value = static_cast<int32>(static_cast<uint32>(Current) << 16);
			Ramp.Step = static_cast<int32>(static_cast<uint32>(Current - Target) * 0xFFFF0000u) / Ticks;
		}

		/** A0: turns the camera to one of the eight directions at once, with its pitch and projection distance. */
		void CameraSet(FEventContext& Context)
		{
			FFieldCamera& Camera = Context.Camera;
			Camera.Pitch = static_cast<int16>(ReadValue(Context, 3));
			const uint32 Step = static_cast<uint32>(ReadValue(Context, 1) + 4) & 7;
			Camera.YawGoal = static_cast<int32>(Step << 9);
			Camera.TurningYaw = static_cast<int32>(Step << 25);
			Camera.Rotation[1] = static_cast<int16>(Step << 9);
			Camera.ProjectionDistance = ReadValue(Context, 5);
			// The GTE takes the new distance at once: the scripts' projections use it from here on.
			Context.ScriptView.ProjectionDistance = static_cast<uint16>(Camera.ProjectionDistance);
			Advance(Context, 7);
		}

		/** 9D: moves the eye's distance scale to a value over a number of ticks; none snaps the camera. */
		void CameraZoom(FEventContext& Context)
		{
			FFieldCamera& Camera = Context.Camera;
			int32 Ticks = CodeByte(Context, 3);
			if (Ticks == 0)
			{
				Ticks = 1;
				Camera.SnapTicks += SnapTicks;
			}
			Camera.ZoomRamp.Ticks = static_cast<int16>(Ticks);
			StartRamp(Camera.ZoomRamp, Camera.DistanceScale, ReadValue(Context, 1), Ticks);
			Camera.Flags |= FieldCameraFlags::ZoomRamp;
			Advance(Context, 4);
		}

		/** A4: moves the pitch to a value over a number of ticks (the flags byte's low bits); none snaps the camera. */
		void CameraPitch(FEventContext& Context)
		{
			FFieldCamera& Camera = Context.Camera;
			const uint8 Flags = CodeByte(Context, 3);
			const int32 Pitch = ReadOperand(Context, 1, Flags, 0x80);
			int32 Ticks = Flags & 0x7F;
			if (Ticks == 0)
			{
				Camera.SnapTicks = SnapTicks;
				Ticks = 1;
			}
			Camera.PitchRamp.Ticks = static_cast<int16>(Ticks);
			StartRamp(Camera.PitchRamp, Camera.Pitch, Pitch, Ticks);
			Camera.Flags |= FieldCameraFlags::PitchRamp;
			Advance(Context, 4);
		}

		/** B6: moves the projection distance to a value over a number of ticks, or sets it and snaps the camera. */
		void CameraScreenDistance(FEventContext& Context)
		{
			FFieldCamera& Camera = Context.Camera;
			const int32 Distance = ReadValue(Context, 1);
			const int32 Ticks = ReadValue(Context, 3);
			if (Ticks == 0)
			{
				Camera.ProjectionRamp.Ticks = 0;
				Camera.SnapTicks += SnapTicks;
				Camera.ProjectionDistance = Distance;
			}
			else
			{
				StartRamp(Camera.ProjectionRamp, Camera.ProjectionDistance, Distance, Ticks);
				Camera.ProjectionRamp.Ticks = static_cast<int16>(Ticks);
				Camera.Flags |= FieldCameraFlags::ProjectionRamp;
			}
			Camera.Flags &= ~UnusedCameraFlag;
			Advance(Context, 5);
		}

		/** E6: the rectangle the camera's follow point is kept in: x, z, width and depth (stored negated). */
		void SetCameraBounds(FEventContext& Context)
		{
			std::array<int16, 4>& Bounds = Context.Camera.Bounds;
			for (int32 Side = 0; Side < 4; ++Side)
			{
				Bounds[static_cast<std::size_t>(Side)] = static_cast<int16>(ReadS16(Context, 1 + 2 * Side));
			}
			Bounds[3] = static_cast<int16>(-Bounds[3]);
			Advance(Context, 9);
		}

		/**
		 * 99: hands the camera to the scripts' paths: the player cannot turn it, it eases in by 1/12
		 * per tick, and the scripted camera starts from where the camera is.
		 */
		void CameraScriptMode(FEventContext& Context)
		{
			FFieldCamera& Camera = Context.Camera;
			Camera.Mode = ScriptMode;
			Context.Run.Budget += 4;
			Advance(Context, 1);
			Camera.ScriptScale = 0x1000;
			Camera.TargetDivisor = 12;
			Camera.EyeDivisor = 12;
			Camera.ScriptYaw = Camera.Rotation[1];
			Camera.Flags |= FieldCameraFlags::NoButtonTurn;
			Camera.ScriptPitch = Camera.Pitch;
			Camera.ScriptDistance = Camera.ProjectionDistance * Camera.DistanceScale >> 12;
		}

		/**
		 * 9A: hands the camera back to the follow: from the scripts' paths, easing back by 1/divisor of
		 * the way per tick, or at once with a divisor of 0 (the player may turn it again, and it snaps
		 * for two ticks); while it eases back the instruction waits, not ending the actor's turn. The
		 * original moves past three bytes more after the one at once.
		 */
		void CameraFollowMode(FEventContext& Context)
		{
			FFieldCamera& Camera = Context.Camera;
			if (Camera.Mode == ScriptMode)
			{
				const int32 Divisor = ReadValue(Context, 1);
				if (Divisor == 0)
				{
					Camera.Mode = FollowMode;
					Camera.Flags &= KeptWithButtonTurn;
					Advance(Context, 3);
					Camera.SnapTicks = SnapTicks;
				}
				else
				{
					Camera.Mode = ReturnMode;
					Camera.TargetDivisor = Divisor;
					Camera.EyeDivisor = Divisor;
				}
			}
			else if (Camera.Mode == FollowMode)
			{
				Camera.Flags &= KeptWithButtonTurn;
			}
			else
			{
				return;
			}
			Advance(Context, 3);
		}

		/** 9B: how fast the target and the eye ease toward their goals: 1/divisor of the way per tick, 0 as 1. */
		void CameraSetSmoothing(FEventContext& Context)
		{
			FFieldCamera& Camera = Context.Camera;
			Camera.TargetDivisor = ReadValue(Context, 1);
			Camera.EyeDivisor = ReadValue(Context, 3);
			if (Camera.TargetDivisor == 0)
			{
				Camera.TargetDivisor = 1;
			}
			if (Camera.EyeDivisor == 0)
			{
				Camera.EyeDivisor = 1;
			}
			Context.Run.Budget += 1;
			Advance(Context, 5);
		}

		/** FE 25: whether the camera's target keeps following the actor's height inside the camera's area. */
		void SetCameraFollowY(FEventContext& Context)
		{
			Context.Camera.FollowHeight = CodeByte(Context, 1);
			Advance(Context, 2);
		}

		/** A1: the directions the camera may not stay in or be turned to; all eight forbid turning. */
		void SetCameraLockMask(FEventContext& Context)
		{
			Context.Camera.ForbiddenDirections = static_cast<uint8>(ReadValue(Context, 1));
			Advance(Context, 3);
		}

		/** A2: waits, ending the actor's turn, while any of the camera's changes in the mask is under way. */
		void WaitCameraFlags(FEventContext& Context)
		{
			if ((Context.Camera.Flags & CodeByte(Context, 1)) == 0)
			{
				Advance(Context, 2);
			}
			else
			{
				Context.Run.bYield = true;
			}
		}

		/**
		 * AD, AE: where the camera's target or eye is headed; FE A8, FE A9: where it is now. Whole units,
		 * into three variables: x, z, then y.
		 */
		template <FFieldPoint FFieldCamera::*Point>
		void GetCameraPoint(FEventContext& Context)
		{
			const FFieldPoint& Where = Context.Camera.*Point;
			SetVar(Context, ReadU16(Context, 1), static_cast<int16>(Where[0] >> 16));
			SetVar(Context, ReadU16(Context, 3), static_cast<int16>(Where[2] >> 16));
			SetVar(Context, ReadU16(Context, 5), static_cast<int16>(Where[1] >> 16));
			Context.Run.Budget += 1;
			Advance(Context, 7);
		}

		/**
		 * 8A, FE 02: whether an actor (an empty slot: the first party member) is on the screen, its
		 * origin through its world matrix and the view the scripts find: within the screen less its
		 * edge rows and columns (8A), or less a margin of 33 pixels, 32 at the right and bottom
		 * (FE 02). Not on screen jumps to the target. In the map's first ticks every actor counts as
		 * on screen and the actor's turn goes on; otherwise it ends.
		 */
		template <int32 Margin, uint32 Width, uint32 Height>
		void IfActorOnScreen(FEventContext& Context)
		{
			// The original projects the actor first, but only uses where it lands once the map has settled.
			if (Context.SettleTicks != 0)
			{
				Advance(Context, 4);
				return;
			}
			uint32 Actor = ReadActor(Context, 1);
			if (Actor == FEventContext::NoActor)
			{
				Actor = Context.PartyActors[0];
			}
			const FFieldActor* Record = RecordOf(Context, Actor);
			if (Record == nullptr)
			{
				return;
			}
			FGteMatrix World;
			World.M = Record->WorldMatrix;
			World.T = Record->Position;
			FGte Gte;
			Gte.Transform = CompMatrix(Context.ScriptView.View, World);
			Gte.OffsetX = FieldScreenCenterX << 16;
			Gte.OffsetY = FieldScreenCenterY << 16;
			Gte.ProjectionDistance = Context.ScriptView.ProjectionDistance;
			const FGteProjection Origin = Gte.Rtps({0, 0, 0});
			const bool bOnScreen = static_cast<uint32>(Origin.ScreenY - Margin) < Height && static_cast<uint32>(Origin.ScreenX - Margin) < Width;
			Branch(Context, bOnScreen, 4, 2);
			Context.Run.bYield = true;
		}

		/** FE AA: the actor the camera follows; an empty party slot's FF is kept, which stops the field when the camera next follows it. */
		void SetCameraFollowActor(FEventContext& Context)
		{
			Context.Camera.FollowedActor = static_cast<uint8>(ReadActor(Context, 1));
			Advance(Context, 2);
		}
	}

	void RegisterCamera(FBaseTable& Base, FExtendedTable& Extended)
	{
		Extended[0x25] = &SetCameraFollowY;
		Base[0x99] = &CameraScriptMode;
		Base[0x9A] = &CameraFollowMode;
		Base[0x9B] = &CameraSetSmoothing;
		Base[0x9D] = &CameraZoom;
		Base[0xA0] = &CameraSet;
		Base[0x8A] = &IfActorOnScreen<1, 0x13F, 0xDF>;
		Extended[0x02] = &IfActorOnScreen<0x21, 0xFF, 0x9F>;
		Base[0xA1] = &SetCameraLockMask;
		Base[0xA2] = &WaitCameraFlags;
		Base[0xA4] = &CameraPitch;
		Base[0xAD] = &GetCameraPoint<&FFieldCamera::TargetGoal>;
		Base[0xAE] = &GetCameraPoint<&FFieldCamera::EyeGoal>;
		Extended[0xA8] = &GetCameraPoint<&FFieldCamera::Target>;
		Extended[0xA9] = &GetCameraPoint<&FFieldCamera::Eye>;
		Extended[0xAA] = &SetCameraFollowActor;
		Base[0xB6] = &CameraScreenDistance;
		Base[0xE6] = &SetCameraBounds;
	}
}
