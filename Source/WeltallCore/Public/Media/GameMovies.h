#pragma once

#include "Media/GameFileIndex.h"

#include <optional>

namespace Weltall
{
	/** Where a movie lies on its disc. */
	struct FMovieLocation
	{
		uint32 FirstLba = 0;
		uint32 SectorCount = 0;
	};

	/**
	 * Movie Number (from 0) of a disc, as the game's Movie program finds it: the group the movie
	 * directory starts with holds the disc's movies in order. Nothing when the disc has fewer movies,
	 * which the program then does not play.
	 */
	WELTALLCORE_API std::optional<FMovieLocation> FindMovie(const FGameFileIndex& Index, int32 Number);
}
