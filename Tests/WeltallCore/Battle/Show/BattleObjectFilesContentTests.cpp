#include "Battle/Show/BattleObjectFiles.h"
#include "Battle/Show/BattleSceneGraphics.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <optional>
#include <vector>

using namespace Weltall;

namespace
{
	std::optional<FGameFileIndex> ReadIndex(const FIso9660& Disc)
	{
		const auto Executable = ReadBootExecutable(Disc);
		if (!Executable)
		{
			return std::nullopt;
		}
		auto Index = FGameFileIndex::Read(*Executable);
		return Index ? std::optional<FGameFileIndex>(std::move(*Index)) : std::nullopt;
	}

	int32 SequenceCount(const FGearMotionPack& Pack)
	{
		// The scripts' container holds the animation bank first, then the sequences.
		return static_cast<int32>(Pack.ScriptEntries) - 1;
	}
}

TEST_CASE("The Battle program's object tables name every gear's files, the gears' texture places and the extension groups", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const std::optional<FGameFileIndex> Index = ReadIndex(*Disc);
	REQUIRE(Index);
	auto Tables = FBattleObjectTables::Load(Disc->Image(), *Index);
	REQUIRE(Tables);
	const std::vector<std::pair<int32, int32>> Gears = {{1, 0}, {3, 0}, {5, 6}, {13, 0}, {15, 3}, {20, 4}, {26, 0}, {28, 0}, {30, 0}, {32, 0},
		{34, 0}, {36, 4}, {42, 3}, {47, 4}, {53, 0}, {55, 0}, {57, 0}, {59, 0}, {61, 0}};
	for (std::size_t Gear = 0; Gear < Gears.size(); ++Gear)
	{
		INFO("gear " << Gear);
		CHECK(Tables->Gears[Gear].First == Gears[Gear].first);
		CHECK(Tables->Gears[Gear].LastVariant == Gears[Gear].second);
	}
	const std::array<uint8, 17> Extensions = {1, 108, 164, 99, 94, 220, 22, 123, 151, 158, 161, 143, 139, 141, 40, 214, 219};
	CHECK(Tables->Extensions == Extensions);
	CHECK(Tables->GearPlaces[0].X == 0x340);
	CHECK(Tables->GearPlaces[0].Y == 0);
	CHECK(Tables->GearPlaces[1].X == 0x2C0);
	CHECK(Tables->GearPlaces[1].Y == 0x100);
	CHECK(Tables->GearPlaces[2].X == 0x300);
	CHECK(Tables->GearPlaces[2].Y == 0x100);
	CHECK(BattleObjectFiles::ExtensionFile(*Tables, 3, 5) == 223);
	// The seventeenth group is the burning village's battles'.
	CHECK(BattleObjectFiles::ExtensionFile(*Tables, 3, 16) == 222);
	CHECK_FALSE(BattleObjectFiles::ExtensionFile(*Tables, 3, 17));
}

TEST_CASE("Every gear's battle model parses with each of its variants' sub-objects or images", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const std::optional<FGameFileIndex> Index = ReadIndex(*Disc);
	REQUIRE(Index);
	auto Tables = FBattleObjectTables::Load(Disc->Image(), *Index);
	REQUIRE(Tables);
	std::size_t Bones = 0;
	int32 Variants = 0;
	int32 Parts = 0;
	int32 ImageVariants = 0;
	std::vector<int32> ClothedGears;
	for (int32 Gear = 0; Gear < FBattleObjectTables::GearCount; ++Gear)
	{
		INFO("gear " << Gear);
		auto Files = BattleObjectFiles::ReadGear(Disc->Image(), *Index, *Tables, Gear, 0);
		REQUIRE(Files);
		CHECK_FALSE(Files->Variant);
		Bones += Files->Model.Skeleton.size() + 1;
		CHECK(Files->Motion.bHasSoundEffects);
		const bool bLonger = Gear == 1 || Gear == 2 || Gear == 8 || Gear == 10 || Gear == 15 || Gear == 17 || Gear == 18;
		CHECK(SequenceCount(Files->Motion) == (bLonger ? 75 : 74));
		if (!Files->Model.Cloths.empty())
		{
			ClothedGears.push_back(Gear);
		}
		for (int32 Variant = 1; Variant <= Tables->Gears[static_cast<std::size_t>(Gear)].LastVariant; ++Variant)
		{
			INFO("variant " << Variant);
			auto WithVariant = BattleObjectFiles::ReadGear(Disc->Image(), *Index, *Tables, Gear, Variant);
			REQUIRE(WithVariant);
			REQUIRE(WithVariant->Variant);
			++Variants;
			const FBattleGearVariant& Found = *WithVariant->Variant;
			if (Found.Parts.empty())
			{
				++ImageVariants;
				CHECK_FALSE(Found.Images.empty());
				CHECK_FALSE(Found.Model);
			}
			else
			{
				Parts += static_cast<int32>(Found.Parts.size());
				REQUIRE(Found.Model);
				REQUIRE(Found.Motion);
				for (const FBattleSubObjectPlace& Part : Found.Parts)
				{
					CHECK(Part.Bone < WithVariant->Model.Skeleton.size() + 1);
				}
			}
		}
		// A variant past the gear's last loads none, as the program chooses.
		auto Past = BattleObjectFiles::ReadGear(Disc->Image(), *Index, *Tables, Gear, Tables->Gears[static_cast<std::size_t>(Gear)].LastVariant + 1);
		REQUIRE(Past);
		CHECK_FALSE(Past->Variant);
	}
	CHECK(Bones == 1153);
	CHECK(Variants == 24);
	CHECK(Parts == 26);
	CHECK(ImageVariants == 6);
	CHECK(ClothedGears == std::vector<int32>{5});
	CHECK_FALSE(BattleObjectFiles::ReadGear(Disc->Image(), *Index, *Tables, FBattleObjectTables::GearCount, 0));
}

TEST_CASE("The special models and every sequence extension parse", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const std::optional<FGameFileIndex> Index = ReadIndex(*Disc);
	REQUIRE(Index);
	const std::vector<std::pair<std::size_t, int32>> Specials = {{62, 8}, {60, 4}, {60, 4}};
	for (int32 Model = 0; Model < BattleObjectFiles::SpecialModelCount; ++Model)
	{
		INFO("special model " << Model);
		auto Files = BattleObjectFiles::ReadSpecialModel(Disc->Image(), *Index, Model);
		REQUIRE(Files);
		CHECK(Files->Model.Skeleton.size() + 1 == Specials[static_cast<std::size_t>(Model)].first);
		CHECK(SequenceCount(Files->Motion) == Specials[static_cast<std::size_t>(Model)].second);
	}
	int32 Extensions = 0;
	int32 WithSequences = 0;
	int32 WithSprites = 0;
	int32 WithSoundEffects = 0;
	int32 WithImages = 0;
	for (int32 File = 2; File <= 232; ++File)
	{
		INFO("extension file " << File);
		auto Extension = BattleObjectFiles::ReadExtension(Disc->Image(), *Index, File);
		REQUIRE(Extension);
		++Extensions;
		WithSequences += SequenceCount(Extension->Pack) > 0 ? 1 : 0;
		WithSprites += Extension->Sprite.IsEmpty() ? 0 : 1;
		WithSoundEffects += Extension->SoundEffects.IsEmpty() ? 0 : 1;
		WithImages += Extension->Images.IsEmpty() ? 0 : 1;
		CHECK(Extension->Sprite.End <= Extension->Pack.Bytes.size());
		CHECK(Extension->Images.End <= Extension->Pack.Bytes.size());
	}
	CHECK(Extensions == 231);
	CHECK(WithSequences == 88);
	CHECK(WithSprites == 191);
	CHECK(WithSoundEffects == 210);
	CHECK(WithImages == 188);
}

TEST_CASE("Every scene's model enemies' archives parse", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const std::optional<FGameFileIndex> Index = ReadIndex(*Disc);
	REQUIRE(Index);
	const FIndexRecord* Group = Index->Find(FBattleSceneGraphics::SceneDirectory, 1);
	REQUIRE(Group);
	const int32 Scenes = -Group->Size / 2;
	int32 Models = 0;
	int32 ScenesWithModels = 0;
	for (int32 Scene = 0; Scene < Scenes; ++Scene)
	{
		INFO("scene " << Scene);
		auto Graphics = FBattleSceneGraphics::Load(Disc->Image(), *Index, Scene);
		REQUIRE(Graphics);
		int32 Found = 0;
		for (std::size_t Entry = 0; Entry < Graphics->Entries().size(); ++Entry)
		{
			const FBattleSceneEntry& Record = Graphics->Entries()[Entry];
			auto Files = BattleObjectFiles::SceneModel(*Graphics, static_cast<int32>(Entry));
			CHECK(static_cast<bool>(Files) == (!Record.IsSprite() && Record.HasOwnImages()));
			Found += Files ? 1 : 0;
		}
		Models += Found;
		ScenesWithModels += Found > 0 ? 1 : 0;
	}
	CHECK(Models == 118);
	CHECK(ScenesWithModels == 52);
}
