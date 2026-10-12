#include "Story/StoryWayBack.h"

#include "Field/EventAssemblyText.h"
#include "Field/FieldMap.h"
#include "Foundation/NumberText.h"
#include "Game/GameState.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

namespace Weltall
{
	namespace
	{
		constexpr int64 LastStory = 0xFFFF;
		constexpr int64 LastEntrance = 0xFF;
		constexpr std::string_view RowShape = "a row reads 'story S to map M entrance E story T', S a value or a range of them (254-257)";

		FError LineError(int32 Line, std::string_view Words)
		{
			std::string Message = "line ";
			NumberText::Append(Message, Line);
			Message += ": ";
			Message += Words;
			return MakeError(std::move(Message));
		}

		/** "253", or "254-257": the first and last values. */
		std::optional<std::pair<int64, int64>> StoryRange(std::string_view Word)
		{
			const std::size_t Dash = Word.find('-', 1);
			const std::optional<int64> First = NumberText::NumberWithin(Word.substr(0, Dash), 0, LastStory);
			const std::optional<int64> Last = Dash == std::string_view::npos ? First : NumberText::NumberWithin(Word.substr(Dash + 1), 0, LastStory);
			if (!First || !Last || *Last < *First)
			{
				return std::nullopt;
			}
			return std::pair{*First, *Last};
		}
	}

	TResult<FStoryWayBack> FStoryWayBack::Parse(std::string_view Text)
	{
		using EventAssemblyText::WordsOf;
		FStoryWayBack WayBack;
		bool bBegun = false;
		for (int32 Number = 1; !Text.empty(); ++Number)
		{
			const std::size_t End = std::min(Text.find('\n'), Text.size());
			const std::vector<std::string_view> Words = WordsOf(Text.substr(0, End));
			Text.remove_prefix(std::min(End + 1, Text.size()));
			if (Words.empty())
			{
				continue;
			}
			if (!bBegun)
			{
				if (Words.size() != 2 || Words[0] != "way" || Words[1] != "back")
				{
					return LineError(Number, "the text begins 'way back'");
				}
				bBegun = true;
				continue;
			}
			// story S to map M entrance E story T
			const bool bShaped = Words.size() == 9 && Words[0] == "story" && Words[2] == "to" && Words[3] == "map" && Words[5] == "entrance" && Words[7] == "story";
			const std::optional<std::pair<int64, int64>> Range = bShaped ? StoryRange(Words[1]) : std::nullopt;
			const std::optional<int64> Map = bShaped ? NumberText::NumberWithin(Words[4], 0, FieldMapCount - 1) : std::nullopt;
			const std::optional<int64> Entrance = bShaped ? NumberText::NumberWithin(Words[6], 0, LastEntrance) : std::nullopt;
			const std::optional<int64> Story = bShaped ? NumberText::NumberWithin(Words[8], 0, LastStory) : std::nullopt;
			if (!Range || !Map || !Entrance || !Story)
			{
				return LineError(Number, bShaped && Range && !Map ? "the point is a map of the disc's own, below 730" : RowShape);
			}
			const auto Overlaps = [&Range](const FRow& Other) { return Range->first <= Other.Last && Other.First <= Range->second; };
			if (std::any_of(WayBack.Rows.begin(), WayBack.Rows.end(), Overlaps))
			{
				return LineError(Number, "a value of the story another row names already");
			}
			WayBack.Rows.push_back({static_cast<uint16>(Range->first), static_cast<uint16>(Range->second),
				{static_cast<uint16>(*Map), static_cast<uint16>(*Entrance), static_cast<uint16>(*Story)}});
		}
		if (!bBegun)
		{
			return MakeError("the text begins 'way back'");
		}
		return WayBack;
	}

	const FWayBackPoint* FStoryWayBack::For(uint16 Story) const
	{
		const auto Row = std::find_if(Rows.begin(), Rows.end(), [Story](const FRow& Each) { return Story >= Each.First && Story <= Each.Last; });
		return Row != Rows.end() ? &Row->Point : nullptr;
	}

	const FWayBackPoint* FStoryWayBack::For(const FGameState& State) const
	{
		return State.StoryMode() == EStoryMode::Expanded ? For(State.SavedEventVariable(0)) : nullptr;
	}
}
