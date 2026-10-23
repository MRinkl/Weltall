#include "Battle/BattleRuntime.h"
#include "Battle/Show/BattlePresentation.h"
#include "Foundation/LittleEndian.h"
#include "Game/BattleGameProgram.h"
#include "Game/FieldProgram.h"
#include "Game/GameLoop.h"
#include "Game/GameState.h"
#include "Support/GameDiscs.h"
#include "Support/PlayedGame.h"
#include "Timing/PsxTiming.h"
#include "WorldMap/WorldMapScene.h"

#include <catch_amalgamated.hpp>

#include <vector>

using namespace Weltall;
using namespace Weltall::Testing;

namespace
{
	/** A gear ether whose effect drops every party gear: who casts it and in which gear, its place in the list, the effect. */
	struct FGearDropEther
	{
		uint8 Character = 0;
		uint8 Gear = 0;
		uint8 Entry = 0;
		int16 Effect = 0;
		/** The party, the caster among them. */
		std::array<uint8, 3> Party{};
	};

	constexpr std::size_t LearnedSkills = 0x16C0;
	constexpr std::size_t LearnedSize = 0x20;
	constexpr std::size_t GearAbilities = 6;
	constexpr uint16 AllCharacters = 0x1FF;
	/** A forest map whose scripts ask for nothing as a battle is asked for. */
	constexpr uint16 QuietMap = 15;
	/** Era 5's world map (story 186 to 197) and the snowy island's region, whose first formation is two gear-sized enemies. */
	constexpr int16 EraFiveHeader = 98;
	constexpr std::size_t SnowRegion = 13;
	constexpr uint8 GearCharge = 0x12;
	constexpr uint8 GearEther = 0x13;
	constexpr uint8 EtherWindow = 1;
	constexpr int32 FrameLimit = 6000;

	/** What the battle showed of the ether: its gear drop seen, the slots and the actor then, the gears back, the next turn. */
	struct FGearDropSeen
	{
		bool bCast = false;
		bool bDropped = false;
		/** The effect the caster's sequence asked for as its gears went. */
		int16 Effect = 0;
		bool bProxiesFreed = true;
		bool bActorKept = true;
		bool bGearsBack = false;
		bool bNextTurn = false;
	};

	FGearDropSeen PlayGearDropEther(const FPlayedGame& Game, const FIso9660& Disc, const FGearDropEther& Ether)
	{
		FGearDropSeen Seen;
		auto Started = Game.Start(0, nullptr);
		REQUIRE(Started);
		FGameSession& Session = **Started;
		FGameState& State = Session.Game();
		State.SetJoinedCharacters(AllCharacters);
		State.SetParty(Ether.Party);
		uint8 CasterSlot = 0;
		for (uint8 Slot = 0; Slot < 3; ++Slot)
		{
			const uint8 Character = Ether.Party[Slot];
			State.SetCharacterGear(Character, Character == Ether.Character ? Ether.Gear : Character == 0 ? 0 : Character == 1 ? 2 : 3);
			CasterSlot = Character == Ether.Character ? Slot : CasterSlot;
		}
		LittleEndian::WriteU16(State.Block(), LearnedSkills + Ether.Character * LearnedSize + GearAbilities, static_cast<uint16>(0x8000u >> Ether.Entry));
		State.SetCharacterPoints(Ether.Character, FGameState::ECharacterPoints::MaxEp, 999);
		State.SetCharacterPoints(Ether.Character, FGameState::ECharacterPoints::Ep, 999);

		auto Looped = FGameLoop::EnterField(Session, QuietMap);
		REQUIRE(Looped);
		FGameLoop& Loop = **Looped;
		const auto Frame = [&](FPadState Held) {
			const std::vector<FPadState> PerVblank(static_cast<std::size_t>(Loop.VblanksPerFrame()), Held);
			Game.Unheard->Advance(PsxTiming::FrameSeconds * static_cast<double>(PerVblank.size()));
			return Loop.Frame(PerVblank);
		};
		for (int32 Waited = 0; Waited < 100 && !(Loop.Field() != nullptr && Loop.Field()->Runtime().Context().bFieldRunning); ++Waited)
		{
			REQUIRE_FALSE(Frame({}));
		}
		REQUIRE(Loop.Field() != nullptr);

		// The island's battle as a script asks for one, the party riding.
		const auto Scene = ReadWorldMapScene(Disc.Image(), *Game.Discs[0].Index, FWorldMapSceneEntry{EraFiveHeader, 16, 16, 2});
		REQUIRE(Scene);
		REQUIRE(Scene->Regions.size() > SnowRegion);
		const auto& Formation = Scene->Regions[SnowRegion].Formations[0];
		std::copy(Formation.begin(), Formation.end(), Session.Kernel().Formations.begin());
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			State.SetPartySlotInGear(Slot, 1);
		}
		FEventContext& Context = Loop.Field()->Runtime().Context();
		Context.bBattleRequested = true;
		Context.Requests.bScriptedBattle = true;
		Context.Requests.bLeavingField = true;
		Context.Requests.BattleKind = 0;
		Context.Encounters.Formation = 0;
		for (int32 Waited = 0; Waited < 8 && Loop.Program() != EGameProgram::Battle; ++Waited)
		{
			REQUIRE_FALSE(Frame({}));
		}
		REQUIRE(Loop.Program() == EGameProgram::Battle);

		// The caster opens Ether and casts the entry on the first enemy; the others charge.
		for (int32 Fought = 0; Fought < FrameLimit && Loop.Program() == EGameProgram::Battle && !Seen.bNextTurn; ++Fought)
		{
			const FBattleContext& Battle = Loop.Battle()->Runtime().Context();
			const FBattleShow& Show = Loop.Battle()->Presentation().Show;
			const FBattlePartyInput& Party = Battle.Party;
			const EBattlePartyRoutine Top = Party.Depth > 0 ? Party.Routines[Party.Depth - 1].Routine : EBattlePartyRoutine::None;
			if (Show.Playback.DroppedGears() != 0)
			{
				if (!Seen.bDropped)
				{
					// The frame the effect's load dropped the gears: every party slot without its proxy.
					Seen.bDropped = true;
					Seen.Effect = Show.Objects.RequestedEffect;
					for (int32 Slot = 0; Slot < 3; ++Slot)
					{
						Seen.bProxiesFreed = Seen.bProxiesFreed && Show.Battle.SlotSprites[static_cast<std::size_t>(Slot)] == nullptr;
					}
				}
				const FSprite* Actor = Show.Battle.Play.Actor;
				Seen.bActorKept = Seen.bActorKept && Actor != nullptr && FBattleSprites::SlotOf(*Actor) == CasterSlot;
			}
			else if (Seen.bDropped && !Seen.bGearsBack)
			{
				Seen.bGearsBack = Show.Battle.SlotSprites[0] != nullptr && Show.Battle.SlotSprites[1] != nullptr && Show.Battle.SlotSprites[2] != nullptr;
			}
			FPadState Held;
			if ((Fought & 1) == 0)
			{
				if (Party.InputMode == 2)
				{
					Held.Buttons = FPadState::Bit(EPadButton::Cross);
				}
				else if (Top == EBattlePartyRoutine::Turn && Party.MenuState >= 0x10 && Party.MenuState <= 0x18)
				{
					Seen.bNextTurn = Seen.bGearsBack;
					const bool bCasts = Party.Actor == CasterSlot && !Seen.bCast;
					const uint8 Wanted = bCasts ? GearEther : GearCharge;
					Held.Buttons = FPadState::Bit(Party.MenuState == Wanted ? EPadButton::Cross : bCasts ? EPadButton::Up : EPadButton::Left);
				}
				else if (Top == EBattlePartyRoutine::EtherMenu && Party.ListWindow == EtherWindow)
				{
					const int32 At = Party.List.Cell + Party.List.TopRow * 2;
					Held.Buttons = FPadState::Bit(At < Ether.Entry ? EPadButton::Right : At > Ether.Entry ? EPadButton::Left : EPadButton::Cross);
				}
				else if (Top == EBattlePartyRoutine::SelectTarget)
				{
					Seen.bCast = Seen.bCast || Party.Actor == CasterSlot;
					Held.Buttons = FPadState::Bit(EPadButton::Cross);
				}
			}
			if (const auto Error = Frame(Held))
			{
				FAIL("frame " << Fought << ": " << Error->Message);
			}
		}
		return Seen;
	}

	void CheckGearDropEther(const FGearDropEther& Ether)
	{
		REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
		auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
		REQUIRE(Opened);
		const FGearDropSeen Seen = PlayGearDropEther(*Opened, *Disc, Ether);
		CHECK(Seen.bCast);
		// The effect's load frees every party proxy, the actor's too, and the effect starts on the actor as its memory was left.
		CHECK(Seen.bDropped);
		CHECK(Seen.Effect == Ether.Effect);
		CHECK(Seen.bProxiesFreed);
		CHECK(Seen.bActorKept);
		// Once the cast is over the gears are made again with their proxies, and the battle goes on to the next turn.
		CHECK(Seen.bGearsBack);
		CHECK(Seen.bNextTurn);
	}
}

TEST_CASE("Elly's Thermo Largo in Vierge plays on her freed proxy and the gears come back", "[Battle][Content]")
{
	CheckGearDropEther({1, 2, 10, 36, {0, 2, 1}});
}

TEST_CASE("Fei's sixth gear ether in Weltall plays on his freed proxy and the gears come back", "[Battle][Content]")
{
	CheckGearDropEther({0, 0, 5, 13, {0, 1, 2}});
}

TEST_CASE("Emeralda's eighth gear ether in Crescens plays on her freed proxy and the gears come back", "[Battle][Content]")
{
	CheckGearDropEther({6, 9, 7, 138, {0, 6, 1}});
}
