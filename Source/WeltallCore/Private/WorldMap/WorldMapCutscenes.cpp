#include "WorldMap/WorldMapCutscenes.h"

#include "WorldMap/WorldMapCutsceneScript.h"

#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "WorldMap/WorldMapStateLayout.h"

namespace Weltall::WorldMapOps
{
	namespace CutsceneShared
	{
		/** The geometry's screen distance every scene's display sets. */
		constexpr int32 ScreenDistance = 0x100;

		using R = EWorldMapRoutine;

		/** The glowing packets' texture page (8-bit at (340h, 100h), adding a quarter) and CLUT (100h, 1FFh): the particles'. */
		constexpr uint16 GlowPage = 0xFD;
		constexpr uint16 GlowClut = 0x7FD0;

		/** The routines several cutscenes share. */
		constexpr FCutsceneRoutine SharedRoutines[] = {
			{R::InitCutsceneFrame, [](FWorldMapContext&, int32) -> FResult { return WorldMapTaskState::Update; }},
			{R::UpdateCutsceneFrame, [](FWorldMapContext& Context, int32 Slot) { return UpdateCutsceneFrame(Context, Slot, EWorldMapFrameTask::Cutscene); }},
			{R::InitCutsceneFrameTrees, [](FWorldMapContext&, int32) -> FResult { return WorldMapTaskState::Update; }},
			{R::UpdateCutsceneFrameTrees,
				[](FWorldMapContext& Context, int32 Slot) { return UpdateCutsceneFrame(Context, Slot, EWorldMapFrameTask::CutsceneTrees); }},
			{R::InitIdleTask, InitIdleTask},
			{R::RunScript, RunCutsceneScript},
		};

		/** The cutscenes the port plays. */
		constexpr const FCutscene* Cutscenes[] = {
			&TelescopeCutscene,
			&Scene9Cutscene,
			&Scene10Cutscene,
			&Scene12Cutscene,
			&Scene13Cutscene,
			&Scene14Cutscene,
			&Scene15Cutscene,
			&Scene16Cutscene,
			&Scene17Cutscene,
			&Scene18Cutscene,
		};

		void SetWord(FWorldMapContext& Context, std::size_t Offset, uint16 Value)
		{
			LittleEndian::WriteU16(Context.Game->Block(), Offset, Value);
		}
	}

	const FCutscene* FindCutscene(int32 Scene)
	{
		for (const FCutscene* Cutscene : CutsceneShared::Cutscenes)
		{
			for (const int16 Number : Cutscene->Scenes)
			{
				if (Number == Scene)
				{
					return Cutscene;
				}
			}
		}
		return nullptr;
	}

	std::optional<FResult> RunCutsceneRoutine(FWorldMapContext& Context, int32 Slot, EWorldMapRoutine Routine)
	{
		for (const FCutsceneRoutine& Shared : CutsceneShared::SharedRoutines)
		{
			if (Shared.Routine == Routine)
			{
				return Shared.Run(Context, Slot);
			}
		}
		for (const FCutscene* Cutscene : CutsceneShared::Cutscenes)
		{
			for (const FCutsceneRoutine& Own : Cutscene->Routines)
			{
				if (Own.Routine == Routine)
				{
					return Own.Run(Context, Slot);
				}
			}
		}
		return std::nullopt;
	}

	void StartCutscene(FWorldMapContext& Context, const FCutscene& Cutscene)
	{
		Context.bCutscene = true;
		Context.ProjectionDistance = CutsceneShared::ScreenDistance;
		Context.Tasks = {};
		Context.FadeBlend = Cutscene.FadeBlend;
		Context.FadeStep = Cutscene.FadeStep;
		Context.MenuRequested = 0;
		Context.LookAt = 0;
		// Only the place name's and the notice's tasks set these, which no cutscene runs: they stay as the program's start clears them.
		Context.Place = 0;
		Context.Notice = 0;
		Cutscene.Start(Context);
	}

	void StartCutsceneWorld(FWorldMapContext& Context, const FWorldMapVector& Start)
	{
		Context.Start.X = Start.X;
		Context.Start.Y = Start.Y;
		Context.Start.Z = Start.Z;
		InitObjects(Context);
		InitClouds(Context);
	}

	void RegisterCutsceneEffects(FWorldMapContext& Context)
	{
		if (Context.Sound != nullptr)
		{
			Context.Sound->RegisterEffects();
		}
	}

	void StopCutsceneEffects(FWorldMapContext& Context)
	{
		if (Context.Sound != nullptr)
		{
			Context.Sound->StopAllEffects();
			Context.Sound->UnregisterEffects();
		}
	}

	void EndCutscene(FWorldMapContext& Context, uint16 Map, uint16 Entrance)
	{
		CutsceneShared::SetWord(Context, WorldMapStateLayout::Place, Map);
		CutsceneShared::SetWord(Context, WorldMapStateLayout::Scene, Entrance);
		CutsceneShared::SetWord(Context, WorldMapStateLayout::CameraYaw, static_cast<uint16>(Context.CameraYaw));
	}

	void SetGlowLook(FWorldMapObject& Object)
	{
		FWorldMapPacketLook Look;
		Look.Page = CutsceneShared::GlowPage;
		Look.Clut = CutsceneShared::GlowClut;
		Look.bSemiTransparent = true;
		Look.Colours = {{{0x80, 0x80, 0x80}, {0x80, 0x80, 0x80}}};
		Object.Look = Look;
	}

	void PlayCutsceneEffect(FWorldMapContext& Context, int32 Effect)
	{
		PlaySceneEffect(Context, Effect);
	}

	void PlayProtectedCutsceneEffect(FWorldMapContext& Context, int32 Effect)
	{
		if (Context.Sound != nullptr)
		{
			Context.Sound->PlayProtectedEffect(Effect);
		}
	}

	void WaitForCutsceneStep(FWorldMapContext& Context, FWorldMapTask& Task, std::span<const int16> Steps, std::span<const int16> Waits)
	{
		Task.Timer = static_cast<int16>(Task.Timer - 1);
		if (Task.Timer >= 0)
		{
			return;
		}
		int32& Index = Task.Word(0);
		if (Index < 0 || static_cast<std::size_t>(Index) >= Steps.size() || static_cast<std::size_t>(Index) >= Waits.size())
		{
			Context.Fault = MakeError("a world map cutscene's director reads past its steps, at " + std::to_string(Index));
			return;
		}
		Task.Step = Steps[static_cast<std::size_t>(Index)];
		Task.Timer = Waits[static_cast<std::size_t>(Index)];
		++Index;
	}

	void SetCutsceneEffectVolume(FWorldMapContext& Context, int32 Effect, int32 Volume)
	{
		if (Context.Sound != nullptr)
		{
			Context.Sound->SetEffectVolume(Effect, static_cast<int16>(Volume));
		}
	}

	void FadeCutsceneEffect(FWorldMapContext& Context, int32 Effect, int32 Volume, int32 Ticks)
	{
		if (Context.Sound != nullptr)
		{
			Context.Sound->FadeEffectVolume(Effect, Volume, Ticks);
		}
	}
}
