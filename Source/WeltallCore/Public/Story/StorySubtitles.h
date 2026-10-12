#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	/** A subtitle of the story: the first value of the story's progress under it, and its words. */
	struct FStorySubtitle
	{
		uint16 First = 0;
		std::string Text;
	};

	/**
	 * The titles the expanded second disc gives stretches of its own inside the original's chapters, written as text:
	 *
	 *   subtitles
	 *   story 254 Secret Battleship     from the story's 254 on, until the next subtitle or the next chapter
	 *   story 258 Soylent System
	 *
	 * Each is shown as a card of its own once the player has control under it (ChapterCard.h), numbered inside its
	 * chapter: "Chapter 57a: Secret Battleship", "Chapter 57b: Soylent System". The values rise from line to line; the
	 * words are the message font's plain characters, at most MaxLetters of them. A ';' begins a comment.
	 */
	class FStorySubtitles
	{
	public:
		/** The most letters a subtitle has: the card's one line with "Chapter 57a: " before it, the screen's width. */
		static constexpr std::size_t MaxLetters = 24;

		/** Reads the text; the error names the line. */
		WELTALLCORE_API static TResult<FStorySubtitles> Parse(std::string_view Text);

		/** The last subtitle whose first value is at most Story; null before the first. */
		WELTALLCORE_API const FStorySubtitle* At(uint16 Story) const;

		/** Which of the subtitles from ChapterFirst on the one beginning at First is, from 1; 0 where none begins there. */
		WELTALLCORE_API int32 Part(uint16 First, uint16 ChapterFirst) const;

		const std::vector<FStorySubtitle>& All() const { return List; }

	private:
		std::vector<FStorySubtitle> List;
	};
}
