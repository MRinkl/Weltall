#include "Battle/BattleContext.h"
#include "Battle/BattleProgram.h"
#include "Battle/BattleScene.h"
#include "Battle/BattleSetup.h"
#include "Battle/BattleTables.h"
#include "Battle/Show/BattleHudSheet.h"
#include "Battle/Show/BattleObjectFiles.h"
#include "Battle/Show/BattleSceneGraphics.h"
#include "Battle/Show/BattleShow.h"
#include "Battle/Show/BattleSpriteFiles.h"
#include "Battle/Show/BattleStageFiles.h"
#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteMath.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <memory>
#include <optional>

using namespace Weltall;

namespace
{
	/** A battle set up by the rules from the disc's files: Fei, Elly and Citan on foot against a scene's sprite enemies. */
	struct FShowBattle
	{
		FGameState State;
		FPsxRandom Random{0x1234};
		std::unique_ptr<FBattleContext> Context = std::make_unique<FBattleContext>();
		std::optional<FGameFileIndex> Index;
		std::optional<FGteArctangents> Arctangents;
		std::optional<FBattleTables> Tables;
		std::optional<FBattleProgramTables> Program;
		std::optional<FBattleScene> Scene;
		std::optional<FBattleStage> Layout;
		std::optional<FBattleStageFiles> Stage;
		std::optional<FBattleSceneGraphics> Graphics;
		std::optional<FBattleHudSheet> Sheet;
		std::optional<FBattleSpriteFiles> Files;
		std::optional<FBattleObjectDisc> Objects;
		std::unique_ptr<FBattleShow> Show = std::make_unique<FBattleShow>();

		bool Load(const FIso9660& Disc, int32 SceneNumber, int32 StageNumber)
		{
			const auto Executable = ReadBootExecutable(Disc);
			if (!Executable)
			{
				return false;
			}
			auto ReadIndex = FGameFileIndex::Read(*Executable);
			auto ReadArctangents = FGteArctangents::Read(*Executable);
			if (!ReadIndex || !ReadArctangents)
			{
				return false;
			}
			Index = std::move(*ReadIndex);
			Arctangents = *ReadArctangents;
			const FCdImage& Image = Disc.Image();
			auto ReadTables = FBattleTables::Load(Image, *Index);
			auto ReadProgram = FBattleProgramTables::Load(Image, *Index);
			auto ReadScene = FBattleScene::Load(Image, *Index, SceneNumber);
			auto ReadLayout = FBattleStage::Load(Image, *Index, StageNumber);
			auto ReadStage = FBattleStageFiles::Load(Image, *Index, StageNumber);
			auto ReadGraphics = FBattleSceneGraphics::Load(Image, *Index, SceneNumber);
			auto ReadSheet = FBattleHudSheet::Load(Image, *Index);
			auto ReadFiles = FBattleSpriteFiles::Load(Image, *Index);
			auto ReadObjects = FBattleObjectDisc::Load(Image, *Index);
			if (!ReadTables || !ReadProgram || !ReadScene || !ReadLayout || !ReadStage || !ReadGraphics || !ReadSheet || !ReadFiles || !ReadObjects)
			{
				return false;
			}
			Objects = std::move(*ReadObjects);
			Tables = std::move(*ReadTables);
			Program = std::move(*ReadProgram);
			Scene = std::move(*ReadScene);
			Layout = *ReadLayout;
			Stage = std::move(*ReadStage);
			Graphics = std::move(*ReadGraphics);
			Sheet = std::move(*ReadSheet);
			Files = std::move(*ReadFiles);
			return true;
		}

		void SetUp(const std::array<uint8, FBattleFormation::RecordSize>& Record)
		{
			std::span<uint8> Block = State.Block();
			for (std::size_t Slot = 0; Slot < 3; ++Slot)
			{
				Block[0x1D34 + Slot] = static_cast<uint8>(Slot);
				Block[0x22B1 + Slot] = 0;
			}
			Block[0x1D30] = 0xFF;
			Block[0x1D31] = 0x07;
			Block[0x1D32] = 0xFF;
			Block[0x1D33] = 0xFF;
			for (std::size_t Character = 0; Character < 11; ++Character)
			{
				const std::size_t At = 0x26C + Character * 0xA4;
				Block[At + 0x56] = static_cast<uint8>(Character);
				Block[At + 0xA0] = 0xFF;
				// Some HP, so that everyone stands.
				Block[At + 0x00] = 100;
				Block[At + 0x02] = 100;
			}
			Context->GameState = &State;
			Context->Random = &Random;
			Context->Program = &*Program;
			Context->Arctangents = &*Arctangents;
			SetUpBattle(*Context, FBattleFormation::FromRecord(Record), *Tables, *Scene, *Layout);
		}

		void Begin()
		{
			FBattleShowSetup Setup;
			Setup.Context = Context.get();
			Setup.Arctangents = &*Arctangents;
			Setup.SpriteFiles = &*Files;
			Setup.Stage = &*Stage;
			Setup.SceneGraphics = &*Graphics;
			Setup.Sheet = &*Sheet;
			Setup.ObjectFiles = &*Objects;
			Show->Begin(Setup);
		}

		/** A frame with no input step (the rules' part), as the opening runs them. */
		bool Frame()
		{
			Show->BeginFrame();
			return Show->EndFrame();
		}
	};

	/** The first scene whose graphics' first entry is a sprite enemy. */
	int32 SpriteScene(const FIso9660& Disc)
	{
		const auto Executable = ReadBootExecutable(Disc);
		const auto Index = Executable ? FGameFileIndex::Read(*Executable) : TResult<FGameFileIndex>(MakeError("no executable"));
		if (!Index)
		{
			return -1;
		}
		for (int32 Scene = 0; Scene < 76; ++Scene)
		{
			const auto Graphics = FBattleSceneGraphics::Load(Disc.Image(), *Index, Scene);
			if (Graphics && !Graphics->Entries().empty() && Graphics->Entries()[0].IsSprite() && Graphics->Entries()[0].HasOwnImages())
			{
				return Scene;
			}
		}
		return -1;
	}
}

TEST_CASE("The battle's show opens a battle as the setup does: the enemies, the party running in, the start once they land", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const int32 Scene = SpriteScene(*Disc);
	REQUIRE(Scene >= 0);
	FShowBattle Battle;
	REQUIRE(Battle.Load(*Disc, Scene, 0));
	std::array<uint8, FBattleFormation::RecordSize> Record{};
	Record[0] = static_cast<uint8>(Scene);
	Record[2] = 0;
	for (std::size_t Enemy = 0; Enemy < 8; ++Enemy)
	{
		Record[8 + Enemy] = Enemy < 2 ? 0 : FBattleFormation::EmptyType;
		Record[0x18 + Enemy] = static_cast<uint8>(Enemy);
	}
	Battle.SetUp(Record);
	REQUIRE(Battle.Context->Fault == EBattleFault::None);
	Battle.Begin();
	REQUIRE(Battle.Show->Fault() == EBattleShowFault::None);
	CHECK_FALSE(Battle.Show->HasStarted());

	// The first frame's load step makes the enemies' sprites; the next the party's, running in.
	CHECK(Battle.Frame());
	CHECK(Battle.Show->Battle.SlotSprites[3] != nullptr);
	CHECK(Battle.Show->Battle.SlotSprites[4] != nullptr);
	CHECK(Battle.Show->Battle.SlotSprites[0] == nullptr);
	CHECK(Battle.Frame());
	for (std::size_t Slot = 0; Slot < 3; ++Slot)
	{
		REQUIRE(Battle.Show->Battle.SlotSprites[Slot] != nullptr);
		CHECK(Battle.Show->Battle.SlotSprites[Slot]->Animation() == 0x17);
	}

	int32 Frames = 2;
	while (!Battle.Show->HasStarted() && Frames < 300)
	{
		CHECK(Battle.Frame());
		++Frames;
		REQUIRE(Battle.Show->Fault() == EBattleShowFault::None);
	}
	CHECK(Battle.Show->HasStarted());
	// The loads' steps and the countdown take their frames before the landing can start the battle.
	CHECK(Frames > 21);

	const FBattleShowFrame& Frame = Battle.Show->Frame();
	CHECK(Frame.Vram != nullptr);
	CHECK_FALSE(Frame.Quads.empty());
	for (std::size_t Link = 1; Link < Frame.Order.size(); ++Link)
	{
		CHECK(Frame.Order[Link - 1].Entry >= Frame.Order[Link].Entry);
	}
	Battle.Show->End();
}
