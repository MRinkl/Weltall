#include "Battle/BattleRuntime.h"
#include "Battle/Show/BattleFrameBlend.h"
#include "Battle/Show/BattleGameHud.h"
#include "Battle/Show/BattlePresentation.h"
#include "Game/GameState.h"
#include "Game/GameStateNames.h"
#include "Game/KernelState.h"
#include "Game/PsxRandom.h"
#include "Input/KernelPad.h"
#include "Support/BattleProgramDisc.h"
#include "Support/BattleSoundRecorder.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

using namespace Weltall;

TEST_CASE("A real battle's frames pair their anchors from frame to frame, so the display draws them between", "[Battle][Content][blend]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 Map = 15;
	constexpr int32 Formation = 1;
	constexpr std::array<int32, 1> Maps{Map};
	Testing::FBattleProgramDisc Loaded;
	REQUIRE(Testing::LoadBattleProgramDisc(*Disc, Maps, Loaded));

	FGameState Game = *Loaded.NewGame;
	Game.SetParty({0, 1, 2});
	Game.SetJoinedCharacters(0x1FF);
	FPsxRandom Random(0x1234);
	FKernelPad Pad;
	FGameStateNames Names(Game);
	FMesContext Text;
	Text.Font = &Loaded.Resident.Font;
	Text.Tables = &Loaded.Resident.NameTables;
	Text.State = &Names;
	Text.ButtonMap = MesKernelButtonMap;
	FKernelState Kernel;
	Kernel.InitNewGame();

	FBattleRequest Request;
	const std::vector<uint8>& Encounters = Loaded.Encounters.at(Map);
	std::copy_n(Encounters.begin() + Formation * static_cast<std::ptrdiff_t>(FBattleFormation::RecordSize), FBattleFormation::RecordSize,
		Request.Formation.begin());
	Testing::FBattleSoundRecorder Sound;
	FBattleGameHud Hud(*Loaded.HudResources, Text, FBattleHudSettings{Kernel.WindowColour, Kernel.BattleWindowBlend});
	const auto Presentation = std::make_unique<FBattlePresentation>(Loaded.PresentationFiles(), &Sound, &Hud);
	FBattleRuntimeServices Services;
	Services.Image = &Disc->Image();
	Services.Index = &*Loaded.Index;
	Services.Game = &Game;
	Services.Random = &Random;
	Services.Pad = &Pad;
	Services.Show = Presentation.get();
	Services.Tables = &*Loaded.Tables;
	Services.Program = &*Loaded.Program;
	Services.Growth = &*Loaded.Growth;
	Services.Events = &*Loaded.Events;
	Services.Arctangents = &*Loaded.Arctangents;
	auto Runtime = FBattleRuntime::Begin(Request, Services);
	REQUIRE(Runtime);

	FBattleFrameKeep Latest;
	FBattleFrameKeep Previous;
	FBattleFrameBlend Blend;
	FBattleFrameBlendScratch Scratch;
	bool bKept = false;
	int32 Pairs = 0;
	int32 Cuts = 0;
	int64 Anchors = 0;
	int64 Paired = 0;
	float Furthest = 0.0f;
	// Cross pressed now and then: the menus' confirm, and the attacks' press.
	for (int32 Frame = 0; Frame < 1500 && !(*Runtime)->IsOver(); ++Frame)
	{
		for (int32 Count = (*Runtime)->VblanksPerFrame(); Count > 0; --Count)
		{
			Pad.Vblank(FPadState{static_cast<uint16>(Frame % 24 < 2 ? 1u << 14 : 0u)}, Game.PlayTime());
		}
		(*Runtime)->Frame();
		REQUIRE((*Runtime)->Fault() == EBattleFault::None);
		if (!Presentation->HasBegun() || (bKept && Presentation->Show.Frame().Step == Latest.Step))
		{
			continue;
		}
		std::swap(Previous, Latest);
		KeepBattleFrame(Presentation->Show.Frame(), Latest);
		if (!bKept)
		{
			bKept = true;
			continue;
		}
		++Pairs;
		BlendBattleFrames(Latest, &Previous, 0.5f, Blend, Scratch);
		if (Blend.View == UnmovedView && Latest.View != Previous.View)
		{
			++Cuts;
			continue;
		}
		for (std::size_t Index = 0; Index < Latest.Anchors.size(); ++Index)
		{
			++Anchors;
			const uint64 Key = Latest.Anchors[Index].Key;
			const bool bPartner = std::any_of(Previous.Anchors.begin(), Previous.Anchors.end(), [Key](const FBattleFrameAnchor& Anchor) { return Anchor.Key == Key; });
			Paired += bPartner ? 1 : 0;
			// Halfway, an anchor's origin is drawn no further from the latest's place than half the way it came.
			const std::array<float, 3>& Origin = Latest.Anchors[Index].Matrix.T;
			const std::array<float, 3> Drawn = MoveViewPoint(Blend.Moves[Index], Origin);
			Furthest = std::max({Furthest, std::fabs(Drawn[0] - Origin[0]), std::fabs(Drawn[1] - Origin[1]), std::fabs(Drawn[2] - Origin[2])});
		}
	}
	INFO("pairs " << Pairs << ", cuts " << Cuts << ", anchors " << Anchors << ", paired " << Paired << ", furthest " << Furthest);
	REQUIRE(Pairs > 500);
	CHECK(Cuts * 50 < Pairs);
	CHECK(Paired * 100 >= Anchors * 95);
	CHECK(Furthest <= 1024.0f);
}
