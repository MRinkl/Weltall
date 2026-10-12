#include "Game/ChapterTitles.h"

#include "Media/GameFileIndex.h"
#include "Text/MesGlyphs.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 TitlesDirectory = 17;
		constexpr int32 TitlesFile = 1;

		/** A line's title as plain text, the spaces that fill it out to the line's width cut. */
		std::string LineTitle(std::span<const uint8> Line)
		{
			std::string Title;
			for (std::size_t At = 0; At + 1 < Line.size() && Line[At] > 0x7F; At += 2)
			{
				Title.push_back(PlainOfFullWidth(Line[At], Line[At + 1]).value_or(' '));
			}
			Title.erase(Title.find_last_not_of(' ') + 1);
			return Title;
		}
	}

	TResult<FChapterTitles> FChapterTitles::Read(const FCdImage& Image, const FGameFileIndex& Index)
	{
		const FIndexRecord* Record = Index.Find(TitlesDirectory, TitlesFile);
		if (Record == nullptr)
		{
			return MakeError("the disc lacks the chapters' titles");
		}
		TResult<std::vector<uint8>> Lines = ReadFileSectors(Image, *Record);
		if (!Lines)
		{
			return Lines.Error();
		}
		Lines->resize(static_cast<std::size_t>(Record->Size));
		return Parse(*Lines);
	}

	FChapterTitles FChapterTitles::Parse(std::span<const uint8> Lines)
	{
		FChapterTitles Titles;
		std::size_t At = 0;
		while (At < Lines.size())
		{
			// A line ends at its line feed; a two-byte character is passed whole.
			std::size_t End = At;
			while (End < Lines.size() && Lines[End] != '\n')
			{
				End += Lines[End] > 0x7F ? 2 : 1;
			}
			End = std::min(End, Lines.size());
			std::string Title = LineTitle(Lines.subspan(At, End - At));
			if (!Title.empty())
			{
				if (Titles.List.empty() || Titles.List.back().Title != Title)
				{
					Titles.List.push_back({static_cast<int32>(Titles.List.size()) + 1, Titles.LineCount, std::move(Title)});
				}
				++Titles.LineCount;
			}
			At = End + 1;
		}
		return Titles;
	}

	const FChapter* FChapterTitles::Find(uint16 Progress) const
	{
		const auto After = std::upper_bound(List.begin(), List.end(), Progress, [](uint16 Value, const FChapter& Chapter) { return Value < Chapter.FirstProgress; });
		return After == List.begin() ? nullptr : &*(After - 1);
	}
}
