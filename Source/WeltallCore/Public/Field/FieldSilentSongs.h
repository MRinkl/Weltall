#pragma once

#include "Field/EventContext.h"

namespace Weltall
{
	/**
	 * The field's songs where nothing is heard: a song asked for loads by the field's next poll and
	 * then plays, as far as the scripts can tell, until another replaces it; the changes the scripts
	 * make to it (volume, tempo...) go nowhere.
	 */
	class FFieldSilentSongs final : public IFieldSongs
	{
	public:
		static constexpr int32 NoSong = 0xFF;

		void SetSongStart(int32, bool) override {}

		void ChooseSong(int32 InSong) override
		{
			if (InSong != Song)
			{
				bPlaying = false;
				bLoading = InSong != NoSong;
			}
			Song = InSong;
		}

		bool IsLoading() const override { return bLoading; }

		bool ChangeSong(int32 InSong) override
		{
			ChooseSong(InSong);
			return true;
		}

		void StartMapSong(bool bAfterDefeat) override
		{
			if (bAfterDefeat)
			{
				ChooseSong(DefeatSong);
			}
			bLoading = Song != NoSong && !bPlaying;
		}

		std::optional<FError> Poll() override
		{
			if (bLoading)
			{
				bLoading = false;
				bPlaying = true;
			}
			return std::nullopt;
		}

		int32 ChosenSong() const override { return Song; }
		bool IsMusicLoaded() const override { return bPlaying; }
		void SetMusicVolume(int32, int32) override {}
		void SetMusicPitch(int32, int32) override {}
		void SetMusicTempo(int32, int32) override {}
		void SetMusicPan(int32, int32) override {}
		void MuteMusicTracks(uint32) override {}

	private:
		int32 Song = NoSong;
		bool bLoading = false;
		bool bPlaying = false;
	};
}
