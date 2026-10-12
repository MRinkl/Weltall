#pragma once

#include "Foundation/Result.h"
#include "Media/DiscIdentification.h"

#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/** What the field's song table says of one song. */
	struct FFieldSong
	{
		/** The bank song meaning the battle themes' bank, which stays loaded. */
		static constexpr uint8 ResidentBank = 0xFF;

		/** The song whose bank the song plays with (songs share banks), or ResidentBank. */
		uint8 BankSong = 0;
		/** 1: the song's bank takes the room of the battle themes' bank; 0: it needs that bank loaded. */
		uint8 Flag = 0;

		bool EvictsBattleThemes() const { return Flag == 1; }
		bool NeedsBattleThemes() const { return Flag == 0; }
	};

	/** The Field program's table of its songs, which directory 28 holds as a bank and a sequence each. */
	class FFieldSongTable
	{
	public:
		/** Reads it from the Field program of a disc of the given release. */
		WELTALLCORE_API static TResult<FFieldSongTable> Read(const FCdImage& Image, const FGameFileIndex& Index, EDiscRelease Release);

		int32 Count() const { return static_cast<int32>(Songs.size()); }
		const FFieldSong& operator[](int32 Song) const { return Songs[static_cast<std::size_t>(Song)]; }

	private:
		std::vector<FFieldSong> Songs;
	};
}
