#include "WorldMap/WorldMapCutscenes.h"

#include "WorldMap/WorldMapInput.h"

namespace Weltall::WorldMapOps
{
	namespace TelescopeScene
	{
		using R = EWorldMapRoutine;

		/** Where the telescope stands (20.12): 768 units above the sea. */
		constexpr FWorldMapVector TelescopeStart{0x7702000, static_cast<int32>(0xFFD00000), 0x27C0000, 0};
		/** The particle effect the scene starts, era 0's. */
		constexpr int32 AmbientEffect = 0xE;
		constexpr uint16 FieldMap = 0x11;
		constexpr uint16 FieldEntrance = 7;

		constexpr int16 StartPitch = -0x80;
		constexpr int16 StartYaw = 0x200;
		constexpr int32 Distance = 0x400000;
		constexpr int32 ScreenCentre = 0x78;
		/** The pitch's goal moves 8 angle units a tick within -384 and +24; the yaw's 16, unbounded. */
		constexpr int32 PitchStep = 0x8000;
		constexpr int32 LowestPitch = -0x180000;
		constexpr int32 HighestPitch = 0x18000;
		constexpr int32 YawStep = 0x10000;
		constexpr uint16 TiltUp = WorldMapButton::Up;
		constexpr uint16 TiltDown = WorldMapButton::Down;
		constexpr uint16 TurnLeft = WorldMapButton::Left | WorldMapButton::L1;
		constexpr uint16 TurnRight = WorldMapButton::Right | WorldMapButton::R1;

		/*
		 * The task's words: the pitch and yaw it heads for (+28h, +2Ch: Position.X, Y) and the eased pitch and yaw (+38h,
		 * +3Ch: Direction.X, Y), all 20.12.
		 */

		int32 Wrapping(int64 Value)
		{
			return static_cast<int32>(static_cast<uint32>(Value));
		}

		/**
		 * The orbit about the start point at the camera's angles, its eye and target then swapped: the camera stands at
		 * the start point (the orbit's target) and looks out.
		 */
		void StandAtStart(FWorldMapContext& Context)
		{
			SetCameraOrbit(Context, Context.CameraDistance, {Context.CameraPitch, Context.CameraYaw, Context.CameraRoll});
			std::swap(Context.Orbit.Eye, Context.Orbit.Target);
		}

		FResult InitTelescopeCamera(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = Context.Tasks[static_cast<std::size_t>(Slot)];
			Context.CameraPitch = StartPitch;
			Context.CameraYaw = StartYaw;
			Context.CameraDistance = Distance;
			Context.CameraRoll = 0;
			Task.Position.X = StartPitch * 0x1000;
			Context.ScreenCentreY = ScreenCentre;
			Task.Position.Y = Context.CameraYaw * 0x1000;
			Task.Direction = Task.Position;
			Context.LookAt = 0;
			Context.CameraTarget.X = Context.Start.X;
			Context.CameraTarget.Y = Context.Start.Y;
			Context.CameraTarget.Z = Context.Start.Z;
			StandAtStart(Context);
			return WorldMapTaskState::Update;
		}

		FResult UpdateTelescopeCamera(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = Context.Tasks[static_cast<std::size_t>(Slot)];
			if ((Context.Pad.Pressed & WorldMapButton::Cancel) != 0)
			{
				Context.Running = 0;
				Context.ExitCode = 0;
			}
			const uint16 Held = Context.Pad.Held;
			int32& GoalPitch = Task.Position.X;
			int32& GoalYaw = Task.Position.Y;
			int32& Pitch = Task.Direction.X;
			int32& Yaw = Task.Direction.Y;
			if ((Held & TiltUp) != 0)
			{
				GoalPitch += PitchStep;
			}
			if ((Held & TiltDown) != 0)
			{
				GoalPitch -= PitchStep;
			}
			if (GoalPitch < LowestPitch)
			{
				GoalPitch = LowestPitch;
			}
			else if (GoalPitch > HighestPitch)
			{
				GoalPitch = HighestPitch;
			}
			if ((Held & TurnLeft) != 0)
			{
				GoalYaw = Wrapping(int64{GoalYaw} - YawStep);
			}
			if ((Held & TurnRight) != 0)
			{
				GoalYaw = Wrapping(int64{GoalYaw} + YawStep);
			}
			if (Pitch != GoalPitch || Yaw != GoalYaw)
			{
				// Each eases by an eighth of what is left; the shifts round down, so a positive remainder under 8 never closes.
				const int32 PitchLeft = Wrapping(int64{GoalPitch} - Pitch);
				const int32 YawLeft = Wrapping(int64{GoalYaw} - Yaw);
				Pitch = Wrapping(int64{Pitch} + (PitchLeft >> 3));
				Yaw = Wrapping(int64{Yaw} + (YawLeft >> 3));
			}
			Context.CameraPitch = static_cast<int16>(Pitch >> 12);
			Context.CameraYaw = static_cast<int16>(Yaw >> 12);
			StandAtStart(Context);
			// The camera looks away from the orbit's point: outward from where it stands.
			Context.CameraYaw = static_cast<int16>((Context.CameraYaw + 0x800) & 0xFFF);
			return WorldMapTaskState::Update;
		}

		constexpr FCutsceneRoutine Routines[] = {
			{R::InitTelescopeCamera, InitTelescopeCamera},
			{R::UpdateTelescopeCamera, UpdateTelescopeCamera},
		};

		void StartTelescope(FWorldMapContext& Context)
		{
			StartCutsceneWorld(Context, TelescopeStart);
			AddTask(Context, R::InitFade, R::UpdateFade);
			AddTask(Context, R::InitTelescopeCamera, R::UpdateTelescopeCamera);
			AddTask(Context, R::InitSpinner, R::UpdateSpinner);
			AddTask(Context, R::InitFrame, R::UpdateFrame);
			ResetEncounters(Context);
			Context.Particles.Start(AmbientEffect, std::nullopt, std::nullopt);
		}

		void EndTelescope(FWorldMapContext& Context)
		{
			EndCutscene(Context, FieldMap, FieldEntrance);
		}
	}

	/** Scenes 8 and 11 have one row of the scene table and the same functions: they are one scene. */
	const FCutscene TelescopeCutscene{{8, 11}, 2, 4, TelescopeScene::StartTelescope, TelescopeScene::EndTelescope, TelescopeScene::Routines};
}
