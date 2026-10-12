#include "Battle/BattleEvent.h"
#include "Battle/BattleProgram.h"
#include "Battle/BattleResult.h"
#include "Battle/BattleRuntime.h"
#include "Battle/BattleTables.h"
#include "Battle/Show/BattleShowTransition.h"
#include "Field/FieldMap.h"
#include "Game/GameState.h"
#include "Game/GameStateNames.h"
#include "Game/PartyOrder.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteMath.h"
#include "Input/KernelPad.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/BattleStepShow.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <functional>
#include <memory>

using namespace Weltall;

namespace
{
	template <typename T>
	std::unique_ptr<T> Loaded(TResult<T> Result)
	{
		REQUIRE(Result);
		return std::make_unique<T>(std::move(*Result));
	}

	/** A battle of map 15's first formation (a random encounter of the early forest) begun from a new game, the show the step oracle's harness. */
	struct FRuntimeBattle
	{
		std::unique_ptr<FPsxExecutable> Executable;
		std::unique_ptr<FGameFileIndex> Index;
		std::unique_ptr<FBattleTables> Tables;
		std::unique_ptr<FBattleProgramTables> Program;
		std::unique_ptr<FBattleGrowthTable> Growth;
		std::unique_ptr<FBattleEventArchive> Events;
		std::unique_ptr<FGteArctangents> Arctangents;
		FMesResident Resident;
		std::unique_ptr<FGameState> Game;
		std::unique_ptr<FGameStateNames> Names;
		FPsxRandom Random{0x1234};
		FKernelPad Pad;
		FMesContext Text;
		std::unique_ptr<Testing::FBattleStepShow> Show;
		std::unique_ptr<FBattleRuntime> Runtime;

		/** Stage, where given, changes the new game's state before the battle begins; bPlayersOrder as FBattleRequest's. */
		FRuntimeBattle(const FIso9660& Disc, uint8 Kind, const std::function<void(FGameState&)>& Stage = {}, bool bPlayersOrder = false)
		{
			Executable = Loaded(ReadBootExecutable(Disc));
			Index = Loaded(FGameFileIndex::Read(*Executable));
			Tables = Loaded(FBattleTables::Load(Disc.Image(), *Index));
			Program = Loaded(FBattleProgramTables::Load(Disc.Image(), *Index));
			Growth = Loaded(FBattleGrowthTable::Load(Disc.Image(), *Index));
			Events = Loaded(FBattleEventArchive::Load(Disc.Image(), *Index));
			Arctangents = Loaded(FGteArctangents::Read(*Executable));
			Resident = Testing::LoadMesResident(Disc);
			Game = Loaded(LoadNewGameState(Disc.Image(), *Index, Resident.NameTables));
			if (Stage)
			{
				Stage(*Game);
			}
			Names = std::make_unique<FGameStateNames>(*Game);
			Text.Font = &Resident.Font;
			Text.Tables = &Resident.NameTables;
			Text.State = Names.get();
			Text.ButtonMap = MesKernelButtonMap;
			Show = std::make_unique<Testing::FBattleStepShow>(Text, Testing::FBattleStepShowTimes{});

			auto Map = LoadFieldMap(Disc.Image(), *Index, 15);
			REQUIRE(Map);
			FBattleRequest Request;
			std::copy_n(Map->Encounters.begin(), FBattleFormation::RecordSize, Request.Formation.begin());
			Request.Kind = Kind;
			Request.bPlayersOrder = bPlayersOrder;
			FBattleRuntimeServices Services;
			Services.Image = &Disc.Image();
			Services.Index = Index.get();
			Services.Game = Game.get();
			Services.Random = &Random;
			Services.Pad = &Pad;
			Services.Show = Show.get();
			Services.Tables = Tables.get();
			Services.Program = Program.get();
			Services.Growth = Growth.get();
			Services.Events = Events.get();
			Services.Arctangents = Arctangents.get();
			auto Begun = FBattleRuntime::Begin(Request, Services);
			REQUIRE(Begun);
			Runtime = std::move(*Begun);
		}

		/** A frame after its vertical blanks with Held down; the vertical blanks it took. */
		int32 Frame(FPadState Held = {})
		{
			const int32 Vblanks = Runtime->VblanksPerFrame();
			for (int32 Count = 0; Count < Vblanks; ++Count)
			{
				Game->SetPlayTime(Game->PlayTime() + 1);
				Pad.Vblank(Held, Game->PlayTime());
			}
			Runtime->Frame();
			Show->EndFrame();
			return Vblanks;
		}
	};

	constexpr uint16 SoftReset = FPadState::Bit(EPadButton::L1) | FPadState::Bit(EPadButton::R1) | FPadState::Bit(EPadButton::Start)
		| FPadState::Bit(EPadButton::Select);
}

TEST_CASE("A battle's default transition takes 86 frames of two vertical blanks, then the battle's frames take one", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FRuntimeBattle Battle(*Disc, 0);
	REQUIRE(Battle.Runtime->IsInTransition());
	CHECK(Battle.Runtime->DefaultTransition() != nullptr);
	int32 Frames = 1;
	while (Battle.Runtime->IsInTransition())
	{
		CHECK(Battle.Frame() == 2);
		++Frames;
	}
	// The frame after the transition's last ran the setup's end and the battle's first frame.
	CHECK(Frames == FBattleDefaultTransition::FrameCount + 1);
	CHECK(Battle.Runtime->TransitionFrames() == FBattleDefaultTransition::FrameCount);
	CHECK(Battle.Runtime->Context().Party.InputMode == BattleInputModes::Messages);
	CHECK(Battle.Frame() == 1);
	CHECK_FALSE(Battle.Runtime->IsOver());
}

TEST_CASE("The setup's own transitions take 82 frames", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FRuntimeBattle Battle(*Disc, 2);
	CHECK(Battle.Runtime->DefaultTransition() == nullptr);
	while (Battle.Runtime->IsInTransition())
	{
		Battle.Frame();
	}
	CHECK(Battle.Runtime->TransitionFrames() == FBattleRuntime::SetupTransitionFrames);
}

TEST_CASE("The pad's soft reset stops a battle in its transition and in its frames", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FRuntimeBattle InTransition(*Disc, 0);
	InTransition.Frame(FPadState{SoftReset});
	CHECK(InTransition.Runtime->IsSoftReset());
	CHECK(InTransition.Runtime->IsOver());
	CHECK_FALSE(InTransition.Runtime->Outcome());

	FRuntimeBattle Running(*Disc, 1);
	while (Running.Runtime->IsInTransition())
	{
		Running.Frame();
	}
	Running.Frame();
	CHECK_FALSE(Running.Runtime->IsSoftReset());
	Running.Frame(FPadState{SoftReset});
	CHECK(Running.Runtime->IsSoftReset());
	CHECK(Running.Runtime->IsOver());
}

TEST_CASE("An encounter takes the party in the order its player keeps, each member with its own gear's byte", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr uint8 Fei = 0;
	constexpr uint8 Citan = 2;
	// A new game's Fei and Citan, Citan in his gear and placed first.
	const auto Stage = [](FGameState& Game)
	{
		Game.SetPartySlotInGear(0, 0);
		Game.SetPartySlotInGear(1, 1);
		FPartyOrder Order;
		const std::array<uint8, 2> CitanFirst{Citan, Fei};
		Order.Arrange(Game.Party(), CitanFirst);
		Game.SetPartyOrder(Order);
	};
	for (const bool bPlayersOrder : {true, false})
	{
		FRuntimeBattle Battle(*Disc, 0, Stage, bPlayersOrder);
		while (Battle.Runtime->IsInTransition())
		{
			Battle.Frame();
		}
		const FBattleContext& Context = Battle.Runtime->Context();
		INFO("in the player's order " << bPlayersOrder);
		const std::size_t CitansSlot = bPlayersOrder ? 0 : 1;
		const std::size_t FeisSlot = bPlayersOrder ? 1 : 0;
		CHECK(Context.PartyCharacters[CitansSlot] == Citan);
		CHECK(Context.PartyCharacters[FeisSlot] == Fei);
		CHECK(Context.Slots[CitansSlot].InGear == 1);
		CHECK(Context.Slots[FeisSlot].InGear == 0);
		CHECK(Battle.Game->Party()[0] == Fei);
	}
}
