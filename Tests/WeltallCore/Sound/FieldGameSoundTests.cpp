#include "Sound/FieldGameSound.h"
#include "Sound/GameSoundSession.h"
#include "Support/GameDiscs.h"
#include "Support/SoundRendering.h"

#include <catch_amalgamated.hpp>

#include <functional>

using namespace Weltall;
using Testing::Peak;

namespace
{
	constexpr int32 Second = 44100;
	/** A sound plays far above this, and the silence after one stays below Quiet. */
	constexpr int32 Playing = 1000;
	constexpr int32 Quiet = 64;
	/** Effect 9 of effect file 0, a sound the maps' actors give off. */
	constexpr uint32 Effect = 9;
	/** The scripts' channel 3, the driver's 6, whose tracks are the effect player's 14 and 15. */
	constexpr int32 Channel = 6;
	constexpr uint32 ChannelTracks = 0xC000;
	/** The gear battles' bank, and the one-instrument bank the scripts swap in for it (directory 28's file 6). */
	constexpr int32 GearBattleBank = 2;
	constexpr int32 SwappedBank = 0xC6;

	/** The game's sound as the field has it: started from the disc, effect file 0 registered. */
	struct FFieldSound
	{
		explicit FFieldSound(const FIso9660& Disc)
		{
			auto Started = FGameSoundSession::Start(Disc);
			REQUIRE(Started);
			Session = std::move(*Started);
			REQUIRE(Session->Sound()->Lock()->RegisterEffects(Session->Resident().MainEffects));
		}

		const FSoundBank* Bank(int32 Id) { return Session->Sound()->Lock()->FindBank(Id); }
		std::vector<int16> Render(int32 Frames) { return Testing::RenderSound(*Session->Sound(), Frames); }

		std::unique_ptr<FGameSoundSession> Session;
	};

	/**
	 * What the field's effects do through the session's service, and what the same driver calls made
	 * directly do, rendered side by side: the samples must be the same.
	 */
	void CheckAgainstDriver(const FIso9660& Disc, const std::function<void(IFieldSoundEffects&)>& ThroughService,
		const std::function<void(FSoundDriver&)>& Direct)
	{
		FFieldSound Service(Disc);
		FFieldSound Driven(Disc);
		ThroughService(Service.Session->FieldSoundEffects());
		{
			const FLockedGameSound Locked = Driven.Session->Sound()->Lock();
			Direct(Locked.Driver());
		}
		const std::vector<int16> Heard = Service.Render(Second);
		CHECK(Peak(Heard) > Playing);
		CHECK(Heard == Driven.Render(Second));
	}
}

TEST_CASE("The field's sound effects play, change and stop on the sound driver's channels", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckAgainstDriver(
		*Disc, [](IFieldSoundEffects& Effects) { Effects.PlayEffectOn(Effect, Channel, 0x60, 0x20); },
		[](FSoundDriver& Driver) { Driver.PlayEffectOnVolumePan(Effect, Channel, 0x60, 0x20); });
	CheckAgainstDriver(
		*Disc,
		[](IFieldSoundEffects& Effects) {
			Effects.PlayEffectOn(Effect, Channel, 0x7F, 0x40);
			Effects.SetEffectVolumeOn(Channel, 0x30);
			Effects.SetEffectPanOn(Channel, 0x70);
		},
		[](FSoundDriver& Driver) {
			Driver.PlayEffectOnVolumePan(Effect, Channel, 0x7F, 0x40);
			Driver.SetEffectVolumeOn(Channel, 0x30);
			Driver.SetEffectPanOn(Channel, 0x70);
		});
	CheckAgainstDriver(
		*Disc,
		[](IFieldSoundEffects& Effects) {
			Effects.PlayEffectOn(Effect, Channel, 0x7F, 0x40);
			Effects.FadeEffectVolumeOn(Channel, 0x10, 40);
		},
		[](FSoundDriver& Driver) {
			Driver.PlayEffectOnVolumePan(Effect, Channel, 0x7F, 0x40);
			Driver.FadeEffectVolumeOn(Channel, 0x10, 40);
		});

	FFieldSound Field(*Disc);
	IFieldSoundEffects& Effects = Field.Session->FieldSoundEffects();
	Effects.PlayEffectOn(Effect, Channel, 0x7F, 0x40);
	CHECK(Effects.PlayingEffects() == ChannelTracks);
	Effects.StopEffectOn(Channel);
	CHECK(Effects.PlayingEffects() == 0);
	Field.Render(3 * Second);
	CHECK(Peak(Field.Render(Second)) < Quiet);
}

TEST_CASE("The bank the field's scripts swap in takes the gear battles' bank's place in sound RAM", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FFieldSound Field(*Disc);
	FFieldGameSoundEffects& Banks = Field.Session->FieldSoundEffects();
	FResidentSound& Resident = Field.Session->Resident();
	const FSoundBank* const Resident2 = Field.Bank(GearBattleBank);
	REQUIRE(Resident2 != nullptr);
	CHECK(Resident.GearBattleBank == Resident2);
	CHECK_FALSE(Banks.FinishBankLoad());

	// The start gives the resident bank up at once and reads the new one; the finish uploads it.
	REQUIRE(Banks.BeginBankLoad(28, 6));
	CHECK(Field.Bank(GearBattleBank) == nullptr);
	CHECK(Resident.GearBattleBank == nullptr);
	CHECK(Field.Bank(SwappedBank) == nullptr);
	REQUIRE(Banks.FinishBankLoad());
	REQUIRE(Field.Bank(SwappedBank) != nullptr);
	CHECK(Resident.GearBattleBank == Field.Bank(SwappedBank));
	CHECK(FSoundDriver::BankAddress(*Field.Bank(SwappedBank)) == 0x68000);
	CHECK_FALSE(Banks.FinishBankLoad());

	// The scripts put the resident bank back the same way (directory 28's file 5).
	REQUIRE(Banks.BeginBankLoad(28, 5));
	REQUIRE(Banks.FinishBankLoad());
	CHECK(Field.Bank(SwappedBank) == nullptr);
	REQUIRE(Field.Bank(GearBattleBank) != nullptr);
	CHECK(Resident.GearBattleBank == Field.Bank(GearBattleBank));
	CHECK(FSoundDriver::BankAddress(*Field.Bank(GearBattleBank)) == 0x68000);

	// A file that is no bank is not read.
	CHECK_FALSE(Banks.BeginBankLoad(28, 0x14));
	CHECK_FALSE(Banks.FinishBankLoad());
}

TEST_CASE("The field's songs load at the field's poll, and no song stops the music at once", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FFieldSound Field(*Disc);
	IFieldSongs& Songs = Field.Session->FieldSongs();
	FFieldMusic& Music = Field.Session->FieldMusic();

	// A map's init chooses its song, which its entry leaves loading for the first tick's poll.
	Songs.SetSongStart(6, false);
	Songs.ChooseSong(6);
	Songs.StartMapSong(false);
	CHECK(Songs.IsLoading());
	CHECK_FALSE(Songs.IsMusicLoaded());
	REQUIRE_FALSE(Songs.Poll().has_value());
	CHECK_FALSE(Songs.IsLoading());
	CHECK(Songs.IsMusicLoaded());
	CHECK(Songs.ChosenSong() == 6);
	REQUIRE(Music.Sequence() != nullptr);
	CHECK(Peak(Field.Render(Second)) > Playing);

	// The same song asked for again once the map runs goes on; another stops the music until the poll.
	CHECK(Songs.ChangeSong(6));
	CHECK_FALSE(Songs.IsLoading());
	CHECK(Songs.IsMusicLoaded());
	CHECK(Songs.ChangeSong(0));
	CHECK(Songs.IsLoading());
	CHECK_FALSE(Songs.IsMusicLoaded());
	CHECK(Music.Sequence() == nullptr);
	REQUIRE_FALSE(Songs.Poll().has_value());
	CHECK(Songs.IsMusicLoaded());

	// The scripts' changes reach the song playing.
	Songs.MuteMusicTracks(0xFFFFFFFF);
	Field.Render(3 * Second);
	CHECK(Peak(Field.Render(Second)) < Quiet);
	Songs.MuteMusicTracks(0);
	Songs.SetMusicVolume(0x7F, 0);
	CHECK(Peak(Field.Render(Second)) > Playing);
	Songs.SetMusicVolume(0, 0);
	Field.Render(3 * Second);
	CHECK(Peak(Field.Render(Second)) < Quiet);

	// No song stops the music with nothing left to load; a song the field does not have is refused.
	CHECK(Songs.ChangeSong(FFieldMusic::NoSong));
	CHECK_FALSE(Songs.IsLoading());
	CHECK_FALSE(Songs.IsMusicLoaded());
	CHECK_FALSE(Songs.ChangeSong(200));
	CHECK(Songs.ChosenSong() == FFieldMusic::NoSong);

	// A silent start leaves the song for the scripts to fade in.
	Songs.SetSongStart(4, true);
	CHECK(Songs.ChangeSong(4));
	REQUIRE_FALSE(Songs.Poll().has_value());
	Field.Render(3 * Second);
	CHECK(Peak(Field.Render(Second)) < Quiet);
	Songs.SetMusicVolume(0x7F, 0);
	CHECK(Peak(Field.Render(Second)) > Playing);
}
