#include "WorldMap/WorldMapCutsceneScript.h"

#include <optional>
#include <string>

namespace Weltall::WorldMapOps
{
	namespace Scene18
	{
		using R = EWorldMapRoutine;

		constexpr FWorldMapVector SeaPlace{0x1800000, static_cast<int32>(0xFFF00000), 0x1A00000, 0};
		constexpr uint16 FieldMap = 0x269;
		constexpr uint16 FieldEntrance = 4;
		/** Mahanon's two models. */
		constexpr std::size_t MahanonModels[] = {8, 7};
		constexpr std::size_t ModelCount = 9;

		FWorldMapTask& TaskAt(FWorldMapContext& Context, int32 Slot)
		{
			return Context.Tasks[static_cast<std::size_t>(Slot)];
		}

		FResult InitScript(FWorldMapContext& Context, int32 Slot)
		{
			TaskAt(Context, Slot).Word(ScriptAt) = 0;
			return WorldMapTaskState::Update;
		}

		/** The camera's steps (+20h) that ease: toward straight down over the sea, and tilting up as Mahanon rises. */
		namespace CameraStep
		{
			constexpr int16 LookDown = 1;
			constexpr int16 TiltUp = 5;
		}

		/**
		 * A message of the camera's, each a cut: the step it leaves it in, its distance and angles, and the parts of
		 * the place it shows that it moves (the target moves at once, unlike scene 17's).
		 */
		struct FCut
		{
			int16 Message;
			int16 Step;
			int32 Distance;
			int16 Pitch;
			int16 Yaw;
			std::optional<int32> X;
			std::optional<int32> Y;
			std::optional<int32> Z;
		};

		constexpr FCut Cuts[] = {
			{1, CameraStep::LookDown, 0x960000, -0x20, 0x400, {}, {}, {}},
			{2, 2, 0x180000, 0x40, 0x1A0, {}, static_cast<int32>(0xFFF60000), {}},
			{3, 3, 0x120000, -0x2A0, 0x6A0, 0x1700000, {}, {}},
			{4, 4, 0x180000, -0x10, 0xA50, 0x1800000, static_cast<int32>(0xFFF80000), 0x1900000},
			{5, CameraStep::TiltUp, 0x260000, -0x100, 0x670, {}, static_cast<int32>(0xFFEF0000), 0x1A80000},
			{6, 6, 0x620000, -0x80, 0xA70, {}, static_cast<int32>(0xFFEE0000), 0x1980000},
		};

		FResult InitCamera(FWorldMapContext& Context, int32 Slot)
		{
			InitScriptCamera(Context, TaskAt(Context, Slot), 0x960000, -0x20, 0x400);
			return WorldMapTaskState::Update;
		}

		void MovePlace(FWorldMapContext& Context, FWorldMapTask& Task, int32 FWorldMapVector::* Axis, const std::optional<int32>& Value)
		{
			if (Value)
			{
				Context.CameraTarget.*Axis = Context.Controlled.*Axis = Task.Position.*Axis = *Value;
			}
		}

		FResult UpdateCamera(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			for (const FCut& Cut : Cuts)
			{
				if (Cut.Message != Task.Message)
				{
					continue;
				}
				Task.Message = 0;
				Task.Step = Cut.Step;
				CutScriptCamera(Context, Task, Cut.Distance, Cut.Pitch, Cut.Yaw);
				MovePlace(Context, Task, &FWorldMapVector::X, Cut.X);
				MovePlace(Context, Task, &FWorldMapVector::Y, Cut.Y);
				MovePlace(Context, Task, &FWorldMapVector::Z, Cut.Z);
				break;
			}
			if (Context.LookAt == 0)
			{
				SetCameraOrbit(Context, Context.CameraDistance, {Context.CameraPitch, Context.CameraYaw, Context.CameraRoll});
			}
			int32& Pitch = Task.Word(ScriptCameraWord::GoalPitch);
			if (Task.Step == CameraStep::LookDown)
			{
				int32& Yaw = Task.Word(ScriptCameraWord::GoalYaw);
				int32& Distance = Task.Word(ScriptCameraWord::GoalDistance);
				Pitch = StepTowards(Pitch, -0x400000, -0x8000);
				Yaw = StepTowards(Yaw, 0, -0x8000);
				Distance = StepTowards(Distance, 0x380000, -0xD000);
			}
			else if (Task.Step == CameraStep::TiltUp)
			{
				Pitch = StepTowards(Pitch, 0x200000, 0x2000);
			}
			FollowScriptCamera(Context, Task);
			return WorldMapTaskState::Update;
		}

		/** Mahanon's steps (+20h): under the sea, then the four stages of its rise, each with its own effects about it. */
		constexpr int16 Sunk = 0;
		constexpr int32 SunkHeight = 0x80000;
		constexpr int32 RisenHeight = -0x180000;
		constexpr int32 Rise = -0x400;

		/** A stage: the effects it stops as it begins, those it keeps about Mahanon, and those it keeps at the sea's level below it. */
		struct FStage
		{
			std::span<const int32> Stopped;
			std::span<const int32> Following;
			std::span<const int32> AtSeaLevel;
		};

		constexpr int32 FirstEffects[] = {3, 5, 6, 7, 10};
		constexpr int32 SecondEffects[] = {7, 8, 9};
		constexpr int32 ThirdEffects[] = {5, 6, 7, 10, 11, 12};
		constexpr int32 LastEffects[] = {13};
		constexpr int32 SeaEffects[] = {14};
		constexpr FStage Stages[] = {
			{{}, FirstEffects, {}},
			{FirstEffects, SecondEffects, {}},
			{SecondEffects, ThirdEffects, {}},
			{ThirdEffects, LastEffects, SeaEffects},
		};

		bool HasModels(FWorldMapContext& Context)
		{
			if (Context.Objects.size() < ModelCount)
			{
				Context.Fault = MakeError("scene 18 has " + std::to_string(Context.Objects.size()) + " models where Mahanon is models 7 and 8");
				return false;
			}
			return true;
		}

		void PlaceMahanon(FWorldMapContext& Context, const FWorldMapTask& Task)
		{
			for (const std::size_t Model : MahanonModels)
			{
				FWorldMapObject& Object = Context.Objects[Model];
				Object.X = Task.Position.X >> 12;
				Object.Y = Task.Position.Y >> 12;
				Object.Z = Task.Position.Z >> 12;
			}
		}

		FResult InitMahanon(FWorldMapContext& Context, int32 Slot)
		{
			if (!HasModels(Context))
			{
				return WorldMapTaskState::Init;
			}
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Position.X = SeaPlace.X;
			Task.Position.Y = SunkHeight;
			Task.Position.Z = SeaPlace.Z;
			Task.Step = Sunk;
			for (const std::size_t Model : MahanonModels)
			{
				Context.Objects[Model].Hidden = 1;
			}
			PlaceMahanon(Context, Task);
			return WorldMapTaskState::Update;
		}

		FResult UpdateMahanon(FWorldMapContext& Context, int32 Slot)
		{
			if (!HasModels(Context))
			{
				return WorldMapTaskState::Init;
			}
			FWorldMapTask& Task = TaskAt(Context, Slot);
			const int16 Message = Task.Message;
			if (Message >= 1 && Message <= static_cast<int16>(std::size(Stages)))
			{
				Task.Message = 0;
				Task.Step = Message;
				if (Message == 1)
				{
					for (const std::size_t Model : MahanonModels)
					{
						Context.Objects[Model].Hidden = 0;
					}
				}
				for (const int32 Effect : Stages[static_cast<std::size_t>(Message - 1)].Stopped)
				{
					Context.Particles.Stop(Effect);
				}
			}
			if (Task.Step >= 1 && Task.Step <= static_cast<int16>(std::size(Stages)))
			{
				const FStage& Stage = Stages[static_cast<std::size_t>(Task.Step - 1)];
				Task.Position.Y = StepTowards(Task.Position.Y, RisenHeight, Rise);
				// Starting an effect that runs only moves it: the effects follow Mahanon up.
				std::array<int16, 4> At{static_cast<int16>(Task.Position.X >> 12), static_cast<int16>(Task.Position.Y >> 12),
					static_cast<int16>(Task.Position.Z >> 12), 0};
				for (const int32 Effect : Stage.Following)
				{
					Context.Particles.Start(Effect, At, std::nullopt);
				}
				At[1] = 0;
				for (const int32 Effect : Stage.AtSeaLevel)
				{
					Context.Particles.Start(Effect, At, std::nullopt);
				}
			}
			PlaceMahanon(Context, Task);
			return WorldMapTaskState::Update;
		}

		constexpr FCutsceneRoutine Routines[] = {
			{R::InitScene18Script, InitScript},
			{R::InitScene18Camera, InitCamera},
			{R::UpdateScene18Camera, UpdateCamera},
			{R::InitMahanon, InitMahanon},
			{R::UpdateMahanon, UpdateMahanon},
		};

		void Start(FWorldMapContext& Context)
		{
			StartCutsceneWorld(Context, SeaPlace);
			RegisterCutsceneEffects(Context);
			if (Context.Options.bScene18EffectsWait)
			{
				// The file marks effects as running from the load; they start when Mahanon's stages start them.
				Context.Particles.StopAll();
			}
			AddTask(Context, R::InitFade, R::UpdateFade);
			AddTask(Context, R::InitScene18Script, R::RunScript);
			AddTask(Context, R::InitScene18Camera, R::UpdateScene18Camera);
			AddTask(Context, R::InitMahanon, R::UpdateMahanon);
			AddTask(Context, R::InitCutsceneFrame, R::UpdateCutsceneFrame);
			ResetEncounters(Context);
		}

		void End(FWorldMapContext& Context)
		{
			StopCutsceneEffects(Context);
			EndCutscene(Context, FieldMap, FieldEntrance);
		}
	}

	/** The sea churns and Mahanon rises out of it, on a script. */
	const FCutscene Scene18Cutscene{{18, 18}, 2, 0x10, Scene18::Start, Scene18::End, Scene18::Routines};
}
