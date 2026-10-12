#pragma once

#include "Field/EventContext.h"

namespace Weltall::Testing
{
	/**
	 * The field's music as a map's scripts leave the Field program's globals, for the oracles: the
	 * song asked for (0xFF none), whether its load is pending, the start the last request set (-1
	 * full volume, 0 silent), and how often the music stopped. A load finishes at the field's next
	 * poll, at once, but never starts the song: one plays only when a test says so, until the music
	 * stops; asking for no song once the map runs, or starting a map without one, loads nothing. Its
	 * changes to the playing song go nowhere unless a test records them.
	 */
	class FFieldSongsModel : public IFieldSongs
	{
	public:
		static constexpr int32 NoSong = 0xFF;

		void SetSongStart(int32, bool bSilent) override { StartVolume = bSilent ? 0 : -1; }

		void ChooseSong(int32 InSong) override
		{
			if (InSong != Song)
			{
				StopMusic();
				bLoading = true;
			}
			Song = InSong;
		}

		bool IsLoading() const override { return bLoading; }

		bool ChangeSong(int32 InSong) override
		{
			if (InSong != Song)
			{
				StopMusic();
				bLoading = InSong != NoSong;
			}
			Song = InSong;
			return true;
		}

		void StartMapSong(bool bAfterDefeat) override
		{
			bLoading = false;
			if (bAfterDefeat)
			{
				Song = DefeatSong;
			}
			if (Song != Playing)
			{
				StopMusic();
				bLoading = Song != NoSong;
			}
		}

		std::optional<FError> Poll() override
		{
			bLoading = false;
			return std::nullopt;
		}

		int32 ChosenSong() const override { return Song; }

		bool IsMusicLoaded() const override { return bMusicLoaded; }
		void SetMusicVolume(int32, int32) override {}
		void SetMusicPitch(int32, int32) override {}
		void SetMusicTempo(int32, int32) override {}
		void SetMusicPan(int32, int32) override {}
		void MuteMusicTracks(uint32) override {}

		int32 Song = 0xFF;
		/** The song the driver plays: none at power-on, and the oracles' loads never start one. */
		int32 Playing = -1;
		bool bLoading = false;
		int32 StartVolume = -1;
		int32 Stops = 0;
		bool bMusicLoaded = false;

	private:
		void StopMusic()
		{
			++Stops;
			bMusicLoaded = false;
		}
	};
}
