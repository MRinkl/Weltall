#include "Battle/BattleEvent.h"
#include "Battle/BattleRuntime.h"
#include "Field/FieldMap.h"
#include "Field/FieldRuntime.h"
#include "Field/FieldScriptFixes.h"
#include "Game/BattleGameProgram.h"
#include "Game/FieldProgram.h"
#include "Story/DiscText.h"
#include "Story/StoryPacks.h"
#include "Support/FieldTickSession.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/GameText.h"
#include "Support/PortStory.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace Weltall;

namespace
{
	/** A map of its own over the house in Lahan (map 7), with the forest's (map 15) encounters. */
	constexpr int32 OwnMap = 1000;
	constexpr int32 OwnBase = 7;
	constexpr int32 OwnEncounters = 15;
	/** Its one actor's init marks a scratch variable; its default script counts the ticks in another. */
	constexpr uint32 InitRan = 0x7F0;
	constexpr uint32 TicksCounted = 0x7F2;
	constexpr std::string_view OwnMapText =
		"map 1000 from 7\n"
		"encounters from 15\n"
		"message Hello\n"
		"{\n"
		"A map of its own.\n"
		"}\n"
		"actors 1\n"
		"actor 0 script 0:\n"
		"    SetVar var[7F0] #1\n"
		"    Return\n"
		"actor 0 script 1:\n"
		"    IncVar var[7F2]\n"
		"    Return\n";

	/** The same house as a map over its base: map 7's own scripts and messages, with a mark set on the way through its first actor's init. */
	constexpr int32 OverMap = 1002;
	constexpr std::string_view OverMapText =
		"map 1002 over 7\n"
		"encounters from 15\n"
		"message Hello\n"
		"{\n"
		"A map of its own.\n"
		"}\n"
		"detour @0x0011 to Marked CameraSet #5 #25 #512\n"
		"Marked:\n"
		"    SetVar var[7F0] #1\n"
		"    CameraSet #5 #25 #512\n"
		"    Jump @0x0018\n"
		"actor 0 script 1:\n"
		"    IncVar var[7F2]\n"
		"    Return\n";

	/**
	 * The second disc's first parting: the Ministry's scene after Fei's launch (map 273 by entrance 8, the story at 252)
	 * ends in the rematch with Vendetta, formation 6 of that map, and the game goes on at the chair (map 617 by
	 * entrance 2). The expanded second disc plays the sky above the forest, a map of its own, between the two, and the
	 * same sky again after the battle (another, by its entrance 0), from which it goes on to the chair.
	 */
	constexpr uint16 MinistryMap = 273;
	constexpr uint16 MinistryEntrance = 8;
	constexpr uint16 ProgressAfterLaunch = 252;
	constexpr int32 SkyMap = 730;
	constexpr uint16 AfterRematchMap = 731;
	constexpr uint16 AfterRematchEntrance = 0;
	constexpr int32 RematchFormation = 6;
	constexpr uint8 RematchKind = 1;
	constexpr int32 RematchSong = 20;
	/** The rematch's event script, whose first message is Fei's narration of the meeting and of Ramsus in his way. */
	constexpr int32 RematchEvent = 19;
	constexpr int32 EveryPage = std::numeric_limits<int32>::max();
	constexpr uint16 ChairMap = 617;
	constexpr uint16 ChairEntrance = 2;
	/**
	 * The expanded second disc's errand to the Mass Driver: after the sky, Taura's house by the entrance Fei woke by,
	 * where Elly sets out with Emeralda and the story's progress becomes the errand's own, until the launch in the
	 * Mass Driver's control room. The chair's narration of the Mass Driver goes on to the world map's scene of the
	 * launch, which brings the game back to the chair; the control room goes to the same scene with the story as the
	 * chair leaves it.
	 */
	constexpr uint16 TaurasHouseMap = 616;
	constexpr uint16 ErrandProgress = 253;
	constexpr uint16 ControlRoomMap = 734;
	constexpr uint16 LaunchScene = 17;
	constexpr uint8 Elly = 1;
	constexpr uint8 Emeralda = 6;
	constexpr uint8 NoOne = 0xFF;
	/** The chair's message that tells of the stream in the sky, the first after the launch is seen. */
	constexpr int32 StreamMessage = 18;
	/**
	 * The saved variables that say where the field was and for how long, not what the story has done, by their
	 * addresses: the map left, the leader's facing and the camera's; the clock; the count of map changes; where the
	 * leader stood and the camera's pitch; the map, x and z where the first slot's Gear was left (only a map that
	 * places a parked Gear reads them, against its own number: neither Taura's house nor the chair does); and the two
	 * the scripts only ever copy a value into for the developers to watch.
	 */
	constexpr uint16 MapLeftVariable = 0x004;
	constexpr std::array<uint16, 15> PlaceAndTimeVariables = {MapLeftVariable, 0x006, 0x008, 0x00C, 0x00E, 0x012, 0x01E, 0x020, 0x022, 0x024, 0x02A, 0x02C, 0x02E, 0x048, 0x04A};
	/** The bits the chair's narrations keep between their passes: the one for the Mass Driver's says the launch has been seen. */
	constexpr int32 ChairPassVariable = 0x248 / 2;

	using FCodes = std::vector<uint8>;

	/** A message's codes to its end. */
	FCodes WholeMessage(std::span<const uint8> Message)
	{
		const std::optional<FCodes> Codes = MesPagesOfCodes(Message, 1, EveryPage);
		REQUIRE(Codes);
		return *Codes;
	}

	/**
	 * The game as the rematch's event has its text, the field maps played on the way to it, every message of the sky's
	 * map when it was one of them, and the battle's narration as the event will tell it.
	 */
	struct FAtRematch
	{
		std::unique_ptr<Testing::FGameLoopSession> Game;
		std::vector<int32> Maps;
		std::vector<FCodes> SkyMessages;
		FCodes Narration;
	};

	/**
	 * Plays the Ministry's scene on the second disc with the port's story, in a game of Mode, confirming each message,
	 * until the Battle program has the game.
	 */
	FAtRematch PlayToRematch(const FIso9660& Disc, EStoryMode Mode)
	{
		constexpr FPadState CrossHeld{FPadState::Bit(EPadButton::Cross)};
		constexpr int32 FramesAPress = 8;
		constexpr int32 FrameLimit = 20000;
		FAtRematch At;
		At.Game = Testing::FGameLoopSession::Start(Disc);
		Testing::FGameLoopSession& Game = *At.Game;
		Game.Session->SetStory(&Testing::PortStory());
		REQUIRE_FALSE(Game.Session->InitNewGame());
		FGameState& State = Game.Session->Game();
		State.SetStoryMode(Mode);
		State.SetSavedEventVariable(0, ProgressAfterLaunch);
		State.SetSavedEventVariable(1, MinistryEntrance);
		State.SetFieldEntrance(MinistryEntrance);
		Game.EnterField(MinistryMap);
		for (int32 Frame = 0; Game.Loop->Program() != EGameProgram::Battle; ++Frame)
		{
			REQUIRE(Frame < FrameLimit);
			if (FFieldProgram* Field = Game.Loop->Field())
			{
				const FEventContext& Context = Field->Runtime().Context();
				const int32 Map = Context.RunningMap;
				if (At.Maps.empty() || At.Maps.back() != Map)
				{
					At.Maps.push_back(Map);
				}
				if (Map == SkyMap && At.SkyMessages.empty())
				{
					for (int32 Message = 0; Message < Context.Messages.Table.Count(); ++Message)
					{
						At.SkyMessages.push_back(WholeMessage(Context.Messages.Table.String(Message)));
					}
				}
			}
			const std::optional<FError> Error = Game.Frame((Frame / FramesAPress) % 2 == 0 ? CrossHeld : FPadState{});
			INFO((Error ? Error->Message : std::string()));
			REQUIRE_FALSE(Error);
		}
		return At;
	}

	/** Plays the battle on, no button held, until its event has its text: the narration it will tell first. */
	void PlayToNarration(FAtRematch& At)
	{
		constexpr int32 FrameLimit = 2000;
		const FBattleGameProgram* Battle = At.Game->Loop->Battle();
		REQUIRE(Battle != nullptr);
		const FBattleContext& Context = Battle->Runtime().Context();
		At.Game->RunUntil([&Context] { return Context.Event.Messages.Count() > 0; }, FrameLimit);
		CHECK(Context.Formation.EventScript == RematchEvent);
		At.Narration = WholeMessage(Context.Event.Messages.String(0));
	}

	/** Whether a field message's page waits for the confirm button. */
	bool PageWaits(const FEventContext& Context)
	{
		return std::any_of(Context.Messages.Windows.begin(), Context.Messages.Windows.end(),
			[](const FFieldMessageWindow& Window) { return Window.bOpen && Window.bTextWaits; });
	}

	/**
	 * Plays the field on, confirming each message, until Done says so (asked before each frame, with the field's
	 * context while there is a field); the field maps played.
	 */
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

	/** Until the chair (map 617) shows a page, unconfirmed. */
	bool AtChairPage(const FEventContext* Context)
	{
		return Context != nullptr && Context->RunningMap == ChairMap && PageWaits(*Context);
	}

	/**
	 * Goes on from the battle as its end does, into the field at the place and entrance the battle leads to, and plays
	 * on, confirming each message, until Done; the field maps played.
	 */
	std::vector<int32> PlayFromBattle(FAtRematch& At, const std::function<bool(const FEventContext*)>& Done)
	{
		constexpr int32 FrameLimit = 12000;
		FGameState& State = At.Game->Session->Game();
		State.SetFieldEntrance(State.SavedEventVariable(1));
		At.Game->EnterField(State.Place());
		return PlayFieldUntil(*At.Game, Done, FrameLimit);
	}

	/** Until Elly has set out on her errand at Taura's house and the player has the party again. */
	bool SetOut(const FEventContext* Context)
	{
		return Context != nullptr && Context->RunningMap == TaurasHouseMap && Context->GameState->SavedEventVariable(0) == ErrandProgress
			&& Context->PlayerLock == 0 && !Context->bMenuLocked && !PageWaits(*Context);
	}

	/** Until the field has handed the game to the world map. */
	bool LeftForWorldMap(const Testing::FGameLoopSession& Game)
	{
		return Game.Loop->Program() == EGameProgram::WorldMap;
	}

	/**
	 * The saved variables that differ between two games apart from those of the field's place and time, each as its
	 * address and the two values ("042: 9 255").
	 */
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

	/** Plays the world map's scene of the launch, no button held, until the chair shows its next page; the number of the message shown. */
	int32 PlayLaunchToChair(Testing::FGameLoopSession& Game)
	{
		constexpr int32 FrameLimit = 12000;
		Game.RunUntil([&Game] { return Game.Loop->Field() != nullptr && AtChairPage(&Game.Loop->Field()->Runtime().Context()); }, FrameLimit);
		const FEventContext& Context = Game.Loop->Field()->Runtime().Context();
		const auto Waiting = std::find_if(Context.Messages.Windows.begin(), Context.Messages.Windows.end(),
			[](const FFieldMessageWindow& Window) { return Window.bOpen && Window.bTextWaits; });
		REQUIRE(Waiting != Context.Messages.Windows.end());
		for (int32 Message = 0; Message < Context.Messages.Table.Count(); ++Message)
		{
			if (Context.Messages.Table.String(Message).data() == Waiting->Message.data())
			{
				return Message;
			}
		}
		return -1;
	}

	/**
	 * The field entering field map Number with Story, in a game of the second disc's story mode Mode, and ticking through
	 * its entry and Ticks more; the entry's error, if any.
	 */
	std::optional<FError> EnterAndTick(const Testing::FOracleFieldFiles& Files, const FMesResident& Resident, const FStoryPacks& Story, EStoryMode Mode,
		int32 Number, int32 Ticks, const std::function<void(const FFieldRuntime&)>& Check, const FEventOptions& Options = FEventOptions::Original())
	{
		Testing::FOracleTickSession Session(Files);
		FFieldServices Services = Session.Services(Files, Resident);
		Services.Story = &Story;
		Services.Options = Options;
		Services.Game->SetStoryMode(Mode);
		FFieldRuntime Runtime(Services);
		if (std::optional<FError> Error = Runtime.EnterMap(Number))
		{
			return Error;
		}
		for (int32 Tick = 0; Tick < FFieldRuntime::EntryTicks + Ticks; ++Tick)
		{
			const std::vector<FPadState> Vblanks(static_cast<std::size_t>(Runtime.VblanksPerTick()));
			REQUIRE_FALSE(Runtime.Tick(Vblanks));
		}
		Check(Runtime);
		return std::nullopt;
	}
}

TEST_CASE("Every document of the port's story fits the map it is written for, with the game's fixes and without", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const FStoryPacks& Story = Testing::PortStory();
	const FGameDiscText DiscText(Disc->Image(), Files->Index);
	CHECK_FALSE(Story.Documents().empty());
	// A battle event's text changes the event's own table on the disc.
	const auto Events = FBattleEventArchive::Load(Disc->Image(), Files->Index);
	REQUIRE(Events);
	for (const FStoryEventDocument& Document : Story.EventDocuments())
	{
		INFO(Document.Name);
		REQUIRE(Document.Patch.Event() < Events->Count());
		CHECK(Document.Patch.Apply(Events->Messages(Document.Patch.Event())));
	}
	for (const FStoryDocument& Document : Story.Documents())
	{
		if (Document.Patch.Base())
		{
			// A map of its own: the field enters it, its scripts' init and its entry run.
			INFO(Document.Name);
			const EStoryMode Mode = Document.bExpansion ? EStoryMode::Expanded : EStoryMode::Unchosen;
			const auto Entered = EnterAndTick(*Files, Resident, Story, Mode, Document.Patch.Map(), 0, [](const FFieldRuntime&) {});
			INFO((Entered ? Entered->Message : std::string()));
			CHECK_FALSE(Entered);
			continue;
		}
		// As the field loads the map: its scripts fixed by the options first, the document added under its own.
		FEventOptions Unfixed = FEventOptions::Original();
		if (Document.Option != nullptr)
		{
			Unfixed.*Document.Option = true;
		}
		const std::array<std::pair<const char*, FEventOptions>, 2> Cases = {{{"with the fixes", FEventOptions()}, {"without them", Unfixed}}};
		for (const auto& [Words, Options] : Cases)
		{
			INFO(Document.Name << " " << Words);
			// A patch of a map of its own goes on that map as it is built: its own sections, or its base map's with
			// that map's fixes and the map's document added.
			const FStoryDocument* Own = Story.MapOfItsOwn(Document.Patch.Map(), Document.bExpansion);
			const bool bOverBase = Own != nullptr && Own->Patch.KeepsBaseScripts();
			auto Loaded = LoadFieldMap(Disc->Image(), Files->Index, Own != nullptr ? *Own->Patch.Base() : Document.Patch.Map());
			REQUIRE(Loaded);
			if (Own != nullptr && !bOverBase)
			{
				auto Built = Own->Patch.Build(&DiscText);
				REQUIRE(Built);
				Loaded->EventScripts = std::move(Built->EventScripts);
				Loaded->Messages = std::move(Built->Messages);
			}
			auto Scripts = FEventScripts::Parse(Loaded->EventScripts);
			REQUIRE(Scripts);
			ApplyFieldScriptFixes(bOverBase ? *Own->Patch.Base() : Document.Patch.Map(), *Scripts, Options);
			if (bOverBase)
			{
				const TResult<bool> Based = Own->Patch.Apply(*Scripts, Loaded->Messages, &DiscText);
				REQUIRE(Based);
				REQUIRE(*Based);
			}
			const TResult<bool> Applied = Document.Patch.Apply(*Scripts, Loaded->Messages, &DiscText);
			INFO((Applied ? std::string() : Applied.Error().Message));
			REQUIRE(Applied);
			CHECK(*Applied);
		}
	}
}

TEST_CASE("A map of its own is entered as a disc map is: its base map's ground and scenery under its own scripts and messages", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const auto Base = LoadFieldMap(Disc->Image(), Files->Index, OwnBase);
	REQUIRE(Base);
	const auto Other = LoadFieldArchive(Disc->Image(), Files->Index, OwnEncounters);
	REQUIRE(Other);
	FStoryPacks Story;
	REQUIRE_FALSE(Story.Add("test", "own.txt", OwnMapText));
	constexpr int32 Ticks = 30;
	const auto Entered = EnterAndTick(*Files, Resident, Story, EStoryMode::Unchosen, OwnMap, Ticks, [&](const FFieldRuntime& Runtime)
	{
		const FFieldMap& Map = Runtime.Map();
		// Compared as a whole: a difference would print the sections byte by byte.
		const bool bBaseSprites = Map.SpriteSets == Base->SpriteSets;
		const bool bOtherEncounters = Map.Encounters == Other->Section(EFieldSection::Encounters);
		CHECK(bBaseSprites);
		CHECK(Map.Actors.size() == Base->Actors.size());
		CHECK(Map.Models.size() == Base->Models.size());
		CHECK(bOtherEncounters);
		const FEventContext& Context = Runtime.Context();
		CHECK(Context.Scripts->ActorCount() == 1);
		CHECK(Context.Messages.Table.Count() == 1);
		CHECK(Context.Variables.Get(InitRan, *Context.Scripts) == 1);
		CHECK(Context.Variables.Get(TicksCounted, *Context.Scripts) >= Ticks);
	});
	INFO((Entered ? Entered->Message : std::string()));
	CHECK_FALSE(Entered);

	// Its actors are its base map's script actors: no more of them than the base's scripts have.
	FStoryPacks TooMany;
	REQUIRE_FALSE(TooMany.Add("test", "many.txt", "map 1001 from 7\nactors 99\n"));
	const auto Refused = EnterAndTick(*Files, Resident, TooMany, EStoryMode::Unchosen, 1001, 0, [](const FFieldRuntime&) {});
	REQUIRE(Refused);
	CHECK(Refused->Message.find("test/many.txt: 99 script actors, more than the") != std::string::npos);
}

TEST_CASE("A map over its base map runs that map's scripts and messages with its own added, the base map's fixes first", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const auto Base = LoadFieldMap(Disc->Image(), Files->Index, OwnBase);
	REQUIRE(Base);
	const auto BaseScripts = FEventScripts::Parse(Base->EventScripts);
	const auto BaseMessages = FMessageTable::Parse(Base->Messages);
	REQUIRE(BaseScripts);
	REQUIRE(BaseMessages);
	const auto Other = LoadFieldArchive(Disc->Image(), Files->Index, OwnEncounters);
	REQUIRE(Other);
	FStoryPacks Story;
	REQUIRE_FALSE(Story.Add("test", "over.txt", OverMapText));
	constexpr int32 Ticks = 30;
	const auto Entered = EnterAndTick(*Files, Resident, Story, EStoryMode::Unchosen, OverMap, Ticks, [&](const FFieldRuntime& Runtime)
	{
		const FEventContext& Context = Runtime.Context();
		// The base map's actors, code and messages, by identity, and after them the document's.
		CHECK(Context.Scripts->ActorCount() == BaseScripts->ActorCount());
		REQUIRE(Context.Scripts->CodeSize() > BaseScripts->CodeSize());
		const bool bBaseCodeBeforeTheDetour = std::equal(BaseScripts->Code(), BaseScripts->Code() + 0x11, Context.Scripts->Code());
		const bool bBaseCodeAfterIt = std::equal(BaseScripts->Code() + 0x14, BaseScripts->Code() + BaseScripts->CodeSize(), Context.Scripts->Code() + 0x14);
		CHECK(bBaseCodeBeforeTheDetour);
		CHECK(bBaseCodeAfterIt);
		CHECK(Context.Messages.Table.Count() == BaseMessages->Count() + 1);
		const bool bOtherEncounters = Runtime.Map().Encounters == Other->Section(EFieldSection::Encounters);
		CHECK(bOtherEncounters);
		CHECK(Context.Variables.Get(InitRan, *Context.Scripts) == 1);
		CHECK(Context.Variables.Get(TicksCounted, *Context.Scripts) >= Ticks);
	});
	INFO((Entered ? Entered->Message : std::string()));
	CHECK_FALSE(Entered);

	// A base map that is not the one the document expects is no base for it.
	FStoryPacks Unfit;
	REQUIRE_FALSE(Unfit.Add("test", "unfit.txt", "map 1003 over 7\nexpect @0x000F End\n"));
	const auto Refused = EnterAndTick(*Files, Resident, Unfit, EStoryMode::Unchosen, 1003, 0, [](const FFieldRuntime&) {});
	REQUIRE(Refused);
	CHECK(Refused->Message.find("test/unfit.txt: field map 7 or the disc's text is not what it expects") != std::string::npos);

	// The hotel's head count (map 104) is one of the port's script fixes: a map over it has the fix when the game does.
	constexpr int32 FixedMap = 104;
	constexpr std::size_t FixedAt = 0x3A0;
	FStoryPacks Hotel;
	REQUIRE_FALSE(Hotel.Add("test", "hotel.txt", "map 1004 over 104\n"));
	uint8 Fixed = 0;
	uint8 Unfixed = 0;
	uint8 OnDisc = 0;
	REQUIRE_FALSE(EnterAndTick(*Files, Resident, Hotel, EStoryMode::Unchosen, 1004, 0,
		[&Fixed](const FFieldRuntime& Runtime) { Fixed = Runtime.Context().Scripts->Code()[FixedAt]; }, FEventOptions()));
	REQUIRE_FALSE(EnterAndTick(*Files, Resident, Hotel, EStoryMode::Unchosen, 1004, 0,
		[&Unfixed](const FFieldRuntime& Runtime) { Unfixed = Runtime.Context().Scripts->Code()[FixedAt]; }, FEventOptions::Original()));
	REQUIRE_FALSE(EnterAndTick(*Files, Resident, FStoryPacks(), EStoryMode::Unchosen, FixedMap, 0,
		[&OnDisc](const FFieldRuntime& Runtime) { OnDisc = Runtime.Context().Scripts->Code()[FixedAt]; }, FEventOptions::Original()));
	CHECK(Unfixed == OnDisc);
	CHECK(Fixed != OnDisc);
}

TEST_CASE("The expanded second disc plays the sky before and after the rematch and Elly's errand to the Mass Driver, and rejoins the original at the launch", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FAtRematch Expanded = PlayToRematch(*Disc, EStoryMode::Expanded);
	FAtRematch Original = PlayToRematch(*Disc, EStoryMode::Original);
	CHECK(Expanded.Maps == std::vector<int32>{MinistryMap, SkyMap});
	CHECK(Original.Maps == std::vector<int32>{MinistryMap});

	// The battle's narration begins with the meeting, which the expanded game has just played: there its first page
	// is told in the sky, in the disc's own words, and the battle's begins at the second.
	const auto Events = FBattleEventArchive::Load(Disc->Image(), *Expanded.Game->Index);
	REQUIRE(Events);
	const auto EventText = FMessageTable::Parse(Events->Messages(RematchEvent));
	REQUIRE(EventText);
	const std::optional<FCodes> Meeting = MesPagesOfCodes(EventText->String(0), 1, 1);
	const std::optional<FCodes> InHisWay = MesPagesOfCodes(EventText->String(0), 2, EveryPage);
	REQUIRE(Meeting);
	REQUIRE(InHisWay);
	CHECK(Meeting->size() > 1);
	CHECK(InHisWay->size() > 1);
	const bool bMeetingToldInTheSky = std::find(Expanded.SkyMessages.begin(), Expanded.SkyMessages.end(), *Meeting) != Expanded.SkyMessages.end();
	CHECK(bMeetingToldInTheSky);

	// The same battle from the same formations; after it the original goes on at the chair and the expanded game in
	// the sky again.
	const auto Ministry = LoadFieldArchive(Disc->Image(), *Expanded.Game->Index, MinistryMap);
	REQUIRE(Ministry);
	const std::vector<uint8>& Encounters = Ministry->Section(EFieldSection::Encounters);
	for (const FAtRematch* At : {&Expanded, &Original})
	{
		const FKernelState& Kernel = At->Game->Session->Kernel();
		const FGameState& State = At->Game->Session->Game();
		const std::size_t Kept = std::min(Encounters.size(), Kernel.Formations.size());
		const bool bMinistryFormations = std::equal(Encounters.begin(), Encounters.begin() + static_cast<std::ptrdiff_t>(Kept), Kernel.Formations.begin());
		CHECK(bMinistryFormations);
		CHECK(Kernel.Field.BattleFormation == RematchFormation);
		CHECK(Kernel.BattleKind == RematchKind);
		CHECK(State.FieldSong() == RematchSong);
		CHECK(State.PartySlotInGear(0) == 1);
	}
	CHECK(Expanded.Game->Session->Game().Place() == AfterRematchMap);
	CHECK(Expanded.Game->Session->Game().SavedEventVariable(1) == AfterRematchEntrance);
	CHECK(Original.Game->Session->Game().Place() == ChairMap);
	CHECK(Original.Game->Session->Game().SavedEventVariable(1) == ChairEntrance);
	CHECK(Expanded.Game->Session->Game().SavedEventVariable(MapLeftVariable / 2) == SkyMap);
	CHECK(Original.Game->Session->Game().SavedEventVariable(MapLeftVariable / 2) == MinistryMap);

	PlayToNarration(Expanded);
	PlayToNarration(Original);
	// In the expanded game a page of the port's own tells how the two came down to the forest, then the disc's own
	// from where Ramsus stands in the way; the original's is the disc's whole.
	const std::optional<FCodes> Bridge = MesPagesOfCodes(Expanded.Narration, 1, 1);
	const std::optional<FCodes> AfterBridge = MesPagesOfCodes(Expanded.Narration, 2, EveryPage);
	REQUIRE(Bridge);
	REQUIRE(AfterBridge);
	const bool bBridgeIsThePorts = Bridge->size() > 1 && *Bridge != *Meeting;
	const bool bThenInHisWay = *AfterBridge == *InHisWay;
	const bool bOriginalWhole = Original.Narration == WholeMessage(EventText->String(0));
	CHECK(bBridgeIsThePorts);
	CHECK(bThenInHisWay);
	CHECK(bOriginalWhole);

	// On from the battle's end: the original straight to the chair's narration of the Mass Driver; the expanded game by
	// the sky after the rematch to Taura's house, where Elly sets out with Emeralda.
	CHECK(PlayFromBattle(Original, AtChairPage) == std::vector<int32>{ChairMap});
	CHECK(Original.Game->Session->Game().SavedEventVariable(1) == ChairEntrance);
	CHECK(PlayFromBattle(Expanded, SetOut) == std::vector<int32>{AfterRematchMap, TaurasHouseMap});
	FGameState& Ours = Expanded.Game->Session->Game();
	const FGameState& Theirs = Original.Game->Session->Game();
	CHECK(Ours.SavedEventVariable(0) == ErrandProgress);
	CHECK(Ours.Party() == std::array<uint8, 3>{Elly, Emeralda, NoOne});

	// The errand's walk, from the house to the Mass Driver's halls, writes nothing of the story (the documents' own
	// tests walk it); its east door leads into the control room.
	Ours.SetSavedEventVariable(1, 0);
	Ours.SetFieldEntrance(0);
	Expanded.Game->EnterField(ControlRoomMap);
	constexpr int32 LaunchFrames = 12000;
	CHECK(PlayFieldUntil(*Expanded.Game, [&Expanded](const FEventContext*) { return LeftForWorldMap(*Expanded.Game); }, LaunchFrames)
		== std::vector<int32>{ControlRoomMap});
	CHECK(PlayFieldUntil(*Original.Game, [&Original](const FEventContext*) { return LeftForWorldMap(*Original.Game); }, LaunchFrames)
		== std::vector<int32>{ChairMap});

	// Both go to the world map's scene of the launch, the story's progress and the chair's mark of it as the chair
	// leaves them (where the field stands and who is in the party the chair sets anew as the game comes back to it).
	CHECK(Ours.Scene() == LaunchScene);
	CHECK(Theirs.Scene() == LaunchScene);
	CHECK(Ours.SavedEventVariable(0) == ProgressAfterLaunch);
	CHECK(Theirs.SavedEventVariable(0) == ProgressAfterLaunch);
	CHECK(Ours.SavedEventVariable(ChairPassVariable) == Theirs.SavedEventVariable(ChairPassVariable));

	// After it, back at the chair, it tells of the stream in the sky to both, with the same party, and what the story
	// has done is the same in both: every saved variable but those of the field's place and time.
	CHECK(PlayLaunchToChair(*Expanded.Game) == StreamMessage);
	CHECK(PlayLaunchToChair(*Original.Game) == StreamMessage);
	CHECK(Ours.SavedEventVariable(1) == ChairEntrance);
	CHECK(StoryDifferences(Ours, Theirs) == std::vector<std::string>{});
	CHECK(Ours.Party() == Theirs.Party());
}

TEST_CASE("The field has the expanded second disc's maps and patches in a game that plays it, and in no other", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	// The expansion's map over the house in Lahan, and its patch of the tavern: code after the map's own.
	constexpr int32 PatchedMap = 9;
	FStoryPacks Story;
	REQUIRE_FALSE(Story.Add(FStoryPacks::ExpansionPack, "own.txt", OwnMapText));
	REQUIRE_FALSE(Story.Add(FStoryPacks::ExpansionPack, "tavern.txt", "map 9\nAdded:\n    End\n"));
	std::size_t Code = 0;
	const auto ReadCode = [&Code](const FFieldRuntime& Runtime) { Code = Runtime.Context().Scripts->CodeSize(); };
	REQUIRE_FALSE(EnterAndTick(*Files, Resident, FStoryPacks(), EStoryMode::Expanded, PatchedMap, 0, ReadCode));
	const std::size_t OwnCode = Code;

	REQUIRE_FALSE(EnterAndTick(*Files, Resident, Story, EStoryMode::Expanded, PatchedMap, 0, ReadCode));
	CHECK(Code == OwnCode + 1);
	CHECK_FALSE(EnterAndTick(*Files, Resident, Story, EStoryMode::Expanded, OwnMap, 0, [](const FFieldRuntime&) {}));

	for (const EStoryMode Mode : {EStoryMode::Unchosen, EStoryMode::Original})
	{
		REQUIRE_FALSE(EnterAndTick(*Files, Resident, Story, Mode, PatchedMap, 0, ReadCode));
		CHECK(Code == OwnCode);
		// Its map is no map there: the disc has none of its number.
		CHECK(EnterAndTick(*Files, Resident, Story, Mode, OwnMap, 0, [](const FFieldRuntime&) {}));
	}
}

TEST_CASE("A field map's change of map goes into a map of its own as into a disc map", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const auto Tavern = LoadFieldMap(Disc->Image(), Files->Index, 9);
	REQUIRE(Tavern);
	// Map 1000, over the house in Lahan, goes on to map 1001, over the tavern, by its entrance.
	FStoryPacks Story;
	REQUIRE_FALSE(Story.Add("test", "from.txt", "map 1000 from 7\nactors 1\nactor 0 script 0:\n    Return\nactor 0 script 1:\n    ChangeMap #1001 #0\n    Return\n"));
	REQUIRE_FALSE(Story.Add("test", "to.txt",
		"map 1001 from 9\nactors 1\nentrances\nentrance x 0 z 0 layer 0 camera 0 facing keep\nactor 0 script 0:\n    SetVar var[7F0] #1\n    Return\n"));
	Testing::FOracleTickSession Session(*Files);
	FFieldServices Services = Session.Services(*Files, Resident);
	Services.Story = &Story;
	FFieldRuntime Runtime(Services);
	REQUIRE_FALSE(Runtime.EnterMap(1000));
	// Until map 1001's init has run: its one actor's mark.
	const auto Arrived = [&Runtime]() { return Runtime.Context().Variables.Get(0x7F0, *Runtime.Context().Scripts) == 1; };
	constexpr int32 TickLimit = 600;
	int32 Ticks = 0;
	for (; Ticks < TickLimit && !Arrived(); ++Ticks)
	{
		const std::vector<FPadState> Vblanks(static_cast<std::size_t>(Runtime.VblanksPerTick()));
		const std::optional<FError> Error = Runtime.Tick(Vblanks);
		INFO((Error ? Error->Message : std::string()));
		REQUIRE_FALSE(Error);
	}
	CHECK(Ticks < TickLimit);
	CHECK((Runtime.Context().MapNumber & 0xFFF) == 1001);
	const bool bTavernSprites = Runtime.Map().SpriteSets == Tavern->SpriteSets;
	CHECK(bTavernSprites);
}
