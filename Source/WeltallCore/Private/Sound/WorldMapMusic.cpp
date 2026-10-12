#include "Sound/WorldMapMusic.h"

#include "Sound/FieldMusic.h"
#include "Sound/GameSoundSystem.h"
#include "Sound/SoundDiscFiles.h"
#include "Sound/SoundDriver.h"
#include "Sound/SoundDriverState.h"
#include "Sound/SoundFiles.h"
#include "WorldMap/WorldMapTables.h"

#include <string>

namespace Weltall
{
	namespace
	{
		constexpr int32 EffectFileShift = 16;

		int32 SongOffset(EWorldMapSong Song)
		{
			switch (Song)
			{
			case EWorldMapSong::Vehicle:
				return WorldMapFiles::VehicleSong(0);
			case EWorldMapSong::Battle:
				return WorldMapFiles::BattleSong(0);
			default:
				return WorldMapFiles::MapSong(0);
			}
		}
	}

	FWorldMapMusic::FWorldMapMusic(FGameSoundSystem& InSound, FFieldMusic& InFieldMusic, const FCdImage& InImage, const FGameFileIndex& InIndex)
		: Sound(InSound), FieldMusic(InFieldMusic), Image(InImage), Index(InIndex)
	{
	}

	FWorldMapMusic::~FWorldMapMusic()
	{
		// The driver lets go of the sequences and effects before their files go.
		Free(Current);
		Free(Kept);
		UnregisterEffects();
	}

	std::optional<FError> FWorldMapMusic::TakeError()
	{
		std::optional<FError> Taken = std::move(Error);
		Error.reset();
		return Taken;
	}

	std::optional<std::pair<int32, int32>> FWorldMapMusic::SongPlace() const
	{
		const FLockedGameSound Locked = Sound.Lock();
		if (Current.Sequence == nullptr || !Current.Sequence->IsPlaying())
		{
			return std::nullopt;
		}
		return std::pair<int32, int32>{Current.Sequence->Measure, Current.Sequence->Beat};
	}

	void FWorldMapMusic::SetSceneGroup(int32 InHeader)
	{
		if (InHeader == Header)
		{
			return;
		}
		// Another group's files are read as they are asked for; the songs playing keep their own.
		Header = InHeader;
		BankFile.reset();
		EffectFile.reset();
		SongFiles = {};
	}

	void FWorldMapMusic::StopFieldSong()
	{
		FieldMusic.Stop();
	}

	void FWorldMapMusic::LoadBank()
	{
		const FSoundBankFile* File = Scene(BankFile, WorldMapFiles::Bank(0));
		if (File == nullptr)
		{
			return;
		}
		const FLockedGameSound Locked = Sound.Lock();
		// The last world map's instruments give way to this one's.
		ReleaseBank(Locked.Driver());
		Bank = Locked->LoadBank(*File);
		if (Bank == nullptr)
		{
			Fail(MakeError("the world map's instruments (file " + std::to_string(WorldMapFiles::Bank(Header)) + ") find no room in sound memory"));
		}
	}

	void FWorldMapMusic::ReleaseBank(FSoundDriver& Driver)
	{
		if (Bank != nullptr)
		{
			Driver.UnloadBank(*Bank);
			Bank = nullptr;
		}
	}

	void FWorldMapMusic::RegisterEffects()
	{
		if (Scene(EffectFile, WorldMapFiles::Effects(0)) == nullptr)
		{
			return;
		}
		UnregisterEffects();
		const FLockedGameSound Locked = Sound.Lock();
		if (Locked->RegisterEffects(*EffectFile))
		{
			Registered = EffectFile;
		}
	}

	void FWorldMapMusic::UnregisterEffects()
	{
		if (Registered)
		{
			const FLockedGameSound Locked = Sound.Lock();
			Locked->UnregisterEffects(*Registered);
			Registered.reset();
		}
	}

	void FWorldMapMusic::LoadSong(EWorldMapSong Song)
	{
		std::shared_ptr<const FSoundSequenceFile>& Slot = SongFiles[static_cast<std::size_t>(Song)];
		if (Scene(Slot, SongOffset(Song)) == nullptr)
		{
			return;
		}
		// The song before is left where the world map left it: faded out and paused, or stopped. It plays
		// no more and only its memory stays in the original, which the port gives back here.
		Free(Current);
		const FLockedGameSound Locked = Sound.Lock();
		Current.Sequence = Locked->LoadSequence(*Slot);
		Current.File = Slot;
		if (Current.Sequence == nullptr)
		{
			Current.File.reset();
			Fail(MakeError("the world map's song (file " + std::to_string(Header + SongOffset(Song)) + ") finds no room in the sound driver"));
		}
	}

	void FWorldMapMusic::PlaySong(int32 Volume, int32 Ticks)
	{
		if (Current.Sequence != nullptr)
		{
			Sound.Lock()->PlaySequence(*Current.Sequence, Volume, Ticks);
		}
	}

	void FWorldMapMusic::ResumeSong(int32 Volume, int32 Ticks)
	{
		if (Current.Sequence != nullptr)
		{
			Sound.Lock()->ResumeSequence(*Current.Sequence, Volume, Ticks);
		}
	}

	void FWorldMapMusic::SetSongVolume(int32 Volume, int32 Ticks)
	{
		if (Current.Sequence != nullptr)
		{
			Sound.Lock()->SetSequenceVolume(*Current.Sequence, Volume, Ticks);
		}
	}

	void FWorldMapMusic::FreeSong()
	{
		Free(Current);
	}

	void FWorldMapMusic::StopAllSongs()
	{
		Sound.Lock()->StopAllSequences();
	}

	void FWorldMapMusic::KeepSong()
	{
		Free(Kept);
		Kept = std::move(Current);
		Current = {};
	}

	void FWorldMapMusic::TakeKeptSong()
	{
		Free(Current);
		Current = std::move(Kept);
		Kept = {};
	}

	void FWorldMapMusic::PlayEffect(int32 Effect)
	{
		if (Registered)
		{
			Sound.Lock()->PlayEffect(EffectId(Effect));
		}
	}

	void FWorldMapMusic::PlayProtectedEffect(int32 Effect)
	{
		if (Registered)
		{
			Sound.Lock()->PlayProtectedEffect(EffectId(Effect));
		}
	}

	void FWorldMapMusic::SetEffectVolume(int32 Effect, int32 Volume)
	{
		if (Registered)
		{
			Sound.Lock()->SetEffectVolume(EffectId(Effect), Volume);
		}
	}

	void FWorldMapMusic::FadeEffectVolume(int32 Effect, int32 Volume, int32 Ticks)
	{
		if (Registered)
		{
			Sound.Lock()->FadeEffectVolume(EffectId(Effect), Volume, Ticks);
		}
	}

	uint32 FWorldMapMusic::EffectId(int32 Effect) const
	{
		return static_cast<uint32>(Registered->FileId()) << EffectFileShift | static_cast<uint32>(Effect);
	}

	void FWorldMapMusic::StopAllEffects()
	{
		Sound.Lock()->StopAllEffects();
	}

	void FWorldMapMusic::Pause()
	{
		Sound.Lock()->Pause();
	}

	void FWorldMapMusic::Resume()
	{
		Sound.Lock()->Resume();
	}

	template <typename TFile>
	const TFile* FWorldMapMusic::Scene(std::shared_ptr<const TFile>& Slot, int32 Offset)
	{
		if (Slot)
		{
			return Slot.get();
		}
		if (Header < 0)
		{
			Fail(MakeError("the world map asked for its sound before naming its scene group"));
			return nullptr;
		}
		TResult<TFile> Read = ReadSoundFile<TFile>(Image, Index, WorldMapFiles::Directory, Header + Offset);
		if (!Read)
		{
			Fail(Read.Error());
			return nullptr;
		}
		Slot = std::make_shared<const TFile>(std::move(*Read));
		return Slot.get();
	}

	void FWorldMapMusic::Fail(FError NewError)
	{
		if (!Error)
		{
			Error = std::move(NewError);
		}
	}

	void FWorldMapMusic::Free(FSong& Song)
	{
		if (Song.Sequence != nullptr)
		{
			Sound.Lock()->FreeSequence(*Song.Sequence);
		}
		Song = {};
	}
}
