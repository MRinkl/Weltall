#include "Battle/BattleContext.h"
#include "Battle/BattleResult.h"
#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

using namespace Weltall;

namespace
{
	constexpr uint8 NoPress = 0xFF;
	constexpr std::size_t LearnedSkills = 0x16C0;
	constexpr std::size_t LearnedSkillsSize = 0x20;
	constexpr std::size_t CharactersOffset = 0x26C;
	constexpr std::size_t CharacterSize = 0xA4;
	constexpr std::size_t CharacterExp = 0x3C;
	/** The screen's sounds: the count's tick, the fanfare's three. */
	constexpr uint8 Tick = 0x5B;

	/** A show that keeps the result's cues. */
	class FCueShow final : public IBattleShow
	{
	public:
		struct FMenuCue
		{
			EBattleMenuCue Cue;
			uint8 Slot;
			uint8 Arg;
		};

		void FrameCamera(uint16) override {}
		void SetMessageBox(uint8, uint8) override {}
		void ShowNameBox(uint8) override {}
		void BeginPlayback(bool, uint8, uint8, uint8) override {}
		void CancelPlayback() override {}
		void SetTargetMarkers(uint16) override {}
		void PlaySound(uint8 Effect) override { SystemSounds.push_back(Effect); }
		void BoardGear(uint8) override {}
		void ShowTurnDamage(uint8, const std::array<uint32, 3>&) override {}
		void BeginEvent() override {}
		void LoadBattleBank() override {}
		void PlayEventCue(const FBattleEventCue&) override {}
		bool IsBlocking() const override { return false; }
		void PlayTurnCue(EBattleTurnCue, uint8) override {}
		void PlayMenuCue(EBattleMenuCue Cue, uint8 Slot, uint8 Arg) override { Menu.push_back({Cue, Slot, Arg}); }
		void PlayEndCue(EBattleEndCue Cue, uint8 Arg) override
		{
			if (Cue == EBattleEndCue::ResultSound)
			{
				Sounds.push_back(Arg);
			}
			else if (Cue == EBattleEndCue::StopSounds)
			{
				++Stops;
			}
		}

		int32 Count(EBattleMenuCue Cue) const
		{
			int32 Found = 0;
			for (const FMenuCue& Played : Menu)
			{
				Found += Played.Cue == Cue;
			}
			return Found;
		}

		std::vector<FMenuCue> Menu;
		std::vector<uint8> Sounds;
		std::vector<uint8> SystemSounds;
		int32 Stops = 0;
	};

	/** A battle won by a party of three, its rewards given: nothing gained, nothing learned. */
	struct FWonBattle
	{
		std::unique_ptr<FBattleContext> Context = std::make_unique<FBattleContext>();
		std::unique_ptr<FGameState> State = std::make_unique<FGameState>();
		FCueShow Show;
		FBattleGrowthTable Growth;

		FWonBattle()
		{
			Context->GameState = State.get();
			Context->Show = &Show;
			for (uint8 Slot = 0; Slot < 3; ++Slot)
			{
				Context->PartyCharacters[Slot] = Slot;
				Context->Slots[Slot].Occupant = Slot;
			}
			Context->End = BattleEnd::Won;
			BeginBattleResultScreen(*Context);
		}

		/** The routine to its next frame, then the frame's input step: the button's press taken or not. False once the routine is over. */
		bool Frame(bool bPressed)
		{
			if (!StepBattleResult(*Context, Growth, 0))
			{
				return false;
			}
			if (!Context->Event.bKeyWait)
			{
				Context->Party.Event = NoPress;
			}
			if (StepBattleResultCount(*Context))
			{
				Show.PlayMenuCue(EBattleMenuCue::ResultExpCounters, 0, 0);
				Show.PlayMenuCue(EBattleMenuCue::ResultExpGains, 0, 0);
			}
			if (bPressed)
			{
				Context->Party.Event = BattleMenuEvents::Confirm;
			}
			return true;
		}

		/** The frames the screen takes with the button pressed in every one. */
		int32 FramesPressing()
		{
			int32 Frames = 0;
			while (Frames < 1000 && Frame(true))
			{
				++Frames;
			}
			return Frames;
		}
	};
}

TEST_CASE("The result screen takes twenty-four frames when nothing holds it: its panels' six, the count, each member, the spoils, its windows' closes", "[Battle]")
{
	FWonBattle Battle;
	CHECK(Battle.FramesPressing() == 24);
	const FBattleResultRun& Run = Battle.Context->ResultRun;
	CHECK_FALSE(Run.bRunning);
	CHECK_FALSE(Run.bScreenWork);
	CHECK(Battle.Context->End == BattleEnd::Won);
	// The panels' six parts, then the counters and what is left once more as the count ends, and the new levels.
	CHECK(Battle.Show.Count(EBattleMenuCue::ResultPortraits) == 1);
	CHECK(Battle.Show.Count(EBattleMenuCue::ResultPanelLabels) == 1);
	CHECK(Battle.Show.Count(EBattleMenuCue::ResultHpEp) == 1);
	CHECK(Battle.Show.Count(EBattleMenuCue::ResultLevels) == 2);
	CHECK(Battle.Show.Count(EBattleMenuCue::ResultExpCounters) == 2);
	CHECK(Battle.Show.Count(EBattleMenuCue::ResultExpGains) == 2);
	// No level gained and nothing learned: no window for a member, no fanfare; the spoils' chime and the sounds' stop.
	CHECK(Battle.Show.Count(EBattleMenuCue::ResultLevelUp) == 0);
	CHECK(Battle.Show.Count(EBattleMenuCue::ResultLearned) == 0);
	CHECK(Battle.Show.Count(EBattleMenuCue::ResultSpoils) == 1);
	CHECK(Battle.Show.Sounds.empty());
	CHECK(Battle.Show.SystemSounds == std::vector<uint8>{Tick});
	CHECK(Battle.Show.Stops == 1);
}

TEST_CASE("The result screen waits for the confirm button with its panels up, and a press before the wait does not pass it", "[Battle]")
{
	FWonBattle Battle;
	// A press while the panels build is forgotten by the next frame's input step.
	for (int32 Frame = 0; Frame < 6; ++Frame)
	{
		REQUIRE(Battle.Frame(Frame == 2));
	}
	for (int32 Frame = 0; Frame < 50; ++Frame)
	{
		REQUIRE(Battle.Frame(false));
	}
	const FBattleResultRun& Run = Battle.Context->ResultRun;
	CHECK(Run.bPanelsShown);
	CHECK(Battle.Context->Event.bKeyWait);
	CHECK_FALSE(Run.bCounting);
	CHECK(Battle.Context->End == BattleEnd::Running);
	// The press passes this wait alone: a frame, then the count starts and, with nothing to add, ends at its first step.
	REQUIRE(Battle.Frame(true));
	REQUIRE(Battle.Frame(false));
	REQUIRE(Battle.Frame(false));
	CHECK_FALSE(Run.bCounting);
	CHECK(Run.bExpCounted[0]);
	CHECK(Battle.Show.Sounds == std::vector<uint8>{Tick});
	// The count's end waits for the button again.
	for (int32 Frame = 0; Frame < 20; ++Frame)
	{
		REQUIRE(Battle.Frame(false));
	}
	CHECK(Battle.Context->Event.bKeyWait);
	CHECK(Battle.Show.Count(EBattleMenuCue::ResultLevels) == 2);
}

TEST_CASE("The result screen counts the EXP up one a step for every member at once, ticking each frame, until the largest gain is counted", "[Battle]")
{
	FWonBattle Battle;
	Battle.Context->StartFigures[0].Exp = 100;
	Battle.Context->StartFigures[2].Exp = 7;
	Battle.Context->Result.ExpGains = {3, 0, 5};
	LittleEndian::WriteU32(Battle.State->Block(), CharactersOffset + CharacterExp, 103);
	LittleEndian::WriteU32(Battle.State->Block(), CharactersOffset + 2 * CharacterSize + CharacterExp, 12);
	for (int32 Frame = 0; Frame < 7; ++Frame)
	{
		REQUIRE(Battle.Frame(false));
	}
	// The press that starts the count, the frame it passes the wait in, and the count's first frame.
	REQUIRE(Battle.Frame(true));
	REQUIRE(Battle.Frame(false));
	REQUIRE(Battle.Frame(false));
	const FBattleResultRun& Run = Battle.Context->ResultRun;
	REQUIRE(Run.bCounting);
	int32 Steps = 1;
	while (Run.bCounting && Steps < 100)
	{
		CHECK(Battle.Context->StartFigures[0].Exp == 100u + static_cast<uint32>(std::min(Steps, 3)));
		CHECK(Battle.Context->StartFigures[2].Exp == 7u + static_cast<uint32>(std::min(Steps, 5)));
		REQUIRE(Battle.Frame(false));
		++Steps;
	}
	// The largest gain's steps and one more to find every member done; a tick for each frame the count ran.
	CHECK(Steps == 6);
	CHECK(Battle.Show.Sounds == std::vector<uint8>(6, Tick));
	CHECK(Battle.Context->Result.ExpGains == std::array<uint32, 3>{0, 0, 0});

	// A press cuts a count short: the counters take the records' EXP.
	FWonBattle Cut;
	Cut.Context->Result.ExpGains = {900, 0, 0};
	LittleEndian::WriteU32(Cut.State->Block(), CharactersOffset + CharacterExp, 900);
	for (int32 Frame = 0; Frame < 7; ++Frame)
	{
		REQUIRE(Cut.Frame(false));
	}
	REQUIRE(Cut.Frame(true));
	REQUIRE(Cut.Frame(false));
	REQUIRE(Cut.Frame(true));
	REQUIRE(Cut.Frame(false));
	CHECK_FALSE(Cut.Context->ResultRun.bCounting);
	CHECK(Cut.Context->StartFigures[0].Exp == 900);
	CHECK(Cut.Context->Result.ExpGains[0] == 0);
}

TEST_CASE("A level gained or a skill learned shows in the level-ups' window, a frame and a wait each, with the fanfare once", "[Battle]")
{
	// A level for the first member: a frame for its window, and the fanfare.
	FWonBattle Leveled;
	Leveled.Context->Result.LevelsGained[0] = {1, 0};
	CHECK(Leveled.FramesPressing() == 25);
	CHECK(Leveled.Show.Count(EBattleMenuCue::ResultLevelUp) == 1);
	CHECK(Leveled.Show.Sounds == std::vector<uint8>{0x5C, 0x5D, 0x5E});
	CHECK(Leveled.Context->ResultRun.bFanfarePlayed);

	// A Deathblow and an ability learned by the second, a level gained by the third: the window opened again for the
	// messages (a frame), a frame a message, the Deathblow's first; the fanfare still once.
	FWonBattle Learned;
	LittleEndian::WriteU16(Learned.State->Block(), LearnedSkills + LearnedSkillsSize, 0xC000);
	LittleEndian::WriteU16(Learned.State->Block(), LearnedSkills + LearnedSkillsSize + 2, 0x0101);
	Learned.Context->SkillsAtStart[1] = {0x8000, 0x0100};
	Learned.Context->Result.LevelsGained[2] = {2, 0};
	CHECK(Learned.FramesPressing() == 28);
	CHECK(Learned.Show.Count(EBattleMenuCue::ResultLearnedPortrait) == 1);
	std::vector<uint8> Skills;
	for (const FCueShow::FMenuCue& Cue : Learned.Show.Menu)
	{
		if (Cue.Cue == EBattleMenuCue::ResultLearned)
		{
			CHECK(Cue.Slot == 1);
			Skills.push_back(Cue.Arg);
		}
	}
	CHECK(Skills == std::vector<uint8>{1, 0x80 | 15});
	CHECK(Learned.Show.Count(EBattleMenuCue::ResultLevelUp) == 1);
	CHECK(Learned.Show.Sounds == std::vector<uint8>{0x5C, 0x5D, 0x5E});
}

TEST_CASE("The result screen lists the drops and gives them as its spoils appear", "[Battle]")
{
	FWonBattle Battle;
	Battle.Context->Result.DropKinds = {BattleDropKinds::Item, BattleDropKinds::Item, BattleDropKinds::Weapon, 0, 0, 0, 0, 0};
	Battle.Context->Result.DropIds = {5, 5, 9, 0, 0, 0, 0, 0};
	CHECK(Battle.FramesPressing() == 24);
	const FBattleResultWork& Result = Battle.Context->Result;
	CHECK(Result.DropGroups[0].Id == 5);
	CHECK(Result.DropGroups[0].Count == 2);
	CHECK(Result.DropGroups[1].Kind == BattleDropKinds::Weapon);
	CHECK(Result.DropGroups[1].Count == 1);
	CHECK(Result.DropGroups[2].Id == 0);
}
