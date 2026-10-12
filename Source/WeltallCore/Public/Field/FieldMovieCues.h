#pragma once

#include "Foundation/Result.h"
#include "Media/DiscIdentification.h"

#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/** A sound effect the field plays along a movie: due once the movie shows its frame, counted from the movie's start frame. */
	struct FFieldMovieCue
	{
		uint16 Frame = 0;
		/** The effect of the set's effect file, and the sound driver's effect channel it plays on. */
		uint8 Effect = 0;
		int32 Channel = 0;
	};

	/**
	 * The sets of sound effects the field plays along a movie whose own sound it silences (FE A0's cue
	 * sets), as the Field program keeps them: set n's effects are those of effect file 115h + n of
	 * directory 1Ch, which the field loads for the movie.
	 */
	class FFieldMovieCues
	{
	public:
		static constexpr int32 EffectDirectory = 0x1C;
		static constexpr int32 FirstEffectFile = 0x115;

		/** Reads the table from a disc of the given release. */
		WELTALLCORE_API static TResult<FFieldMovieCues> Read(const FCdImage& Image, const FGameFileIndex& Index, EDiscRelease Release);

		int32 SetCount() const { return static_cast<int32>(Sets.size()); }

		/** Set Number's cues in their order; none past the table. */
		WELTALLCORE_API std::span<const FFieldMovieCue> Set(int32 Number) const;

	private:
		std::vector<std::vector<FFieldMovieCue>> Sets;
	};
}
