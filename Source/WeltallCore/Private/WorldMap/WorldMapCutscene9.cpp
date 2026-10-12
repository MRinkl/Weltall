#include "WorldMap/WorldMapCutscenes.h"

#include "Graphics/GteVectors.h"

namespace Weltall::WorldMapOps
{
	namespace Scene9
	{
		using R = EWorldMapRoutine;

		constexpr FWorldMapVector CraftStart{0x2000000, static_cast<int32>(0xFFE00000), 0x2000000, 0};
		constexpr uint16 FieldMap = 0x10E;
		constexpr uint16 FieldEntrance = 0;
		/** The craft's engine, the scene's effect A4h, and its trail, particle effect 8. */
		constexpr int32 EngineSound = 0xA4;
		constexpr int32 TrailEffect = 8;
		constexpr int16 FadeOutMessage = 0xD;
		constexpr int32 ScreenCentre = 0x78;
		constexpr int32 CameraDistance = 0x200000;

		/** The camera task's steps (+20h) and words: its place on the path (+50h, 20.12: the segment and the place in it), the segment (+54h), the speed a tick (+58h). */
		namespace CameraStep
		{
			constexpr int16 Starting = 0;
			constexpr int16 Cruising = 1;
			constexpr int16 Slowing = 2;
			constexpr int16 Gliding = 3;
			constexpr int16 Settling = 4;
			constexpr int16 Hovering = 5;
			constexpr int16 Leaving = 6;
		}
		constexpr int32 PathPlace = 0;
		constexpr int32 PathSegment = 1;
		constexpr int32 PathSpeed = 2;

		/** The craft's spinning parts: three angles and their turns a tick (+50h to +64h). */
		constexpr int32 SpinA = 0;
		constexpr int32 SpinStepA = 1;
		constexpr int32 SpinB = 2;
		constexpr int32 SpinStepB = 3;
		constexpr int32 SpinC = 4;
		constexpr int32 SpinStepC = 5;
		/** The craft's parts: turning about y by each angle, and about z by the third. */
		constexpr int32 PartsA[] = {12, 8, 10, 6};
		constexpr int32 PartsB[] = {13, 9, 11, 7};
		constexpr int32 PartsC[] = {5, 4};
		constexpr int32 PartsRolling[] = {3, 2, 1};
		constexpr int32 PartCount = 13;
		/** 4 units north a tick. */
		constexpr int32 CraftStep = -0x4000;

		FWorldMapTask& TaskAt(FWorldMapContext& Context, int32 Slot)
		{
			return Context.Tasks[static_cast<std::size_t>(Slot)];
		}

		FResult InitCamera(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Context.ScreenCentreY = ScreenCentre;
			Context.CameraDistance = CameraDistance;
			Task.Step = CameraStep::Starting;
			Task.Word(PathSpeed) = 0;
			Task.Word(PathSegment) = 0;
			Task.Word(PathPlace) = 0;
			Context.CameraRoll = 0;
			Context.CameraYaw = 0;
			Context.CameraPitch = 0;
			Context.LookAt = 1;
			PlayCutsceneEffect(Context, EngineSound);
			Task.Timer = 0x18;
			return WorldMapTaskState::Update;
		}

		/** The eye on the path about the craft: x and z from the craft (z the drawing's), its height its own. */
		void FollowPath(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			const FWorldMapPath& Path = Context.Tables->Cutscenes.Scene9Camera;
			const int32 Segment = Task.Word(PathPlace) >> 12;
			Task.Word(PathSegment) = Segment;
			if (Segment < 0 || static_cast<std::size_t>(Segment) + 2 >= Path.size())
			{
				Context.Fault = MakeError("scene 9's camera reads past its path, at segment " + std::to_string(Segment));
				return;
			}
			const std::size_t At = static_cast<std::size_t>(Segment);
			if (Path[At + 2][3] == -1)
			{
				return;
			}
			const std::array<int32, 3> Point = QuadraticBlend(Task.Word(PathPlace) & 0xFFF, Path[At], Path[At + 1], Path[At + 2]);
			FWorldMapOrbit& Orbit = Context.Orbit;
			Orbit.Target = {0, static_cast<int16>(Context.CameraTarget.Y >> 12), 0};
			Orbit.Up = {0, -0x1000, 0};
			Orbit.Eye = {static_cast<int16>(Point[0] >> 16), static_cast<int16>(Point[1] >> 16), static_cast<int16>(-static_cast<int16>(Point[2] >> 16))};
		}

		/** Where the craft is heard from: louder as the eye nears it. */
		void SetEngineVolume(FWorldMapContext& Context)
		{
			const FWorldMapOrbit& Orbit = Context.Orbit;
			const int32 X = std::abs(Orbit.Target[0] - Orbit.Eye[0]);
			const int32 Z = std::abs(Orbit.Target[2] - Orbit.Eye[2]);
			const int32 Distance = SquareRoot0(X * X + Z * Z);
			SetCutsceneEffectVolume(Context, EngineSound, 0x87 - static_cast<int16>(Distance >> 3));
		}

		FResult UpdateCamera(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			FollowPath(Context, Task);
			if (Context.Fault)
			{
				return Task.State;
			}
			LookAtCamera(Context);
			FGteVector Angles{Context.CameraPitch, Context.CameraYaw, Context.CameraRoll};
			AnglesFromMatrix(Context.CameraMatrix, *Context.Arctangents, Angles);
			Context.CameraPitch = Angles[0];
			Context.CameraYaw = Angles[1];
			Context.CameraRoll = Angles[2];
			int32& Place = Task.Word(PathPlace);
			int32& Speed = Task.Word(PathSpeed);
			switch (Task.Step)
			{
			case CameraStep::Starting:
				Task.Timer = static_cast<int16>(Task.Timer - 1);
				Speed += 5;
				if (Task.Timer < 0)
				{
					Speed = 0x80;
					Task.Step = CameraStep::Cruising;
					const std::array<int16, 4> Behind{static_cast<int16>((Context.CameraTarget.X >> 12) + 0x32), static_cast<int16>(-0x20D),
						static_cast<int16>((Context.CameraTarget.Z >> 12) - 0x70), 0};
					Context.Particles.Start(TrailEffect, Behind, std::nullopt);
				}
				break;
			case CameraStep::Cruising:
				if (Place >= 0x6000)
				{
					Task.Step = CameraStep::Slowing;
				}
				break;
			case CameraStep::Slowing:
				Speed -= 2;
				if (Speed <= 0x18)
				{
					Speed = 0x18;
					Task.Step = CameraStep::Gliding;
				}
				break;
			case CameraStep::Gliding:
				if (Place >= 0x8000)
				{
					Task.Step = CameraStep::Settling;
				}
				break;
			case CameraStep::Settling:
				Speed -= 1;
				if (Speed <= 4)
				{
					Speed = 4;
					Task.Step = CameraStep::Hovering;
				}
				break;
			case CameraStep::Hovering:
				if (Place > 0x83FF)
				{
					FadeCutsceneEffect(Context, EngineSound, 0, 0x100);
					SendMessage(Context, FWorldMapContext::FadeSlot, FadeOutMessage);
					Context.FadeBlend = 2;
					Context.FadeStep = 4;
					Task.Timer = 0x50;
					Speed = 0;
					Task.Step = CameraStep::Leaving;
				}
				break;
			case CameraStep::Leaving:
				Task.Timer = static_cast<int16>(Task.Timer - 1);
				if (Task.Timer < 1)
				{
					Context.Running = 0;
					Context.ExitCode = 0;
				}
				break;
			default:
				break;
			}
			Place += Speed;
			if (Task.Step < CameraStep::Leaving)
			{
				SetEngineVolume(Context);
			}
			return WorldMapTaskState::Update;
		}

		FResult InitCraft(FWorldMapContext& Context, int32 Slot)
		{
			for (int32 Part = 1; Part <= PartCount; ++Part)
			{
				AttachObject(Context, 0, Part);
			}
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Step = 0;
			Task.Position = Context.Start;
			Task.Word(SpinStepA) = 0x40;
			Task.Word(SpinB) = 0x200;
			Task.Word(SpinStepB) = 0x60;
			Task.Word(SpinC) = 0x300;
			Task.Word(SpinA) = 0;
			Task.Word(SpinStepC) = 0x50;
			Context.CameraTarget = Task.Position;
			return WorldMapTaskState::Update;
		}

		void Turn(FWorldMapContext& Context, std::span<const int32> Parts, const FGteVector& Angles)
		{
			const FGteMatrix Rotation = RotationYxz(Angles);
			for (const int32 Part : Parts)
			{
				Context.Objects[static_cast<std::size_t>(Part)].Matrix.M = Rotation.M;
			}
		}

		FResult UpdateCraft(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Position.Z += CraftStep;
			Context.Ground.Wrap(Task.Position);
			FWorldMapObject& Hull = Context.Objects[0];
			Hull.X = Task.Position.X >> 12;
			Hull.Y = Task.Position.Y >> 12;
			Hull.Z = Task.Position.Z >> 12;
			Context.CameraTarget = Task.Position;
			Task.Word(SpinA) = (Task.Word(SpinA) + Task.Word(SpinStepA)) & 0xFFF;
			Task.Word(SpinB) = (Task.Word(SpinB) + Task.Word(SpinStepB)) & 0xFFF;
			Task.Word(SpinC) = (Task.Word(SpinC) + Task.Word(SpinStepC)) & 0xFFF;
			const int16 C = static_cast<int16>(Task.Word(SpinC));
			Turn(Context, PartsRolling, {0, 0, C});
			Turn(Context, PartsC, {0, C, 0});
			Turn(Context, PartsA, {0, static_cast<int16>(Task.Word(SpinA)), 0});
			Turn(Context, PartsB, {0, static_cast<int16>(Task.Word(SpinB)), 0});
			return WorldMapTaskState::Update;
		}

		constexpr FCutsceneRoutine Routines[] = {
			{R::InitScene9Camera, InitCamera},
			{R::UpdateScene9Camera, UpdateCamera},
			{R::InitScene9Craft, InitCraft},
			{R::UpdateScene9Craft, UpdateCraft},
		};

		void Start(FWorldMapContext& Context)
		{
			StartCutsceneWorld(Context, CraftStart);
			RegisterCutsceneEffects(Context);
			AddTask(Context, R::InitFade, R::UpdateFade);
			AddTask(Context, R::InitScene9Camera, R::UpdateScene9Camera);
			AddTask(Context, R::InitScene9Craft, R::UpdateScene9Craft);
			AddTask(Context, R::InitCutsceneFrame, R::UpdateCutsceneFrame);
		}

		void End(FWorldMapContext& Context)
		{
			StopCutsceneEffects(Context);
			EndCutscene(Context, FieldMap, FieldEntrance);
		}
	}

	/** A craft flying north, the camera circling it on a path: the screen fades in from white. */
	const FCutscene Scene9Cutscene{{9, 9}, 1, 4, Scene9::Start, Scene9::End, Scene9::Routines};
}
