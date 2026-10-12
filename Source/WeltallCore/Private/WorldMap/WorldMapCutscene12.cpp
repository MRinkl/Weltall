#include "WorldMap/WorldMapCutscenes.h"

#include "Game/PsxRandom.h"
#include "Graphics/GteVectors.h"

namespace Weltall::WorldMapOps
{
	namespace Scene12
	{
		using R = EWorldMapRoutine;

		constexpr FWorldMapVector FleetStart{0xD00000, static_cast<int32>(0xFFF60000), 0x400000, 0};
		constexpr uint16 FieldMap = 0x111;
		constexpr uint16 FieldEntrance = 2;
		constexpr int16 FadeInMessage = 0xC;
		constexpr int16 FadeOutMessage = 0xD;
		constexpr int32 ModelCount = 17;
		/** The tasks' slots, as the start adds them. */
		constexpr int32 CameraSlot = 2;
		constexpr int32 LeadSlot = 3;
		constexpr int32 HitShipSlots[] = {4, 5, 6, 7};
		constexpr int32 LateShipSlot = 9;
		/** The scene's sound effects, three at a time: the fleet under way, the bursts in the sky, the flash. */
		constexpr int32 FleetSounds[] = {0xD, 0xE, 0xF};
		constexpr int32 BurstSounds[] = {0x10, 0x11, 0x12};
		constexpr int32 FlashSounds[] = {0x13, 0x14, 0x15};
		/** The particle effects: the bursts in the sky, the fire about the camera's target, the fires of the ships hit. */
		constexpr int32 BurstEffect = 0x14;
		constexpr int32 DebrisEffects[] = {0x1A, 0x1B};
		constexpr int32 LeadSmokeEffect = 0x1F;
		constexpr int32 SmokeEffect = 0x1E;

		FWorldMapTask& TaskAt(FWorldMapContext& Context, int32 Slot)
		{
			return Context.Tasks[static_cast<std::size_t>(Slot)];
		}

		FWorldMapObject& ObjectAt(FWorldMapContext& Context, int32 Number)
		{
			return Context.Objects[static_cast<std::size_t>(Number)];
		}

		std::array<int16, 4> PlaceOf(const FWorldMapVector& Position)
		{
			return {static_cast<int16>(Position.X >> 12), static_cast<int16>(Position.Y >> 12), static_cast<int16>(Position.Z >> 12), 0};
		}

		/** Where the camera's target stands on the sea's level. */
		std::array<int16, 4> UnderTarget(const FWorldMapContext& Context)
		{
			return {static_cast<int16>(Context.CameraTarget.X >> 12), 0, static_cast<int16>(Context.CameraTarget.Z >> 12), 0};
		}

		/** The director's steps (+20h), each followed by the wait before the next; its timer is the wait, its first word the next step's index. */
		namespace DirectorStep
		{
			constexpr int16 Waiting = 1;
			constexpr int16 Circle = 2;
			constexpr int16 Bursts = 0x10;
			constexpr int16 Flash = 0x11;
			constexpr int16 Hits = 0x12;
			constexpr int16 LateShip = 0x13;
			constexpr int16 Overtaking = 0x14;
			constexpr int16 FadeOut = 0x16;
			constexpr int16 Leave = 0x40;
		}
		constexpr int16 Steps[] = {DirectorStep::Circle, DirectorStep::Bursts, DirectorStep::Flash, DirectorStep::Hits, DirectorStep::LateShip,
			DirectorStep::Overtaking, DirectorStep::FadeOut, DirectorStep::Leave};
		constexpr int16 Waits[] = {600, 60, 8, 255, 180, 270, 75, 0};

		/** The camera's messages. */
		namespace CameraMessage
		{
			constexpr int16 Circle = 1;
			constexpr int16 Shake = 2;
			constexpr int16 Debris = 3;
			constexpr int16 Approach = 4;
			constexpr int16 FollowLateShip = 6;
		}

		void PlaySounds(FWorldMapContext& Context, std::span<const int32> Sounds)
		{
			for (const int32 Sound : Sounds)
			{
				PlayCutsceneEffect(Context, Sound);
			}
		}

		FResult InitDirector(FWorldMapContext& Context, int32 Slot)
		{
			if (Context.Objects.size() < static_cast<std::size_t>(ModelCount))
			{
				Context.Fault = MakeError("scene 12 has " + std::to_string(Context.Objects.size()) + " models where it moves 17");
				return WorldMapTaskState::Init;
			}
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Step = Steps[0];
			Task.Timer = Waits[0];
			Task.Word(0) = 1;
			return WorldMapTaskState::Update;
		}

		FResult UpdateDirector(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			switch (Task.Step)
			{
			case DirectorStep::Waiting:
				WaitForCutsceneStep(Context, Task, Steps, Waits);
				return WorldMapTaskState::Update;
			case DirectorStep::Circle:
				SendMessage(Context, CameraSlot, CameraMessage::Circle);
				PlaySounds(Context, FleetSounds);
				break;
			case DirectorStep::Bursts:
				Context.Particles.Start(BurstEffect, UnderTarget(Context), std::nullopt);
				PlaySounds(Context, BurstSounds);
				break;
			case DirectorStep::Flash:
				SendMessage(Context, CameraSlot, CameraMessage::Shake);
				SendMessage(Context, FWorldMapContext::FadeSlot, FadeOutMessage);
				Context.FadeBlend = 1;
				Context.FadeStep = 0x40;
				PlaySounds(Context, FlashSounds);
				break;
			case DirectorStep::Hits:
				SendMessage(Context, CameraSlot, CameraMessage::Debris);
				SendMessage(Context, LeadSlot, 1);
				for (const int32 Ship : HitShipSlots)
				{
					SendMessage(Context, Ship, 1);
				}
				SendMessage(Context, FWorldMapContext::FadeSlot, FadeInMessage);
				Context.FadeBlend = 1;
				Context.FadeStep = 1;
				break;
			case DirectorStep::LateShip:
				SendMessage(Context, CameraSlot, CameraMessage::Approach);
				SendMessage(Context, LateShipSlot, 1);
				break;
			case DirectorStep::Overtaking:
				SendMessage(Context, CameraSlot, CameraMessage::FollowLateShip);
				SendMessage(Context, LeadSlot, 2);
				SendMessage(Context, LateShipSlot, 2);
				break;
			case DirectorStep::FadeOut:
				SendMessage(Context, FWorldMapContext::FadeSlot, FadeOutMessage);
				Context.FadeBlend = 2;
				Context.FadeStep = 4;
				break;
			case DirectorStep::Leave:
				Context.Running = 0;
				Context.ExitCode = 0;
				break;
			default:
				return WorldMapTaskState::Update;
			}
			Task.Step = DirectorStep::Waiting;
			return WorldMapTaskState::Update;
		}

		/**
		 * The camera's steps (+20h) and words: its place on its path (+50h, 20.12: the segment and the place in it), its
		 * speed a tick (+58h), the place it brakes from (+5Ch), how much a tick (+60h), and how far it shakes (+7Ch, 20.12).
		 */
		namespace CameraStep
		{
			constexpr int16 Parked = 0;
			constexpr int16 Moving = 1;
			constexpr int16 Braking = 2;
			constexpr int16 StartShake = 3;
			constexpr int16 Debris = 4;
			constexpr int16 Approach = 5;
			constexpr int16 LateShip = 6;
		}
		constexpr int32 PathPlace = 0;
		constexpr int32 PathSpeed = 2;
		constexpr int32 BrakeFrom = 3;
		constexpr int32 BrakeStep = 4;
		constexpr int32 ShakeSpread = 11;
		constexpr int32 Still = 0x1000;
		/** On the late ship's path the camera slows by 1 a tick past this place. */
		constexpr int32 LateShipSlowFrom = 0x4800;

		FResult InitCamera(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Context.ScreenCentreY = 0x78;
			Context.CameraDistance = 0x400000;
			Context.LookAt = 1;
			Task.Word(ShakeSpread) = Still;
			Context.CameraPitch = -0x40;
			Context.CameraYaw = 0;
			Context.CameraRoll = 0;
			Context.CameraTarget.X = Context.Start.X;
			Context.CameraTarget.Y = Context.Start.Y;
			Context.CameraTarget.Z = Context.Start.Z;
			Context.Controlled.X = Context.Start.X;
			Context.Controlled.Y = Context.Start.Y;
			Context.Controlled.Z = Context.Start.Z;
			Task.Word(PathSpeed) = 0x40;
			Task.Word(PathPlace) = 0;
			return WorldMapTaskState::Update;
		}

		void TakeCameraMessage(FWorldMapTask& Task)
		{
			switch (Task.Message)
			{
			case CameraMessage::Circle:
				Task.Step = CameraStep::Moving;
				Task.Word(BrakeFrom) = 0x8200;
				Task.Word(BrakeStep) = 1;
				break;
			case CameraMessage::Shake:
				Task.Step = CameraStep::StartShake;
				break;
			case CameraMessage::Debris:
				Task.Step = CameraStep::Debris;
				break;
			case CameraMessage::Approach:
				Task.Step = CameraStep::Approach;
				Task.Word(BrakeFrom) = 0xA800;
				Task.Word(BrakeStep) = 8;
				break;
			case CameraMessage::FollowLateShip:
				Task.Step = CameraStep::LateShip;
				Task.Word(PathPlace) = 0;
				Task.Word(PathSpeed) = 0x40;
				break;
			default:
				return;
			}
			Task.Message = 0;
		}

		/** The eye on a path about the target: x and z from it (z the drawing's), its height its own; then the look from there. */
		void LookFromPath(FWorldMapContext& Context, const FWorldMapTask& Task, const FWorldMapPath& Path)
		{
			const int32 Segment = Task.Word(PathPlace) >> 12;
			if (Segment < 0 || static_cast<std::size_t>(Segment) + 2 >= Path.size())
			{
				Context.Fault = MakeError("scene 12's camera reads past its path, at segment " + std::to_string(Segment));
				return;
			}
			const std::size_t At = static_cast<std::size_t>(Segment);
			if (Path[At + 2][3] != -1)
			{
				const std::array<int32, 3> Point = QuadraticBlend(Task.Word(PathPlace) & 0xFFF, Path[At], Path[At + 1], Path[At + 2]);
				FWorldMapOrbit& Orbit = Context.Orbit;
				Orbit.Target = {0, static_cast<int16>(Context.CameraTarget.Y >> 12), 0};
				Orbit.Up = {0, -0x1000, 0};
				Orbit.Eye = {static_cast<int16>(Point[0] >> 16), static_cast<int16>(Point[1] >> 16), static_cast<int16>(-static_cast<int16>(Point[2] >> 16))};
			}
			LookAtCamera(Context);
			FGteVector Angles{Context.CameraPitch, Context.CameraYaw, Context.CameraRoll};
			AnglesFromMatrix(Context.CameraMatrix, *Context.Arctangents, Angles);
			Context.CameraPitch = Angles[0];
			Context.CameraYaw = Angles[1];
			Context.CameraRoll = Angles[2];
		}

		FResult UpdateCamera(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			TakeCameraMessage(Task);
			if (Context.LookAt == 0)
			{
				SetCameraOrbit(Context, Context.CameraDistance, {Context.CameraPitch, Context.CameraYaw, Context.CameraRoll});
			}
			int32& Place = Task.Word(PathPlace);
			int32& Speed = Task.Word(PathSpeed);
			int32& Spread = Task.Word(ShakeSpread);
			const FWorldMapCutsceneTables& Tables = Context.Tables->Cutscenes;
			const FWorldMapPath* Path = &Tables.Scene12FleetCamera;
			switch (Task.Step)
			{
			case CameraStep::Parked:
				break;
			case CameraStep::Braking:
				Speed -= Task.Word(BrakeStep);
				if (Speed < 0)
				{
					Speed = 0;
					Task.Step = CameraStep::Parked;
				}
				[[fallthrough]];
			case CameraStep::Moving:
				Place += Speed;
				if (Task.Word(BrakeFrom) < Place)
				{
					Task.Step = CameraStep::Braking;
				}
				break;
			case CameraStep::StartShake:
				Spread = 0xF000;
				Task.Step = CameraStep::Parked;
				break;
			case CameraStep::Debris:
				for (const int32 Effect : DebrisEffects)
				{
					Context.Particles.Start(Effect, UnderTarget(Context), std::nullopt);
				}
				Spread -= 0x120;
				if (Spread < Still)
				{
					Spread = Still;
					Task.Step = CameraStep::Parked;
					for (const int32 Effect : DebrisEffects)
					{
						Context.Particles.Stop(Effect);
					}
				}
				break;
			case CameraStep::Approach:
				Speed = 0x100;
				Task.Step = CameraStep::Moving;
				break;
			case CameraStep::LateShip:
				Context.CameraTarget.X = Context.Controlled.X;
				Context.CameraTarget.Y = Context.Controlled.Y;
				Place += Speed;
				if (Place > LateShipSlowFrom)
				{
					Speed = Speed > 0 ? Speed - 1 : 0;
				}
				Path = &Tables.Scene12LateShipCamera;
				break;
			default:
				Path = nullptr;
				break;
			}
			if (Path != nullptr)
			{
				// The fleet sails north under the camera: its target keeps the place across of the start.
				Context.CameraTarget.Z = Context.Controlled.Z;
				LookFromPath(Context, Task, *Path);
				if (Context.Fault)
				{
					return Task.State;
				}
			}
			const int32 Range = Spread >> 12;
			if (Range == 0)
			{
				Context.Fault = MakeError("scene 12's camera shakes by nothing");
				return Task.State;
			}
			const int16 Shake = static_cast<int16>(static_cast<int16>(Context.Random->Next() % Range) - static_cast<int16>(Spread >> 13));
			Context.Orbit.Eye[1] = static_cast<int16>(Context.Orbit.Eye[1] + Shake);
			Context.Orbit.Target[1] = static_cast<int16>(Context.Orbit.Target[1] + Shake);
			return WorldMapTaskState::Update;
		}

		/** A ship of the fleet: the model its task moves, those attached to it, where it starts and its wake. */
		struct FShip
		{
			int32 Hull;
			std::span<const int32> Attached;
			int32 X;
			int32 Z;
			int32 Wake;
		};
		constexpr int32 LeadParts[] = {0, 2, 1, 3};
		constexpr int32 Ship5Parts[] = {6};
		constexpr int32 Ship9Parts[] = {7, 8};
		constexpr int32 Ship10Parts[] = {11};
		constexpr int32 Ship12Parts[] = {15};
		constexpr int32 Ship13Parts[] = {14};
		constexpr FShip Lead{4, LeadParts, 0xD00000, 0x400000, 0x13};
		constexpr FShip Ship5{5, Ship5Parts, 0xB00000, 0x200000, 0x15};
		constexpr FShip Ship9{9, Ship9Parts, 0xC00000, 0, 0x16};
		constexpr FShip Ship10{10, Ship10Parts, 0xE00000, 0x100000, 0x17};
		constexpr FShip Ship12{12, Ship12Parts, 0xE80000, 0x280000, 0x18};
		constexpr FShip Ship13{13, Ship13Parts, 0xF80000, 0x380000, 0x19};
		/** 4 units north a tick; the craft that overtakes them (the late ship, flying 640 units up) 6. */
		constexpr int32 FleetSpeed = -0x4000;
		constexpr int32 LateShipSpeed = -0x6000;
		/** The ships ride 4 units above the waves. */
		constexpr int32 Freeboard = 0x4000;
		constexpr int32 LateShip = 16;

		void Straighten(FWorldMapObject& Object)
		{
			Object.Angles = {0, 0, 0};
			Object.Matrix.M = RotationYxz(Object.Angles).M;
		}

		FResult InitShip(FWorldMapContext& Context, int32 Slot, const FShip& Ship)
		{
			for (const int32 Part : Ship.Attached)
			{
				AttachObject(Context, Ship.Hull, Part);
			}
			FWorldMapObject& Hull = ObjectAt(Context, Ship.Hull);
			Hull.Hidden = 0;
			Straighten(Hull);
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Direction.X = 0;
			Task.Direction.Y = 0;
			Task.Direction.Z = FleetSpeed;
			Task.Position.X = Ship.X;
			Task.Position.Y = 0;
			Task.Position.Z = Ship.Z;
			return WorldMapTaskState::Update;
		}

		void PlaceHull(FWorldMapContext& Context, const FWorldMapTask& Task, int32 Hull)
		{
			FWorldMapObject& Object = ObjectAt(Context, Hull);
			Object.X = Task.Position.X >> 12;
			Object.Y = Task.Position.Y >> 12;
			Object.Z = Task.Position.Z >> 12;
		}

		/** A tick's sailing: north, on the waves as the frame before left them (the lead keeps its height), its wake with it. */
		void Sail(FWorldMapContext& Context, FWorldMapTask& Task, const FShip& Ship, bool bOnTheWaves)
		{
			Task.Position.Z += Task.Direction.Z;
			Context.Ground.Wrap(Task.Position);
			if (bOnTheWaves)
			{
				Task.Position.Y = Context.Ground.SeaHeight(Task.Position.X, Task.Position.Z, Context.WavePhase) - Freeboard;
			}
			PlaceHull(Context, Task, Ship.Hull);
			Context.Particles.Start(Ship.Wake, PlaceOf(Task.Position), std::nullopt);
		}

		/** A ship hit: it and what it carries gone, its wake ended, a splash where it was, if it has one; its task rests for good. */
		FResult Sink(FWorldMapContext& Context, FWorldMapTask& Task, const FShip& Ship, std::optional<int32> Splash)
		{
			Task.Message = 0;
			ObjectAt(Context, Ship.Hull).Hidden = 1;
			for (const int32 Part : Ship.Attached)
			{
				ObjectAt(Context, Part).Hidden = 1;
			}
			Context.Particles.Stop(Ship.Wake);
			if (Splash)
			{
				Context.Particles.Start(*Splash, PlaceOf(Task.Position), std::nullopt);
			}
			return WorldMapTaskState::Idle;
		}

		FResult UpdateSunkShip(FWorldMapContext& Context, int32 Slot, const FShip& Ship, std::optional<int32> Splash)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			if (Task.Message == 1)
			{
				return Sink(Context, Task, Ship, Splash);
			}
			Sail(Context, Task, Ship, true);
			return WorldMapTaskState::Update;
		}

		/** The lead's steps (+20h): leading the camera, hit and smoking, smoking as the late ship takes the camera. */
		namespace LeadStep
		{
			constexpr int16 Leading = 0;
			constexpr int16 Smoking = 1;
			constexpr int16 Passed = 2;
		}

		FResult UpdateLead(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			if (Task.Message == 1 || Task.Message == 2)
			{
				Task.Step = Task.Message;
				Task.Message = 0;
			}
			Sail(Context, Task, Lead, false);
			if (Task.Step == LeadStep::Smoking || Task.Step == LeadStep::Passed)
			{
				Context.Particles.Start(LeadSmokeEffect, PlaceOf(Task.Position), std::nullopt);
			}
			if (Task.Step == LeadStep::Leading || Task.Step == LeadStep::Smoking)
			{
				Context.Controlled.Z = Task.Position.Z;
			}
			return WorldMapTaskState::Update;
		}

		FResult InitShip5(FWorldMapContext& Context, int32 Slot)
		{
			TaskAt(Context, Slot).Step = 0;
			return InitShip(Context, Slot, Ship5);
		}

		FResult UpdateShip5(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			if (Task.Message == 1)
			{
				Task.Message = 0;
				Task.Step = 1;
			}
			Sail(Context, Task, Ship5, true);
			if (Task.Step == 1)
			{
				Context.Particles.Start(SmokeEffect, PlaceOf(Task.Position), std::nullopt);
			}
			return WorldMapTaskState::Update;
		}

		FResult UpdateShip13(FWorldMapContext& Context, int32 Slot)
		{
			Sail(Context, TaskAt(Context, Slot), Ship13, true);
			return WorldMapTaskState::Update;
		}

		FResult InitLateShip(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapObject& Object = ObjectAt(Context, LateShip);
			Object.Hidden = 1;
			Straighten(Object);
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Position.X = 0xD00000;
			Task.Position.Z = 0x400000;
			Task.Position.Y = static_cast<int32>(0xFFD80000);
			Task.Direction.Y = 0;
			Task.Direction.X = 0;
			Task.Direction.Z = LateShipSpeed;
			return WorldMapTaskState::Idle;
		}

		FResult UpdateLateShip(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			if (Task.Message == 1)
			{
				// It comes in 1024 units behind what the camera looks at.
				Task.Message = 0;
				ObjectAt(Context, LateShip).Hidden = 0;
				Task.Position.X = Context.CameraTarget.X;
				Task.Position.Z = Context.CameraTarget.Z + 0x400000;
			}
			else if (Task.Message == 2)
			{
				Task.Message = 0;
				Task.Step = 2;
			}
			Task.Position.Z += Task.Direction.Z;
			Context.Ground.Wrap(Task.Position);
			PlaceHull(Context, Task, LateShip);
			if (Task.Step == 2)
			{
				Context.Controlled.X = Task.Position.X;
				Context.Controlled.Y = Task.Position.Y;
				Context.Controlled.Z = Task.Position.Z;
			}
			return WorldMapTaskState::Update;
		}

		constexpr FCutsceneRoutine Routines[] = {
			{R::InitScene12Director, InitDirector},
			{R::UpdateScene12Director, UpdateDirector},
			{R::InitScene12Camera, InitCamera},
			{R::UpdateScene12Camera, UpdateCamera},
			{R::InitScene12Lead, [](FWorldMapContext& Context, int32 Slot) { return InitShip(Context, Slot, Lead); }},
			{R::UpdateScene12Lead, UpdateLead},
			{R::InitScene12Ship5, InitShip5},
			{R::UpdateScene12Ship5, UpdateShip5},
			{R::InitScene12Ship9, [](FWorldMapContext& Context, int32 Slot) { return InitShip(Context, Slot, Ship9); }},
			{R::UpdateScene12Ship9, [](FWorldMapContext& Context, int32 Slot) { return UpdateSunkShip(Context, Slot, Ship9, std::nullopt); }},
			{R::InitScene12Ship10, [](FWorldMapContext& Context, int32 Slot) { return InitShip(Context, Slot, Ship10); }},
			{R::UpdateScene12Ship10, [](FWorldMapContext& Context, int32 Slot) { return UpdateSunkShip(Context, Slot, Ship10, 0x1C); }},
			{R::InitScene12Ship12, [](FWorldMapContext& Context, int32 Slot) { return InitShip(Context, Slot, Ship12); }},
			{R::UpdateScene12Ship12, [](FWorldMapContext& Context, int32 Slot) { return UpdateSunkShip(Context, Slot, Ship12, 0x1D); }},
			{R::InitScene12Ship13, [](FWorldMapContext& Context, int32 Slot) { return InitShip(Context, Slot, Ship13); }},
			{R::UpdateScene12Ship13, UpdateShip13},
			{R::InitScene12LateShip, InitLateShip},
			{R::UpdateScene12LateShip, UpdateLateShip},
		};

		void Start(FWorldMapContext& Context)
		{
			StartCutsceneWorld(Context, FleetStart);
			RegisterCutsceneEffects(Context);
			AddTask(Context, R::InitFade, R::UpdateFade);
			AddTask(Context, R::InitScene12Director, R::UpdateScene12Director);
			AddTask(Context, R::InitScene12Camera, R::UpdateScene12Camera);
			AddTask(Context, R::InitScene12Lead, R::UpdateScene12Lead);
			AddTask(Context, R::InitScene12Ship5, R::UpdateScene12Ship5);
			AddTask(Context, R::InitScene12Ship9, R::UpdateScene12Ship9);
			AddTask(Context, R::InitScene12Ship10, R::UpdateScene12Ship10);
			AddTask(Context, R::InitScene12Ship12, R::UpdateScene12Ship12);
			AddTask(Context, R::InitScene12Ship13, R::UpdateScene12Ship13);
			AddTask(Context, R::InitScene12LateShip, R::UpdateScene12LateShip);
			AddTask(Context, R::InitCutsceneFrame, R::UpdateCutsceneFrame);
			ResetEncounters(Context);
		}

		void End(FWorldMapContext& Context)
		{
			StopCutsceneEffects(Context);
			EndCutscene(Context, FieldMap, FieldEntrance);
		}
	}

	/**
	 * A fleet sails north, the camera circling its lead; bursts in the sky, a white flash, three ships gone and two
	 * smoking; a great craft flies up from behind and passes over them, the camera rising about it.
	 */
	const FCutscene Scene12Cutscene{{12, 12}, 2, 0x10, Scene12::Start, Scene12::End, Scene12::Routines};
}
