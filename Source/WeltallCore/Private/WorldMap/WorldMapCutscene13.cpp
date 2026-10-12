#include "WorldMap/WorldMapCutsceneDirector.h"

namespace Weltall::WorldMapOps
{
	namespace Scene13
	{
		using R = EWorldMapRoutine;

		constexpr FWorldMapVector SceneStart{0x4100000, static_cast<int32>(0xFFF80000), 0x13C0000, 0};
		constexpr uint16 FieldMap = 0x84;
		constexpr uint16 FieldEntrance = 2;
		/** The tasks' slots, as the start adds them. */
		constexpr int32 CameraSlot = 2;
		constexpr int32 EffectsSlot = 3;
		constexpr int32 RingSlot = 4;
		/** The models: the ring's two discs, from this one. */
		constexpr int32 Ring = 0;
		constexpr int32 RingCount = 2;
		constexpr int32 Sounds[] = {0x16, 0x17, 0x18};
		constexpr int32 Effects[] = {0x28, 0x29, 0x2A};

		FWorldMapTask& TaskAt(FWorldMapContext& Context, int32 Slot)
		{
			return Context.Tasks[static_cast<std::size_t>(Slot)];
		}

		/** The director's steps (+20h) besides the wait and the end. */
		namespace Step
		{
			constexpr int16 StartEffects = 2;
			constexpr int16 Rumble = 3;
			constexpr int16 WhiteOut = 4;
			constexpr int16 Burst = 5;
			constexpr int16 Quake = 6;
			constexpr int16 FadeToBlack = 7;
			constexpr int16 PlaySounds = 8;
		}

		/** The camera's messages, which are its steps (+20h) too: it closes in, rumbles, quakes. */
		namespace CameraStep
		{
			constexpr int16 Still = 0;
			constexpr int16 ClosingIn = 1;
			constexpr int16 Rumbling = 2;
			constexpr int16 Quaking = 3;
		}
		constexpr int16 WakeMessage = 1;

		const FWorldMapDirectorScript& Script(const FWorldMapContext& Context)
		{
			return Context.Tables->Cutscenes.Directors.Scene13;
		}

		FResult InitDirector(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Word(DirectorScriptIndex) = 0;
			// The index stays on the script's first entry, which the first wait's end takes again.
			TakeDirectorStep(Context, Task, Script(Context));
			return WorldMapTaskState::Update;
		}

		FResult UpdateDirector(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			switch (Task.Step)
			{
			case DirectorStep::Wait:
				WaitDirector(Context, Task, Script(Context));
				return WorldMapTaskState::Update;
			case Step::StartEffects:
				SendMessage(Context, EffectsSlot, WakeMessage);
				break;
			case Step::Rumble:
				SendMessage(Context, CameraSlot, CameraStep::Rumbling);
				break;
			case Step::WhiteOut:
				SendFade(Context, DirectorFade::Out, DirectorFade::White, 0x80);
				break;
			case Step::Burst:
				SendMessage(Context, FWorldMapContext::FadeSlot, DirectorFade::In);
				SendMessage(Context, RingSlot, WakeMessage);
				Context.FadeBlend = DirectorFade::White;
				Context.FadeStep = 0x80;
				break;
			case Step::Quake:
				SendMessage(Context, CameraSlot, CameraStep::Quaking);
				break;
			case Step::FadeToBlack:
				SendFade(Context, DirectorFade::Out, DirectorFade::Black, 4);
				break;
			case Step::PlaySounds:
				for (const int32 Sound : Sounds)
				{
					PlayCutsceneEffect(Context, Sound);
				}
				break;
			case DirectorStep::Leave:
				LeaveCutscene(Context, Task);
				return WorldMapTaskState::Update;
			default:
				return WorldMapTaskState::Update;
			}
			Task.Step = DirectorStep::Wait;
			return WorldMapTaskState::Update;
		}

		/** The camera closes in from 2048 units to 768, tilting down and turning. */
		constexpr int32 FirstDistance = 0x800000;
		constexpr int16 FirstPitch = 0x10;
		constexpr int16 FirstYaw = 0x8E0;
		constexpr FCameraEase ClosingIn{-0x10000, 0x300000, -0x4000, -0x100000, -0x10000, 0x2E0000};
		constexpr int32 RumbleReach = 0x8000;
		constexpr int32 QuakeReach = 0x80000;
		constexpr int32 QuakeFade = 0x4000;

		FResult InitCamera(FWorldMapContext& Context, int32 Slot)
		{
			InitDirectedCamera(Context, TaskAt(Context, Slot));
			return WorldMapTaskState::Update;
		}

		FResult UpdateCamera(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			switch (Task.Message)
			{
			case CameraStep::ClosingIn:
				Task.Step = CameraStep::ClosingIn;
				Task.Message = 0;
				StartCameraEase(Context, Task, FirstDistance, FirstPitch, FirstYaw);
				break;
			case CameraStep::Rumbling:
				Task.Step = CameraStep::Rumbling;
				Task.Message = 0;
				Task.Word(CameraShakeReach) = RumbleReach;
				break;
			case CameraStep::Quaking:
				Task.Step = CameraStep::Quaking;
				Task.Message = 0;
				Task.Word(CameraShakeReach) = QuakeReach;
				break;
			default:
				break;
			}
			if (Context.LookAt == 0)
			{
				SetCameraOrbit(Context, Context.CameraDistance, {Context.CameraPitch, Context.CameraYaw, Context.CameraRoll});
			}
			if (Task.Step == CameraStep::ClosingIn)
			{
				EaseCamera(Context, Task, ClosingIn);
			}
			else if (Task.Step == CameraStep::Quaking)
			{
				Task.Word(CameraShakeReach) -= QuakeFade;
				if (Task.Word(CameraShakeReach) < CameraShakeStill)
				{
					Task.Word(CameraShakeReach) = CameraShakeStill;
					Task.Step = CameraStep::Still;
				}
			}
			ShakeCamera(Context, Task);
			return WorldMapTaskState::Update;
		}

		FResult InitEffects(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Position.X = Context.Start.X;
			Task.Position.Y = Context.Start.Y;
			Task.Position.Z = Context.Start.Z;
			return WorldMapTaskState::Update;
		}

		FResult UpdateEffects(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			if (Task.Message == WakeMessage)
			{
				Task.Message = 0;
				for (const int32 Effect : Effects)
				{
					Context.Particles.Start(Effect, EffectPlace(Task.Position), std::nullopt);
				}
			}
			return WorldMapTaskState::Update;
		}

		/** The ring dims by this a tick, from 128. */
		constexpr int32 RingDimming = 4;

		FResult InitRing(FWorldMapContext& Context, int32 Slot)
		{
			if (Context.Objects.size() < static_cast<std::size_t>(RingCount))
			{
				Context.Fault = MakeError("scene 13 has " + std::to_string(Context.Objects.size()) + " models where its ring is two");
				return WorldMapTaskState::Init;
			}
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Position.X = Context.Start.X;
			Task.Position.Y = SceneStart.Y;
			Task.Position.Z = Context.Start.Z;
			return InitGlowRing(Context, Task, Ring);
		}

		FResult UpdateRing(FWorldMapContext& Context, int32 Slot)
		{
			return UpdateGlowRing(Context, TaskAt(Context, Slot), Ring, RingDimming);
		}

		constexpr FCutsceneRoutine Routines[] = {
			{R::InitScene13Director, InitDirector},
			{R::UpdateScene13Director, UpdateDirector},
			{R::InitScene13Camera, InitCamera},
			{R::UpdateScene13Camera, UpdateCamera},
			{R::InitScene13Effects, InitEffects},
			{R::UpdateScene13Effects, UpdateEffects},
			{R::InitScene13Ring, InitRing},
			{R::UpdateScene13Ring, UpdateRing},
		};

		void Start(FWorldMapContext& Context)
		{
			StartCutsceneWorld(Context, SceneStart);
			RegisterCutsceneEffects(Context);
			AddTask(Context, R::InitFade, R::UpdateFade);
			AddTask(Context, R::InitScene13Director, R::UpdateScene13Director);
			AddTask(Context, R::InitScene13Camera, R::UpdateScene13Camera);
			AddTask(Context, R::InitScene13Effects, R::UpdateScene13Effects);
			AddTask(Context, R::InitScene13Ring, R::UpdateScene13Ring);
			AddTask(Context, R::InitCutsceneFrameTrees, R::UpdateCutsceneFrameTrees);
			ResetEncounters(Context);
		}

		void End(FWorldMapContext& Context)
		{
			StopCutsceneEffects(Context);
			EndCutscene(Context, FieldMap, FieldEntrance);
		}
	}

	/** The camera closes in on a landmark by the coast; effects rise from it, the ground rumbles, a white flash, a ring of light, a quake. */
	const FCutscene Scene13Cutscene{{13, 13}, 2, 0x10, Scene13::Start, Scene13::End, Scene13::Routines};
}
