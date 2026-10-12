#include "WorldMap/WorldMapCutscenes.h"

#include "Game/PsxRandom.h"
#include "Graphics/GteMatrix.h"

namespace Weltall::WorldMapOps
{
	namespace Scene10
	{
		using R = EWorldMapRoutine;

		constexpr FWorldMapVector CraftStart{0x2000000, static_cast<int32>(0xFFD00000), 0x2000000, 0};
		constexpr uint16 FieldMap = 0x110;
		constexpr uint16 FieldEntrance = 0;
		constexpr int16 FadeInMessage = 0xC;
		constexpr int16 FadeOutMessage = 0xD;
		constexpr int16 WakeMessage = 1;
		/** The tasks' slots, as the start adds them. */
		constexpr int32 DirectorSlot = 1;
		constexpr int32 CraftSlot = 2;
		constexpr int32 RingsSlot = 3;
		constexpr int32 TrailSlot = 4;
		constexpr int32 BurstSlot = 5;
		constexpr int32 SeaObjectSlot = 6;
		/** The models: the craft's hull and its 13 turning parts, the two rings, the object in the sea. */
		constexpr int32 Hull = 0;
		constexpr int32 PartCount = 13;
		constexpr int32 InnerRing = 14;
		constexpr int32 OuterRing = 15;
		constexpr int32 SeaObject = 16;
		constexpr int32 ModelCount = 17;
		/** The scene's sound effects. */
		constexpr int32 EngineSound = 0x36;
		constexpr int32 SplashSound = 0x71;
		constexpr int32 BurstSounds[] = {0x62, 0x63};
		constexpr int32 RumbleSound = 0x79;
		/** The particle effects: the craft's wake, its spray, the trail, the burst. */
		constexpr int32 WakeEffect = 0xB;
		constexpr int32 SprayEffect = 0xC;
		constexpr int32 TrailEffect = 9;
		constexpr int32 BurstEffect = 0xA;

		FWorldMapTask& TaskAt(FWorldMapContext& Context, int32 Slot)
		{
			return Context.Tasks[static_cast<std::size_t>(Slot)];
		}

		FWorldMapObject& ObjectAt(FWorldMapContext& Context, int32 Number)
		{
			return Context.Objects[static_cast<std::size_t>(Number)];
		}

		/** The director's steps (+20h); its counter is the task's timer. */
		namespace DirectorStep
		{
			constexpr int16 Burst = 0;
			constexpr int16 Rings = 1;
			constexpr int16 Trail = 2;
			constexpr int16 Wait = 3;
			constexpr int16 StrongShake = 4;
			constexpr int16 MildShake = 5;
			constexpr int16 Settle = 6;
			constexpr int16 Leave = 7;
			constexpr int16 Follow = 0x10;
			constexpr int16 Cut = 0x11;
			constexpr int16 FadeIn = 0x12;
		}

		/** The counter run down a tick; whether it has run out. */
		bool CountDown(FWorldMapTask& Task)
		{
			Task.Timer = static_cast<int16>(Task.Timer - 1);
			return Task.Timer < 1;
		}

		FResult InitDirector(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Context.ScreenCentreY = 0x78;
			Context.CameraDistance = 0x200000;
			Context.LookAt = 0;
			Task.Step = DirectorStep::Follow;
			Task.Word(2) = 0;
			Task.Word(1) = 0;
			Task.Word(0) = 0;
			Context.CameraPitch = -0xC0;
			Context.CameraYaw = 0x680;
			Context.CameraRoll = 0;
			Task.Timer = 0x40;
			Task.Word(9) = 0;
			return WorldMapTaskState::Update;
		}

		/** The camera shaken: the orbit set again, then its eye and target moved by two draws within Spread about 0. */
		void Shake(FWorldMapContext& Context, int32 Distance, int32 Spread)
		{
			SetCameraOrbit(Context, Distance, {Context.CameraPitch, Context.CameraYaw, Context.CameraRoll});
			const int16 X = static_cast<int16>(Context.Random->Next() % Spread - Spread / 2);
			const int16 Y = static_cast<int16>(Context.Random->Next() % Spread - Spread / 2);
			FWorldMapOrbit& Orbit = Context.Orbit;
			Orbit.Eye[0] = static_cast<int16>(Orbit.Eye[0] + X);
			Orbit.Eye[1] = static_cast<int16>(Orbit.Eye[1] + Y);
			Orbit.Target[0] = static_cast<int16>(Orbit.Target[0] + X);
			Orbit.Target[1] = static_cast<int16>(Orbit.Target[1] + Y);
		}

		FResult UpdateDirector(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			const int32 Distance = Context.CameraDistance;
			switch (Task.Step)
			{
			case DirectorStep::Burst:
				if (CountDown(Task))
				{
					Task.Timer = 0x20;
					++Task.Step;
					SendMessage(Context, BurstSlot, WakeMessage);
					for (const int32 Sound : BurstSounds)
					{
						PlayCutsceneEffect(Context, Sound);
					}
				}
				break;
			case DirectorStep::Rings:
				if (CountDown(Task))
				{
					Task.Timer = 0x20;
					++Task.Step;
					SendMessage(Context, RingsSlot, WakeMessage);
				}
				break;
			case DirectorStep::Trail:
				if (CountDown(Task))
				{
					Task.Timer = 0x30;
					++Task.Step;
					SendMessage(Context, TrailSlot, WakeMessage);
					SendMessage(Context, CraftSlot, WakeMessage);
					SendMessage(Context, SeaObjectSlot, WakeMessage);
				}
				break;
			case DirectorStep::Wait:
				if (CountDown(Task))
				{
					Task.Timer = 0x28;
					++Task.Step;
				}
				break;
			case DirectorStep::StrongShake:
				Task.Timer = static_cast<int16>(Task.Timer - 1);
				Shake(Context, Distance, 12);
				if (Task.Timer < 1)
				{
					Task.Timer = 0x78;
					++Task.Step;
					PlayCutsceneEffect(Context, RumbleSound);
					for (const int32 Sound : BurstSounds)
					{
						FadeCutsceneEffect(Context, Sound, 0, 0x100);
					}
				}
				break;
			case DirectorStep::MildShake:
				Task.Timer = static_cast<int16>(Task.Timer - 1);
				Shake(Context, Distance, 4);
				if (Task.Timer < 1)
				{
					Task.Timer = 0x5A;
					++Task.Step;
				}
				break;
			case DirectorStep::Settle:
				if (CountDown(Task))
				{
					Task.Timer = 0x50;
					++Task.Step;
					SendMessage(Context, FWorldMapContext::FadeSlot, FadeOutMessage);
					FadeCutsceneEffect(Context, RumbleSound, 0, 0x100);
					Context.FadeBlend = 2;
					Context.FadeStep = 4;
				}
				break;
			case DirectorStep::Leave:
				if (CountDown(Task))
				{
					Task.Timer = 0;
					Context.Running = 0;
					Context.ExitCode = 0;
				}
				break;
			case DirectorStep::Follow:
				if (Task.Message == WakeMessage)
				{
					FadeCutsceneEffect(Context, EngineSound, 0, 8);
					SendMessage(Context, FWorldMapContext::FadeSlot, FadeOutMessage);
					Context.FadeStep = 0x20;
					Context.FadeBlend = Task.Message;
					Task.Timer = 8;
					Task.Message = 0;
					++Task.Step;
				}
				break;
			case DirectorStep::Cut:
				if (CountDown(Task))
				{
					Context.CameraDistance = 0x960000;
					Context.CameraPitch = -0x30;
					Context.CameraYaw = 0x40;
					Context.CameraRoll = 0;
					Task.Timer = 0x10;
					++Task.Step;
				}
				break;
			case DirectorStep::FadeIn:
				if (CountDown(Task))
				{
					SendMessage(Context, FWorldMapContext::FadeSlot, FadeInMessage);
					Context.FadeBlend = 1;
					Context.FadeStep = 0x80;
					Task.Step = DirectorStep::Burst;
					Task.Timer = 1;
				}
				break;
			default:
				break;
			}
			if (Task.Step != DirectorStep::StrongShake && Task.Step != DirectorStep::MildShake)
			{
				SetCameraOrbit(Context, Context.CameraDistance, {Context.CameraPitch, Context.CameraYaw, Context.CameraRoll});
			}
			return WorldMapTaskState::Update;
		}

		/** The craft's steps (+20h), its fall's speed (+3Ch) and its speed north as it brakes (+40h). */
		namespace CraftStep
		{
			constexpr int16 Descent = 0;
			constexpr int16 Bounce = 1;
			constexpr int16 Braking = 2;
			constexpr int16 Stopping = 3;
			constexpr int16 Stopped = 4;
		}
		/** It skims the sea once 24 units above it. */
		constexpr int32 SeaLevel = -0x18001;
		constexpr int32 StopZ = 0x1580001;
		constexpr int32 Northward = -0x8000;
		/** The parts' three angles and their turns a tick (+50h to +64h), as scene 9's craft. */
		constexpr int32 SpinA = 0;
		constexpr int32 SpinStepA = 1;
		constexpr int32 SpinB = 2;
		constexpr int32 SpinStepB = 3;
		constexpr int32 SpinC = 4;
		constexpr int32 SpinStepC = 5;
		constexpr int32 PartsA[] = {12, 8, 10, 6};
		constexpr int32 PartsB[] = {13, 9, 11, 7};
		constexpr int32 PartsC[] = {5, 4};
		constexpr int32 PartsRolling[] = {3, 2, 1};

		FResult InitCraft(FWorldMapContext& Context, int32 Slot)
		{
			if (Context.Objects.size() < static_cast<std::size_t>(ModelCount))
			{
				Context.Fault = MakeError("scene 10 has " + std::to_string(Context.Objects.size()) + " models where it moves 17");
				return WorldMapTaskState::Init;
			}
			for (int32 Part = 1; Part <= PartCount; ++Part)
			{
				AttachObject(Context, Hull, Part);
			}
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Step = CraftStep::Descent;
			Task.Position = Context.Start;
			Task.Word(SpinStepA) = 0x40;
			Task.Word(SpinB) = 0x200;
			Task.Word(SpinStepB) = 0x60;
			Task.Word(SpinC) = 0x300;
			Task.Word(SpinA) = 0;
			Task.Word(SpinStepC) = 0x50;
			Context.CameraTarget = Task.Position;
			ObjectAt(Context, Hull).Angles = {-0x100, 0, -0x40};
			PlayCutsceneEffect(Context, EngineSound);
			return WorldMapTaskState::Update;
		}

		std::array<int16, 4> PlaceOf(const FWorldMapTask& Task)
		{
			return {static_cast<int16>(Task.Position.X >> 12), static_cast<int16>(Task.Position.Y >> 12), static_cast<int16>(Task.Position.Z >> 12), 0};
		}

		void TurnHull(FWorldMapContext& Context, int32 Pitch, int32 Roll)
		{
			FGteVector& Angles = ObjectAt(Context, Hull).Angles;
			Angles[0] = static_cast<int16>(Angles[0] + Pitch);
			Angles[2] = static_cast<int16>(Angles[2] + Roll);
		}

		void Turn(FWorldMapContext& Context, std::span<const int32> Parts, const FGteVector& Angles)
		{
			const FGteMatrix Rotation = RotationYxz(Angles);
			for (const int32 Part : Parts)
			{
				ObjectAt(Context, Part).Matrix.M = Rotation.M;
			}
		}

		FResult UpdateCraft(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			int32& Fall = Task.Direction.Y;
			int32& Speed = Task.Direction.Z;
			const std::array<int16, 4> Place = PlaceOf(Task);
			switch (Task.Step)
			{
			case CraftStep::Descent:
				Context.Particles.Start(WakeEffect, Place, std::nullopt);
				Task.Position.Y += 0x4000;
				Task.Position.Z += Northward;
				if (Task.Position.Y > SeaLevel)
				{
					Fall = -0x4000;
					++Task.Step;
					Context.Particles.Start(SprayEffect, Place, std::nullopt);
					PlayCutsceneEffect(Context, SplashSound);
					Task.Timer = 0x20;
				}
				break;
			case CraftStep::Bounce:
				Context.Particles.Start(WakeEffect, Place, std::nullopt);
				Context.Particles.Start(SprayEffect, Place, std::nullopt);
				Task.Timer = static_cast<int16>(Task.Timer - 1);
				if (Task.Timer < 1)
				{
					Context.Particles.Stop(SprayEffect);
				}
				TurnHull(Context, 4, 2);
				Task.Position.Y += Fall;
				Fall += 0x100;
				Task.Position.Z += Northward;
				if (Task.Position.Y > SeaLevel)
				{
					Speed = Northward;
					++Task.Step;
					Context.Particles.Start(SprayEffect, Place, std::nullopt);
					PlayCutsceneEffect(Context, SplashSound);
				}
				break;
			case CraftStep::Braking:
				Context.Particles.Start(WakeEffect, Place, std::nullopt);
				Context.Particles.Start(SprayEffect, Place, std::nullopt);
				TurnHull(Context, -2, -1);
				Task.Position.Z += Speed;
				Speed += 0x100;
				if (Task.Position.Z < StopZ)
				{
					Task.Timer = 8;
					++Task.Step;
					Context.Particles.Stop(SprayEffect);
					SendMessage(Context, DirectorSlot, WakeMessage);
				}
				break;
			case CraftStep::Stopping:
				TurnHull(Context, -2, -1);
				Task.Position.Z += Speed;
				Task.Timer = static_cast<int16>(Task.Timer - 1);
				Speed += 0x100;
				if (Task.Timer < 1)
				{
					SendMessage(Context, DirectorSlot, WakeMessage);
					++Task.Step;
				}
				break;
			case CraftStep::Stopped:
				if (Task.Message != 0)
				{
					Task.Message = 0;
					for (int32 Part = PartCount; Part >= 0; --Part)
					{
						ObjectAt(Context, Part).Hidden = 1;
					}
					Context.Particles.Stop(WakeEffect);
				}
				break;
			default:
				break;
			}
			Context.Ground.Wrap(Task.Position);
			FWorldMapObject& Craft = ObjectAt(Context, Hull);
			Craft.X = Task.Position.X >> 12;
			Craft.Y = Task.Position.Y >> 12;
			Craft.Z = Task.Position.Z >> 12;
			Context.CameraTarget = Task.Position;
			Task.Word(SpinA) = (Task.Word(SpinA) + Task.Word(SpinStepA)) & 0xFFF;
			Task.Word(SpinB) = (Task.Word(SpinB) + Task.Word(SpinStepB)) & 0xFFF;
			Task.Word(SpinC) = (Task.Word(SpinC) + Task.Word(SpinStepC)) & 0xFFF;
			const int16 C = static_cast<int16>(Task.Word(SpinC));
			Turn(Context, PartsRolling, {0, 0, C});
			Turn(Context, PartsC, {0, C, 0});
			Turn(Context, PartsA, {0, static_cast<int16>(Task.Word(SpinA)), 0});
			Turn(Context, PartsB, {0, static_cast<int16>(Task.Word(SpinB)), 0});
			Craft.Matrix.M = RotationYxz(Craft.Angles).M;
			return WorldMapTaskState::Update;
		}

		/** The rings' scales across (+6Ch, +70h, 4.12). */
		constexpr int32 InnerScale = 7;
		constexpr int32 OuterScale = 8;

		FResult InitRings(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Step = 0;
			Task.Word(OuterScale) = 0;
			Task.Word(InnerScale) = 0;
			SetGlowLook(ObjectAt(Context, InnerRing));
			SetGlowLook(ObjectAt(Context, OuterRing));
			return WorldMapTaskState::Idle;
		}

		FResult UpdateRings(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			const FWorldMapObject& Craft = ObjectAt(Context, Hull);
			for (const int32 Ring : {InnerRing, OuterRing})
			{
				FWorldMapObject& Object = ObjectAt(Context, Ring);
				Object.X = Craft.X;
				Object.Y = Craft.Y;
				Object.Z = Craft.Z;
				Object.W = Craft.W;
			}
			FResult Result = WorldMapTaskState::Update;
			int32& Inner = Task.Word(InnerScale);
			int32& Outer = Task.Word(OuterScale);
			Inner += 0xC0;
			if (Inner > 0x7FF)
			{
				Outer = (Outer + 0x100) & 0x7FFF;
			}
			if (Outer > 0x5FFF)
			{
				Result = WorldMapTaskState::Idle;
				Task.Message = 0;
				Outer = 0;
				Inner = 0;
			}
			ObjectAt(Context, InnerRing).Matrix.M = ScaleColumns(FGteMatrix::Identity(), {Inner, FGteMatrix::One, Inner}).M;
			ObjectAt(Context, OuterRing).Matrix.M = ScaleColumns(FGteMatrix::Identity(), {Outer, FGteMatrix::One, Outer}).M;
			return Result;
		}

		FResult InitTrail(FWorldMapContext& Context, int32 Slot)
		{
			TaskAt(Context, Slot).Timer = 0x60;
			return WorldMapTaskState::Idle;
		}

		FResult UpdateTrail(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			if (Task.Message == WakeMessage)
			{
				const FWorldMapObject& Craft = ObjectAt(Context, Hull);
				Task.Message = 0;
				Task.Position.X = static_cast<int32>(static_cast<uint32>(Craft.X) << 12);
				Task.Position.Y = static_cast<int32>(static_cast<uint32>(Craft.Y) << 12);
				Task.Position.Z = static_cast<int32>(static_cast<uint32>(Craft.Z) << 12);
			}
			if (CountDown(Task))
			{
				Task.Timer = 0x60;
				Task.Position = Context.Start;
				Context.Particles.Stop(TrailEffect);
				return WorldMapTaskState::Idle;
			}
			// South, 2 units a tick.
			Task.Position.Z += 0x20000;
			Context.Particles.Start(TrailEffect, PlaceOf(Task), std::nullopt);
			return WorldMapTaskState::Update;
		}

		FResult UpdateBurst(FWorldMapContext& Context, int32 Slot)
		{
			TaskAt(Context, Slot).Message = 0;
			const FWorldMapObject& Craft = ObjectAt(Context, Hull);
			Context.Particles.Start(BurstEffect, std::array<int16, 4>{static_cast<int16>(Craft.X), static_cast<int16>(Craft.Y), static_cast<int16>(Craft.Z), 0},
				std::nullopt);
			return WorldMapTaskState::Idle;
		}

		FResult InitSeaObject(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Position.X = 0x2000000;
			Task.Position.Z = 0x1500000;
			Task.Step = 0;
			FWorldMapObject& Object = ObjectAt(Context, SeaObject);
			Object.Angles = {0, 0x780, 0};
			Object.Matrix.M = RotationYxz(Object.Angles).M;
			return WorldMapTaskState::Update;
		}

		FResult UpdateSeaObject(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			FWorldMapObject& Object = ObjectAt(Context, SeaObject);
			if (Task.Message != 0)
			{
				Task.Message = 0;
				Object.Hidden = 1;
			}
			Context.Ground.Wrap(Task.Position);
			// Half sunk: 24 units under the waves as the frame before left them.
			Task.Position.Y = Context.Ground.SeaHeight(Task.Position.X, Task.Position.Z, Context.WavePhase) + 0x18000;
			Object.X = Task.Position.X >> 12;
			Object.Y = Task.Position.Y >> 12;
			Object.Z = Task.Position.Z >> 12;
			return WorldMapTaskState::Update;
		}

		constexpr FCutsceneRoutine Routines[] = {
			{R::InitScene10Director, InitDirector},
			{R::UpdateScene10Director, UpdateDirector},
			{R::InitScene10Craft, InitCraft},
			{R::UpdateScene10Craft, UpdateCraft},
			{R::InitScene10Rings, InitRings},
			{R::UpdateScene10Rings, UpdateRings},
			{R::InitScene10Trail, InitTrail},
			{R::UpdateScene10Trail, UpdateTrail},
			{R::UpdateScene10Burst, UpdateBurst},
			{R::InitScene10SeaObject, InitSeaObject},
			{R::UpdateScene10SeaObject, UpdateSeaObject},
		};

		void Start(FWorldMapContext& Context)
		{
			StartCutsceneWorld(Context, CraftStart);
			RegisterCutsceneEffects(Context);
			AddTask(Context, R::InitFade, R::UpdateFade);
			AddTask(Context, R::InitScene10Director, R::UpdateScene10Director);
			AddTask(Context, R::InitScene10Craft, R::UpdateScene10Craft);
			AddTask(Context, R::InitScene10Rings, R::UpdateScene10Rings);
			AddTask(Context, R::InitScene10Trail, R::UpdateScene10Trail);
			AddTask(Context, R::InitIdleTask, R::UpdateScene10Burst);
			AddTask(Context, R::InitScene10SeaObject, R::UpdateScene10SeaObject);
			AddTask(Context, R::InitCutsceneFrame, R::UpdateCutsceneFrame);
			ResetEncounters(Context);
		}

		void End(FWorldMapContext& Context)
		{
			StopCutsceneEffects(Context);
			EndCutscene(Context, FieldMap, FieldEntrance);
		}
	}

	/** A craft skims the sea and stops over an object in it; a burst, two rings, a trail south, the screen shaken. */
	const FCutscene Scene10Cutscene{{10, 10}, 2, 0x10, Scene10::Start, Scene10::End, Scene10::Routines};
}
