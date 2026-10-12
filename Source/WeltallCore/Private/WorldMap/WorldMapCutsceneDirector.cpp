#include "WorldMap/WorldMapCutsceneDirector.h"

#include "Game/PsxRandom.h"
#include "Graphics/GteMatrix.h"

#include <algorithm>

namespace Weltall::WorldMapOps
{
	void TakeDirectorStep(FWorldMapContext& Context, FWorldMapTask& Task, const FWorldMapDirectorScript& Script)
	{
		const int32 Index = Task.Word(DirectorScriptIndex);
		if (Index < 0 || static_cast<std::size_t>(Index) >= Script.Steps.size())
		{
			Context.Fault = MakeError("a world map cutscene's director reads entry " + std::to_string(Index) + " of a script of "
				+ std::to_string(Script.Steps.size()));
			return;
		}
		Task.Step = static_cast<int16>(Script.Steps[static_cast<std::size_t>(Index)]);
		Task.Timer = static_cast<int16>(Script.Waits[static_cast<std::size_t>(Index)]);
	}

	void WaitDirector(FWorldMapContext& Context, FWorldMapTask& Task, const FWorldMapDirectorScript& Script)
	{
		Task.Timer = static_cast<int16>(Task.Timer - 1);
		if (Task.Timer < 0)
		{
			TakeDirectorStep(Context, Task, Script);
			++Task.Word(DirectorScriptIndex);
		}
	}

	void LeaveCutscene(FWorldMapContext& Context, FWorldMapTask& Task)
	{
		Context.Running = 0;
		Context.ExitCode = 0;
		Task.Step = DirectorStep::Done;
	}

	void SendFade(FWorldMapContext& Context, int16 Message, int32 Blend, int32 Step)
	{
		SendMessage(Context, FWorldMapContext::FadeSlot, Message);
		Context.FadeBlend = Blend;
		Context.FadeStep = Step;
	}

	void ShakeCamera(FWorldMapContext& Context, const FWorldMapTask& Task)
	{
		const int32 Reach = Task.Word(CameraShakeReach);
		const int32 Units = Reach >> 12;
		if (Units == 0)
		{
			Context.Fault = MakeError("a world map cutscene's camera shakes within nothing: the original stops on a division by zero");
			return;
		}
		const int32 Drawn = static_cast<int32>(Context.Random->Next());
		const int16 Offset = static_cast<int16>(static_cast<int16>(Drawn % Units) - static_cast<int16>(Reach >> 13));
		FWorldMapOrbit& Orbit = Context.Orbit;
		Orbit.Eye[1] = static_cast<int16>(Orbit.Eye[1] + Offset);
		Orbit.Target[1] = static_cast<int16>(Orbit.Target[1] + Offset);
	}

	void InitDirectedCamera(FWorldMapContext& Context, FWorldMapTask& Task)
	{
		Context.LookAt = 0;
		Task.Word(CameraShakeReach) = CameraShakeStill;
		Context.ScreenCentreY = 0x78;
		Context.CameraTarget.X = Context.Controlled.X = Context.Start.X;
		Context.CameraTarget.Y = Context.Controlled.Y = Context.Start.Y;
		Context.CameraTarget.Z = Context.Controlled.Z = Context.Start.Z;
		Task.Message = 1;
		Task.Step = 0;
	}

	namespace CameraEase
	{
		constexpr int32 GoalDistance = 0;
		constexpr int32 GoalPitch = 1;
		constexpr int32 SmoothPitch = 2;
		constexpr int32 GoalYaw = 3;
		constexpr int32 SmoothYaw = 4;

		/** A goal a step on, no further than its end. */
		int32 Stepped(int32 Goal, int32 Step, int32 End)
		{
			return Step < 0 ? std::max(Goal + Step, End) : std::min(Goal + Step, End);
		}
	}

	void StartCameraEase(FWorldMapContext& Context, FWorldMapTask& Task, int32 Distance, int16 Pitch, int16 Yaw)
	{
		Context.CameraPitch = Pitch;
		Context.CameraYaw = Yaw;
		Context.CameraRoll = 0;
		Task.Word(CameraEase::GoalDistance) = Distance;
		Task.Word(CameraEase::SmoothPitch) = Task.Word(CameraEase::GoalPitch) = Pitch * 0x1000;
		Context.CameraDistance = Distance;
		Task.Word(CameraEase::SmoothYaw) = Task.Word(CameraEase::GoalYaw) = Yaw * 0x1000;
	}

	void EaseCamera(FWorldMapContext& Context, FWorldMapTask& Task, const FCameraEase& Ease)
	{
		int32& Distance = Task.Word(CameraEase::GoalDistance);
		Distance = CameraEase::Stepped(Distance, Ease.DistanceStep, Ease.DistanceEnd);
		if (const int32 Left = Distance - Context.CameraDistance; Left != 0)
		{
			Context.CameraDistance += Left >> 5;
		}
		int32& Pitch = Task.Word(CameraEase::GoalPitch);
		Pitch = CameraEase::Stepped(Pitch, Ease.PitchStep, Ease.PitchEnd);
		if (const int32 Left = Pitch - Task.Word(CameraEase::SmoothPitch); Left != 0)
		{
			Task.Word(CameraEase::SmoothPitch) += Left >> 4;
			Context.CameraPitch = static_cast<int16>(Task.Word(CameraEase::SmoothPitch) >> 12);
		}
		int32& Yaw = Task.Word(CameraEase::GoalYaw);
		Yaw = CameraEase::Stepped(Yaw, Ease.YawStep, Ease.YawEnd);
		if (const int32 Left = Yaw - Task.Word(CameraEase::SmoothYaw); Left != 0)
		{
			Task.Word(CameraEase::SmoothYaw) += Left >> 5;
			Context.CameraYaw = static_cast<int16>(Task.Word(CameraEase::SmoothYaw) >> 12);
		}
	}

	std::array<int16, 4> EffectPlace(const FWorldMapVector& Position)
	{
		return {static_cast<int16>(Position.X >> 12), static_cast<int16>(Position.Y >> 12), static_cast<int16>(Position.Z >> 12), 0};
	}

	namespace GlowRing
	{
		constexpr int32 DiscCount = 2;
		constexpr int32 FirstWidth = 0;
		constexpr int32 SecondWidth = 1;
		constexpr int32 Brightness = 2;
		constexpr int32 WidthStep = 0x180;
		constexpr int32 WidthLimit = 0x7FFF;
	}

	FResult InitGlowRing(FWorldMapContext& Context, FWorldMapTask& Task, int32 First)
	{
		Task.Word(GlowRing::SecondWidth) = -0x800;
		Task.Word(GlowRing::FirstWidth) = 0;
		Task.Word(GlowRing::Brightness) = 0x80;
		for (int32 Disc = 0; Disc < GlowRing::DiscCount; ++Disc)
		{
			SetGlowLook(Context.Objects[static_cast<std::size_t>(First + Disc)]);
		}
		return WorldMapTaskState::Idle;
	}

	FResult UpdateGlowRing(FWorldMapContext& Context, FWorldMapTask& Task, int32 First, int32 Dimming)
	{
		if (Task.Message == 1)
		{
			Task.Message = 0;
		}
		const uint8 Shade = static_cast<uint8>(Task.Word(GlowRing::Brightness));
		for (int32 Disc = 0; Disc < GlowRing::DiscCount; ++Disc)
		{
			FWorldMapObject& Object = Context.Objects[static_cast<std::size_t>(First + Disc)];
			const int32 Width = Task.Word(GlowRing::FirstWidth + Disc);
			Object.X = Task.Position.X >> 12;
			Object.Y = Task.Position.Y >> 12;
			Object.Z = Task.Position.Z >> 12;
			Object.Matrix.M = ScaleColumns(FGteMatrix::Identity(), {Width, FGteMatrix::One, Width}).M;
			if (Object.Look)
			{
				Object.Look->Colours[static_cast<std::size_t>(Context.DrawBuffer & 1)] = {Shade, Shade, Shade};
			}
		}
		for (int32 Disc = 0; Disc < GlowRing::DiscCount; ++Disc)
		{
			int32& Width = Task.Word(GlowRing::FirstWidth + Disc);
			Width = std::min(Width + GlowRing::WidthStep, GlowRing::WidthLimit);
		}
		Task.Word(GlowRing::Brightness) -= Dimming;
		if (Task.Word(GlowRing::Brightness) < 0)
		{
			Task.Word(GlowRing::Brightness) = 0;
			return WorldMapTaskState::Idle;
		}
		return WorldMapTaskState::Update;
	}

	void StopFieldSong(FWorldMapContext& Context)
	{
		if (Context.Sound != nullptr)
		{
			Context.Sound->StopFieldSong();
		}
	}

	void StartCutsceneSong(FWorldMapContext& Context)
	{
		if (Context.Sound != nullptr)
		{
			Context.Sound->LoadBank();
			Context.Sound->RegisterEffects();
			Context.Sound->LoadSong(EWorldMapSong::Map);
			Context.Sound->PlaySong(WorldMapFullVolume, 0);
		}
	}
}
