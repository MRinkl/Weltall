#pragma once

#include "Foundation/Result.h"
#include "WorldMap/WorldMapSound.h"

#include <array>
#include <memory>
#include <optional>
#include <utility>

namespace Weltall
{
	class FCdImage;
	class FFieldMusic;
	class FGameFileIndex;
	class FGameSoundSystem;
	class FSoundBankFile;
	class FSoundDriver;
	class FSoundEffectFile;
	class FSoundSequenceFile;
	struct FSoundBank;
	struct FSoundSequence;

	/**
	 * The world map's sound in the game: its scene group's bank, effects and songs read from the disc as
	 * the world map asks for them and played through the driver. Its current song outlives the world map
	 * (the battle plays the one the world map started, and a place entered leaves the map's faded out),
	 * as does the song kept across a battle, which the next world map takes back. Game thread only.
	 */
	class FWorldMapMusic final : public IWorldMapSound
	{
	public:
		WELTALLCORE_API FWorldMapMusic(FGameSoundSystem& InSound, FFieldMusic& InFieldMusic, const FCdImage& InImage, const FGameFileIndex& InIndex);
		WELTALLCORE_API ~FWorldMapMusic() override;
		FWorldMapMusic(const FWorldMapMusic&) = delete;
		FWorldMapMusic& operator=(const FWorldMapMusic&) = delete;

		/** The first of the scene's files that could not be read or had no room since the last call; the calls after it went on silent. */
		WELTALLCORE_API std::optional<FError> TakeError();
		/** The song playing now (the driver's current sequence), none before the world map starts one. */
		FSoundSequence* CurrentSequence() const { return Current.Sequence; }
		/** How far the song playing now has got: its measure and the beat within it. Nothing while none plays. */
		WELTALLCORE_API std::optional<std::pair<int32, int32>> SongPlace() const;
		/** The instruments leave sound RAM, whose room a field song's bank takes (the original loads over them); the driver locked. */
		WELTALLCORE_API void ReleaseBank(FSoundDriver& Driver);

		WELTALLCORE_API void SetSceneGroup(int32 Header) override;
		WELTALLCORE_API void StopFieldSong() override;
		WELTALLCORE_API void LoadBank() override;
		WELTALLCORE_API void RegisterEffects() override;
		WELTALLCORE_API void UnregisterEffects() override;
		WELTALLCORE_API void LoadSong(EWorldMapSong Song) override;
		WELTALLCORE_API void PlaySong(int32 Volume, int32 Ticks) override;
		WELTALLCORE_API void ResumeSong(int32 Volume, int32 Ticks) override;
		WELTALLCORE_API void SetSongVolume(int32 Volume, int32 Ticks) override;
		WELTALLCORE_API void FreeSong() override;
		WELTALLCORE_API void StopAllSongs() override;
		WELTALLCORE_API void KeepSong() override;
		WELTALLCORE_API void TakeKeptSong() override;
		WELTALLCORE_API void PlayEffect(int32 Effect) override;
		WELTALLCORE_API void PlayProtectedEffect(int32 Effect) override;
		WELTALLCORE_API void StopAllEffects() override;
		WELTALLCORE_API void SetEffectVolume(int32 Effect, int32 Volume) override;
		WELTALLCORE_API void FadeEffectVolume(int32 Effect, int32 Volume, int32 Ticks) override;
		WELTALLCORE_API void Pause() override;
		WELTALLCORE_API void Resume() override;

	private:
		/** A loaded sequence and the file it plays from, which must outlive it. */
		struct FSong
		{
			FSoundSequence* Sequence = nullptr;
			std::shared_ptr<const FSoundSequenceFile> File;
		};

		/** A file of the scene group, read once for the group; none (and the error kept) when it cannot be. */
		template <typename TFile>
		const TFile* Scene(std::shared_ptr<const TFile>& Slot, int32 Offset);
		void Fail(FError Error);
		/** The driver's id of the registered effects' effect Effect. */
		uint32 EffectId(int32 Effect) const;
		void Free(FSong& Song);

		FGameSoundSystem& Sound;
		FFieldMusic& FieldMusic;
		const FCdImage& Image;
		const FGameFileIndex& Index;
		int32 Header = -1;
		std::shared_ptr<const FSoundBankFile> BankFile;
		std::shared_ptr<const FSoundEffectFile> EffectFile;
		std::array<std::shared_ptr<const FSoundSequenceFile>, 3> SongFiles{};
		const FSoundBank* Bank = nullptr;
		/** The effects registered with the driver, which the unregistering names. */
		std::shared_ptr<const FSoundEffectFile> Registered;
		FSong Current;
		FSong Kept;
		std::optional<FError> Error;
	};
}
