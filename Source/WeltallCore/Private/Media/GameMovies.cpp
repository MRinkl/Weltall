#include "Media/GameMovies.h"

namespace Weltall
{
	namespace
	{
		/** The directory whose first record heads the movie group (both discs of the USA release). */
		constexpr int32 MovieDirectory = 25;
		/** A movie's recorded size is its sectors times the bytes of a raw sector after its header. */
		constexpr uint32 MovieSectorBytes = 2336;
	}

	std::optional<FMovieLocation> FindMovie(const FGameFileIndex& Index, int32 Number)
	{
		const FIndexRecord* Group = Index.Find(MovieDirectory, 1);
		if (Group == nullptr || Group->IsFile() || Number < 0 || Number >= -Group->Size)
		{
			return std::nullopt;
		}
		// File 1 is the group's header, so movie n is file n + 2.
		const FIndexRecord* Movie = Index.Find(MovieDirectory, Number + 2);
		if (Movie == nullptr || !Movie->IsFile())
		{
			return std::nullopt;
		}
		return FMovieLocation{Movie->Lba, static_cast<uint32>(Movie->Size) / MovieSectorBytes};
	}
}
