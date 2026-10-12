#include "Game/ChapterCard.h"
#include "Game/ChapterCardDrawing.h"
#include "Game/ChapterCues.h"
#include "Game/ChapterTitles.h"
#include "Game/FieldProgram.h"
#include "Game/WorldMapProgram.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/GameText.h"
#include "Text/MesGlyphs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace Weltall;

// The story's chapters and the title card shown as one begins.
namespace
{
	/** Full-width lines of a title file: each title padded with full-width spaces to 15 characters, then a carriage return and a line feed. */
	std::vector<uint8> TitleLines(std::initializer_list<const char*> Titles)
	{
		std::vector<uint8> Lines;
		for (const char* Title : Titles)
		{
			const std::string Text(Title);
			for (std::size_t At = 0; At < 15; ++At)
			{
				const char Character = At < Text.size() ? Text[At] : ' ';
				if (Character == ' ')
				{
					Lines.insert(Lines.end(), {0x81, 0x40});
				}
				else if (Character == '!')
				{
					Lines.insert(Lines.end(), {0x81, 0x49});
				}
				else if (Character >= 'a')
				{
					Lines.insert(Lines.end(), {0x82, static_cast<uint8>(0x81 + Character - 'a')});
				}
				else
				{
					Lines.insert(Lines.end(), {0x82, static_cast<uint8>(0x60 + Character - 'A')});
				}
			}
			Lines.insert(Lines.end(), {'\r', '\n'});
		}
		return Lines;
	}

	constexpr int32 CardVblanks = FChapterCard::RiseVblanks + FChapterCard::StayVblanks + FChapterCard::FadeVblanks;
	/** A forest map a new game's party walks at once. */
	constexpr uint16 ForestMap = 22;
	/** The story's progress, its value as the sixth chapter (Into the Woods) begins, and the world map's place by Lahan. */
	constexpr uint32 StoryProgress = 0;
	constexpr int32 IntoTheWoods = 24;
	constexpr int32 IntoTheWoodsChapter = 6;
	constexpr uint16 LahanPlace = 0x400;
	constexpr FPadState StartHeld{FPadState::Bit(EPadButton::Start)};
	constexpr FPadState CrossHeld{FPadState::Bit(EPadButton::Cross)};
	/** Lahan as the party comes back to it from the mountain path at night, the story where the path left it. */
	constexpr uint16 LahanMap = 2;
	constexpr uint16 LahanFromThePath = 8;
	constexpr int32 FallenShadows = 16;
	constexpr int32 FallenShadowsChapter = 4;
	constexpr int32 AttackOnLahanChapter = 5;
	/** The gears' dock of Solaris as the party leaves it for the last time: its scene goes on to the Yggdrasil's bridge. */
	constexpr uint16 SolarisDockMap = 610;
	constexpr int32 TearsForFears = 234;
	constexpr int32 FallOfSolarisChapter = 53;
	/** Shevat's throne room as Citan goes on with his account of Fei, and the cage Fei is kept in after the council. */
	constexpr uint16 ThroneRoomMap = 453;
	constexpr uint16 CageMap = 467;
	constexpr int32 ShevatsMove = 241;
	constexpr int32 ShevatsMoveChapter = 54;
	constexpr int32 BrokenTiesChapter = 55;
	/** Lacan's memory as the dreams' last scene, the story at the dreams' last value, and the voice's scene it goes on to. */
	constexpr uint16 LacansMemoryMap = 682;
	constexpr uint16 LacansMemoryEntrance = 1;
	constexpr uint16 VoiceMap = 668;
	constexpr int32 DreamsEnd = 324;
	constexpr int32 FallenStarChapter = 66;
	/** In the voice's scene, the director's second message, Fei's first line, shown where a chapter's card stands, and the wait for its end. */
	constexpr int32 VoiceDirector = 5;
	constexpr uint16 FeisFirstLine = 0x0287;
	constexpr uint16 FeisFirstLineRead = 0x028B;
	/** The chair's narration that the story's 63rd chapter is. */
	constexpr int32 WaitingDeath = 295;
	constexpr int32 WaitingDeathChapter = 63;
}

TEST_CASE("The message font's codes for plain and full-width characters", "[Text]")
{
	CHECK(MesCodeOfCharacter(' ') == FMesCode{0, 0x10});
	CHECK(MesCodeOfCharacter('-') == FMesCode{0, 0x13});
	CHECK(MesCodeOfCharacter('0') == FMesCode{0, 0x16});
	CHECK(MesCodeOfCharacter('Z') == FMesCode{0, 0x39});
	CHECK(MesCodeOfCharacter('=') == FMesCode{0, 0x3C});
	CHECK(MesCodeOfCharacter('a') == FMesCode{0, 0x3D});
	CHECK(MesCodeOfCharacter('!') == FMesCode{0, 0x57});
	CHECK(MesCodeOfCharacter('\'') == FMesCode{0, 0x5C});
	CHECK(MesCodeOfCharacter(':') == FMesCode{0, 0x5F});
	CHECK(MesCodeOfCharacter('?') == FMesCode{0, 0x60});
	CHECK_FALSE(MesCodeOfCharacter('@'));
	CHECK(MesCodeOfFullWidth(0x82, 0x4F) == FMesCode{0, 0x16});
	CHECK(MesCodeOfFullWidth(0x82, 0x77) == FMesCode{0, 0x37});
	CHECK(MesCodeOfFullWidth(0x82, 0x9A) == FMesCode{0, 0x56});
	CHECK(MesCodeOfFullWidth(0x81, 0x7C) == FMesCode{0, 0x13});
	CHECK(PlainOfFullWidth(0x81, 0x66) == '\'');
	CHECK_FALSE(MesCodeOfFullWidth(0x88, 0x9F));
}

TEST_CASE("A title file's lines of one title are a chapter, numbered in the story's order", "[Game]")
{
	const FChapterTitles Titles = FChapterTitles::Parse(TitleLines({"Lahan Village", "Lahan Village", "Mountain Path", "Shot Down!", "Shot Down!", "Shot Down!"}));
	REQUIRE(Titles.Chapters().size() == 3);
	CHECK(Titles.Chapters()[0].Title == "Lahan Village");
	CHECK(Titles.Chapters()[2].Title == "Shot Down!");
	CHECK(Titles.Chapters()[2].FirstProgress == 3);
	CHECK(Titles.Find(0)->Number == 1);
	CHECK(Titles.Find(1)->Number == 1);
	CHECK(Titles.Find(2)->Number == 2);
	CHECK(Titles.Find(5)->Number == 3);
	// Past the file's end the story is still in its last chapter.
	CHECK(Titles.Find(400)->Number == 3);
	CHECK(FChapterTitles::Parse({}).Find(0) == nullptr);
}

TEST_CASE("The disc names the story's 68 chapters", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Titles = FChapterTitles::Read(Disc->Image(), *Index);
	REQUIRE(Titles);
	REQUIRE(Titles->Chapters().size() == 68);
	CHECK(Titles->Find(0)->Title == "Lahan Village");
	CHECK(Titles->Find(7)->Number == 1);
	CHECK(Titles->Find(8)->Title == "Mountain Path");
	CHECK(Titles->Find(54)->Title == "Pirate's Lair");
	CHECK(Titles->Find(54)->Number == 13);
	CHECK(Titles->Find(198)->Title == "Gate1 -Margie");
	CHECK(Titles->Find(249)->Title == "Shot Down!");
	CHECK(Titles->Find(249)->Number == 56);
	CHECK(Titles->Find(310)->Title == "Dreams...");
	CHECK(Titles->Find(353)->Title == "First and Last");
	CHECK(Titles->Find(353)->Number == 68);
	// Every title is one the message font can write.
	for (const FChapter& Chapter : Titles->Chapters())
	{
		INFO(Chapter.Title);
		CHECK(std::all_of(Chapter.Title.begin(), Chapter.Title.end(), [](char Character) { return MesCodeOfCharacter(Character).has_value(); }));
	}
}

TEST_CASE("A chapter's card waits for the player's control, rises, stays and fades, once a chapter", "[Game]")
{
	FChapterCard Card;
	// The story is in chapter 1, the scripts hold the party: nothing yet.
	Card.Advance(1, 0, false, 2);
	CHECK(Card.Shown() == 0);
	CHECK(Card.Level() == 0);
	Card.Advance(1, 0, true, 2);
	CHECK(Card.Shown() == 1);
	CHECK(Card.Level() == 0);
	Card.Advance(1, 0, true, FChapterCard::RiseVblanks / 2);
	CHECK(Card.Level() == FChapterCard::FullLevel / 2);
	// Once begun it goes on, whoever holds the party, through another chapter's start.
	Card.Advance(2, 0, false, FChapterCard::RiseVblanks);
	CHECK(Card.Shown() == 1);
	CHECK(Card.Level() == FChapterCard::FullLevel);
	Card.Advance(2, 0, true, FChapterCard::StayVblanks);
	CHECK(Card.Level() < FChapterCard::FullLevel);
	CHECK(Card.Level() > 0);
	Card.Advance(2, 0, true, FChapterCard::FadeVblanks);
	CHECK(Card.Shown() == 0);
	// The next chapter's follows; the one shown never comes again.
	Card.Advance(2, 0, true, 2);
	CHECK(Card.Shown() == 2);
	Card.Advance(2, 0, true, CardVblanks);
	Card.Advance(2, 0, true, 2);
	CHECK(Card.Shown() == 0);

	// A loaded game's chapter has no card; a new game's first has.
	Card.Know(5);
	Card.Advance(5, 0, true, 2);
	CHECK(Card.Shown() == 0);
	Card.Advance(6, 0, true, 2);
	CHECK(Card.Shown() == 6);
	Card.Reset();
	CHECK(Card.Shown() == 0);
	Card.Advance(0, 0, true, 2);
	CHECK(Card.Shown() == 0);
	Card.Advance(5, 0, true, 2);
	CHECK(Card.Shown() == 5);
}

TEST_CASE("A scene's asking begins a chapter's card without the player's control, and no card goes back", "[Game]")
{
	FChapterCard Card;
	Card.Know(4);
	// The story still in chapter 4 and the scripts holding the party: the scene asks for chapter 5's.
	Card.Advance(4, 0, false, 2);
	CHECK(Card.Shown() == 0);
	Card.Ask(5);
	Card.Advance(4, 0, false, 2);
	CHECK(Card.Shown() == 5);
	Card.Advance(4, 0, false, CardVblanks);
	CHECK(Card.Shown() == 0);
	// Shown once, however often the scene asks; and the chapter before it never comes back, control or not.
	Card.Ask(5);
	Card.Advance(4, 0, true, 2);
	CHECK(Card.Shown() == 0);
	Card.Advance(5, 0, true, 2);
	CHECK(Card.Shown() == 0);
	// The next chapter's comes with control as ever.
	Card.Advance(6, 0, false, 2);
	CHECK(Card.Shown() == 0);
	Card.Advance(6, 0, true, 2);
	CHECK(Card.Shown() == 6);

	// Asked while another card shows, it follows that one.
	FChapterCard Following;
	Following.Advance(1, 0, true, 2);
	Following.Ask(2);
	Following.Advance(1, 0, false, 2);
	CHECK(Following.Shown() == 1);
	Following.Advance(1, 0, false, CardVblanks);
	CHECK(Following.Shown() == 0);
	Following.Advance(1, 0, false, 2);
	CHECK(Following.Shown() == 2);
}

TEST_CASE("The chapters whose cards begin with the story's value are the second disc's narrated ones", "[Game]")
{
	int32 Count = 0;
	for (int32 Chapter = 1; Chapter <= 68; ++Chapter)
	{
		Count += ChapterCues::BeginsWithStory(Chapter) ? 1 : 0;
	}
	CHECK(Count == 6);
	CHECK(ChapterCues::BeginsWithStory(WaitingDeathChapter));
	CHECK_FALSE(ChapterCues::BeginsWithStory(FallenShadowsChapter));
	CHECK_FALSE(ChapterCues::BeginsWithStory(6));
	// The ones a scene has a place for wait for it.
	CHECK_FALSE(ChapterCues::BeginsWithStory(AttackOnLahanChapter));
	CHECK_FALSE(ChapterCues::BeginsWithStory(FallOfSolarisChapter));
	CHECK_FALSE(ChapterCues::BeginsWithStory(ShevatsMoveChapter));
	CHECK_FALSE(ChapterCues::BeginsWithStory(BrokenTiesChapter));
	CHECK_FALSE(ChapterCues::BeginsWithStory(FallenStarChapter));
}

TEST_CASE("The attack on Lahan shows its chapter's card as Fei runs off into the burning village, long before the story's value says so", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameState& State = Game->Session->Game();
	State.SetSavedEventVariable(StoryProgress, FallenShadows);
	State.SetFieldEntrance(LahanFromThePath);
	Game->Session->ChapterCard().Know(FallenShadowsChapter);
	Game->EnterField(LahanMap);
	const FChapterCard& Card = Game->Session->ChapterCard();
	// The scene plays with its messages read on; the card comes at its place and nowhere before, the party never the player's.
	int32 Frames = 0;
	bool bControlled = false;
	while (Card.Shown() == 0 && Frames < 12000)
	{
		REQUIRE_FALSE(Game->Frame(Frames % 12 < 2 ? CrossHeld : FPadState{}));
		REQUIRE(Game->Loop->Field() != nullptr);
		const FEventContext& Context = Game->Loop->Field()->Runtime().Context();
		bControlled = bControlled || (Context.bFieldRunning && Context.bPlayerControlled && !Context.bMenuLocked);
		++Frames;
	}
	CHECK(Card.Shown() == AttackOnLahanChapter);
	CHECK_FALSE(bControlled);
	// Well into the scene: its first minute is the villagers' talk.
	CHECK(Frames > 1500);
	const FEventContext& Context = Game->Loop->Field()->Runtime().Context();
	CHECK(Context.RunningMap == LahanMap);
	CHECK(Context.Variables.Raw(StoryProgress) == FallenShadows);
	CHECK(ChapterCues::AskedByScene(Context) == AttackOnLahanChapter);
	// It runs its course over the scene and does not come again while the scene's place still holds or after.
	Game->RunUntil([&] { return Card.Shown() == 0; }, CardVblanks, FPadState{});
	CHECK(Card.Shown() == 0);
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(Frame % 12 < 2 ? CrossHeld : FPadState{}));
		REQUIRE(Card.Shown() == 0);
	}
}

TEST_CASE("The fall of Solaris shows its chapter's card as the dock's scene ends, after its movie and before the story's value says so", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->Session->Game().SetSavedEventVariable(StoryProgress, TearsForFears);
	Game->Session->ChapterCard().Know(FallOfSolarisChapter - 1);
	Game->EnterField(SolarisDockMap);
	const FChapterCard& Card = Game->Session->ChapterCard();
	// The dock's scene and its movie play; the card comes as the screen goes to black for the bridge.
	int32 Frames = 0;
	bool bMoviePlayed = false;
	while (Card.Shown() == 0 && Frames < 6000)
	{
		REQUIRE_FALSE(Game->Frame(Frames % 12 < 2 ? CrossHeld : FPadState{}));
		REQUIRE(Game->Loop->Field() != nullptr);
		bMoviePlayed = bMoviePlayed || Game->Loop->Field()->Runtime().Request().Kind == EFieldRequest::Movie;
		++Frames;
	}
	CHECK(Card.Shown() == FallOfSolarisChapter);
	CHECK(bMoviePlayed);
	const FEventContext& Context = Game->Loop->Field()->Runtime().Context();
	CHECK(Context.RunningMap == SolarisDockMap);
	CHECK(Context.Variables.Raw(StoryProgress) == TearsForFears);
	CHECK(Game->Loop->Field()->Runtime().Request().Kind != EFieldRequest::Movie);
	CHECK(ChapterCues::AskedByScene(Context) == FallOfSolarisChapter);
	CHECK_FALSE((Context.bPlayerControlled && !Context.bMenuLocked));
}

TEST_CASE("Shevat's council and the flight from Shevat show their chapters' cards over the fade that ends the scene before", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	struct FPlace
	{
		uint16 Map;
		int32 Story;
		int32 Chapter;
	};
	// The throne room with the story already in the council's chapter; the cage with the story still in it.
	for (const FPlace& Place : {FPlace{ThroneRoomMap, ShevatsMove, ShevatsMoveChapter}, FPlace{CageMap, ShevatsMove + 1, BrokenTiesChapter}})
	{
		INFO(Place.Map);
		auto Game = Testing::FGameLoopSession::Start(*Disc);
		Game->Session->Game().SetSavedEventVariable(StoryProgress, static_cast<uint16>(Place.Story));
		Game->Session->ChapterCard().Know(Place.Chapter - 1);
		Game->EnterField(Place.Map);
		const FChapterCard& Card = Game->Session->ChapterCard();
		int32 Frames = 0;
		while (Card.Shown() == 0 && Frames < 9000)
		{
			REQUIRE_FALSE(Game->Frame(Frames % 12 < 2 ? CrossHeld : FPadState{}));
			REQUIRE(Game->Loop->Field() != nullptr);
			++Frames;
		}
		CHECK(Card.Shown() == Place.Chapter);
		// At the scene's end, not as its map starts, and on the scene's own map with the story where it was.
		CHECK(Frames > 1500);
		const FEventContext& Context = Game->Loop->Field()->Runtime().Context();
		CHECK(Context.RunningMap == Place.Map);
		CHECK(Context.Variables.Raw(StoryProgress) == Place.Story);
		CHECK(ChapterCues::AskedByScene(Context) == Place.Chapter);
	}
}

TEST_CASE("Fallen Star's card comes as Lacan's memory fades out and is gone before Fei's first line stands where it stood", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameState& State = Game->Session->Game();
	State.SetSavedEventVariable(StoryProgress, DreamsEnd);
	State.SetFieldEntrance(LacansMemoryEntrance);
	Game->Session->ChapterCard().Know(FallenStarChapter - 1);
	Game->EnterField(LacansMemoryMap);
	const FChapterCard& Card = Game->Session->ChapterCard();
	int32 Frames = 0;
	while (Card.Shown() == 0 && Frames < 9000)
	{
		REQUIRE_FALSE(Game->Frame(Frames % 12 < 2 ? CrossHeld : FPadState{}));
		REQUIRE(Game->Loop->Field() != nullptr);
		++Frames;
	}
	CHECK(Card.Shown() == FallenStarChapter);
	// At the memory's end, not as its map starts, with the story where the dreams left it.
	CHECK(Frames > 1000);
	{
		const FEventContext& Context = Game->Loop->Field()->Runtime().Context();
		CHECK(Context.RunningMap == LacansMemoryMap);
		CHECK(Context.Variables.Raw(StoryProgress) == DreamsEnd);
		CHECK(ChapterCues::AskedByScene(Context) == FallenStarChapter);
	}
	// The game goes on to the voice's scene by itself; by Fei's first line there the card is over, and it does not come again.
	const auto IsAtFeisLine = [&]
	{
		const FEventContext& Context = Game->Loop->Field()->Runtime().Context();
		if (Context.RunningMap != VoiceMap || !Context.bFieldRunning)
		{
			return false;
		}
		for (const FEventThread& Thread : Context.States[VoiceDirector].Threads)
		{
			if (!Thread.IsFree() && (Thread.Pc == FeisFirstLine || Thread.Pc == FeisFirstLineRead))
			{
				return true;
			}
		}
		return false;
	};
	for (Frames = 0; !IsAtFeisLine() && Frames < 1200; ++Frames)
	{
		REQUIRE_FALSE(Game->Frame(Frames % 12 < 2 ? CrossHeld : FPadState{}));
		REQUIRE(Game->Loop->Field() != nullptr);
	}
	REQUIRE(IsAtFeisLine());
	CHECK(Card.Shown() == 0);
	CHECK(Game->Session->Chapter() == FallenStarChapter);
	for (Frames = 0; Frames < 60; ++Frames)
	{
		REQUIRE_FALSE(Game->Frame());
		REQUIRE(Card.Shown() == 0);
	}
}

TEST_CASE("A chapter's card the story asks for while a movie plays waits for the movie's end", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->Session->Game().SetSavedEventVariable(StoryProgress, ShevatsMove + 1);
	Game->Session->ChapterCard().Know(ShevatsMoveChapter);
	Game->EnterField(CageMap);
	const FChapterCard& Card = Game->Session->ChapterCard();
	const auto IsMoviePlaying = [&] { return Game->Loop->Field()->Runtime().Request().Kind == EFieldRequest::Movie; };
	// The cage's scene up to its movie, then the story moved into a chapter whose card begins with the story's value.
	int32 Frames = 0;
	while (!IsMoviePlaying() && Frames < 9000)
	{
		REQUIRE_FALSE(Game->Frame(Frames % 12 < 2 ? CrossHeld : FPadState{}));
		++Frames;
	}
	REQUIRE(IsMoviePlaying());
	REQUIRE(Card.Shown() == 0);
	Game->Loop->Field()->Runtime().Context().Variables.Set(StoryProgress, WaitingDeath);
	int32 MovieFrames = 0;
	while (IsMoviePlaying() && MovieFrames < 3000)
	{
		REQUIRE_FALSE(Game->Frame());
		if (IsMoviePlaying())
		{
			REQUIRE(Card.Shown() == 0);
		}
		++MovieFrames;
	}
	CHECK(MovieFrames > 30);
	REQUIRE_FALSE(IsMoviePlaying());
	REQUIRE_FALSE(Game->Frame());
	CHECK(Card.Shown() == WaitingDeathChapter);
}

TEST_CASE("A chapter the story passes inside scenes shows its card as its first map starts, and a setting keeps it away", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	for (const bool bCards : {true, false})
	{
		auto Game = Testing::FGameLoopSession::Start(*Disc);
		FPlayerSettings Settings;
		Settings.bChapterCards = bCards;
		Game->Session->SetSettings(Settings);
		Game->Session->Game().SetSavedEventVariable(StoryProgress, WaitingDeath);
		Game->Session->ChapterCard().Know(WaitingDeathChapter - 1);
		Game->EnterField(ForestMap);
		// Nothing while the map loads; then the map's first running frame, before any control could have begun it.
		INFO(bCards);
		int32 Frames = 0;
		while (!Game->Loop->Field()->Runtime().Context().bFieldRunning && Frames < 100)
		{
			CHECK(Game->Session->ChapterCard().Shown() == 0);
			REQUIRE_FALSE(Game->Frame());
			++Frames;
		}
		REQUIRE(Game->Loop->Field()->Runtime().Context().bFieldRunning);
		CHECK(Game->Session->ChapterCard().Shown() == (bCards ? WaitingDeathChapter : 0));
	}
}

TEST_CASE("A chapter's card is its number and title in the game's font, over a band, as strong as its level", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const FChapter Chapter{13, 54, "Pirate's Lair"};
	CHECK(FChapterCardDrawing::Text(Chapter) == "Chapter 13: Pirate's Lair");

	FChapterCardDrawing Drawing;
	FDrawList List;
	Drawing.Build(FChapterCardDrawing::Text(Chapter), 0, Resident.Font, List);
	CHECK(List.Primitives.empty());
	Drawing.Build(FChapterCardDrawing::Text(Chapter), FChapterCard::FullLevel, Resident.Font, List);
	REQUIRE(List.Primitives.size() == 5);
	// The band's rows take from the picture behind, across the whole display; its edges fade to nothing.
	for (std::size_t Row = 0; Row < 3; ++Row)
	{
		const FDrawPrimitive& Band = List.Primitives[Row];
		CHECK_FALSE(Band.Texture);
		CHECK(Band.Blend == EDrawBlend::SemiTransparent);
		CHECK(Band.UntexturedBlend == EBlendMode::Subtract);
		CHECK(Band.Corners[0].X < -1000.0f);
		CHECK(Band.Corners[1].X > 1320.0f);
	}
	CHECK(List.Primitives[0].Corners[0].Color.R == 0);
	CHECK(List.Primitives[0].Corners[2].Color.R == List.Primitives[1].Corners[0].Color.R);
	CHECK(List.Primitives[2].Corners[2].Color.R == 0);
	// The outlines take everything away, the letters add white; both about the screen's middle.
	const FDrawPrimitive& Shade = List.Primitives[3];
	const FDrawPrimitive& Letters = List.Primitives[4];
	REQUIRE(Shade.Texture);
	REQUIRE(Letters.Texture);
	CHECK(Shade.Texture->Page.Blend == EBlendMode::Subtract);
	CHECK(Letters.Texture->Page.Blend == EBlendMode::Add);
	CHECK(Letters.Corners[0].Color.R == 255);
	CHECK(Letters.Corners[0].X > 40.0f);
	CHECK(Letters.Corners[1].X < 280.0f);
	CHECK((Letters.Corners[0].X + Letters.Corners[1].X) / 2 == Catch::Approx(166.0f).margin(1.0f));
	// The pictures: every letter's texel is in the outlines' picture too, which has more.
	int32 Ink = 0;
	int32 Shaded = 0;
	const int32 Width = static_cast<int32>(Letters.Corners[1].X - Letters.Corners[0].X);
	for (int32 Row = 0; Row < FMesFont::CellHeight; ++Row)
	{
		for (int32 Column = 0; Column < Width; ++Column)
		{
			const bool bLetter = Drawing.Pictures().At(Column, static_cast<int32>(Letters.Corners[0].V) + Row) != 0;
			const bool bShade = Drawing.Pictures().At(Column, static_cast<int32>(Shade.Corners[0].V) + Row) != 0;
			Ink += bLetter ? 1 : 0;
			Shaded += bShade ? 1 : 0;
			CHECK((!bLetter || bShade));
		}
	}
	CHECK(Ink > 200);
	CHECK(Shaded > 2 * Ink);

	// Half shown, everything is half as strong; the text is not drawn again.
	const uint32 Uploads = Drawing.Pictures().Revision();
	Drawing.Build(FChapterCardDrawing::Text(Chapter), FChapterCard::FullLevel / 2, Resident.Font, List);
	REQUIRE(List.Primitives.size() == 5);
	CHECK(List.Primitives[4].Corners[0].Color.R == 127);
	CHECK(Drawing.Pictures().Revision() == Uploads);
	// Another chapter's text replaces it.
	Drawing.Build("Chapter 14: Operation Aveh", FChapterCard::FullLevel, Resident.Font, List);
	CHECK(Drawing.Pictures().Revision() > Uploads);
}

TEST_CASE("In the field a new game's first chapter shows its card once the player walks, and a setting keeps the cards away", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	for (const bool bCards : {true, false})
	{
		auto Game = Testing::FGameLoopSession::Start(*Disc);
		FPlayerSettings Settings;
		Settings.bChapterCards = bCards;
		Game->Session->SetSettings(Settings);
		Game->EnterField(ForestMap);
		CHECK(Game->Session->Chapter() == 1);
		CHECK(Game->Session->ChapterCard().Shown() == 0);
		INFO(bCards);
		if (!bCards)
		{
			for (int32 Frame = 0; Frame < 300; ++Frame)
			{
				REQUIRE_FALSE(Game->Frame());
				REQUIRE(Game->Session->ChapterCard().Shown() == 0);
			}
		}
		else
		{
			Game->RunUntil([&] { return Game->Session->ChapterCard().Shown() != 0; }, 300);
			CHECK(Game->Session->ChapterCard().Shown() == 1);
			// It runs its course and does not come back.
			Game->RunUntil([&] { return Game->Session->ChapterCard().Shown() == 0; }, CardVblanks);
			CHECK(Game->Session->ChapterCard().Shown() == 0);
			for (int32 Frame = 0; Frame < 30; ++Frame)
			{
				REQUIRE_FALSE(Game->Frame());
			}
			CHECK(Game->Session->ChapterCard().Shown() == 0);
		}
	}
}

TEST_CASE("A chapter begun as the field leaves for the world map shows its card with the first control there", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->EnterField(ForestMap);
	Game->RunUntil([&] { return Game->Loop->Field()->Runtime().Context().bFieldRunning; }, 100);
	// As the scene after Lahan's fall ends, the party not the player's: the story moved into its sixth chapter, which the
	// field's frames write into the game's state, then the world map by Lahan.
	FEventContext& Context = Game->Loop->Field()->Runtime().Context();
	Context.PlayerLock = 0xFFFF;
	Context.Variables.Set(StoryProgress, IntoTheWoods);
	REQUIRE_FALSE(Game->Frame());
	Context.Requests.bWorldMap = true;
	Context.bFrameVariablesFrozen = true;
	Game->Session->Game().SetWorldMapPlace(LahanPlace, 1, 0xC00, 1);
	Game->Session->ChapterCard().Know(1);
	Game->RunUntil([&] { return Game->Loop->WorldMap() != nullptr; }, 4);
	REQUIRE(Game->Loop->WorldMap() != nullptr);
	CHECK(Game->Session->Chapter() == IntoTheWoodsChapter);

	// Nothing through the screen's fades: the card waits for the map to be the player's.
	const FWorldMapProgram& WorldMap = *Game->Loop->WorldMap();
	int32 FadeFrames = 0;
	while (Game->Session->ChapterCard().Shown() == 0 && FadeFrames < 400)
	{
		REQUIRE_FALSE(Game->Frame());
		++FadeFrames;
		const bool bFading = WorldMap.IsFadingOut() || WorldMap.Runtime().Context().Fading != 0;
		CHECK((Game->Session->ChapterCard().Shown() != 0) == !bFading);
	}
	CHECK(FadeFrames > FWorldMapProgram::EntryFadeFrames);
	CHECK(Game->Session->ChapterCard().Shown() == IntoTheWoodsChapter);

	// It stands still through the pause, then runs its course on the map.
	const int32 Level = Game->Session->ChapterCard().Level();
	REQUIRE_FALSE(Game->Frame(StartHeld));
	Game->RunUntil([&] { return WorldMap.Runtime().LastFrame() == EWorldMapFrame::Paused; }, 10);
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame());
	}
	CHECK(WorldMap.Runtime().LastFrame() == EWorldMapFrame::Paused);
	CHECK(Game->Session->ChapterCard().Level() <= Level + FChapterCard::FullLevel * 8 / FChapterCard::RiseVblanks);
	REQUIRE_FALSE(Game->Frame(StartHeld));
	Game->RunUntil([&] { return Game->Session->ChapterCard().Shown() == 0; }, CardVblanks);
	CHECK(Game->Session->ChapterCard().Shown() == 0);
	CHECK(Game->Loop->WorldMap() != nullptr);
}
