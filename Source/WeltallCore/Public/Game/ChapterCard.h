#pragma once

#include "Foundation/Types.h"

#include <algorithm>

namespace Weltall
{
	/**
	 * The title card of the story's chapters: when the story moves into a later chapter, the card of that
	 * chapter is shown once the player has control again, or as soon as a scene asks for it (a chapter the
	 * story passes inside scenes); it rises, stays and fades, counted in vertical blanks. A chapter's card is
	 * shown once, and none for a chapter before one already shown. A game loaded from a save shows no card
	 * for the chapter the save was made in. The expanded second disc's subtitles (StorySubtitles.h) are
	 * shown the same way, each known by its first value of the story's progress, on a card of their own, once
	 * the player has control under one: a subtitle after every one shown; a chapter's own card begun under a
	 * subtitle is followed by the subtitle's.
	 */
	class FChapterCard
	{
	public:
		/** The card's times, in vertical blanks. */
		static constexpr int32 RiseVblanks = 40;
		static constexpr int32 StayVblanks = 200;
		static constexpr int32 FadeVblanks = 60;
		/** The level of a card fully shown. */
		static constexpr int32 FullLevel = 128;

		/** A new game: no chapter has been shown, and no card is. */
		void Reset() { *this = {}; }

		/** The game goes on in Chapter and under Subtitle (a save's; 0 for none) without a card for either. */
		void Know(int32 Chapter, int32 Subtitle = 0)
		{
			Reset();
			Known = Chapter;
			KnownSubtitle = Subtitle;
		}

		/** A scene asks for Chapter's card: it begins once no other card is shown, whoever has the party. */
		void Ask(int32 Chapter) { Asked = std::max(Asked, Chapter); }

		/**
		 * A frame of Vblanks, the story in Chapter and under Subtitle (0 for none): a card shown goes on; otherwise
		 * one begins for a chapter a scene asked for, or for the story's when the player has control, if it is past
		 * every chapter shown, or else for its subtitle when the player has control, if it is past every subtitle
		 * shown. A card is a chapter's or a subtitle's, never both.
		 */
		WELTALLCORE_API void Advance(int32 Chapter, int32 Subtitle, bool bPlayerInControl, int32 Vblanks);

		/** The chapter whose card is shown, 0 for none. */
		int32 Shown() const { return Card; }
		/** The subtitle whose card is shown, 0 for a chapter's own card or none. */
		int32 ShownSubtitle() const { return CardSubtitle; }

		/** How strongly the shown card is drawn, 0 to FullLevel. */
		WELTALLCORE_API int32 Level() const;

	private:
		int32 Known = 0;
		int32 KnownSubtitle = 0;
		int32 Asked = 0;
		int32 Card = 0;
		int32 CardSubtitle = 0;
		int32 Elapsed = 0;
	};
}
