#include "Foundation/Sha1.h"
#include "Menu/MenuMesWindow.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Text/MesWindow.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <string_view>

using namespace Weltall;

namespace
{
	/** The game's encoding of plain text: its narrow English glyphs, and 01 for a line end. */
	std::vector<uint8> Encode(std::string_view Text)
	{
		const std::string_view Symbols = " +,-./";
		const std::string_view Punctuation = "!\"#%&'():?";
		std::vector<uint8> Bytes;
		for (const char Character : Text)
		{
			if (Character >= '0' && Character <= '9')
			{
				Bytes.push_back(static_cast<uint8>(0x16 + Character - '0'));
			}
			else if (Character >= 'A' && Character <= 'Z')
			{
				Bytes.push_back(static_cast<uint8>(0x20 + Character - 'A'));
			}
			else if (Character >= 'a' && Character <= 'z')
			{
				Bytes.push_back(static_cast<uint8>(0x3D + Character - 'a'));
			}
			else if (Character == '\n')
			{
				Bytes.push_back(0x01);
			}
			else if (Symbols.find(Character) != std::string_view::npos)
			{
				Bytes.push_back(static_cast<uint8>(0x10 + Symbols.find(Character)));
			}
			else
			{
				Bytes.push_back(static_cast<uint8>(0x57 + Punctuation.find(Character)));
			}
		}
		return Bytes;
	}

	std::vector<uint8> Join(std::initializer_list<std::vector<uint8>> Parts)
	{
		std::vector<uint8> Bytes;
		for (const std::vector<uint8>& Part : Parts)
		{
			Bytes.insert(Bytes.end(), Part.begin(), Part.end());
		}
		return Bytes;
	}

	/** The names the scenarios insert: Fei and Elly, party slot 80h holds Elly and 81h none. */
	class FTestState final : public IMesGameState
	{
	public:
		FTestState() : Names{Encode("Fei"), Encode("Elly")}
		{
			for (std::vector<uint8>& Name : Names)
			{
				Name.push_back(0);
			}
		}

		std::span<const uint8> Name(int32 Number) const override
		{
			return Number >= 0 && Number < static_cast<int32>(Names.size()) ? std::span<const uint8>(Names[static_cast<std::size_t>(Number)])
																			: std::span<const uint8>();
		}

		uint8 PartyName(int32 Code) const override { return Code == 0x80 ? 1 : 0xFF; }

	private:
		std::vector<std::vector<uint8>> Names;
	};

	/**
	 * Runs a scenario as the oracle ran the kernel's window: 20 units by 3 lines, the numbers
	 * 1234, -56, 0 and 7, weapon 5 selected; each tick one update, and the button after a stop has
	 * lasted 3 ticks or at the given ticks. Returns the SHA-1 of the state after every tick.
	 */
	std::string RunScenario(const FMesResident& Resident, std::initializer_list<std::vector<uint8>> Strings, std::set<int32> PressAt = {})
	{
		const FTestState State;
		FMesContext Context{&Resident.Font, &Resident.NameTables, &State, {1, 0, 3, 2, 4, 5, 6, 7}, {}};
		FMesWindow Window(20, 3);
		const int32 Numbers[] = {1234, -56, 0, 7};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Window.SetNumber(Index, Numbers[Index]);
		}
		Window.SetSelector(0x0105);
		for (const std::vector<uint8>& String : Strings)
		{
			std::vector<uint8> Terminated = String;
			Terminated.push_back(0);
			Window.Queue(Terminated);
		}
		std::string Log;
		int32 Stopped = 0;
		for (int32 Tick = 0; Tick < 160; ++Tick)
		{
			Window.Update(Context);
			Stopped = Window.StopReason() != EMesStop::Typing ? Stopped + 1 : 0;
			if (Stopped == 3 || PressAt.contains(Tick))
			{
				Window.Continue();
				Stopped = 0;
			}
			const FMesWindow::FState S = Window.State();
			std::string Widths;
			for (std::size_t Index = 0; Index < S.Widths.size(); ++Index)
			{
				Widths += (Index > 0 ? "." : "") + std::to_string(S.Widths[Index]);
			}
			Log += std::to_string(S.X) + "," + std::to_string(S.Line) + "," + std::to_string(S.TopLine) + "," + std::to_string(S.LinesTyped) + ","
				+ std::to_string(S.Flags) + "," + std::to_string(S.Reason) + "," + std::to_string(S.bClosing ? 1 : 0) + ","
				+ std::to_string(S.WaitTimer) + "," + std::to_string(S.DelayCounter) + "," + std::to_string(S.DelayReload) + ","
				+ std::to_string(S.BaseSpeed) + "," + std::to_string(S.Speed) + "," + std::to_string(S.SavedSpeed) + "," + std::to_string(S.Queued)
				+ "," + Widths + "," + std::to_string(S.Next) + "\n";
		}
		return FSha1::ToHex(FSha1::Hash(std::span(reinterpret_cast<const uint8*>(Log.data()), Log.size())));
	}

	std::vector<uint8> Code(std::initializer_list<uint8> Bytes) { return Bytes; }
}

// Each scenario's digest comes from the kernel's own message window (made, queued, updated and
// continued) run in an emulated CPU with the same strings, names and presses.
TEST_CASE("The message window types, wraps and pages as the kernel's does", "[Text][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	CHECK(RunScenario(Resident, {Join({Encode("Fei\n\"Hi there, it's a long line of text.\""), Code({0x02, 0x01}), Encode("Next page")})})
		== "28cd9159d1c0206ec618277fae714850c6bc7188");
	CHECK(RunScenario(Resident, {Encode("L1\nL2\nL3\nL4\nL5")}) == "f16e22271a9d5620f61c8ab7ebea9c447e020cde");
}

TEST_CASE("The message window keeps the kernel's speeds, waits, holds and closes", "[Text][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	CHECK(RunScenario(Resident, {Join({Code({0x0F, 0x01, 0x03}), Encode("Fast text here\n"), Code({0x0F, 0x01, 0x00}), Encode("slow"),
									 Code({0x0F, 0x00, 0x07}), Encode("after"), Code({0x03, 0x01}), Encode("held"), Code({0x0F, 0x02, 0x05})})})
		== "d43e91479a5b4b358829d389f0e2591353ccb5df");
	CHECK(RunScenario(Resident, {Join({Code({0x0F, 0x0E, 0x02}), Encode("Delayed"), Code({0x0F, 0x0D, 0x06}), Encode(" more")}),
									Join({Encode("Second"), Code({0x0F, 0x0D, 0x08})})},
			  {12})
		== "c6ae439e7b5f9d0ddd5aabe86a47ebf72200db1e");
}

TEST_CASE("The message window tells its drawing each line's strip of the text image and the glyphs each tick typed", "[Text][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const FTestState State;
	FMesContext Context{&Resident.Font, &Resident.NameTables, &State, {1, 0, 3, 2, 4, 5, 6, 7}, {}};
	FMesWindow Window(20, 3);
	std::vector<uint8> String = Encode("ab\ncd\nef\ngh\nij");
	String.push_back(0);

	// Without text the window draws nothing and types nothing.
	Window.Update(Context);
	CHECK_FALSE(Window.Drew());
	CHECK_FALSE(Window.Stepped());

	// A glyph a tick: the tick that types it draws the lines as they were before it.
	Window.Queue(String);
	Window.Update(Context);
	CHECK(Window.Drew());
	CHECK(Window.Stepped());
	REQUIRE(Window.TypedGlyphs().size() == 1);
	CHECK(Window.TypedGlyphs()[0].XUnits == 0);
	CHECK(Window.CurrentStrip() == 0);
	CHECK(Window.Row(0).WidthUnits == 0);
	Window.Update(Context);
	REQUIRE(Window.TypedGlyphs().size() == 1);
	CHECK(Window.TypedGlyphs()[0].XUnits == Window.Row(0).WidthUnits);
	CHECK(Window.Row(0).WidthUnits > 0);

	// Each line of the three types into its own strip; scrolled, the fourth takes the one strip more the window
	// cycles through, and the fifth the first again, while the lines above keep theirs.
	std::vector<int32> Strips;
	int32 Line = 0;
	for (int32 Tick = 0; Tick < 40 && Window.StopReason() == EMesStop::Typing; ++Tick)
	{
		Window.Update(Context);
		if (Window.CurrentLine() != Line || Strips.empty())
		{
			Line = Window.CurrentLine();
			Strips.push_back(Window.CurrentStrip());
		}
	}
	CHECK(Strips == std::vector<int32>{0, 1, 2, 3, 0});
	Window.Update(Context);
	CHECK(Window.Row(0).Strip == 2);
	CHECK(Window.Row(1).Strip == 3);
	CHECK(Window.Row(2).Strip == 0);
	// Stopped for the button, the window is drawn but types no more.
	CHECK(Window.Drew());
	CHECK_FALSE(Window.Stepped());
	CHECK(Window.TypedGlyphs().empty());
}

TEST_CASE("The message window inserts names, numbers, items and buttons as the kernel's does", "[Text][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	CHECK(RunScenario(Resident, {Join({Code({0x0F, 0x05, 0x00}), Encode(" and "), Code({0x0F, 0x05, 0x80}), Encode(","), Code({0x0F, 0x05, 0x81}),
									 Code({0x01, 0x0F, 0x0A, 0x00}), Encode(" "), Code({0x0F, 0x0C, 0x01}), Encode(" "), Code({0x0F, 0x09, 0x03}),
									 Code({0x01, 0x0F, 0x04}), Encode(" "), Code({0x0F, 0x0F, 0x00}), Code({0x0F, 0x03, 0x16, 0x05})})})
		== "ed8c04e7f02a37c0e520dd82d414e687a8cf0f1d");
}

TEST_CASE("The player's text speed types two or four times the glyphs a tick, or the page at once", "[Text][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	struct FNoNames final : IMesGameState
	{
		std::span<const uint8> Name(int32) const override { return {}; }
		uint8 PartyName(int32) const override { return 0xFF; }
	} State;
	const auto TypedAfter = [&](EMesTextSpeed Speed, int32 Ticks) {
		FMesContext Context{&Resident.Font, &Resident.NameTables, &State, {1, 0, 3, 2, 4, 5, 6, 7}, {}, Speed};
		FMesWindow Window(30, 3);
		Window.Queue(Join({Encode("First line\nSecond line\nThird"), Code({0x02, 0x01}), Encode("Next page")}));
		std::size_t Typed = 0;
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			Window.Update(Context);
			Typed += Window.TypedGlyphs().size();
		}
		return std::pair{Typed, Window.StopReason()};
	};
	// Within the first line: a step stops at a line's end, as the kernel's does.
	const std::size_t Original = TypedAfter(EMesTextSpeed::Original, 2).first;
	CHECK(TypedAfter(EMesTextSpeed::Double, 2).first == 2 * Original);
	CHECK(TypedAfter(EMesTextSpeed::Quadruple, 2).first == 4 * Original);
	// Instant: the whole first page in the first tick that types, then the page's stop.
	const auto [Instant, Stop] = TypedAfter(EMesTextSpeed::Instant, 3);
	CHECK(Instant == std::string_view("First lineSecond lineThird").size());
	CHECK(Stop == EMesStop::Page);
}

TEST_CASE("A tick's typed glyphs are told line by line, and a page typed at once leaves the text image a glyph a tick leaves", "[Text][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	struct FNoNames final : IMesGameState
	{
		std::span<const uint8> Name(int32) const override { return {}; }
		uint8 PartyName(int32) const override { return 0xFF; }
	} State;
	// Three lines of a first page, each shorter than the one before so that a line's strip keeps what the last left, then a second page.
	const std::vector<uint8> String = Join({Encode("The first line\nA second\nThird"), Code({0x02, 0x01}), Encode("Next\npage"), Code({0x00})});
	const FMesWindowPlace Place{0x340, 0x100, 0, 0, 0, 0};

	// What VRAM holds of the window's strips once the string is typed and its pages turned, by each upload's row, and the
	// most lines one tick typed.
	struct FTyped
	{
		std::map<int32, std::vector<uint16>> Strips;
		std::size_t MostLinesATick = 0;
		bool bPageAtOnce = false;
	};
	const auto Type = [&](EMesTextSpeed Speed)
	{
		FMesContext Context{&Resident.Font, &Resident.NameTables, &State, {1, 0, 3, 2, 4, 5, 6, 7}, {}, Speed};
		FMesWindow Window(30, 3);
		FMesWindowImage Image;
		Window.Queue(String);
		FTyped Typed;
		std::vector<FVramImage> Uploads;
		for (int32 Tick = 0; Tick < 200 && Window.StopReason() != EMesStop::End; ++Tick)
		{
			Window.Update(Context);
			const std::span<const FMesTypedLine> Lines = Window.TypedLines();
			Typed.MostLinesATick = std::max(Typed.MostLinesATick, Lines.size());
			// The lines share out the tick's glyphs in order, and the last is the line the text is on.
			int32 Next = 0;
			for (const FMesTypedLine& Line : Lines)
			{
				CHECK(Line.First == Next);
				CHECK(Line.Count > 0);
				Next += Line.Count;
			}
			CHECK(Next == static_cast<int32>(Window.TypedGlyphs().size()));
			if (!Lines.empty())
			{
				CHECK(Lines.back().Strip == Window.CurrentStrip());
			}
			if (Lines.size() == 3)
			{
				Typed.bPageAtOnce = Lines[0].Strip == 0 && Lines[1].Strip == 1 && Lines[2].Strip == 2;
			}
			Uploads.clear();
			Image.Type(&Resident.Font, Window, Place, Uploads);
			for (const FVramImage& Upload : Uploads)
			{
				Typed.Strips[Upload.Rect.Y] = Upload.Units;
			}
			if (Window.StopReason() == EMesStop::Page)
			{
				Window.Continue();
			}
		}
		CHECK(Window.StopReason() == EMesStop::End);
		return Typed;
	};
	const FTyped Original = Type(EMesTextSpeed::Original);
	const FTyped Quadruple = Type(EMesTextSpeed::Quadruple);
	const FTyped Instant = Type(EMesTextSpeed::Instant);
	CHECK(Original.MostLinesATick == 1);
	CHECK(Quadruple.MostLinesATick == 1);
	CHECK(Instant.MostLinesATick == 3);
	CHECK(Instant.bPageAtOnce);
	// Two rows of strips: the first holds the first and second lines' planes, the second the third's.
	CHECK(Original.Strips.size() == 2);
	const bool bQuadrupleImage = Quadruple.Strips == Original.Strips;
	const bool bInstantImage = Instant.Strips == Original.Strips;
	CHECK(bQuadrupleImage);
	CHECK(bInstantImage);
}

TEST_CASE("A message that ends by itself is up as long at any text speed", "[Text][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const FTestState State;
	/** When a string's text was whole (its timed end reached) and when it ended, in ticks since the player last went on. */
	struct FTimes
	{
		int32 Whole = 0;
		int32 Ended = 0;
		bool operator==(const FTimes&) const = default;
	};
	// Plays a string, the button pressed PressAfter ticks into each stop; a choice made of the window once it waits.
	const auto Play = [&](EMesTextSpeed Speed, const std::vector<uint8>& String, int32 PressAfter = 3, bool bChoice = false) {
		FMesContext Context{&Resident.Font, &Resident.NameTables, &State, {1, 0, 3, 2, 4, 5, 6, 7}, {}, Speed};
		FMesWindow Window(60, 3);
		Window.SetNumber(0, 12345);
		Window.Queue(String);
		FTimes Times;
		int32 Stopped = 0;
		int32 Ticks = 0;
		for (int32 Tick = 0; Tick < 4000; ++Tick)
		{
			Window.Update(Context);
			++Ticks;
			if (!Window.HasText())
			{
				Times.Ended = Ticks;
				break;
			}
			if (Window.IsClosing() && !Window.IsStopped() && Times.Whole == 0)
			{
				Times.Whole = Ticks;
				if (bChoice)
				{
					Window.DropOwedWait();
				}
			}
			if (Window.IsStopped() && ++Stopped == PressAfter)
			{
				Window.Continue();
				Stopped = 0;
				Ticks = 0;
			}
		}
		return Times;
	};
	const std::array<EMesTextSpeed, 3> Faster = {EMesTextSpeed::Double, EMesTextSpeed::Quadruple, EMesTextSpeed::Instant};

	// Two lines, a pause, a name and a number put in, then a timer of ten ticks: the text is whole sooner, the end comes when the game's does.
	const std::vector<uint8> Closing = Join({Encode("A line that takes its time,\n"), Code({0x0F, 0x05, 0x00}), Encode(" has "), Code({0x0F, 0x0A, 0x00}),
		Code({0x0F, 0x00, 0x08}), Encode(" of them."), Code({0x0F, 0x02, 0x14})});
	const FTimes Original = Play(EMesTextSpeed::Original, Closing);
	REQUIRE(Original.Ended > Original.Whole + 9);
	int32 WholeBefore = Original.Whole;
	for (const EMesTextSpeed Speed : Faster)
	{
		const FTimes Times = Play(Speed, Closing);
		CHECK(Times.Ended == Original.Ended);
		CHECK(Times.Whole < WholeBefore);
		WholeBefore = Times.Whole;
	}

	// After a page the player reads at leisure, the time counts from the button, however long it took to come.
	const std::vector<uint8> Paged = Join({Encode("The first page."), Code({0x02, 0x01}), Encode("The second page, which closes by itself."), Code({0x0F, 0x02, 0x06})});
	const FTimes PagedOriginal = Play(EMesTextSpeed::Original, Paged);
	for (const EMesTextSpeed Speed : Faster)
	{
		CHECK(Play(Speed, Paged).Ended == PagedOriginal.Ended);
		CHECK(Play(Speed, Paged, 40).Ended == PagedOriginal.Ended);
	}

	// A wait the button answers is the game's own length: its text is whole sooner and its end comes that much sooner.
	const std::vector<uint8> Answered = Join({Encode("One, two...\n    Rock\n    Paper "), Code({0x0F, 0x0D, 0x20}), Code({0x0F, 0x02, 0x01})});
	const FTimes AnsweredOriginal = Play(EMesTextSpeed::Original, Answered);
	const FTimes AnsweredDouble = Play(EMesTextSpeed::Double, Answered);
	CHECK(AnsweredDouble.Whole < AnsweredOriginal.Whole);
	CHECK(AnsweredDouble.Ended - AnsweredDouble.Whole == AnsweredOriginal.Ended - AnsweredOriginal.Whole);
	// And so is a timed end's once its window is a choice.
	const FTimes ChoiceDouble = Play(EMesTextSpeed::Double, Closing, 3, true);
	CHECK(ChoiceDouble.Ended - ChoiceDouble.Whole == Original.Ended - Original.Whole);
	CHECK(ChoiceDouble.Ended < Original.Ended);

	// A string that never ends is not timed for ever: an extended code the kernel does not know holds the text.
	const std::vector<uint8> Stuck = Join({Encode("Held"), Code({0x0F, 0x30}), Code({0x0F, 0x02, 0x04})});
	CHECK(Play(EMesTextSpeed::Double, Stuck) == Play(EMesTextSpeed::Original, Stuck));
}

TEST_CASE("A message that scrolls by itself scrolls each line away no sooner at any text speed", "[Text][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const FTestState State;
	// When the window scrolled, and when each line was whole, in ticks since the string began; and when it stopped at its end.
	struct FTimes
	{
		std::vector<int32> Scrolls;
		std::vector<int32> LinesWhole;
		int32 Ended = 0;
	};
	const auto Play = [&](EMesTextSpeed Speed, const std::vector<uint8>& String) {
		FMesContext Context{&Resident.Font, &Resident.NameTables, &State, {1, 0, 3, 2, 4, 5, 6, 7}, {}, Speed};
		FMesWindow Window(60, 3);
		Window.Queue(String);
		FTimes Times;
		int32 Top = 0;
		int32 Line = 0;
		for (int32 Tick = 1; Tick < 4000 && Times.Ended == 0; ++Tick)
		{
			Window.Update(Context);
			const FMesWindow::FState S = Window.State();
			if (S.TopLine != Top)
			{
				Times.Scrolls.push_back(Tick);
				Top = S.TopLine;
			}
			// A line is whole once the text has gone on to the next, or to the end.
			for (; Line < S.LinesTyped; ++Line)
			{
				Times.LinesWhole.push_back(Tick);
			}
			if (Window.StopReason() == EMesStop::End)
			{
				Times.Ended = Tick;
			}
		}
		return Times;
	};
	// Six lines and a pause in a window of three, with no page: the window scrolls three times as the text types on, as the
	// tournament's opening speech in Aveh's castle does (34 lines in 4, map 131).
	const std::vector<uint8> Speech = Join({Encode("The sun above us is the\nsame as that which burns\nover our fathers at the\nfront."), Code({0x0F, 0x00, 0x1E}),
		Encode(" The wind blowing\nhere is the same as that\nwhich whisks our resolve"), Code({0x00})});
	const FTimes Original = Play(EMesTextSpeed::Original, Speech);
	REQUIRE(Original.Scrolls.size() == 3);
	for (const EMesTextSpeed Speed : {EMesTextSpeed::Double, EMesTextSpeed::Quadruple, EMesTextSpeed::Instant})
	{
		const FTimes Times = Play(Speed, Speech);
		CHECK(Times.Scrolls == Original.Scrolls);
		// The lines are typed at the player's speed: the first page's before the game's.
		REQUIRE(Times.LinesWhole.size() == Original.LinesWhole.size());
		CHECK(Times.LinesWhole[1] < Original.LinesWhole[1]);
		CHECK(Times.Ended < Original.Ended);
	}
}

TEST_CASE("A string's own slow typing keeps the game's pace at any text speed", "[Text][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const FTestState State;
	// The tick each glyph was typed at, in the order typed.
	const auto Play = [&](EMesTextSpeed Speed, const std::vector<uint8>& String) {
		FMesContext Context{&Resident.Font, &Resident.NameTables, &State, {1, 0, 3, 2, 4, 5, 6, 7}, {}, Speed};
		FMesWindow Window(60, 3);
		Window.Queue(String);
		std::vector<int32> Ticks;
		for (int32 Tick = 1; Tick < 2000 && Window.StopReason() != EMesStop::End; ++Tick)
		{
			Window.Update(Context);
			Ticks.insert(Ticks.end(), Window.TypedGlyphs().size(), Tick);
		}
		return Ticks;
	};
	// A line at the window's own speed, then the rest a glyph every three ticks (Elly's "But..." in map 174's message 20).
	const std::string_view Plain = "The only thing left";
	const std::vector<uint8> Voiced = Join({Encode(Plain), Code({0x01, 0x0F, 0x0E, 0x02}), Encode("But..."), Code({0x00})});
	const std::vector<int32> Original = Play(EMesTextSpeed::Original, Voiced);
	const std::size_t Slow = Plain.size();
	REQUIRE(Original.size() == Slow + 6);
	const auto Gaps = [Slow](const std::vector<int32>& Ticks) {
		std::vector<int32> Each;
		for (std::size_t Glyph = Slow + 1; Glyph < Ticks.size(); ++Glyph)
		{
			Each.push_back(Ticks[Glyph] - Ticks[Glyph - 1]);
		}
		return Each;
	};
	CHECK(Gaps(Original) == std::vector<int32>(5, 3));
	for (const EMesTextSpeed Speed : {EMesTextSpeed::Double, EMesTextSpeed::Quadruple, EMesTextSpeed::Instant})
	{
		const std::vector<int32> Ticks = Play(Speed, Voiced);
		REQUIRE(Ticks.size() == Original.size());
		// The plain line comes sooner, the voiced words as slowly.
		CHECK(Ticks[Slow - 1] < Original[Slow - 1]);
		CHECK(Gaps(Ticks) == Gaps(Original));
	}
}
