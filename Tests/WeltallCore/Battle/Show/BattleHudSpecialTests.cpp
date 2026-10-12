#include "Battle/BattleContext.h"
#include "Battle/BattleProgram.h"
#include "Battle/BattleTables.h"
#include "Battle/Show/BattleShowHud.h"
#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Graphics/Vram.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Support/MenuFrameDigest.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <memory>
#include <vector>

using namespace Weltall;
using namespace Weltall::Testing;

namespace
{
	/** The learned-skills block of the game state, Fei's first: its Specials' word at +1Ah (the first Special the high bit). */
	constexpr std::size_t LearnedSkills = 0x16C0;
	constexpr std::size_t SpecialSkills = 0x1A;
	constexpr uint16 FirstSpecialLearned = 0x8000;
	/** Repair, a gear's first Special: its command. */
	constexpr int32 RepairCommand = 0x25;
	/** Where the list's text strip takes the cost's four figures. */
	constexpr int16 CostLeft = 0x3C0;
	constexpr int16 CostRight = 0x3C6;

	/** The cost's figures as the Special list uploads them for Fei's Weltall with a Repair of Cost. */
	std::vector<FVramImage> CostFigures(const FIso9660& Disc, uint16 Cost)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		const auto Resources = FBattleHudResources::Load(Disc.Image(), *Index, *Executable);
		REQUIRE(Resources);
		const auto Program = FBattleProgramTables::Load(Disc.Image(), *Index);
		REQUIRE(Program);
		const auto Tables = FBattleTables::Load(Disc.Image(), *Index);
		REQUIRE(Tables);
		const FMesResident Resident = LoadMesResident(Disc);
		FMesContext Text;
		Text.Font = &Resident.Font;
		Text.Tables = &Resident.NameTables;
		Text.ButtonMap = MesKernelButtonMap;

		auto Context = std::make_unique<FBattleContext>();
		auto State = std::make_unique<FGameState>();
		Context->Program = &*Program;
		Context->GameState = State.get();
		Context->Items = Tables->Items();
		Context->PartyCharacters[0] = 0;
		Context->Slots[0].Occupant = 0;
		Context->Combatants[0].Character = 0;
		Context->Combatants[0].Gear = 0;
		LittleEndian::WriteU16(State->Block(), LearnedSkills + SpecialSkills, FirstSpecialLearned);
		Context->Commands[static_cast<std::size_t>(FBattleContext::GearCommandIndex(0, RepairCommand))].FuelCost = Cost;

		FVram Vram;
		FRecordedUploads Recorded;
		FBattleHud Hud(*Resources, Text, Vram, &Recorded);
		Hud.Begin(*Context, {});
		Hud.MakeCommandHud();
		Recorded.Images.clear();
		Hud.BuildSpecialList(0);
		CHECK(Hud.Fault() == EBattleHudFault::None);
		std::vector<FVramImage> Figures;
		std::copy_if(Recorded.Images.begin(), Recorded.Images.end(), std::back_inserter(Figures), [](const FVramImage& Image)
			{
				return Image.Rect.X >= CostLeft && Image.Rect.X <= CostRight && Image.Rect.Y == 0x102 && Image.Rect.Width == 6;
			});
		return Figures;
	}
}

TEST_CASE("A Special dearer than the list's four figures shows 9999 where the original draws a picture past its digits", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Repair at level 18 (two level-9 parts) on a frame of 36,000: (3600 x 18 x 2) / 9 = 14,400. No engine holds that
	// much fuel; the original reads its thousands' picture from past its ten digits'.
	const std::vector<FVramImage> Dear = CostFigures(*Disc, 14400);
	const std::vector<FVramImage> Most = CostFigures(*Disc, 9999);
	REQUIRE(Dear.size() == 4);
	REQUIRE(Most.size() == 4);
	for (std::size_t Figure = 0; Figure < Dear.size(); ++Figure)
	{
		CHECK(Dear[Figure].Rect.X == Most[Figure].Rect.X);
		CHECK(Dear[Figure].Units == Most[Figure].Units);
	}
	// And a cost of four figures shows as itself: 7200 is not 9999.
	const std::vector<FVramImage> Within = CostFigures(*Disc, 7200);
	REQUIRE(Within.size() == 4);
	CHECK(Within[0].Units != Most[0].Units);
}
