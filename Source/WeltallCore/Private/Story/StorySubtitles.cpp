#include "Story/StorySubtitles.h"

#include "Field/EventAssemblyText.h"
#include "Foundation/NumberText.h"
#include "Text/MesGlyphs.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int64 LastStory = 0x7FFF;

		FError LineError(int32 Line, std::string_view Words)
		{
			std::string Message = "line ";
			NumberText::Append(Message, Line);
			Message += ": ";
			Message += Words;
			return MakeError(std::move(Message));
		}
	}

	TResult<FStorySubtitles> FStorySubtitles::Parse(std::string_view Text)
	{
		using EventAssemblyText::WordsOf;
		FStorySubtitles Subtitles;
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
				if (Words.size() != 1 || Words[0] != "subtitles")
				{
					return LineError(Number, "the text begins 'subtitles'");
				}
				bBegun = true;
				continue;
			}
			// story S Words...
			const std::optional<int64> First = Words.size() > 2 && Words[0] == "story" ? NumberText::NumberWithin(Words[1], 1, LastStory) : std::nullopt;
			if (!First)
			{
				return LineError(Number, "a subtitle reads 'story S' and its words");
			}
			if (!Subtitles.List.empty() && *First <= Subtitles.List.back().First)
			{
				return LineError(Number, "the story's values rise from subtitle to subtitle");
			}
			FStorySubtitle Subtitle{static_cast<uint16>(*First), std::string(Words[2])};
			for (std::size_t Word = 3; Word < Words.size(); ++Word)
			{
				Subtitle.Text += ' ';
				Subtitle.Text += Words[Word];
			}
			const bool bPlain = std::all_of(Subtitle.Text.begin(), Subtitle.Text.end(), [](char Letter) { return MesCodeOfCharacter(Letter).has_value(); });
			if (!bPlain || Subtitle.Text.size() > MaxLetters)
			{
				return LineError(Number, "a subtitle is a line of the message font's plain characters, 24 at most");
			}
			Subtitles.List.push_back(std::move(Subtitle));
		}
		if (!bBegun)
		{
			return MakeError("the text begins 'subtitles'");
		}
		return Subtitles;
	}

	const FStorySubtitle* FStorySubtitles::At(uint16 Story) const
	{
		const auto After = std::upper_bound(List.begin(), List.end(), Story, [](uint16 Value, const FStorySubtitle& Subtitle) { return Value < Subtitle.First; });
		return After == List.begin() ? nullptr : &*(After - 1);
	}

	int32 FStorySubtitles::Part(uint16 First, uint16 ChapterFirst) const
	{
		const auto Begins = std::find_if(List.begin(), List.end(), [First](const FStorySubtitle& Subtitle) { return Subtitle.First == First; });
		if (Begins == List.end() || First < ChapterFirst)
		{
			return 0;
		}
		return static_cast<int32>(std::count_if(List.begin(), Begins + 1, [ChapterFirst](const FStorySubtitle& Subtitle) { return Subtitle.First >= ChapterFirst; }));
	}
}
