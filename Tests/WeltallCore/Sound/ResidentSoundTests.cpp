#include "Foundation/Sha1.h"
#include "Sound/GameSoundSession.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

// The digest of the original's sound RAM after its start-up comes from the oracle harness running
// SpuInit, the sound driver's start and the four bank loads.

TEST_CASE("The resident sound fills sound RAM as the game's start-up does", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Session = FGameSoundSession::Start(*Disc);
	REQUIRE(Session);
	const FLockedGameSound Locked = (*Session)->Sound()->Lock();
	CHECK(FSha1::ToHex(FSha1::Hash(std::as_const(Locked.Spu()).Ram())) == "df9a2aaeb5eafc09ba8dfa4a047f69482bd4695f");
	const FResidentSound& Resident = (*Session)->Resident();
	CHECK(Locked->FindBank(0x00) == Resident.CommonBank);
	CHECK(Locked->FindBank(0x26) == Resident.BattleThemeBank);
	CHECK(Locked->FindBank(0x01) == Resident.BattleBank);
	CHECK(Locked->FindBank(0x02) == Resident.GearBattleBank);
	CHECK(FSoundDriver::BankAddress(*Resident.BattleThemeBank) == 0x12000);
	// Effect file 0 is read, for the modes that play it to register.
	CHECK(Resident.MainEffects.FileId() == 0);
	CHECK(Resident.MainEffects.EffectCount() == 468);
	CHECK(Locked->FindEffects(0) == nullptr);
}
