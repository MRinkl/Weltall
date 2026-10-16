#include "Game/BattleGameFiles.h"
#include "Game/FieldProgram.h"
#include "Game/GameSession.h"
#include "Game/WorldMapProgram.h"
#include "Battling/BattlingArenaData.h"
#include "Media/GameLibrary.h"
#include "Media/GameMovies.h"
#include "Media/PsxExecutable.h"
#include "Sound/SoundFiles.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <algorithm>

using namespace Weltall;

namespace
{
	FGameFileIndex IndexOf(const FIso9660& Disc)
	{
		auto Program = ReadBootExecutable(Disc);
		REQUIRE(Program);
		auto Index = FGameFileIndex::Read(*Program);
		REQUIRE(Index);
		return std::move(*Index);
	}

	void CheckSpriteVoices(const FBattleSpriteFile& Sprite)
	{
		for (const FBattleSpriteSection& Section : Sprite.Sections())
		{
			const auto Bytes = Sprite.Sprite().From(Section.Offset).first(Section.Size);
			if (Section.Kind == EBattleSpriteSectionKind::SoundBank)
			{
				REQUIRE(FSoundBankFile::Parse({Bytes.begin(), Bytes.end()}));
			}
			else if (Section.Kind == EBattleSpriteSectionKind::SoundEffects)
			{
				REQUIRE(FSoundEffectFile::Parse({Bytes.begin(), Bytes.end()}));
			}
		}
	}
}

TEST_CASE("Supported undub discs power on and enter the field with English text", "[Game][Content][Voices]")
{
	for (const EDiscRelease Release : {EDiscRelease::UndubDisc1, EDiscRelease::UndubDisc2})
	{
		REQUIRE_DISC(Disc, Release);
		INFO(DiscReleaseId(Release));
		auto Game = Testing::FGameLoopSession::Start(*Disc);
		CHECK(Game->Session->DiscNumber() == DiscNumberOfRelease(Release) - 1);
		CHECK(Game->Session->Release() == Release);
		Game->PowerOn();
		CHECK(Game->Loop->Stage() == EGameLoopStage::Boot);
		Game->RunUntil([&] { return Game->Loop->Stage() != EGameLoopStage::Boot; }, 200);
		CHECK(Game->Loop->Program() == EGameProgram::Movie);
		Game->RunUntil([&] { return Game->Loop->Program() == EGameProgram::Field; }, 1500, {FPadState::Bit(EPadButton::Cross)});
		REQUIRE(Game->Loop->Field() != nullptr);
		CHECK(Game->Loop->Field()->Runtime().Context().RunningMap == 490);
		Game->EnterField(DiscNumberOfRelease(Release) == 1 ? 35 : 490);
		Game->RunUntil([&] { return Game->Loop->Field()->Runtime().Context().bFieldRunning; }, 100);
		CHECK_FALSE(Game->Frame());
		CHECK(Game->Session->Text().Font.LeadByte() == 0xFE);
		CHECK(FBattlingArenaTables::Read(Disc->Image(), *Game->Index, Release));
		Game->Session->Game().SetSavedEventVariable(0, 30);
		Game->Session->Game().SetWorldMapPlace(0x401, 1, 0xC00, 1);
		auto World = FWorldMapProgram::Start(*Game->Session);
		INFO((World ? std::string() : World.Error().Message));
		REQUIRE(World);
		for (int32 Tick = 0; Tick < FWorldMapProgram::EntryFadeFrames + 10; ++Tick)
		{
			const std::vector<FPadState> Held(static_cast<std::size_t>((*World)->VblanksPerFrame()));
			REQUIRE_FALSE((*World)->Frame(Held));
		}
		const auto Movie = Game->Session->MovieDisc();
		REQUIRE(Movie);
		CHECK(Movie->Image == &Disc->Image());
		for (int32 Character = 0; Character < FBattleSpriteFiles::CharacterCount; ++Character)
		{
			auto Sprite = Game->Session->BattleFiles().SpriteFiles.ReadCharacterSprite(Character);
			auto Attacks = Game->Session->BattleFiles().SpriteFiles.ReadAttacks(Character);
			REQUIRE(Sprite);
			REQUIRE(Attacks);
			CheckSpriteVoices(*Sprite);
			CheckSpriteVoices(*Attacks);
		}
	}
}

TEST_CASE("Voice language chooses matching USA and undub sources without changing text or the drive", "[Game][Content][Voices]")
{
	REQUIRE_DISC(Usa1, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Usa2, EDiscRelease::UsaDisc2);
	REQUIRE_DISC(Undub1, EDiscRelease::UndubDisc1);
	REQUIRE_DISC(Undub2, EDiscRelease::UndubDisc2);
	auto Game = Testing::FGameLoopSession::Start(*Usa1);
	Game->AddDisc(*Usa2);
	FGameFileIndex Japanese1 = IndexOf(*Undub1);
	FGameFileIndex Japanese2 = IndexOf(*Undub2);
	Game->Session->AddVoiceDisc(EVoiceLanguage::Japanese, 0, Undub1->Image(), Japanese1);
	Game->Session->AddVoiceDisc(EVoiceLanguage::Japanese, 1, Undub2->Image(), Japanese2);
	Game->Session->AddVoiceDisc(EVoiceLanguage::English, 1, Usa2->Image(), *Game->OtherIndex);
	const auto EnglishSprite = Game->Session->BattleFiles().SpriteFiles.ReadAttacks(0);
	REQUIRE(EnglishSprite);
	const auto EnglishVoices = Game->Session->BattleFiles().EventFiles.ReadVoiceBank(0, 0);
	REQUIRE(EnglishVoices);
	for (int32 Number = 0; Number < 2; ++Number)
	{
		Game->Session->Kernel().Disc = Number;
		FPlayerSettings Settings = Game->Session->Settings();
		Settings.VoiceLanguage = EVoiceLanguage::Japanese;
		Game->Session->SetSettings(Settings);
		const auto Movie = Game->Session->MovieDisc();
		const auto Drive = Game->Session->DriveDisc();
		REQUIRE(Movie);
		REQUIRE(Drive);
		CHECK(Movie->Image == &(Number == 0 ? Undub1 : Undub2)->Image());
		CHECK(Drive->Image == &(Number == 0 ? Usa1 : Usa2)->Image());
		CHECK(&Game->Session->Image() == &Usa1->Image());
		const auto JapaneseSprite = Game->Session->BattleFiles().SpriteFiles.ReadAttacks(0);
		REQUIRE(JapaneseSprite);
		CHECK(JapaneseSprite->Sprite().Size() != EnglishSprite->Sprite().Size());
		const auto JapaneseVoices = Game->Session->BattleFiles().EventFiles.ReadVoiceBank(0, 0);
		REQUIRE(JapaneseVoices);
		CHECK(*JapaneseVoices != *EnglishVoices);
		for (const EVoiceLanguage Language : {EVoiceLanguage::English, EVoiceLanguage::Automatic})
		{
			Settings.VoiceLanguage = Language;
			Game->Session->SetSettings(Settings);
			REQUIRE(Game->Session->MovieDisc());
			CHECK(Game->Session->MovieDisc()->Image == Drive->Image);
		}
	}
}

TEST_CASE("An unavailable voice track falls back to the gameplay disc and Japanese discs remain extraction only", "[Game][Content][Voices]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FPlayerSettings Settings = Game->Session->Settings();
	Settings.VoiceLanguage = EVoiceLanguage::Japanese;
	Game->Session->SetSettings(Settings);
	REQUIRE(Game->Session->MovieDisc());
	CHECK(Game->Session->MovieDisc()->Image == &Disc->Image());
	Game->Session->Kernel().Disc = 1;
	CHECK_FALSE(Game->Session->MovieDisc());
	for (const EDiscRelease Release : {EDiscRelease::JapanDisc1, EDiscRelease::JapanDisc2})
	{
		REQUIRE_DISC(Japanese, Release);
		const auto Identity = IdentifyDisc(*Japanese);
		REQUIRE(Identity);
		CHECK(Identity->Release == Release);
		CHECK_FALSE(IsPlayableRelease(Release));
		const auto Sound = FGameSoundSession::Start(*Japanese);
		CHECK_FALSE(Sound);
	}
}

TEST_CASE("Battle voices can use the second disc's language when only that source was supplied", "[Game][Content][Voices]")
{
	REQUIRE_DISC(Undub1, EDiscRelease::UndubDisc1);
	REQUIRE_DISC(Usa2, EDiscRelease::UsaDisc2);
	auto Game = Testing::FGameLoopSession::Start(*Undub1);
	const auto Japanese = Game->Session->BattleFiles().SpriteFiles.ReadAttacks(0);
	REQUIRE(Japanese);
	auto EnglishIndex = IndexOf(*Usa2);
	Game->Session->AddVoiceDisc(EVoiceLanguage::English, 1, Usa2->Image(), EnglishIndex);
	auto Settings = Game->Session->Settings();
	Settings.VoiceLanguage = EVoiceLanguage::English;
	Game->Session->SetSettings(Settings);
	const auto English = Game->Session->BattleFiles().SpriteFiles.ReadAttacks(0);
	REQUIRE(English);
	CHECK(English->Sprite().Size() != Japanese->Sprite().Size());
	REQUIRE(Game->Session->MovieDisc());
	CHECK(Game->Session->MovieDisc()->Image == &Undub1->Image());
}

TEST_CASE("The library recognizes voice variants and prefers the original USA gameplay source", "[Media][Content][Voices]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UndubDisc1);
	const FGameLibrary Library = FGameLibrary::Scan(WELTALL_GAMES_DIR);
	REQUIRE(Library.Find(EDiscRelease::UndubDisc1));
	const auto Identity = IdentifyDisc(*Library.Find(EDiscRelease::UndubDisc1));
	REQUIRE(Identity);
	CHECK(Identity->Release == EDiscRelease::UndubDisc1);
	CHECK(DiscReleaseId(Identity->Release) == "usa.undub.disc1");
	for (const int32 Number : {1, 2})
	{
		const auto Usa = Library.Find(Number == 1 ? EDiscRelease::UsaDisc1 : EDiscRelease::UsaDisc2);
		const auto Undub = Library.Find(Number == 1 ? EDiscRelease::UndubDisc1 : EDiscRelease::UndubDisc2);
		CHECK(Library.PlayableDisc(Number) == (Usa ? Usa : Undub));
	}
	CHECK(Library.PlayableDisc(0) == nullptr);
	CHECK(Library.PlayableDisc(3) == nullptr);
}

TEST_CASE("Detected supported undubs never interpret their patch code as indexed files", "[Media][Content][Voices]")
{
	const FGameLibrary Library = FGameLibrary::Scan(WELTALL_GAMES_DIR);
	int32 Known = 0;
	for (const auto Release : {EDiscRelease::UndubDisc1, EDiscRelease::UndubDisc2})
	{
		if (const FIso9660* Disc = Library.Find(Release))
		{
			++Known;
			const auto Index = IndexOf(*Disc);
			for (std::size_t Number = 280; Number <= 376; ++Number) CHECK(Index.Records()[Number] == FIndexRecord{});
			CHECK(Index.Records()[381] == FIndexRecord{});
			CHECK(Index.Records().back().Size == 27648000);
			for (const auto& Record : Index.Records())
			{
				if (Record.IsFile()) CHECK(Record.Lba + (static_cast<uint64>(Record.Size) + 2047) / 2048 <= Disc->Image().SectorCount());
			}
		}
	}
	for (const auto& Note : Library.Notes())
	{
		auto Image = FCdImage::Open(Note.File);
		if (!Image) continue;
		auto Disc = FIso9660::Open(std::move(*Image));
		if (!Disc) continue;
		const auto Identity = IdentifyDisc(*Disc);
		if (!Identity || (Identity->Release != EDiscRelease::UndubDisc1 && Identity->Release != EDiscRelease::UndubDisc2)) continue;
		INFO(Note.Text);
		++Known;
		REQUIRE(Library.Find(Identity->Release) != nullptr);
	}
	if (Known == 0) SKIP("supported undub discs are not present");
}
