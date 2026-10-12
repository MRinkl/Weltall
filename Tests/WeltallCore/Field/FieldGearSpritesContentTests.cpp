#include "Field/FieldGearSprites.h"
#include "Field/FieldMap.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <map>
#include <optional>
#include <set>
#include <vector>

using namespace Weltall;

// The gears the maps draw from sprite files of their own, known by the files' bytes on the real discs: every file the
// table names, on the maps that hold it, and a sprite that is no gear.

namespace
{
	struct FExpected
	{
		int32 Map = 0;
		int32 Slot = 0;
		EFieldGearModelKind Kind = EFieldGearModelKind::BattleGear;
		int16 Number = 0;
		int16 Entry = 0;
	};

	constexpr EFieldGearModelKind Gear = EFieldGearModelKind::BattleGear;
	constexpr EFieldGearModelKind Enemy = EFieldGearModelKind::SceneEnemy;

	/** A map's sprite files as the table knows them, by their order in its section. */
	std::vector<const FFieldGearSprite*> KnownOn(const FIso9660& Disc, int32 Number)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		const auto Map = LoadFieldMap(Disc.Image(), *Index, Number);
		REQUIRE(Map);
		const std::vector<uint8>& Section = Map->SpriteSets;
		const auto Word = [&Section](std::size_t At) { return static_cast<uint32>(Section[At] | Section[At + 1] << 8 | Section[At + 2] << 16 | Section[At + 3] << 24); };
		std::vector<const FFieldGearSprite*> Known;
		for (uint32 File = 0; File < Word(0); ++File)
		{
			Known.push_back(FindFieldGearSprite(FieldGearSpriteDigest(std::span<const uint8>(Section).subspan(Word(4 + 4 * File)))));
		}
		return Known;
	}

	void CheckKnown(const FIso9660& Disc, std::span<const FExpected> Expected, std::set<const FFieldGearSprite*>& Met)
	{
		std::map<int32, std::vector<const FFieldGearSprite*>> Maps;
		for (const FExpected& Sprite : Expected)
		{
			INFO("map " << Sprite.Map << ", sprite " << Sprite.Slot);
			if (!Maps.contains(Sprite.Map))
			{
				Maps[Sprite.Map] = KnownOn(Disc, Sprite.Map);
			}
			const std::vector<const FFieldGearSprite*>& Known = Maps[Sprite.Map];
			REQUIRE(static_cast<std::size_t>(Sprite.Slot) < Known.size());
			const FFieldGearSprite* Found = Known[static_cast<std::size_t>(Sprite.Slot)];
			REQUIRE(Found != nullptr);
			CHECK(Found->Kind == Sprite.Kind);
			CHECK(Found->Number == Sprite.Number);
			CHECK(Found->Entry == Sprite.Entry);
			Met.insert(Found);
		}
	}
}

TEST_CASE("The maps' own gear sprites are known by their files on both discs, a person's sprite is not", "[Field][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	std::set<const FFieldGearSprite*> Met;
	const FExpected OnTheFirst[] = {
		// The hangar's soldiers' gears, a cave's red gears and a row of them, a white gear, the desert's Sand Trippers.
		{260, 0, Enemy, 16, 0},
		{260, 2, Enemy, 9, 1},
		{173, 0, Gear, 17, 0},
		{80, 0, Gear, 17, 0},
		{174, 0, Enemy, 34, 0},
		{180, 1, Enemy, 43, 0},
		{180, 2, Enemy, 43, 0},
		{188, 0, Enemy, 43, 0},
		// The Elements' gears, still and moving; Amphysvena, Opiomorph and the thin white gear; Etone, Alpha Weltall, the
		// Salvager and the Repair Bot.
		{628, 4, Enemy, 54, 0},
		{628, 5, Enemy, 54, 1},
		{628, 6, Enemy, 54, 2},
		{628, 7, Enemy, 54, 3},
		{412, 0, Enemy, 54, 0},
		{412, 1, Enemy, 54, 2},
		{655, 1, Enemy, 20, 0},
		{655, 2, Enemy, 20, 1},
		{655, 3, Enemy, 65, 1},
		{494, 1, Enemy, 61, 0},
		{249, 0, Enemy, 9, 0},
		{319, 2, Enemy, 21, 1},
		{65, 0, Enemy, 21, 0},
		{418, 0, Enemy, 21, 0},
		// The party's gears parked in the Yggdrasil's hangar (each slot its gear) and at the Generator missions.
		{291, 0, Gear, 0, 0},
		{291, 1, Gear, 1, 0},
		{291, 2, Gear, 2, 0},
		{291, 3, Gear, 3, 0},
		{291, 4, Gear, 4, 0},
		{291, 5, Gear, 5, 0},
		{291, 6, Gear, 6, 0},
		{291, 9, Gear, 9, 0},
		{291, 11, Gear, 11, 0},
		{291, 12, Gear, 12, 0},
		{291, 13, Gear, 13, 0},
		{291, 14, Gear, 14, 0},
		{291, 15, Gear, 15, 0},
		{423, 8, Gear, 8, 0},
		{650, 2, Gear, 2, 0},
		{650, 3, Gear, 14, 0},
	};
	CheckKnown(*First, OnTheFirst, Met);
	// Golgoda's crucified gears and Regurus, the second disc's copies of a map of both.
	const FExpected OnTheSecond[] = {
		{657, 3, Gear, 1, 0},
		{657, 4, Gear, 8, 0},
		{657, 5, Gear, 11, 0},
		{657, 6, Gear, 7, 0},
		{657, 7, Gear, 10, 0},
		{628, 4, Enemy, 54, 0},
		{291, 15, Gear, 15, 0},
	};
	CheckKnown(*Second, OnTheSecond, Met);
	// Every file of the table on a map.
	CHECK(Met.size() == 40);

	// The pilots' and the people's sprites are no gears.
	const std::vector<const FFieldGearSprite*> Hangar = KnownOn(*First, 260);
	REQUIRE(Hangar.size() > 3);
	CHECK(Hangar[1] == nullptr);
	CHECK(Hangar[3] == nullptr);
	const std::vector<const FFieldGearSprite*> Golgoda = KnownOn(*Second, 657);
	REQUIRE(Golgoda.size() > 8);
	CHECK(Golgoda[0] == nullptr);
	CHECK(Golgoda[8] == nullptr);
}
