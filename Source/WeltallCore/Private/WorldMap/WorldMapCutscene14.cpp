#include "WorldMap/WorldMapCutscenes.h"

#include "Game/PsxRandom.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteMatrix.h"
#include "Graphics/GteVectors.h"

#include <algorithm>

namespace Weltall::WorldMapOps
{
	namespace Scene14
	{
		using R = EWorldMapRoutine;

		constexpr FWorldMapVector BlastPlace{0x5BED000, static_cast<int32>(0xFFF60000), 0x62A8000, 0};
		constexpr uint16 FieldMap = 0x11A;
		constexpr uint16 FieldEntrance = 0;
		constexpr int16 FadeOutMessage = 0xD;
		constexpr int16 WakeMessage = 1;
		constexpr int32 ModelCount = 12;
		/** The tasks' slots, as the start adds them. */
		constexpr int32 CameraSlot = 2;
		constexpr int32 InnerRingsSlot = 3;
		constexpr int32 OuterRingsSlot = 4;
		constexpr int32 StreakSlot = 5;
		constexpr int32 FlyerSlot = 6;
		/** The scene's sound effects: the first burst, the blast, the streak, the view back, and two that last. */
		constexpr int32 BurstSound = 1;
		constexpr int32 BlastSounds[] = {4, 5, 6};
		constexpr int32 StreakSounds[] = {7, 8, 9};
		constexpr int32 ReturnSound = 0xA;
		constexpr int32 LastingSounds[] = {0xB, 0xC};
		constexpr int32 SweepSound = 0xB;
		/** The particle effects: the first burst and the blast's two (their places their own), the streak, the flyer's spray. */
		constexpr int32 BurstEffect = 0x11;
		constexpr int32 BlastEffects[] = {0xF, 0x10};
		constexpr int32 StreakEffect = 9;
		constexpr int32 SprayEffect = 0x12;

		FWorldMapTask& TaskAt(FWorldMapContext& Context, int32 Slot)
		{
			return Context.Tasks[static_cast<std::size_t>(Slot)];
		}

		FWorldMapObject& ObjectAt(FWorldMapContext& Context, int32 Number)
		{
			return Context.Objects[static_cast<std::size_t>(Number)];
		}

		/** The camera's messages. */
		namespace CameraMessage
		{
			constexpr int16 Tremor = 1;
			constexpr int16 Blast = 2;
			constexpr int16 FarView = 3;
			constexpr int16 NearView = 4;
			constexpr int16 Settle = 5;
			constexpr int16 FlyBy = 6;
			constexpr int16 Turn = 7;
		}

		/** The director's steps (+20h), each followed by the wait before the next; its timer is the wait, its first word the next step's index. */
		namespace DirectorStep
		{
			constexpr int16 Waiting = 1;
			constexpr int16 Burst = 2;
			constexpr int16 Blast = 3;
			constexpr int16 FarView = 4;
			constexpr int16 Streak = 5;
			constexpr int16 NearView = 6;
			constexpr int16 Settle = 7;
			constexpr int16 Tremor = 8;
			constexpr int16 Flyer = 9;
			constexpr int16 FadeOut = 10;
			constexpr int16 Leave = 11;
		}
		constexpr int16 Steps[] = {DirectorStep::Waiting, DirectorStep::Burst, DirectorStep::Blast, DirectorStep::Tremor, DirectorStep::FarView,
			DirectorStep::Streak, DirectorStep::NearView, DirectorStep::Tremor, DirectorStep::Flyer, DirectorStep::Settle, DirectorStep::FadeOut,
			DirectorStep::Leave};
		constexpr int16 Waits[] = {30, 160, 8, 30, 20, 60, 8, 30, 120, 76, 72, 1};

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
				Context.Fault = MakeError("scene 14 has " + std::to_string(Context.Objects.size()) + " models where it moves 12");
				return WorldMapTaskState::Init;
			}
			// The index stays on the first entry, a wait: it is taken twice.
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Word(0) = 0;
			Task.Step = Steps[0];
			Task.Timer = Waits[0];
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
			case DirectorStep::Burst:
				Context.Particles.Start(BurstEffect, std::nullopt, std::nullopt);
				PlayCutsceneEffect(Context, BurstSound);
				break;
			case DirectorStep::Blast:
				SendMessage(Context, CameraSlot, CameraMessage::Blast);
				for (const int32 Effect : BlastEffects)
				{
					Context.Particles.Start(Effect, std::nullopt, std::nullopt);
				}
				PlaySounds(Context, BlastSounds);
				break;
			case DirectorStep::FarView:
				SendMessage(Context, CameraSlot, CameraMessage::FarView);
				SendMessage(Context, InnerRingsSlot, WakeMessage);
				break;
			case DirectorStep::Streak:
				SendMessage(Context, CameraSlot, CameraMessage::FlyBy);
				SendMessage(Context, OuterRingsSlot, WakeMessage);
				SendMessage(Context, StreakSlot, WakeMessage);
				PlaySounds(Context, StreakSounds);
				break;
			case DirectorStep::NearView:
				SendMessage(Context, CameraSlot, CameraMessage::NearView);
				PlayCutsceneEffect(Context, ReturnSound);
				for (const int32 Sound : LastingSounds)
				{
					// The sweep on a pair of its own, or (the original) on the tone's, which takes it at once.
					if (Sound == SweepSound && Context.Options.bScene14SweepHeard)
					{
						PlayCutsceneEffect(Context, Sound);
						continue;
					}
					PlayProtectedCutsceneEffect(Context, Sound);
				}
				break;
			case DirectorStep::Settle:
				SendMessage(Context, CameraSlot, CameraMessage::Settle);
				break;
			case DirectorStep::Tremor:
				SendMessage(Context, CameraSlot, CameraMessage::Tremor);
				break;
			case DirectorStep::Flyer:
				SendMessage(Context, FlyerSlot, WakeMessage);
				SendMessage(Context, CameraSlot, CameraMessage::Turn);
				break;
			case DirectorStep::FadeOut:
				SendMessage(Context, FWorldMapContext::FadeSlot, FadeOutMessage);
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

		/** The camera's steps (+20h) and words: how far it shakes (+50h, 20.12), and where the fly-by's eye stands (+58h, +5Ch: x and z, 20.12). */
		namespace CameraStep
		{
			constexpr int16 Steady = 0;
			constexpr int16 TremorRising = 1;
			constexpr int16 BlastFading = 2;
			constexpr int16 Settling = 3;
			constexpr int16 FlyBy = 4;
			constexpr int16 Turning = 5;
		}
		constexpr int32 ShakeSpread = 0;
		constexpr int32 EyeX = 2;
		constexpr int32 EyeZ = 3;
		constexpr int32 Still = 0x1000;
		constexpr int32 Tremor = 0x8000;
		constexpr int32 BlastSpread = 0x40000;
		/** The fly-by: the target runs south 58 units a tick, watched from 64 units above its height. */
		constexpr int32 FlyByStep = 0x3A000;
		constexpr int32 FlyByEnd = 0x77FFFFF;
		constexpr int32 FlyByEyeX = 0xC7C00;
		constexpr int32 FlyByEyeZ = 0x3EC400;
		constexpr int16 FlyByEyeHeight = 0x40;
		constexpr int16 TurnEnd = 0x600;

		FResult InitCamera(FWorldMapContext& Context, int32 Slot)
		{
			Context.ScreenCentreY = 0x78;
			Context.CameraDistance = 0x1C0000;
			Context.CameraPitch = 0x40;
			Context.CameraYaw = 0x480;
			Context.CameraRoll = 0;
			Context.CameraTarget.X = Context.Start.X;
			Context.CameraTarget.Y = Context.Start.Y;
			Context.CameraTarget.Z = Context.Start.Z;
			Context.Controlled.X = Context.Start.X;
			Context.Controlled.Y = Context.Start.Y;
			Context.Controlled.Z = Context.Start.Z;
			TaskAt(Context, Slot).Word(ShakeSpread) = Still;
			return WorldMapTaskState::Update;
		}

		void TakeCameraMessage(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			switch (Task.Message)
			{
			case CameraMessage::Tremor:
				Task.Step = CameraStep::TremorRising;
				break;
			case CameraMessage::Blast:
				Task.Step = CameraStep::BlastFading;
				Task.Word(ShakeSpread) = BlastSpread;
				break;
			case CameraMessage::FarView:
				Context.CameraDistance = 0x640000;
				Task.Step = CameraStep::Steady;
				Context.CameraPitch = -0x1E0;
				Context.CameraYaw = 0x480;
				Context.CameraRoll = 0;
				break;
			case CameraMessage::NearView:
				Task.Step = CameraStep::Steady;
				Context.CameraDistance = 0x1E0000;
				Context.CameraPitch = 0x40;
				Context.CameraYaw = 0x418;
				Context.CameraRoll = 0;
				Context.LookAt = 0;
				Context.CameraTarget.Z = Context.Start.Z;
				break;
			case CameraMessage::Settle:
				Task.Step = CameraStep::Settling;
				break;
			case CameraMessage::FlyBy:
				Task.Step = CameraStep::FlyBy;
				Task.Word(EyeX) = Context.Start.X + FlyByEyeX;
				Context.LookAt = 1;
				Task.Word(EyeZ) = Context.Start.Z + FlyByEyeZ;
				break;
			case CameraMessage::Turn:
				Task.Step = CameraStep::Turning;
				break;
			default:
				return;
			}
			Task.Message = 0;
		}

		/** The target flown south past a standing eye, which looks at it; the up vector is the orbit's last. */
		void FlyBy(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			Context.CameraTarget.Z += FlyByStep;
			if (Context.CameraTarget.Z > FlyByEnd)
			{
				Task.Step = CameraStep::Steady;
			}
			FWorldMapOrbit& Orbit = Context.Orbit;
			const int16 Height = static_cast<int16>(Context.CameraTarget.Y >> 12);
			Orbit.Target = {0, Height, 0};
			Orbit.Eye = {static_cast<int16>((Task.Word(EyeX) - Context.CameraTarget.X) >> 12), static_cast<int16>(Height - FlyByEyeHeight),
				static_cast<int16>((Context.CameraTarget.Z - Task.Word(EyeZ)) >> 12)};
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
			TakeCameraMessage(Context, Task);
			int32& Spread = Task.Word(ShakeSpread);
			bool bOrbit = true;
			switch (Task.Step)
			{
			case CameraStep::Steady:
				break;
			case CameraStep::TremorRising:
				Spread += 0x200;
				if (Spread > Tremor)
				{
					Spread = Tremor;
					Task.Step = CameraStep::Steady;
				}
				break;
			case CameraStep::BlastFading:
				Spread -= 0x200;
				if (Spread < Tremor)
				{
					Spread = Tremor;
					Task.Step = CameraStep::Steady;
				}
				break;
			case CameraStep::Settling:
				Spread -= 0x100;
				if (Spread < Still)
				{
					Spread = Still;
					Task.Step = CameraStep::Steady;
				}
				break;
			case CameraStep::FlyBy:
				FlyBy(Context, Task);
				bOrbit = false;
				break;
			case CameraStep::Turning:
				Context.CameraYaw = static_cast<int16>(Context.CameraYaw + 4);
				if (Context.CameraYaw > TurnEnd)
				{
					Task.Step = CameraStep::Steady;
				}
				break;
			default:
				bOrbit = false;
				break;
			}
			if (bOrbit)
			{
				SetCameraOrbit(Context, Context.CameraDistance, {Context.CameraPitch, Context.CameraYaw, Context.CameraRoll});
			}
			const int32 Range = Spread >> 12;
			if (Range == 0)
			{
				Context.Fault = MakeError("scene 14's camera shakes by nothing");
				return Task.State;
			}
			const int16 Half = static_cast<int16>(Spread >> 13);
			const int16 X = static_cast<int16>(static_cast<int16>(Context.Random->Next() % Range) - Half);
			const int16 Y = static_cast<int16>(static_cast<int16>(Context.Random->Next() % Range) - Half);
			FWorldMapOrbit& Orbit = Context.Orbit;
			Orbit.Eye[0] = static_cast<int16>(Orbit.Eye[0] + X);
			Orbit.Target[0] = static_cast<int16>(Orbit.Target[0] + X);
			Orbit.Eye[1] = static_cast<int16>(Orbit.Eye[1] + Y);
			Orbit.Target[1] = static_cast<int16>(Orbit.Target[1] + Y);
			return WorldMapTaskState::Update;
		}

		/** A pair of rings: its first model (the second follows it) and their scales across (+50h, +54h, 4.12). */
		constexpr int32 InnerRings = 4;
		constexpr int32 OuterRings = 6;
		constexpr int32 RingScale = 0;
		constexpr int32 SecondRingScale = 1;
		constexpr int32 RingGrowth = 0x180;
		constexpr int32 RingFull = 0x7F00;
		constexpr int32 RingHeight = -0x40;

		FGteMatrix RingMatrix(int32 Scale)
		{
			return ScaleColumns(FGteMatrix::Identity(), {Scale, FGteMatrix::One, Scale});
		}

		FResult InitRings(FWorldMapContext& Context, int32 Slot, int32 First)
		{
			for (const int32 Ring : {First, First + 1})
			{
				SetGlowLook(ObjectAt(Context, Ring));
				ObjectAt(Context, Ring).bOriginalScreenTests = true;
			}
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Word(SecondRingScale) = 0;
			Task.Word(RingScale) = 0;
			// The program flattens and hides the two models after its pair, not the pair.
			for (const int32 Ring : {First + 2, First + 3})
			{
				FWorldMapObject& Object = ObjectAt(Context, Ring);
				Object.Matrix.M = RingMatrix(Task.Word(RingScale)).M;
				Object.Hidden = 1;
			}
			return WorldMapTaskState::Idle;
		}

		FResult UpdateRings(FWorldMapContext& Context, int32 Slot, int32 First)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			FWorldMapObject& Ring = ObjectAt(Context, First);
			FWorldMapObject& Second = ObjectAt(Context, First + 1);
			if (Task.Message != 0)
			{
				Task.Message = 0;
				Second.Hidden = 0;
				Ring.Hidden = 0;
			}
			for (FWorldMapObject* Object : {&Ring, &Second})
			{
				Object->X = Context.Start.X >> 12;
				Object->Y = RingHeight;
				Object->Z = Context.Start.Z >> 12;
			}
			int32& Scale = Task.Word(RingScale);
			int32& SecondScale = Task.Word(SecondRingScale);
			Scale += RingGrowth;
			if (Scale > 0x800)
			{
				SecondScale += RingGrowth;
			}
			Scale = std::min(Scale, RingFull);
			SecondScale = std::min(SecondScale, RingFull);
			Ring.Matrix.M = RingMatrix(Scale).M;
			Second.Matrix.M = RingMatrix(SecondScale).M;
			return WorldMapTaskState::Update;
		}

		/** The streak: carried from the blast for 59 ticks, turned an eighth of a quarter about y. */
		constexpr int16 StreakTicks = 60;
		constexpr int32 StreakStepX = -0x95D0;
		constexpr int32 StreakStepZ = 0x2F130;
		constexpr FGteVector StreakTurn{0, 0x80, 0};

		FResult UpdateStreak(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			if (Task.Message == WakeMessage)
			{
				Task.Message = 0;
				Task.Timer = StreakTicks;
				Task.Position.X = Context.Start.X;
				Task.Position.Y = Context.Start.Y;
				Task.Position.Z = Context.Start.Z;
			}
			Task.Timer = static_cast<int16>(Task.Timer - 1);
			if (Task.Timer < 1)
			{
				Task.Timer = StreakTicks;
				Task.Position = Context.Start;
				Context.Particles.Stop(StreakEffect);
				return WorldMapTaskState::Idle;
			}
			Task.Position.X += StreakStepX;
			Task.Position.Z += StreakStepZ;
			const std::array<int16, 4> Place{static_cast<int16>(Task.Position.X >> 12), static_cast<int16>(Task.Position.Y >> 12),
				static_cast<int16>(Task.Position.Z >> 12), 0};
			Context.Particles.Start(StreakEffect, Place, StreakTurn);
			return WorldMapTaskState::Update;
		}

		/** The flyer: its hull and what it carries; its place on its path (+50h, 20.12), its roll (+54h) and its speed a tick (+58h). */
		constexpr int32 Hull = 0;
		constexpr int32 FlyerParts[] = {1, 2, 3};
		constexpr int32 PathPlace = 0;
		constexpr int32 Roll = 1;
		constexpr int32 PathSpeed = 2;
		constexpr int32 FullSpeed = 0x100;
		constexpr int32 SlowSpeed = 0x80;
		/** How far ahead on the path it looks for its heading. */
		constexpr int32 LookAhead = 0x80;
		/** It throws spray while lower than 128 units up. */
		constexpr int32 SprayCeiling = -0x80;

		FResult InitFlyer(FWorldMapContext& Context, int32 Slot)
		{
			for (const int32 Part : FlyerParts)
			{
				AttachObject(Context, Hull, Part);
			}
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Word(PathPlace) = 0;
			Task.Word(Roll) = 0x100;
			Task.Word(PathSpeed) = FullSpeed;
			FWorldMapObject& Object = ObjectAt(Context, Hull);
			Object.Angles = {0, 0, 0};
			Object.Matrix.M = RotationYxz(Object.Angles).M;
			return WorldMapTaskState::Idle;
		}

		/** The path's point at a place, none where the path the program reads has ended. */
		std::optional<std::array<int32, 3>> PathPoint(FWorldMapContext& Context, int32 Place)
		{
			const FWorldMapPath& Path = Context.Tables->Cutscenes.Scene14Flyer;
			const int32 Segment = Place >> 12;
			if (Segment < 0 || static_cast<std::size_t>(Segment) + 2 >= Path.size())
			{
				Context.Fault = MakeError("scene 14's flyer reads past its path, at segment " + std::to_string(Segment));
				return std::nullopt;
			}
			const std::size_t At = static_cast<std::size_t>(Segment);
			return QuadraticBlend(Place & 0xFFF, Path[At], Path[At + 1], Path[At + 2]);
		}

		FResult UpdateFlyer(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			int32& Place = Task.Word(PathPlace);
			int32& Speed = Task.Word(PathSpeed);
			FResult Result = WorldMapTaskState::Update;
			switch (Place >> 12)
			{
			case 0:
			case 1:
				Place += Speed;
				Task.Word(Roll) += 4;
				break;
			case 2:
			case 3:
			case 4:
				Place += Speed;
				Speed = std::max(Speed - 8, SlowSpeed);
				Task.Word(Roll) += 4;
				break;
			case 5:
			case 6:
			case 7:
				Place += Speed;
				Speed = std::min(Speed + 8, FullSpeed);
				Task.Word(Roll) -= 0x10;
				break;
			case 8:
				Result = WorldMapTaskState::Idle;
				break;
			default:
				break;
			}
			const std::optional<std::array<int32, 3>> Here = PathPoint(Context, Place);
			const std::optional<std::array<int32, 3>> Ahead = PathPoint(Context, Place + LookAhead);
			if (!Here || !Ahead)
			{
				return Task.State;
			}
			FWorldMapObject& Object = ObjectAt(Context, Hull);
			Object.X = static_cast<int16>((*Here)[0] >> 16);
			Object.Y = (*Here)[1] >> 16;
			Object.Z = (*Here)[2] >> 16;
			// Its nose along the path (z as the drawing's), its wings rolled about it, the model's axes squared up from the two.
			const FGteLongVector Forward = VectorNormal({((*Ahead)[0] - (*Here)[0]) >> 12, ((*Ahead)[1] - (*Here)[1]) >> 12, -(((*Ahead)[2] - (*Here)[2]) >> 12)});
			const int16 Yaw = static_cast<int16>(GteRatan2(Forward[0], Forward[2], *Context.Arctangents) & 0xFFF);
			const FGteMatrix Banked = RotationYxz({0, Yaw, static_cast<int16>(Task.Word(Roll))});
			const FGteLongVector Up = ApplyMatrix(Banked, {0, -0x1000, 0});
			const FGteLongVector Right = VectorNormal(OuterProduct12(Forward, Up));
			const FGteLongVector Down = VectorNormal(OuterProduct12(Forward, Right));
			FGteMatrix Axes;
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				Axes.M[0][Column] = static_cast<int16>(Right[Column]);
				Axes.M[1][Column] = static_cast<int16>(Down[Column]);
				Axes.M[2][Column] = static_cast<int16>(Forward[Column]);
			}
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				for (std::size_t Column = 0; Column < 3; ++Column)
				{
					Object.Matrix.M[Row][Column] = Axes.M[Column][Row];
				}
			}
			if (Object.Y > SprayCeiling)
			{
				if (Axes.M[2][0] == 0 && Axes.M[2][2] == 0)
				{
					Context.Fault = MakeError("scene 14's flyer heads straight up or down, where its spray has no heading");
					return Task.State;
				}
				FGteVector Angles{};
				AnglesFromMatrix(Axes, *Context.Arctangents, Angles);
				const std::array<int16, 4> Spot{static_cast<int16>(Object.X), static_cast<int16>(Object.Y), static_cast<int16>(Object.Z), 0};
				Context.Particles.Start(SprayEffect, Spot, FGteVector{Angles[0], Angles[1], static_cast<int16>(-Angles[2])});
			}
			return Result;
		}

		constexpr FCutsceneRoutine Routines[] = {
			{R::InitScene14Director, InitDirector},
			{R::UpdateScene14Director, UpdateDirector},
			{R::InitScene14Camera, InitCamera},
			{R::UpdateScene14Camera, UpdateCamera},
			{R::InitScene14Rings1, [](FWorldMapContext& Context, int32 Slot) { return InitRings(Context, Slot, InnerRings); }},
			{R::UpdateScene14Rings1, [](FWorldMapContext& Context, int32 Slot) { return UpdateRings(Context, Slot, InnerRings); }},
			{R::InitScene14Rings2, [](FWorldMapContext& Context, int32 Slot) { return InitRings(Context, Slot, OuterRings); }},
			{R::UpdateScene14Rings2, [](FWorldMapContext& Context, int32 Slot) { return UpdateRings(Context, Slot, OuterRings); }},
			{R::InitScene14Streak, [](FWorldMapContext&, int32) -> FResult { return WorldMapTaskState::Idle; }},
			{R::UpdateScene14Streak, UpdateStreak},
			{R::InitScene14Flyer, InitFlyer},
			{R::UpdateScene14Flyer, UpdateFlyer},
		};

		void Start(FWorldMapContext& Context)
		{
			StartCutsceneWorld(Context, BlastPlace);
			RegisterCutsceneEffects(Context);
			AddTask(Context, R::InitFade, R::UpdateFade);
			AddTask(Context, R::InitScene14Director, R::UpdateScene14Director);
			AddTask(Context, R::InitScene14Camera, R::UpdateScene14Camera);
			AddTask(Context, R::InitScene14Rings1, R::UpdateScene14Rings1);
			AddTask(Context, R::InitScene14Rings2, R::UpdateScene14Rings2);
			AddTask(Context, R::InitScene14Streak, R::UpdateScene14Streak);
			AddTask(Context, R::InitScene14Flyer, R::UpdateScene14Flyer);
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
	 * A burst, then a blast that shakes the view; seen from afar, glowing rings widen about it and a streak shoots off,
	 * watched flying south; back near it, a flyer swoops past as the tremor dies down.
	 */
	const FCutscene Scene14Cutscene{{14, 14}, 2, 0x10, Scene14::Start, Scene14::End, Scene14::Routines};
}
