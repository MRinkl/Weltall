#include "WorldMap/WorldMapCutsceneScript.h"

#include "Game/PsxRandom.h"

#include <cstdlib>
#include <string>

namespace Weltall::WorldMapOps
{
	namespace
	{
		/** The eases settle once an eighth of what is left is under these (20.12). */
		constexpr int32 AngleSettle = 0x40;
		constexpr int32 DistanceSettle = 0x40;
		constexpr int32 TargetSettle = 0x200;
		constexpr int32 ScreenCentre = 0x78;
		constexpr int32 NoShake = 0x1000;

		int32 Fixed(int16 Units)
		{
			return static_cast<int32>(static_cast<uint32>(static_cast<int32>(Units)) << 12);
		}

		/** An eighth of the way to the goal, or onto it once that step is small. */
		void Ease(int32& Value, int32 Goal, int32 Left, int32 Settle)
		{
			const int32 Step = Left >> 3;
			Value = std::abs(Step) < Settle ? Goal : Value + Step;
		}

		FResult Stop(FWorldMapContext& Context, std::string Why)
		{
			Context.Fault = MakeError(std::move(Why));
			return WorldMapTaskState::Init;
		}
	}

	FResult RunCutsceneScript(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = Context.Tasks[static_cast<std::size_t>(Slot)];
		const FWorldMapScriptTables& Tables = Context.Tables->Cutscenes.Scripts;
		const std::span<const int16> Script = Task.Init == EWorldMapRoutine::InitScene18Script ? Tables.Scene18 : Tables.Scene17;
		// Where the script's effects start: the place an instruction of this run named (the original keeps it in scratch memory).
		std::optional<std::array<int16, 4>> Place;
		for (;;)
		{
			const int32 At = Task.Word(ScriptAt);
			if (At < 0 || At % 2 != 0 || static_cast<std::size_t>(At / 2) >= Script.size())
			{
				return Stop(Context, "a world map cutscene's script ran out of its instructions");
			}
			const std::size_t Index = static_cast<std::size_t>(At / 2);
			const int32 Op = static_cast<uint16>(Script[Index]);
			const int32 Halfwords = WorldMapScriptOp::Halfwords(Op);
			if (Index + static_cast<std::size_t>(Halfwords) > Script.size())
			{
				return Stop(Context, "a world map cutscene's script ends inside an instruction");
			}
			const int16 A = Script[Index + 1];
			const int16 B = Halfwords > 2 ? Script[Index + 2] : int16{0};
			const int16 C = Halfwords > 2 ? Script[Index + 3] : int16{0};
			switch (Op)
			{
			case WorldMapScriptOp::End:
				Context.Running = 0;
				Context.ExitCode = 0;
				return WorldMapTaskState::Update;
			case WorldMapScriptOp::Wait:
				if (Task.Timer == 0)
				{
					Task.Timer = A;
					return WorldMapTaskState::Update;
				}
				Task.Timer = static_cast<int16>(Task.Timer - 1);
				if (Task.Timer > 0)
				{
					return WorldMapTaskState::Update;
				}
				break;
			case WorldMapScriptOp::SendMessage:
				SendMessage(Context, A, B);
				break;
			case WorldMapScriptOp::SetStart:
				Context.Start.X = Fixed(A);
				Context.Start.Y = Fixed(B);
				Context.Start.Z = Fixed(C);
				break;
			case WorldMapScriptOp::SetPlace:
				Place = std::array<int16, 4>{A, B, C, 0};
				break;
			case WorldMapScriptOp::StartEffect:
				if (!Place)
				{
					return Stop(Context, "a world map cutscene's script starts effect " + std::to_string(A) + " at a place it has not named");
				}
				Context.Particles.Start(A, Place, std::nullopt);
				break;
			case WorldMapScriptOp::StopEffect:
				Context.Particles.Stop(A);
				break;
			case WorldMapScriptOp::StopParticles:
				Context.Particles.StopParticles(A);
				break;
			case WorldMapScriptOp::SongVolume:
				SetSongVolume(Context, A, B);
				break;
			case WorldMapScriptOp::PlaySound:
				PlayCutsceneEffect(Context, A);
				break;
			case WorldMapScriptOp::FadeSound:
				FadeCutsceneEffect(Context, A, B, C);
				break;
			case WorldMapScriptOp::SetFade:
				Context.FadeBlend = A;
				Context.FadeStep = B;
				break;
			default:
				return Stop(Context, "a world map cutscene's script has operation " + std::to_string(Op) + ", which the program lacks");
			}
			Task.Word(ScriptAt) = At + 2 * Halfwords;
		}
	}

	void CutScriptCamera(FWorldMapContext& Context, FWorldMapTask& Task, int32 Distance, int16 Pitch, int16 Yaw)
	{
		Task.Word(ScriptCameraWord::GoalDistance) = Distance;
		Context.CameraDistance = Distance;
		Context.CameraPitch = Pitch;
		Context.CameraYaw = Yaw;
		Context.CameraRoll = 0;
		Task.Direction.X = Task.Word(ScriptCameraWord::GoalPitch) = Fixed(Pitch);
		Task.Direction.Y = Task.Word(ScriptCameraWord::GoalYaw) = Fixed(Yaw);
		Task.Direction.Z = Task.Word(ScriptCameraWord::GoalRoll) = 0;
	}

	void InitScriptCamera(FWorldMapContext& Context, FWorldMapTask& Task, int32 Distance, int16 Pitch, int16 Yaw)
	{
		Task.Word(ScriptCameraWord::Shake) = NoShake;
		Task.Position.X = Context.Controlled.X = Context.CameraTarget.X = Context.Start.X;
		Task.Position.Y = Context.Controlled.Y = Context.CameraTarget.Y = Context.Start.Y;
		Task.Position.Z = Context.Controlled.Z = Context.CameraTarget.Z = Context.Start.Z;
		Task.Step = 0;
		Task.Message = 0;
		CutScriptCamera(Context, Task, Distance, Pitch, Yaw);
		Context.LookAt = 0;
		Context.ScreenCentreY = ScreenCentre;
	}

	int32 StepTowards(int32 Value, int32 Goal, int32 Step)
	{
		if (Value == Goal)
		{
			return Value;
		}
		return std::abs(Goal - Value) < std::abs(Step) ? Goal : Value + Step;
	}

	void FollowScriptCamera(FWorldMapContext& Context, FWorldMapTask& Task)
	{
		const int32 GoalPitch = Task.Word(ScriptCameraWord::GoalPitch);
		const int32 GoalYaw = Task.Word(ScriptCameraWord::GoalYaw);
		const int32 GoalRoll = Task.Word(ScriptCameraWord::GoalRoll);
		FWorldMapVector& Angles = Task.Direction;
		if (Angles.X != GoalPitch || Angles.Y != GoalYaw || Angles.Z != GoalRoll)
		{
			// The angles' way goes through the wrap of a way across the world, which a pitch or a roll never reaches.
			FWorldMapVector Left{GoalPitch - Angles.X, GoalYaw - Angles.Y, GoalRoll - Angles.Z, 0};
			Context.Ground.WrapDelta(Left);
			Ease(Angles.X, GoalPitch, Left.X, AngleSettle);
			Ease(Angles.Y, GoalYaw, Left.Y, AngleSettle);
			Ease(Angles.Z, GoalRoll, Left.Z, AngleSettle);
		}
		Context.CameraPitch = static_cast<int16>(Angles.X >> 12);
		Context.CameraYaw = static_cast<int16>(Angles.Y >> 12);
		Context.CameraRoll = static_cast<int16>(Angles.Z >> 12);

		const int32 GoalDistance = Task.Word(ScriptCameraWord::GoalDistance);
		if (GoalDistance != Context.CameraDistance)
		{
			Ease(Context.CameraDistance, GoalDistance, GoalDistance - Context.CameraDistance, DistanceSettle);
		}

		const FWorldMapVector& Goal = Context.Controlled;
		if (Task.Position.X != Goal.X || Task.Position.Y != Goal.Y || Task.Position.Z != Goal.Z)
		{
			FWorldMapVector Left{Goal.X - Task.Position.X, Goal.Y - Task.Position.Y, Goal.Z - Task.Position.Z, 0};
			Context.Ground.WrapDelta(Left);
			Ease(Task.Position.X, Goal.X, Left.X, TargetSettle);
			Ease(Task.Position.Y, Goal.Y, Left.Y, TargetSettle);
			Ease(Task.Position.Z, Goal.Z, Left.Z, TargetSettle);
		}
		Context.CameraTarget = Task.Position;

		// A draw every tick, also while the reach is one unit and the shake nothing.
		const int32 Reach = Task.Word(ScriptCameraWord::Shake);
		const int32 Range = Reach >> 12;
		if (Range == 0)
		{
			Context.Fault = MakeError("a world map cutscene's camera shakes within no range");
			return;
		}
		const int16 Shake = static_cast<int16>(Context.Random->Next() % Range - (Reach >> 13));
		Context.Orbit.Eye[1] = static_cast<int16>(Context.Orbit.Eye[1] + Shake);
		Context.Orbit.Target[1] = static_cast<int16>(Context.Orbit.Target[1] + Shake);
	}
}
