#include "Battle/BattleGameState.h"
#include "Battle/BattleProgramShow.h"
#include "Battle/BattleResult.h"
#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"

namespace Weltall
{
	namespace
	{
		/** Where the result's routine stands between two frames. */
		enum EResultPc : uint8
		{
			ResultDone,
			LoadSecondFrame,
			Loaded,
			ScreenBegin,
			PanelsBegin,
			PanelFrame,
			PanelBuild,
			PanelsWait,
			CountBegin,
			CountLoop,
			LevelsWaitBegin,
			LevelsWait,
			LevelWindow,
			Member,
			MemberWaitBegin,
			MemberWait,
			Learned,
			LearnedOpen,
			LearnedNext,
			LearnedWaitBegin,
			LearnedWait,
			MemberEnd,
			Spoils,
			SpoilsItemWindow,
			SpoilsGoldWindow,
			SpoilsFrame,
			SpoilsBuild,
			SpoilsWait,
			SpoilsCloseItems,
			SpoilsCloseLevels,
			SpoilsCloseGold,
			ScreenEnd,
			ScreenFree,
			CloseNameWindow,
			CloseBoxWindow,
		};

		/** The screen's sounds, of the battle's common effects: the count's tick and the fanfare's three. */
		constexpr uint8 TickSound = 0x5B;
		constexpr std::array<uint8, 3> FanfareSounds{0x5C, 0x5D, 0x5E};
		/** The game state's flag that joins the ether level to the level: clear, the panels show a row for each. */
		constexpr uint16 JoinedLevels = 0x8000;
		/** The menu's windows the screen uses: the items', the level-ups', the gold's; and the two behind the message boxes. */
		constexpr uint8 ItemWindow = 0;
		constexpr uint8 LevelWindowIndex = 1;
		constexpr uint8 GoldWindow = 2;
		constexpr uint8 BoxWindow = 4;
		constexpr uint8 NameWindow = 5;
		constexpr int32 PanelSteps = 6;
		constexpr int32 SkillCount = 16;
		constexpr uint8 AbilityFlag = 0x80;

		void MenuCue(FBattleContext& Context, EBattleMenuCue Cue, uint8 Slot = 0, uint8 Arg = 0)
		{
			if (Context.Show != nullptr)
			{
				Context.Show->PlayMenuCue(Cue, Slot, Arg);
			}
		}

		void EndCue(FBattleContext& Context, EBattleEndCue Cue, uint8 Arg = 0)
		{
			if (Context.Show != nullptr)
			{
				Context.Show->PlayEndCue(Cue, Arg);
			}
		}

		bool Confirmed(const FBattleContext& Context)
		{
			return Context.Party.Event == BattleMenuEvents::Confirm;
		}

		void ShowWindow(FBattleContext& Context, uint8 Window, bool bShown)
		{
			Context.ResultRun.bWindowShown[Window] = bShown;
			MenuCue(Context, EBattleMenuCue::ResultWindow, Window, bShown ? 1 : 0);
		}

		/** The window made at its size and shown at once. */
		void OpenWindow(FBattleContext& Context, uint8 Window)
		{
			Context.ResultRun.bWindowShown[Window] = true;
			MenuCue(Context, EBattleMenuCue::ResultOpenWindow, Window);
		}

		void CloseWindow(FBattleContext& Context, uint8 Window)
		{
			if (Window < Context.ResultRun.bWindowShown.size())
			{
				Context.ResultRun.bWindowShown[Window] = false;
			}
			MenuCue(Context, EBattleMenuCue::CloseListWindow, 0, Window);
		}

		/** The fanfare, once a result screen; the next battle of a chain raises the music it lowered. */
		void PlayFanfare(FBattleContext& Context)
		{
			FBattleResultRun& Run = Context.ResultRun;
			if (Run.bFanfarePlayed)
			{
				return;
			}
			for (const uint8 Sound : FanfareSounds)
			{
				EndCue(Context, EBattleEndCue::ResultSound, Sound);
			}
			Run.bFanfarePlayed = true;
		}

		void BuildPanelStep(FBattleContext& Context, int32 Step)
		{
			switch (Step)
			{
			case 0:
				MenuCue(Context, EBattleMenuCue::ResultPortraits);
				Context.ResultRun.bPanelsShown = true;
				Context.bActionsPlayed = false;
				break;
			case 1: MenuCue(Context, EBattleMenuCue::ResultPanelLabels); break;
			case 2: MenuCue(Context, EBattleMenuCue::ResultHpEp); break;
			case 3: MenuCue(Context, EBattleMenuCue::ResultLevels, 0, 0); break;
			case 4: MenuCue(Context, EBattleMenuCue::ResultExpCounters); break;
			default: MenuCue(Context, EBattleMenuCue::ResultExpGains); break;
			}
		}

		/** The count's end: each member's counter at its record's EXP, nothing left to add. */
		void FinishCount(FBattleContext& Context)
		{
			using namespace GameStateLayout;
			for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				// The original reads the record of whatever id the slot holds, an empty slot's from past the characters'.
				const uint8 Character = Context.PartyCharacters[Slot];
				if (BattleGameState::HasCharacter(Character))
				{
					const std::span<const uint8> Record = BattleGameState::CharacterRecord(*Context.GameState, Character);
					Context.StartFigures[Slot].Exp = LittleEndian::ReadU32(Record, Character::Exp);
					Context.StartFigures[Slot].EtherExp = LittleEndian::ReadU32(Record, Character::EtherExp);
				}
				Context.Result.ExpGains[Slot] = 0;
				Context.Result.EtherExpGains[Slot] = 0;
			}
			MenuCue(Context, EBattleMenuCue::ResultExpCounters);
			MenuCue(Context, EBattleMenuCue::ResultExpGains);
		}

		/** The Deathblows or abilities the member learned in the battle: bit 8000h >> n for skill n. */
		uint16 LearnedSkills(const FBattleContext& Context, std::size_t Slot, bool bAbilities)
		{
			using namespace GameStateLayout;
			const uint8 Character = Context.PartyCharacters[Slot];
			if (!BattleGameState::HasCharacter(Character))
			{
				return 0;
			}
			const uint16 Now = BattleGameState::Word(*Context.GameState, BattleGameState::SkillsOffset(Character, bAbilities ? Skills::Abilities : Skills::Deathblows));
			return static_cast<uint16>(Now & ~Context.SkillsAtStart[Slot][bAbilities ? 1 : 0]);
		}
	}

	bool BattleGivesRewards(const FBattleContext& Context, uint8 ModeResult)
	{
		constexpr uint8 UnusedResult = 0x21;
		return (Context.End & 0xC0) == 0 && Context.End != UnusedResult && !Context.Event.Requests.bEndedBattle && ModeResult != BattleModeResult::ScriptedDefeat;
	}

	void BeginBattleResult(FBattleContext& Context)
	{
		FBattleResultRun& Run = Context.ResultRun;
		Run = FBattleResultRun{};
		Run.bBegun = true;
		Run.bRunning = true;
		Context.Party.InputMode = BattleInputModes::Result;
		MenuCue(Context, EBattleMenuCue::ResultBegin);
		Run.Pc = LoadSecondFrame;
	}

	void BeginBattleResultScreen(FBattleContext& Context)
	{
		FBattleResultRun& Run = Context.ResultRun;
		Run = FBattleResultRun{};
		Run.bBegun = true;
		Run.bRunning = true;
		Run.bScreen = true;
		Context.Party.InputMode = BattleInputModes::Result;
		Run.Pc = ScreenBegin;
	}

	bool StepBattleResult(FBattleContext& Context, const FBattleGrowthTable& Growth, uint8 ModeResult)
	{
		FBattleResultRun& Run = Context.ResultRun;
		while (Run.bRunning && Context.Fault == EBattleFault::None)
		{
			switch (Run.Pc)
			{
			case LoadSecondFrame:
				Run.Pc = Loaded;
				return true;
			case Loaded:
				// The result's files are read: its sheet, images and portraits, the labels typed anew.
				MenuCue(Context, EBattleMenuCue::ResultData);
				WriteBackBattleItems(Context);
				if (BattleGivesRewards(Context, ModeResult))
				{
					if (Context.Show != nullptr)
					{
						Context.Show->LoadBattleBank();
					}
					Run.bScreen = GiveBattleRewards(Context, Growth);
				}
				Run.Pc = Run.bScreen ? ScreenBegin : CloseNameWindow;
				break;
			case ScreenBegin:
				Context.Event.bKeyWait = false;
				Run.bScreenWork = true;
				MenuCue(Context, EBattleMenuCue::ResultScreen);
				Run.bEtherRows = (BattleGameState::Word(*Context.GameState, GameStateLayout::StoryFlagsOffset) & JoinedLevels) == 0;
				Run.Pc = PanelsBegin;
				return true;
			case PanelsBegin:
				Run.SavedEnd = Context.End;
				Context.End = BattleEnd::Running;
				Run.Step = 0;
				Run.Pc = PanelFrame;
				break;
			case PanelFrame:
				Run.Pc = PanelBuild;
				return true;
			case PanelBuild:
				BuildPanelStep(Context, Run.Step);
				if (++Run.Step < PanelSteps)
				{
					Run.Pc = PanelFrame;
					break;
				}
				Context.Event.bKeyWait = true;
				Run.Pc = PanelsWait;
				break;
			case PanelsWait:
				if (!Confirmed(Context))
				{
					return true;
				}
				Context.Event.bKeyWait = false;
				Run.Pc = CountBegin;
				return true;
			case CountBegin:
				Context.Event.bKeyWait = true;
				Run.bCounting = true;
				Run.Pc = CountLoop;
				break;
			case CountLoop:
				if (Run.bCounting && !Confirmed(Context))
				{
					EndCue(Context, EBattleEndCue::ResultSound, TickSound);
					return true;
				}
				Context.Event.bKeyWait = false;
				Run.bCounting = false;
				FinishCount(Context);
				MenuCue(Context, EBattleMenuCue::ResultLevels, 0, 1);
				Run.Pc = LevelsWaitBegin;
				return true;
			case LevelsWaitBegin:
				Context.Event.bKeyWait = true;
				Run.Pc = LevelsWait;
				break;
			case LevelsWait:
				if (!Confirmed(Context))
				{
					return true;
				}
				Context.Event.bKeyWait = false;
				Run.Pc = LevelWindow;
				return true;
			case LevelWindow:
				// The level-ups' window is made here and hidden until a member has something to show in it.
				OpenWindow(Context, LevelWindowIndex);
				ShowWindow(Context, LevelWindowIndex, false);
				Run.Slot = 0;
				Run.Pc = Member;
				break;
			case Member:
				if (Run.Slot >= FBattleContext::PartySlotCount)
				{
					Run.Pc = Spoils;
					break;
				}
				if (Context.Slots[Run.Slot].Occupant == FBattleSlot::Empty)
				{
					++Run.Slot;
					break;
				}
				Run.Pc = MemberWaitBegin;
				if (Context.Result.LevelsGained[Run.Slot][0] != 0)
				{
					ShowWindow(Context, LevelWindowIndex, true);
					MenuCue(Context, EBattleMenuCue::ResultLevelUp, Run.Slot);
					Run.bLevelUpShown = true;
					PlayFanfare(Context);
					Context.Event.bKeyWait = false;
					return true;
				}
				break;
			case MemberWaitBegin:
				Context.Event.bKeyWait = true;
				Run.Pc = MemberWait;
				break;
			case MemberWait:
				if (Context.Result.LevelsGained[Run.Slot][0] != 0 && !Confirmed(Context))
				{
					return true;
				}
				Run.bLevelUpShown = false;
				Run.Pc = Learned;
				return true;
			case Learned:
				if (LearnedSkills(Context, Run.Slot, false) == 0 && LearnedSkills(Context, Run.Slot, true) == 0)
				{
					Run.Pc = MemberEnd;
					break;
				}
				Run.Pc = LearnedOpen;
				if (!Run.bWindowShown[LevelWindowIndex])
				{
					// Hidden, the window is made again for the message, and takes a frame.
					OpenWindow(Context, LevelWindowIndex);
					return true;
				}
				break;
			case LearnedOpen:
				MenuCue(Context, EBattleMenuCue::ResultLearnedPortrait, Run.Slot);
				Context.Event.bKeyWait = false;
				Run.Skill = 0;
				Run.bAbilities = false;
				Run.Pc = LearnedNext;
				break;
			case LearnedNext:
			{
				const uint16 Skills = LearnedSkills(Context, Run.Slot, Run.bAbilities);
				while (Run.Skill < SkillCount && (Skills & (0x8000u >> Run.Skill)) == 0)
				{
					++Run.Skill;
				}
				if (Run.Skill >= SkillCount)
				{
					if (!Run.bAbilities)
					{
						Run.bAbilities = true;
						Run.Skill = 0;
						break;
					}
					Run.bLearnedShown = false;
					Run.Pc = MemberEnd;
					break;
				}
				MenuCue(Context, EBattleMenuCue::ResultLearned, Run.Slot, static_cast<uint8>(Run.Skill | (Run.bAbilities ? AbilityFlag : 0)));
				Run.bLearnedShown = true;
				PlayFanfare(Context);
				++Run.Skill;
				Run.Pc = LearnedWaitBegin;
				return true;
			}
			case LearnedWaitBegin:
				Context.Event.bKeyWait = true;
				Run.Pc = LearnedWait;
				break;
			case LearnedWait:
				if (!Confirmed(Context))
				{
					return true;
				}
				Context.Event.bKeyWait = false;
				Run.Pc = LearnedNext;
				break;
			case MemberEnd:
				Context.Event.bKeyWait = false;
				ShowWindow(Context, LevelWindowIndex, false);
				++Run.Slot;
				Run.Pc = Member;
				break;
			case Spoils:
				Run.bPanelsShown = false;
				Run.bLevelUpShown = false;
				Run.bLearnedShown = false;
				Run.Pc = SpoilsItemWindow;
				return true;
			case SpoilsItemWindow:
				OpenWindow(Context, ItemWindow);
				Run.Pc = SpoilsGoldWindow;
				return true;
			case SpoilsGoldWindow:
				OpenWindow(Context, GoldWindow);
				Run.Pc = SpoilsFrame;
				return true;
			case SpoilsFrame:
				Run.Pc = SpoilsBuild;
				return true;
			case SpoilsBuild:
				// The drops are listed, then given.
				GiveBattleDrops(Context);
				MenuCue(Context, EBattleMenuCue::ResultSpoils);
				Run.bSpoilsShown = true;
				if (Context.Show != nullptr)
				{
					Context.Show->PlaySound(TickSound);
				}
				Context.Event.bKeyWait = true;
				Run.Pc = SpoilsWait;
				break;
			case SpoilsWait:
				if (!Confirmed(Context))
				{
					return true;
				}
				Context.Event.bKeyWait = false;
				Run.bSpoilsShown = false;
				ShowWindow(Context, ItemWindow, false);
				ShowWindow(Context, LevelWindowIndex, false);
				ShowWindow(Context, GoldWindow, false);
				Run.Pc = SpoilsCloseItems;
				return true;
			case SpoilsCloseItems:
				CloseWindow(Context, ItemWindow);
				Run.Pc = SpoilsCloseLevels;
				return true;
			case SpoilsCloseLevels:
				CloseWindow(Context, LevelWindowIndex);
				Run.Pc = SpoilsCloseGold;
				return true;
			case SpoilsCloseGold:
				CloseWindow(Context, GoldWindow);
				Run.Pc = ScreenEnd;
				return true;
			case ScreenEnd:
				Run.bPanelsShown = false;
				Run.bLevelUpShown = false;
				Run.bLearnedShown = false;
				Run.Pc = ScreenFree;
				return true;
			case ScreenFree:
				Context.End = Run.SavedEnd;
				Run.bScreenWork = false;
				EndCue(Context, EBattleEndCue::StopSounds);
				Run.Pc = CloseNameWindow;
				break;
			case CloseNameWindow:
				CloseWindow(Context, NameWindow);
				Run.Pc = CloseBoxWindow;
				return true;
			case CloseBoxWindow:
				CloseWindow(Context, BoxWindow);
				Run.Pc = ResultDone;
				return true;
			default:
				Run.bRunning = false;
				return false;
			}
		}
		Run.bRunning = false;
		return false;
	}

	bool StepBattleResultCount(FBattleContext& Context)
	{
		FBattleResultRun& Run = Context.ResultRun;
		if (!Run.bPanelsShown || !Run.bCounting)
		{
			return false;
		}
		bool bDone = true;
		for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
		{
			if (!Run.bExpCounted[Slot])
			{
				if (Context.Result.ExpGains[Slot] == 0)
				{
					Run.bExpCounted[Slot] = true;
				}
				else
				{
					--Context.Result.ExpGains[Slot];
					++Context.StartFigures[Slot].Exp;
				}
			}
			if (!Run.bEtherCounted[Slot])
			{
				if (Context.Result.EtherExpGains[Slot] == 0)
				{
					Run.bEtherCounted[Slot] = true;
				}
				else
				{
					--Context.Result.EtherExpGains[Slot];
					++Context.StartFigures[Slot].EtherExp;
				}
			}
			bDone = bDone && Run.bExpCounted[Slot] && (!Run.bEtherRows || Run.bEtherCounted[Slot]);
		}
		if (bDone)
		{
			Run.bCounting = false;
			return false;
		}
		return true;
	}
}
