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

#include <catch_amalgamated.hpp>

#include <string_view>
#include <vector>

using namespace Weltall;
using namespace Weltall::Testing;

namespace
{
	constexpr std::size_t LearnedSkills = 0x16C0;
	constexpr std::size_t LearnedSize = 0x20;
	constexpr std::size_t ApPerTurn = 0x17;
	/** The game's flags word whose bit 14 opens the Deathblows of seven AP. */
	constexpr std::size_t DeathblowTierWord = 0x22B6;
	constexpr uint16 SevenApTier = 0x4000;
	constexpr uint16 AllCharacters = 0x1FF;
	constexpr uint8 Citan = 2;
	/** Deathblows 0 to 10 learned, as by the snowy island at story 187. */
	constexpr uint16 CitansDeathblows = 0xFFE0;
	/** The forest's first formation: two enemies on foot. */
	constexpr uint16 ForestMap = 15;
	constexpr uint8 ForestFormation = 1;
	/** Citan's effect codes start at 10h: Tsunami Ice is his fourth effect. */
	constexpr uint8 TsunamiIceCode = 0x13;
	constexpr uint8 AttackCommand = 1;
	constexpr uint8 GuardCommand = 3;
	constexpr uint8 AttackModeState = 5;
	/** The attack mode once the first hit has the command menu fading out. */
	constexpr uint8 FadingAttackModeState = 100;
	constexpr int32 FrameLimit = 4000;

	struct FDeathblowSeen
	{
		bool bStruck = false;
		bool bTsunamiIce = false;
		/** The playback ended and a party member's command ring came up again, or the battle was won. */
		bool bWentOn = false;
	};

	/** Citan strikes the buttons (S square, T triangle, X cross) on his turn; the others guard. */
	FDeathblowSeen PlayDeathblow(const FPlayedGame& Game, std::string_view Buttons)
	{
		FDeathblowSeen Seen;
		auto Started = Game.Start(0, nullptr);
		REQUIRE(Started);
		FGameSession& Session = **Started;
		FGameState& State = Session.Game();
		State.SetJoinedCharacters(AllCharacters);
		State.SetParty({0, Citan, 1});
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			State.SetPartySlotInGear(Slot, 0);
		}
		const std::size_t Skills = LearnedSkills + Citan * LearnedSize;
		LittleEndian::WriteU16(State.Block(), Skills, CitansDeathblows);
		State.Block()[Skills + ApPerTurn] = 7;
		LittleEndian::WriteU16(State.Block(), DeathblowTierWord, static_cast<uint16>(LittleEndian::ReadU16(State.Block(), DeathblowTierWord) | SevenApTier));

		auto Looped = FGameLoop::EnterField(Session, ForestMap);
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
		FEventContext& Context = Loop.Field()->Runtime().Context();
		Context.bBattleRequested = true;
		Context.Requests.bScriptedBattle = true;
		Context.Requests.bLeavingField = true;
		Context.Requests.BattleKind = 0;
		Context.Encounters.Formation = ForestFormation;
		for (int32 Waited = 0; Waited < 8 && Loop.Program() != EGameProgram::Battle; ++Waited)
		{
			REQUIRE_FALSE(Frame({}));
		}
		REQUIRE(Loop.Program() == EGameProgram::Battle);

		for (int32 Fought = 0; Fought < FrameLimit && !Seen.bWentOn; ++Fought)
		{
			if (Loop.Program() != EGameProgram::Battle)
			{
				Seen.bWentOn = Seen.bTsunamiIce;
				break;
			}
			const FBattleContext& Battle = Loop.Battle()->Runtime().Context();
			const FBattlePartyInput& Party = Battle.Party;
			const EBattlePartyRoutine Top = Party.Depth > 0 ? Party.Routines[Party.Depth - 1].Routine : EBattlePartyRoutine::None;
			const bool bCitan = Battle.PartyCharacters[Party.Actor] == Citan;
			for (const FBattlePerformance& Record : Battle.Performances)
			{
				Seen.bTsunamiIce = Seen.bTsunamiIce || (Seen.bStruck && Record.Code == TsunamiIceCode);
			}
			const bool bRing = Top == EBattlePartyRoutine::Turn && Party.MenuState >= 1 && Party.MenuState <= 10 && Party.MenuState != AttackModeState;
			if (bRing && Seen.bTsunamiIce && Party.ButtonCount == 0)
			{
				Seen.bWentOn = true;
				break;
			}
			FPadState Held;
			if ((Fought & 1) == 0)
			{
				if (Party.InputMode == 2 || !Loop.Battle()->Runtime().IsFought())
				{
					Held.Buttons = FPadState::Bit(EPadButton::Cross);
				}
				else if (bRing)
				{
					const bool bStrikes = bCitan && !Seen.bStruck;
					const uint8 Wanted = bStrikes ? AttackCommand : GuardCommand;
					Held.Buttons = FPadState::Bit(Party.MenuState == Wanted ? EPadButton::Cross : bStrikes ? EPadButton::Right : EPadButton::Left);
				}
				else if (Top == EBattlePartyRoutine::Turn && (Party.MenuState == AttackModeState || Party.MenuState == FadingAttackModeState) && bCitan
					&& Party.ButtonCount < Buttons.size())
				{
					const char Button = Buttons[Party.ButtonCount];
					Held.Buttons = FPadState::Bit(Button == 'S' ? EPadButton::Square : Button == 'T' ? EPadButton::Triangle : EPadButton::Cross);
					Seen.bStruck = true;
				}
			}
			if (const auto Error = Frame(Held))
			{
				FAIL("frame " << Fought << ": " << Error->Message);
			}
		}
		return Seen;
	}
}

TEST_CASE("Citan's Tsunami Ice plays to its end while its snowflakes outlive the sprite that spawned them", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	// □ △ △ ×: two AP, one, one and three. Its snowflakes take their goal at their spawner's anchor after the spawner has
	// ended, which the original reads from a stale stack word; they keep the goal they had.
	const FDeathblowSeen Seen = PlayDeathblow(*Opened, "STTX");
	CHECK(Seen.bStruck);
	CHECK(Seen.bTsunamiIce);
	CHECK(Seen.bWentOn);
}
