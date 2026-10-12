#include "WorldMap/WorldMapCutscene15.h"

namespace Weltall::WorldMapOps
{
	namespace Scene15
	{
		using R = EWorldMapRoutine;

		void PlaySounds(FWorldMapContext& Context, std::span<const int32> Sounds)
		{
			for (const int32 Sound : Sounds)
			{
				PlayCutsceneEffect(Context, Sound);
			}
		}

		/** The director's steps (+20h) besides the wait and the end. */
		namespace Step
		{
			constexpr int16 SecondFlight = 2;
			constexpr int16 ThirdFlight = 3;
			constexpr int16 LastFlight = 4;
			constexpr int16 Chase = 5;
			constexpr int16 WhiteOut = 6;
			constexpr int16 Dive = 7;
			constexpr int16 FollowDive = 8;
			constexpr int16 SecondWhiteOut = 9;
			constexpr int16 LightOnGround = 0xA;
			constexpr int16 GrazeFlight = 0x10;
			constexpr int16 GrazeChase = 0x11;
			constexpr int16 GrazeView = 0x12;
			constexpr int16 PassFlight = 0x18;
			constexpr int16 PassChase = 0x19;
			constexpr int16 EntranceSounds = 0x3D;
			constexpr int16 LaunchSound = 0x3E;
			constexpr int16 FadeToBlack = 0x3F;
		}

		/** The director's words after its script's index: which entrance's steps and waits it reads (+54h, +58h). */
		constexpr int32 DirectorSteps = 1;
		constexpr int32 DirectorWaits = 2;

		FResult InitDirector(FWorldMapContext& Context, int32 Slot)
		{
			const FWorldMapShotEntrance* Shot = Entrance(Context);
			if (Shot == nullptr)
			{
				return WorldMapTaskState::Init;
			}
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Word(DirectorSteps) = Context.Entrance;
			Task.Word(DirectorWaits) = Context.Entrance;
			Task.Word(DirectorScriptIndex) = 0;
			TakeDirectorStep(Context, Task, Shot->Script);
			Task.Word(DirectorScriptIndex) = 1;
			return WorldMapTaskState::Update;
		}

		FResult UpdateDirector(FWorldMapContext& Context, int32 Slot)
		{
			const FWorldMapShotEntrance* Shot = Entrance(Context);
			if (Shot == nullptr)
			{
				return WorldMapTaskState::Update;
			}
			FWorldMapTask& Task = TaskAt(Context, Slot);
			switch (Task.Step)
			{
			case DirectorStep::Wait:
				WaitDirector(Context, Task, Shot->Script);
				return WorldMapTaskState::Update;
			case Step::SecondFlight:
				PlaySounds(Context, FlightSounds);
				SendMessage(Context, CameraSlot, CameraMessage::SecondView);
				SendMessage(Context, ShotSlot, ShotMessage::SecondFlight);
				SendRings(Context, RingMessage::Ride);
				break;
			case Step::ThirdFlight:
				Context.Particles.StopParticles(TrailEffect);
				for (const int32 Effect : RingEffects)
				{
					Context.Particles.StopParticles(Effect);
				}
				SendMessage(Context, CameraSlot, CameraMessage::ThirdView);
				SendMessage(Context, ShotSlot, ShotMessage::ThirdFlight);
				SendRings(Context, RingMessage::Ride);
				break;
			case Step::LastFlight:
				SendMessage(Context, CameraSlot, CameraMessage::LastView);
				SendMessage(Context, ShotSlot, ShotMessage::LastFlight);
				SendRings(Context, RingMessage::Ride);
				break;
			case Step::Chase:
				SendMessage(Context, CameraSlot, CameraMessage::Chase);
				break;
			case Step::WhiteOut:
				SendFade(Context, DirectorFade::Out, DirectorFade::White, 0x40);
				break;
			case Step::Dive:
				SendMessage(Context, ShotSlot, ShotMessage::Dive);
				SendRings(Context, RingMessage::Shrink);
				SendFade(Context, DirectorFade::In, DirectorFade::White, 0x40);
				break;
			case Step::FollowDive:
				SendMessage(Context, CameraSlot, CameraMessage::FollowDive);
				break;
			case Step::SecondWhiteOut:
				SendFade(Context, DirectorFade::Out, DirectorFade::White, 0x80);
				break;
			case Step::LightOnGround:
				SendFade(Context, DirectorFade::In, DirectorFade::White, 0x80);
				SendMessage(Context, LightRingSlot, RingMessage::Wake);
				SendMessage(Context, CameraSlot, CameraMessage::Quake);
				break;
			case Step::GrazeFlight:
				SendMessage(Context, CameraSlot, CameraMessage::LastView);
				SendMessage(Context, ShotSlot, ShotMessage::GrazeFlight);
				SendRings(Context, RingMessage::Ride);
				break;
			case Step::GrazeChase:
				SendMessage(Context, CameraSlot, CameraMessage::GrazeChase);
				break;
			case Step::GrazeView:
				SendMessage(Context, CameraSlot, CameraMessage::GrazeView);
				break;
			case Step::PassFlight:
				SendMessage(Context, CameraSlot, CameraMessage::LastView);
				SendMessage(Context, ShotSlot, ShotMessage::PassFlight);
				SendRings(Context, RingMessage::Ride);
				break;
			case Step::PassChase:
				SendMessage(Context, CameraSlot, CameraMessage::PassChase);
				break;
			case Step::EntranceSounds:
				for (const uint16 Sound : Shot->Sounds)
				{
					PlayCutsceneEffect(Context, Sound);
				}
				break;
			case Step::LaunchSound:
				PlaySounds(Context, LaunchSounds);
				break;
			case Step::FadeToBlack:
				SendFade(Context, DirectorFade::Out, DirectorFade::Black, 4);
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

		/** The camera's steps (+20h). */
		namespace CameraStep
		{
			constexpr int16 Still = 0;
			constexpr int16 Chasing = 1;
			constexpr int16 Shaken = 2;
			constexpr int16 FollowingDive = 3;
			constexpr int16 TurningAbout = 4;
			constexpr int16 Quaking = 5;
			constexpr int16 GrazeChasing = 0x10;
			constexpr int16 GrazeShaken = 0x11;
			constexpr int16 PassChasing = 0x18;
			constexpr int16 LookingBack = 0x19;
		}
		/** The camera's words: its pitch's rate in a chase and its turn's as it follows the dive (+5Ch, 20.12), its turn back's rate (+60h). */
		constexpr int32 CameraUnused = 2;
		constexpr int32 CameraRate = 3;
		constexpr int32 CameraTurnBack = 4;
		/** The chase's step a tick, and the dive's. */
		constexpr int32 ChaseStepX = -0x42D00;
		constexpr int32 ChaseStepZ = 0x6D300;
		constexpr int32 DiveStepX = -0x4C400;
		constexpr int32 DiveStepY = 0xFF80;
		constexpr int32 DiveStepZ = -0x65580;
		/** Where the pass's chase stops. */
		constexpr int32 PassStopX = 0x2152000;
		constexpr int32 PassStopZ = 0x5AA3000;
		constexpr int16 ChasePitchLimit = 0x80;

		FResult InitCamera(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			InitDirectedCamera(Context, Task);
			Task.Word(CameraUnused) = 0x40;
			Task.Word(0) = 0;
			return WorldMapTaskState::Update;
		}

		void CutTo(FWorldMapContext& Context, FWorldMapTask& Task, int32 Distance, int16 Pitch, int16 Yaw)
		{
			Context.CameraDistance = Distance;
			Task.Message = 0;
			Context.CameraPitch = Pitch;
			Context.CameraYaw = Yaw;
			Context.CameraRoll = 0;
		}

		void StartChase(FWorldMapTask& Task, int16 Step)
		{
			Task.Step = Step;
			Task.Message = 0;
			Task.Word(CameraRate) = -0x8000;
		}

		/** A chase's tick: the target a step on unless it has come to (StopX, StopZ); whether it has. */
		bool Chase(FWorldMapContext& Context, int32 StopX, int32 StopZ)
		{
			FWorldMapVector& Target = Context.CameraTarget;
			if (Target.X + ChaseStepX < StopX && StopZ < Target.Z + ChaseStepZ)
			{
				Target.X = StopX;
				Target.Z = StopZ;
				return true;
			}
			Target.X += ChaseStepX;
			Target.Z += ChaseStepZ;
			return false;
		}

		/** What every tick of a chase ends with: the camera turning, tilting ever faster up to its limit, and drawing back. */
		void TurnInChase(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			Context.Ground.Wrap(Task.Position);
			Context.CameraYaw = static_cast<int16>(Context.CameraYaw - 8);
			Context.CameraPitch = static_cast<int16>(Context.CameraPitch + static_cast<int16>(Task.Word(CameraRate) >> 12));
			Task.Word(CameraRate) += 0x300;
			Context.CameraDistance += 0x8000;
			if (Context.CameraPitch > ChasePitchLimit)
			{
				Context.CameraPitch = ChasePitchLimit;
			}
		}

		void CalmShake(FWorldMapTask& Task, int32 Step)
		{
			Task.Word(CameraShakeReach) -= Step;
			if (Task.Word(CameraShakeReach) < CameraShakeStill)
			{
				Task.Word(CameraShakeReach) = CameraShakeStill;
			}
		}

		FResult UpdateCamera(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			switch (Task.Message)
			{
			case CameraMessage::FirstView:
				CutTo(Context, Task, 0x500000, 0x150, 0xF50);
				break;
			case CameraMessage::SecondView:
				CutTo(Context, Task, 0x4D0000, 0xC60, 0x380);
				break;
			case CameraMessage::ThirdView:
				CutTo(Context, Task, 0x400000, 0x30, 0x160);
				break;
			case CameraMessage::LastView:
				CutTo(Context, Task, 0x400000, 0x30, 0x960);
				break;
			case CameraMessage::Chase:
				StartChase(Task, CameraStep::Chasing);
				break;
			case CameraMessage::FollowDive:
				Task.Step = CameraStep::FollowingDive;
				Task.Word(CameraShakeReach) = CameraShakeStill;
				CutTo(Context, Task, 0x200000, -0x58, 0xE58);
				break;
			case CameraMessage::Quake:
				Task.Step = CameraStep::Quaking;
				Task.Message = 0;
				Task.Word(CameraShakeReach) = 0x40000;
				break;
			case CameraMessage::GrazeChase:
				StartChase(Task, CameraStep::GrazeChasing);
				break;
			case CameraMessage::GrazeView:
				Task.Step = CameraStep::GrazeChasing;
				Task.Word(CameraShakeReach) = CameraShakeStill;
				CutTo(Context, Task, 0x400000, 0xE0, 0x798);
				break;
			case CameraMessage::PassChase:
				StartChase(Task, CameraStep::PassChasing);
				break;
			default:
				break;
			}
			if (Context.LookAt == 0)
			{
				// The task's own place, which nothing sets: the target itself is never wrapped.
				Context.Ground.Wrap(Task.Position);
				SetCameraOrbit(Context, Context.CameraDistance, {Context.CameraPitch, Context.CameraYaw, Context.CameraRoll});
			}
			switch (Task.Step)
			{
			case CameraStep::Chasing:
				if (Chase(Context, HitX, HitZ))
				{
					Task.Step = CameraStep::Shaken;
					Task.Word(CameraShakeReach) = 0x10000;
				}
				TurnInChase(Context, Task);
				break;
			case CameraStep::Shaken:
				CalmShake(Task, 0x80);
				break;
			case CameraStep::FollowingDive:
			{
				FWorldMapVector& Target = Context.CameraTarget;
				Target.X += DiveStepX;
				Target.Y += DiveStepY;
				Target.Z += DiveStepZ;
				if (Target.X < GroundX && Target.Z < GroundZ)
				{
					Target.X = GroundX;
					Target.Z = GroundZ;
					Task.Step = CameraStep::TurningAbout;
					Task.Word(CameraRate) = 0x20000;
				}
				Context.Ground.Wrap(Task.Position);
				Context.CameraDistance += 0x20000;
				Context.CameraYaw = static_cast<int16>(Context.CameraYaw + 0x20);
				Context.CameraPitch = static_cast<int16>(Context.CameraPitch - 4);
				break;
			}
			case CameraStep::TurningAbout:
				Context.CameraYaw = static_cast<int16>(Context.CameraYaw + static_cast<int16>(Task.Word(CameraRate) >> 12));
				Task.Word(CameraRate) -= 0x800;
				if (Task.Word(CameraRate) < 0)
				{
					Task.Word(CameraRate) = 0;
					Task.Step = CameraStep::Still;
				}
				break;
			case CameraStep::Quaking:
				Task.Word(CameraShakeReach) -= 0x2000;
				if (Task.Word(CameraShakeReach) < CameraShakeStill)
				{
					Task.Word(CameraShakeReach) = CameraShakeStill;
					Task.Step = CameraStep::Still;
				}
				break;
			case CameraStep::GrazeChasing:
				if (Chase(Context, HitX, HitZ))
				{
					Task.Step = CameraStep::GrazeShaken;
					Task.Word(CameraShakeReach) = 0x8000;
				}
				TurnInChase(Context, Task);
				break;
			case CameraStep::GrazeShaken:
				CalmShake(Task, 0x200);
				break;
			case CameraStep::PassChasing:
				if (Chase(Context, PassStopX, PassStopZ))
				{
					Task.Step = CameraStep::LookingBack;
					Task.Word(CameraTurnBack) = 0x10000;
				}
				TurnInChase(Context, Task);
				break;
			case CameraStep::LookingBack:
				Context.CameraYaw = static_cast<int16>(Context.CameraYaw - static_cast<int16>(Task.Word(CameraTurnBack) >> 12));
				Task.Word(CameraTurnBack) -= 0x200;
				if (Task.Word(CameraTurnBack) < 0)
				{
					Task.Word(CameraTurnBack) = 0;
				}
				break;
			default:
				break;
			}
			ShakeCamera(Context, Task);
			return WorldMapTaskState::Update;
		}

		constexpr FCutsceneRoutine Routines[] = {
			{R::InitScene15Director, InitDirector},
			{R::UpdateScene15Director, UpdateDirector},
			{R::InitScene15Camera, InitCamera},
			{R::UpdateScene15Camera, UpdateCamera},
			{R::InitScene15Shot, InitShot},
			{R::UpdateScene15Shot, UpdateShot},
			{R::InitScene15Ring, InitRing},
			{R::UpdateScene15Ring, UpdateRing},
			{R::InitScene15LightRing, InitLightRing},
			{R::UpdateScene15LightRing, UpdateLightRing},
		};

		void Start(FWorldMapContext& Context)
		{
			const FWorldMapShotEntrance* Shot = Entrance(Context);
			if (Shot == nullptr)
			{
				return;
			}
			StopFieldSong(Context);
			StartCutsceneWorld(Context,
				{static_cast<int32>(Shot->Start[0]) << 12, static_cast<int32>(Shot->Start[1]) << 12, static_cast<int32>(Shot->Start[2]) << 12, 0});
			StartCutsceneSong(Context);
			AddTask(Context, R::InitFade, R::UpdateFade);
			AddTask(Context, R::InitScene15Director, R::UpdateScene15Director);
			AddTask(Context, R::InitScene15Camera, R::UpdateScene15Camera);
			AddTask(Context, R::InitScene15Shot, R::UpdateScene15Shot);
			for (int32 Slot = FirstRingSlot; Slot <= LastRingSlot; ++Slot)
			{
				AddTask(Context, R::InitScene15Ring, R::UpdateScene15Ring);
			}
			AddTask(Context, R::InitScene15LightRing, R::UpdateScene15LightRing);
			AddTask(Context, R::InitCutsceneFrame, R::UpdateCutsceneFrame);
			ResetEncounters(Context);
		}

		void End(FWorldMapContext& Context)
		{
			SetSongVolume(Context, 0, WorldMapSongFadeTicks);
			StopCutsceneEffects(Context);
			const FWorldMapShotEntrance* Shot = Entrance(Context);
			EndCutscene(Context, FieldMap, Shot != nullptr ? Shot->FieldEntrance : uint16{0});
		}
	}

	/**
	 * A shot crosses the sea, seen three times as it passes; then, by the entrance, it strikes a structure and dives into
	 * the ground, flies past it, or grazes it and flies on.
	 */
	const FCutscene Scene15Cutscene{{15, 15}, 2, 0x10, Scene15::Start, Scene15::End, Scene15::Routines};
}
