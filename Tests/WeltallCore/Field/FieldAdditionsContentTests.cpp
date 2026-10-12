#include "Field/EventMachine.h"
#include "Field/EventPlacement.h"
#include "Field/FieldMap.h"
#include "Field/FieldRuntime.h"
#include "Support/FieldTickSession.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Support/PortStory.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <optional>
#include <vector>

using namespace Weltall;

// What the port adds to the maps' scripts, played on the maps it is added to, against the original's.

namespace
{
	/** The house in Lahan: the man (actor 3) plays rock-paper-scissors and his wife (actor 2) says her lines when talked to (script 2). */
	constexpr int32 RpsMap = 7;
	constexpr int32 Man = 3;
	constexpr int32 Wife = 2;
	constexpr int32 TalkScript = 2;
	constexpr uint32 TalkPriority = 3;
	/** His "One, two..." choice of hands, her own lines before the Badge, and the map's own messages' count; hers follow, her concern first, then her offer. */
	constexpr int32 Hands = 17;
	constexpr int32 HerOwnLines = 4;
	constexpr int32 OwnMessages = 52;
	constexpr int32 HerConcern = OwnMessages;
	constexpr int32 HerOffer = OwnMessages + 1;
	constexpr uint32 HisHand = 0x408;
	constexpr uint32 BadgeEarned = 0x2DE;
	constexpr uint32 BadgeGiven = 0x2CE;
	constexpr uint32 HelperLosses = 0x7FC;
	/** Where Fei's walk to the man's table ends, as he stands once the game starts. */
	constexpr int32 TableX = 196;
	constexpr int32 TableZ = -52;
	/** Ticks with no window open and the player in control before the next talk: her talk leaves the player in control. */
	constexpr int32 TicksBetweenTalks = 10;

	/** How a visit went: each round's money won or lost in order, and how often the wife offered or said her own lines. */
	struct FRpsVisit
	{
		std::vector<int32> Rounds;
		int32 Offers = 0;
		int32 OwnLines = 0;
		bool bBadgeEarned = false;
		bool bBadgeGiven = false;
		int32 LossesCounted = 0;
	};

	/**
	 * Talks to the man or his wife in Talks's order whenever the player has control, then to the man, and answers their
	 * choices: Fei's hand the one the man's drawn hand beats, until Rounds rounds are over or the Badge is earned and
	 * given; her offer Accept or not. The pad presses the buttons as the player would (Cross the confirm, Down the cursor).
	 */
	FRpsVisit PlayRps(const Testing::FOracleFieldFiles& Files, const FMesResident& Resident, const FEventOptions& Options, const std::vector<int32>& Talks,
		int32 Rounds, bool bAccept)
	{
		Testing::FOracleTickSession Session(Files, RpsMap);
		Session.Game.SetGold(10000);
		FFieldServices Services = Session.Services(Files, Resident);
		Services.Options = Options;
		Services.Story = &Testing::PortStory();
		FFieldRuntime Runtime(Services);
		REQUIRE_FALSE(Runtime.EnterMap(RpsMap));
		const auto Tick = [&Runtime](uint16 Pressed)
		{
			std::vector<FPadState> Vblanks(static_cast<std::size_t>(Runtime.VblanksPerTick()));
			Vblanks[0].Buttons = Pressed;
			REQUIRE_FALSE(Runtime.Tick(Vblanks));
		};
		for (int32 Entry = 0; Entry < FFieldRuntime::EntryTicks; ++Entry)
		{
			Tick(0);
		}
		// Fei at the table, as the player who talks to the man stands beside it.
		const uint8 Fei = Runtime.Context().PartyActors[0];
		REQUIRE(PlaceActor(Runtime.Context().States[Fei], Runtime.Context().Actors[Fei], *Runtime.Context().Walkmesh, TableX, TableZ));
		FRpsVisit Visit;
		uint32 Gold = Session.Game.Gold();
		std::size_t NextTalk = 0;
		int32 Idle = 0;
		bool bOfferShown = false;
		bool bOwnLinesShown = false;
		constexpr int32 TickLimit = 30000;
		for (int32 Count = 0; Count < TickLimit && static_cast<int32>(Visit.Rounds.size()) < Rounds && !Visit.bBadgeGiven; ++Count)
		{
			FEventContext& Context = Runtime.Context();
			if (Session.Game.Gold() != Gold)
			{
				Visit.Rounds.push_back(static_cast<int32>(Session.Game.Gold()) - static_cast<int32>(Gold));
				Gold = Session.Game.Gold();
			}
			Visit.bBadgeEarned = Visit.bBadgeEarned || Context.Variables.Get(BadgeEarned, *Context.Scripts) == 30;
			Visit.bBadgeGiven = (Context.Variables.Get(BadgeGiven, *Context.Scripts) & 0x8000) != 0;
			Visit.LossesCounted = std::max(Visit.LossesCounted, Context.Variables.Get(HelperLosses, *Context.Scripts));
			const auto Front = std::find_if(Context.Messages.Windows.begin(), Context.Messages.Windows.end(),
				[](const FFieldMessageWindow& Window) { return Window.bOpen && Window.Age == 0; });
			if (Front == Context.Messages.Windows.end())
			{
				bOfferShown = false;
				bOwnLinesShown = false;
				Idle = Context.bPlayerControlled ? Idle + 1 : 0;
				if (Idle >= TicksBetweenTalks)
				{
					const int32 Actor = NextTalk < Talks.size() ? Talks[NextTalk] : Man;
					if (FEventMachine::StartScript(Context, Actor, TalkScript, TalkPriority))
					{
						++NextTalk;
						Idle = 0;
					}
				}
				Tick(0);
				continue;
			}
			Idle = 0;
			const auto Shows = [&Context, &Front](int32 Message) { return Front->Message.data() == Context.Messages.Table.String(Message).data(); };
			Visit.Offers += Shows(HerConcern) && !bOfferShown ? 1 : 0;
			bOfferShown = Shows(HerConcern);
			Visit.OwnLines += Shows(HerOwnLines) && !bOwnLinesShown ? 1 : 0;
			bOwnLinesShown = Shows(HerOwnLines);
			if ((Shows(Hands) || Shows(HerOffer)) && !Front->bChoice)
			{
				// A press while the text types would answer the choice the moment it opens.
				Tick(0);
				continue;
			}
			int16 Wanted = 0;
			if (Shows(Hands))
			{
				// Rock, scissors, paper: the hand his rock (0), scissors (1) or paper (2) beats.
				Wanted = static_cast<int16>((Context.Variables.Get(HisHand, *Context.Scripts) + 1) % 3);
			}
			else if (Shows(HerOffer))
			{
				Wanted = bAccept ? 0 : 1;
			}
			const bool bMove = Front->bChoice && Front->ChoiceCursor != Wanted;
			const uint16 Button = FPadState::Bit(bMove ? EPadButton::Down : EPadButton::Cross);
			Tick(Count % 2 == 0 ? Button : 0);
		}
		return Visit;
	}
}

TEST_CASE("After three rounds lost in Lahan's house the wife, talked to, offers help, after which the man loses every round, or in the original says her lines", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);

	// Her own lines before the losses and once she has had her word; her offer in between.
	const FRpsVisit Helped = PlayRps(*Files, Resident, FEventOptions(), {Wife, Man, Man, Man, Wife, Wife}, 9, true);
	CHECK(Helped.OwnLines == 2);
	CHECK(Helped.Offers == 1);
	CHECK(Helped.Rounds == std::vector<int32>{-50, -50, -50, 50, 50, 50, 50, 50});
	CHECK(Helped.bBadgeEarned);
	CHECK(Helped.bBadgeGiven);

	// Declined, she offers again the next time she is talked to.
	const FRpsVisit Declined = PlayRps(*Files, Resident, FEventOptions(), {Man, Man, Man, Wife, Wife}, 4, false);
	CHECK(Declined.OwnLines == 0);
	CHECK(Declined.Offers == 2);
	CHECK(Declined.Rounds == std::vector<int32>(4, -50));

	const FRpsVisit Original = PlayRps(*Files, Resident, FEventOptions::Original(), {Man, Man, Man, Wife}, 4, true);
	CHECK(Original.OwnLines == 1);
	CHECK(Original.Offers == 0);
	CHECK(Original.LossesCounted == 0);
	CHECK(Original.Rounds == std::vector<int32>(4, -50));
}

TEST_CASE("The wife's messages are laid out as the game's: two units a glyph of the disc's font, their widest line and tallest page", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	auto Loaded = LoadFieldMap(Disc->Image(), Files->Index, RpsMap);
	REQUIRE(Loaded);
	auto Scripts = FEventScripts::Parse(Loaded->EventScripts);
	REQUIRE(Scripts);
	const std::vector<uint8> Own = Loaded->Messages;
	CHECK_FALSE(Testing::PortStory().ApplyToMap(RpsMap, *Scripts, Loaded->Messages, FEventOptions::Original(), false));
	CHECK(Loaded->Messages == Own);
	CHECK_FALSE(Testing::PortStory().ApplyToMap(RpsMap, *Scripts, Loaded->Messages, FEventOptions(), false));
	const TResult<FMessageTable> Table = FMessageTable::Parse(Loaded->Messages);
	REQUIRE(Table);
	REQUIRE(Table->Count() == OwnMessages + 4);
	const TResult<FMessageTable> Before = FMessageTable::Parse(Own);
	REQUIRE(Before);
	for (int32 Message = 0; Message < OwnMessages; ++Message)
	{
		INFO("message " << Message);
		const std::span<const uint8> Was = Before->String(Message);
		const std::span<const uint8> Now = Table->String(Message);
		CHECK(std::equal(Was.begin(), Was.end(), Now.begin()));
		CHECK(Table->WidthUnits(Message) == Before->WidthUnits(Message));
	}
	for (int32 Message = OwnMessages; Message < Table->Count(); ++Message)
	{
		INFO("added message " << Message);
		int32 Widest = 0;
		int32 Width = 0;
		int32 Lines = 1;
		int32 Tallest = 1;
		const std::span<const uint8> Codes = Table->String(Message);
		for (std::size_t At = 0; At < Codes.size() && Codes[At] != 0x00; ++At)
		{
			switch (Codes[At])
			{
			case 0x01:
				Width = 0;
				Tallest = std::max(Tallest, ++Lines);
				break;
			case 0x02:
				Width = 0;
				Lines = 1;
				break;
			case 0x0F:
				At += 2;
				break;
			default:
				CHECK(Resident.Font.WidthUnits({0, Codes[At]}) == 2);
				Width += Resident.Font.WidthUnits({0, Codes[At]});
				Widest = std::max(Widest, Width);
			}
		}
		CHECK(Table->WidthUnits(Message) == Widest);
		CHECK(Table->Lines(Message) == Tallest);
	}
	// Her concern in a window of its own size, smaller than her offer's, whose choice is on its fourth and fifth lines.
	CHECK(Table->Lines(HerConcern) == 3);
	CHECK(Table->WidthUnits(HerConcern) < Table->WidthUnits(HerOffer));
	CHECK(Table->Lines(HerOffer) == 5);
}
