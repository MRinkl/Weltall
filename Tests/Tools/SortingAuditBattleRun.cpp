#include "Tools/SortingAuditBattleRun.h"

#include "Battle/BattleRuntime.h"
#include "Battle/BattleScene.h"
#include "Battle/Show/BattlePresentation.h"
#include "Field/FieldMap.h"
#include "Game/BattleGameProgram.h"
#include "Game/FieldProgram.h"
#include "Game/GameState.h"
#include "Support/XorShift32.h"
#include "Timing/PsxTiming.h"

#include <algorithm>
#include <set>
#include <tuple>

namespace Weltall::Testing
{
	namespace
	{
		/** A forest map on each disc whose scripts ask for nothing as a battle is asked for. */
		constexpr std::array<uint16, 2> QuietMaps = {15, 15};
		constexpr uint16 AllCharacters = 0x1FF;
		constexpr std::array<uint8, 3> Party = {0, 1, 2};
		/** Each character's own first gear: Weltall, Vierge, Heimdal, Brigandier, Renmazuo, Stier, Crescens, Chu-Chu, Seibzehn. */
		constexpr std::array<uint8, 9> OwnGears = {0, 2, 3, 4, 5, 6, 9, 7, 8};
		constexpr int32 FormationCount = 16;
		/** A type's bit 7: a gear-sized enemy. */
		constexpr uint8 GearSized = 0x80;
		/** The frames after the battle's start a frame is audited at, every so many, to the battle's end or the last. */
		constexpr int32 LookEvery = 30;
		constexpr int32 LastFrame = 2400;
		/** The pad's buttons, drawn at random: Cross mostly, Circle now and then, the directions, the attacks. */
		constexpr std::array<EPadButton, 18> Buttons = {EPadButton::Cross, EPadButton::Cross, EPadButton::Cross, EPadButton::Cross,
			EPadButton::Cross, EPadButton::Cross, EPadButton::Cross, EPadButton::Cross, EPadButton::Circle, EPadButton::Circle, EPadButton::Up,
			EPadButton::Right, EPadButton::Down, EPadButton::Left, EPadButton::Triangle, EPadButton::Square, EPadButton::Triangle,
			EPadButton::Square};

		bool HasEnemies(const FBattleFormation& Formation)
		{
			return std::any_of(Formation.EnemyTypes.begin(), Formation.EnemyTypes.end(),
				[](uint8 Type) { return (Type & ~GearSized) != FBattleFormation::EmptyType; });
		}

		/** The pad of a player who presses at random: a button held for a frame or three, then nothing for up to six. */
		class FRandomPad
		{
		public:
			explicit FRandomPad(uint32 Seed) : Random(Seed) {}

			FPadState Next()
			{
				if (Left == 0)
				{
					bPressed = !bPressed;
					Held = bPressed ? FPadState::Bit(Buttons[Random.Next() % Buttons.size()]) : uint16{0};
					Left = bPressed ? 1 + static_cast<int32>(Random.Next() % 3) : 1 + static_cast<int32>(Random.Next() % 6);
				}
				--Left;
				return FPadState{Held};
			}

		private:
			FXorShift32 Random;
			uint16 Held = 0;
			int32 Left = 0;
			bool bPressed = false;
		};
	}

	std::string FBattleSortingEntry::Place() const
	{
		return "b" + std::to_string(Disc + 1) + "." + std::to_string(Map) + "." + std::to_string(Formation);
	}

	bool FBattleSortingEntry::InGears() const
	{
		const FBattleFormation Read = FBattleFormation::FromRecord(Record);
		return std::any_of(Read.EnemyTypes.begin(), Read.EnemyTypes.end(),
			[](uint8 Type) { return (Type & ~GearSized) != FBattleFormation::EmptyType && (Type & GearSized) != 0; });
	}

	std::vector<FBattleSortingEntry> FBattleSortingRun::Battles(const FPlayedGame& Game)
	{
		std::vector<FBattleSortingEntry> Found;
		std::set<std::tuple<uint8, uint8, bool>> Seen;
		for (int32 Disc = 0; Disc < static_cast<int32>(Game.Discs.size()); ++Disc)
		{
			const FPlayedDisc& Read = Game.Discs[static_cast<std::size_t>(Disc)];
			if (Read.Iso == nullptr)
			{
				continue;
			}
			for (int32 Map = 0; Map < FieldMapCount; ++Map)
			{
				const auto Archive = LoadFieldArchive(Read.Iso->Image(), *Read.Index, Map);
				if (!Archive)
				{
					continue;
				}
				const std::vector<uint8>& Encounters = Archive->Section(EFieldSection::Encounters);
				for (int32 Formation = 0; Formation < FormationCount; ++Formation)
				{
					const std::size_t At = static_cast<std::size_t>(Formation) * FBattleFormation::RecordSize;
					if (At + FBattleFormation::RecordSize > Encounters.size())
					{
						break;
					}
					FBattleSortingEntry Battle;
					Battle.Disc = Disc;
					Battle.Map = static_cast<uint16>(Map);
					Battle.Formation = static_cast<uint8>(Formation);
					std::copy_n(Encounters.begin() + static_cast<std::ptrdiff_t>(At), Battle.Record.size(), Battle.Record.begin());
					const FBattleFormation Parsed = FBattleFormation::FromRecord(Battle.Record);
					if (HasEnemies(Parsed) && Seen.insert({Parsed.Scene, Parsed.Stage, Battle.InGears()}).second)
					{
						Found.push_back(Battle);
					}
				}
			}
		}
		return Found;
	}

	std::optional<FError> FBattleSortingRun::Run()
	{
		auto Heard = FGameSoundSession::Start(*Game.Discs[0].Iso);
		if (!Heard)
		{
			return Heard.Error();
		}
		Sound = std::move(*Heard);
		Unheard = std::make_unique<FUnheardGameSound>(*Sound->Sound());
		auto Started = Game.Start(Entry.Disc, nullptr, Sound.get());
		if (!Started)
		{
			return Started.Error();
		}
		Session = std::move(*Started);
		FGameState& State = Session->Game();
		State.SetJoinedCharacters(AllCharacters);
		State.SetParty(Party);
		for (std::size_t Character = 0; Character < OwnGears.size(); ++Character)
		{
			State.SetCharacterGear(static_cast<int32>(Character), OwnGears[Character]);
		}
		auto Loop = FGameLoop::EnterField(*Session, QuietMaps[static_cast<std::size_t>(Entry.Disc)]);
		if (!Loop)
		{
			return Loop.Error();
		}
		GameLoop = std::move(*Loop);
		for (int32 Waited = 0; Waited < 100 && !(GameLoop->Field() != nullptr && GameLoop->Field()->Runtime().Context().bFieldRunning); ++Waited)
		{
			if (auto Error = Frame({}))
			{
				return Error;
			}
		}
		if (GameLoop->Field() == nullptr || !GameLoop->Field()->Runtime().Context().bFieldRunning)
		{
			return MakeError("the quiet map did not start");
		}
		// As a script asks for a battle: the formation in the table's first place, the party riding against gears.
		std::copy(Entry.Record.begin(), Entry.Record.end(), Session->Kernel().Formations.begin());
		for (int32 Slot = 0; Slot < static_cast<int32>(Party.size()); ++Slot)
		{
			State.SetPartySlotInGear(Slot, Entry.InGears() ? 1 : 0);
		}
		FEventContext& Context = GameLoop->Field()->Runtime().Context();
		Context.bBattleRequested = true;
		Context.Requests.bScriptedBattle = true;
		Context.Requests.bLeavingField = true;
		Context.Requests.BattleKind = 0;
		Context.Encounters.Formation = 0;
		for (int32 Waited = 0; Waited < 8 && GameLoop->Program() != EGameProgram::Battle; ++Waited)
		{
			if (auto Error = Frame({}))
			{
				return Error;
			}
		}
		if (GameLoop->Program() != EGameProgram::Battle)
		{
			return MakeError("the battle did not start");
		}
		Report.emplace(Rows, Place(), static_cast<uint32>(Options.Scale * Options.Scale), bPictures);
		FRandomPad Pad(static_cast<uint32>(Entry.Disc * 100000 + Entry.Map * 16 + Entry.Formation + 1));
		const int32 Runs = GameLoop->ProgramRuns();
		for (int32 Fought = 0; Fought < LastFrame && GameLoop->ProgramRuns() == Runs; ++Fought)
		{
			if (auto Error = Frame(Pad.Next()))
			{
				return Error;
			}
			if (Fought % LookEvery == 0)
			{
				Look("f" + std::to_string(Fought));
			}
		}
		return std::nullopt;
	}

	void FBattleSortingRun::WritePictures(const std::filesystem::path& Folder) const
	{
		if (Report)
		{
			Report->WritePictures(Folder);
		}
	}

	std::optional<FError> FBattleSortingRun::Frame(FPadState Held)
	{
		const std::vector<FPadState> PerVblank(static_cast<std::size_t>(GameLoop->VblanksPerFrame()), Held);
		Unheard->Advance(PsxTiming::FrameSeconds * static_cast<double>(PerVblank.size()));
		return GameLoop->Frame(PerVblank);
	}

	void FBattleSortingRun::Look(const std::string& Shot)
	{
		const FBattleGameProgram* Battle = GameLoop->Battle();
		if (Battle == nullptr)
		{
			return;
		}
		// The show's own frames: not the transitions' pictures, not before the show has begun.
		const FBattleRuntime& Runtime = Battle->Runtime();
		const FBattlePresentation& Presentation = Battle->Presentation();
		if (Runtime.DefaultTransition() != nullptr || Runtime.SetupTransition() != nullptr || !Presentation.HasBegun())
		{
			return;
		}
		const FBattleShowFrame& Shown = Presentation.Show.Frame();
		if (Shown.Vram == nullptr)
		{
			return;
		}
		const FSortingFrameAudit Audit = AuditBattleSorting(Shown, Options);
		Report->Add(Shot, {}, Audit, [&](uint32 Number) { return DescribeBattleSortingPiece(Shown, Number, Audit.Pieces[Number]); });
	}
}
