#include "Support/GameDiscs.h"

#include "Media/GameLibrary.h"

namespace Weltall::Testing
{
	const FIso9660* FindDisc(EDiscRelease Release)
	{
		static const FGameLibrary Library = FGameLibrary::Scan(WELTALL_GAMES_DIR);
		return Library.Find(Release);
	}
}
