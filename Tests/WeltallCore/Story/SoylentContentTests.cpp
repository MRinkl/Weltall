#include "Field/FieldRuntime.h"
#include "Game/FieldProgram.h"
#include "Story/StoryPacks.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/PortStory.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace Weltall;

namespace
{
	/**
	 * Krelian's laboratory (map 673 by entrance 1) ends with Fei, Elly and Citan in the party and goes on to the stills
	 * of the Soylent System facility (maps 621 to 625, Elly narrating), which go on to the Sufal Mass's room (map 622 by
	 * entrance 0: the narration's end, the fight, Elly's blood and her words). The expanded second disc leaves the
	 * laboratory for the facility's gate hall, a map of its own, with the story at the facility's own value; the holding
	 * hall and the processing line, maps of their own, follow, and the Sufal Mass breaks out at the line's end, from
	 * which the game goes on to the same room by its fight entrance (1), the story at the original's value.
	 */
	constexpr uint16 LaboratoryMap = 673;
	constexpr uint16 LaboratoryEntrance = 1;
	constexpr uint16 StillsProgress = 258;
	constexpr uint16 FacilityProgress = 259;
	constexpr uint16 GateHallMap = 742;
	constexpr uint16 HoldingHallMap = 743;
	constexpr uint16 ProcessingLineMap = 744;
	constexpr uint16 SceneEntrance = 1;
	constexpr uint16 SufalRoomMap = 622;
	constexpr uint16 FightEntrance = 1;
	constexpr uint16 StillsEntrance = 0;
	constexpr uint8 Fei = 0;
	constexpr uint8 Elly = 1;
	constexpr uint8 Citan = 9;
	/** The saved variables of the field's place and time, not of what the story has done (as StoryContentTests has them). */
	constexpr uint16 MapLeftVariable = 0x004;
	constexpr std::array<uint16, 15> PlaceAndTimeVariables = {MapLeftVariable, 0x006, 0x008, 0x00C, 0x00E, 0x012, 0x01E, 0x020, 0x022, 0x024, 0x02A, 0x02C, 0x02E, 0x048, 0x04A};

	/** A game of the second disc's story mode Mode at the laboratory's scene, the story at the stills' value. */
	std::unique_ptr<Testing::FGameLoopSession> StartAtLaboratory(const FIso9660& Disc, EStoryMode Mode)
	{
		std::unique_ptr<Testing::FGameLoopSession> Game = Testing::FGameLoopSession::Start(Disc);
		Game->Session->SetStory(&Testing::PortStory());
		REQUIRE_FALSE(Game->Session->InitNewGame());
		FGameState& State = Game->Session->Game();
		State.SetStoryMode(Mode);
		State.SetSavedEventVariable(0, StillsProgress);
		State.SetSavedEventVariable(1, LaboratoryEntrance);
		State.SetFieldEntrance(LaboratoryEntrance);
		Game->EnterField(LaboratoryMap);
		return Game;
	}

	bool PageWaits(const FEventContext& Context)
	{
		return std::any_of(Context.Messages.Windows.begin(), Context.Messages.Windows.end(),
			[](const FFieldMessageWindow& Window) { return Window.bOpen && Window.bTextWaits; });
	}

	/** Plays the field on, confirming each message, until Done; the field maps played. */
	std::vector<int32> PlayFieldUntil(Testing::FGameLoopSession& Game, const std::function<bool(const FEventContext*)>& Done, int32 FrameLimit)
	{
		constexpr FPadState CrossHeld{FPadState::Bit(EPadButton::Cross)};
		constexpr int32 FramesAPress = 8;
		std::vector<int32> Maps;
		for (int32 Frame = 0;; ++Frame)
		{
			REQUIRE(Frame < FrameLimit);
			const FFieldProgram* Field = Game.Loop->Field();
			const FEventContext* Context = Field != nullptr ? &Field->Runtime().Context() : nullptr;
			if (Context != nullptr && (Maps.empty() || Maps.back() != Context->RunningMap))
			{
				Maps.push_back(Context->RunningMap);
			}
			if (Done(Context))
			{
				return Maps;
			}
			const std::optional<FError> Error = Game.Frame((Frame / FramesAPress) % 2 == 0 ? CrossHeld : FPadState{});
			INFO((Error ? Error->Message : std::string()));
			REQUIRE_FALSE(Error);
		}
	}

	/** Until the player has the party in Map, no page waiting. */
	std::function<bool(const FEventContext*)> PlayerHasPartyIn(int32 Map)
	{
		return [Map](const FEventContext* Context)
		{
			return Context != nullptr && Context->RunningMap == Map && Context->PlayerLock == 0 && !Context->bMenuLocked && !PageWaits(*Context);
		};
	}

	/** Until the field runs Map. */
	std::function<bool(const FEventContext*)> Reached(int32 Map)
	{
		return [Map](const FEventContext* Context) { return Context != nullptr && Context->RunningMap == Map; };
	}

	std::vector<std::string> StoryDifferences(const FGameState& Ours, const FGameState& Theirs)
	{
		std::vector<std::string> Differing;
		for (int32 Variable = 0; Variable < FGameState::SavedEventVariableCount; ++Variable)
		{
			const uint16 Address = static_cast<uint16>(Variable * 2);
			const bool bPlaceOrTime = std::find(PlaceAndTimeVariables.begin(), PlaceAndTimeVariables.end(), Address) != PlaceAndTimeVariables.end();
			if (!bPlaceOrTime && Ours.SavedEventVariable(Variable) != Theirs.SavedEventVariable(Variable))
			{
				char Line[32];
				std::snprintf(Line, sizeof(Line), "%03X: %d %d", Address, Ours.SavedEventVariable(Variable), Theirs.SavedEventVariable(Variable));
				Differing.emplace_back(Line);
			}
		}
		return Differing;
	}

	/** Enters Map by its scene entrance, where the map's scene begins at once. */
	void EnterScene(Testing::FGameLoopSession& Game, int32 Map)
	{
		FGameState& State = Game.Session->Game();
		State.SetSavedEventVariable(1, SceneEntrance);
		State.SetFieldEntrance(SceneEntrance);
		Game.EnterField(Map);
	}
}

TEST_CASE("The expanded second disc plays the Soylent System facility, from Krelian's laboratory to the Sufal Mass's room the stills go on to", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 FrameLimit = 30000;

	// The original: the laboratory's scene, the stills, then the Sufal Mass's room by the stills' entrance.
	std::unique_ptr<Testing::FGameLoopSession> Original = StartAtLaboratory(*Disc, EStoryMode::Original);
	CHECK(PlayFieldUntil(*Original, Reached(SufalRoomMap), FrameLimit) == std::vector<int32>{LaboratoryMap, 621, 620, 623, 624, 626, 625, SufalRoomMap});
	const FGameState& Theirs = Original->Session->Game();
	CHECK(Theirs.SavedEventVariable(1) == StillsEntrance);
	CHECK(Theirs.SavedEventVariable(0) == StillsProgress);
	CHECK(Theirs.Party() == std::array<uint8, 3>{Fei, Elly, Citan});

	// The expanded game: from the laboratory to the gate hall, the story at the facility's value, the player given the party.
	std::unique_ptr<Testing::FGameLoopSession> Expanded = StartAtLaboratory(*Disc, EStoryMode::Expanded);
	CHECK(PlayFieldUntil(*Expanded, PlayerHasPartyIn(GateHallMap), FrameLimit) == std::vector<int32>{LaboratoryMap, GateHallMap});
	FGameState& Ours = Expanded->Session->Game();
	CHECK(Ours.SavedEventVariable(0) == FacilityProgress);
	CHECK(Ours.Party() == std::array<uint8, 3>{Fei, Elly, Citan});

	// The holding hall: the soldier's scene plays through and leaves the player the party.
	EnterScene(*Expanded, HoldingHallMap);
	CHECK(PlayFieldUntil(*Expanded, PlayerHasPartyIn(HoldingHallMap), FrameLimit) == std::vector<int32>{HoldingHallMap});
	CHECK(Ours.SavedEventVariable(0) == FacilityProgress);

	// The processing line: the outbreak, then the Sufal Mass's room by its fight entrance, the story the original's.
	EnterScene(*Expanded, ProcessingLineMap);
	CHECK(PlayFieldUntil(*Expanded, Reached(SufalRoomMap), FrameLimit) == std::vector<int32>{ProcessingLineMap, SufalRoomMap});
	CHECK(Ours.SavedEventVariable(1) == FightEntrance);
	CHECK(Ours.SavedEventVariable(0) == StillsProgress);
	CHECK(StoryDifferences(Ours, Theirs) == std::vector<std::string>{});
	CHECK(Ours.Party() == Theirs.Party());
}

TEST_CASE("The Soylent System documents read as the port's story, a patch of the laboratory and three maps of their own", "[Story][Content][Packs]")
{
	const FStoryPacks& Story = Testing::PortStory();
	std::vector<int32> Maps;
	for (const FStoryDocument& Document : Story.Documents())
	{
		if (Document.bExpansion && Document.Name.find("disc2/soylent-") == 0)
		{
			Maps.push_back(Document.Patch.Map());
		}
	}
	std::sort(Maps.begin(), Maps.end());
	CHECK(Maps == std::vector<int32>{LaboratoryMap, GateHallMap, HoldingHallMap, ProcessingLineMap});
	REQUIRE(Story.WayBack() != nullptr);
	CHECK(Story.SubtitleAt(FacilityProgress, true) != nullptr);
}
