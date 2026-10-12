#include "Game/ChapterCard.h"

#include <algorithm>

namespace Weltall
{
	void FChapterCard::Advance(int32 Chapter, int32 Subtitle, bool bPlayerInControl, int32 Vblanks)
	{
		if (Card != 0)
		{
			Elapsed += Vblanks;
			if (Elapsed >= RiseVblanks + StayVblanks + FadeVblanks)
			{
				Card = 0;
				CardSubtitle = 0;
			}
			return;
		}
		const bool bAsked = Asked > Known;
		const int32 Next = bAsked ? Asked : bPlayerInControl ? Chapter : 0;
		if (Next > Known)
		{
			Known = Next;
			Card = Next;
			// The subtitle the story is under, if any, has its own card once this one is over.
			CardSubtitle = 0;
			Elapsed = 0;
			return;
		}
		if (bPlayerInControl && Chapter != 0 && Subtitle > KnownSubtitle)
		{
			KnownSubtitle = Subtitle;
			Card = Chapter;
			CardSubtitle = Subtitle;
			Elapsed = 0;
		}
	}

	int32 FChapterCard::Level() const
	{
		if (Card == 0)
		{
			return 0;
		}
		if (Elapsed < RiseVblanks)
		{
			return FullLevel * Elapsed / RiseVblanks;
		}
		const int32 Left = RiseVblanks + StayVblanks + FadeVblanks - Elapsed;
		return FullLevel * std::clamp(Left, 0, FadeVblanks) / FadeVblanks;
	}
}
