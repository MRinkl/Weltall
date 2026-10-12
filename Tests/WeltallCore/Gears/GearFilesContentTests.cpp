#include "Gears/GearCloth.h"
#include "Gears/GearFiles.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <map>
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
}

TEST_CASE("Every gear model's two files parse, and their cloths are made", "[Gears][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const std::optional<FGameFileIndex> Index = ReadIndex(*Disc);
	REQUIRE(Index);
	std::size_t Bones = 0;
	uint32 Records = 0;
	uint32 Scripts = 0;
	std::size_t Textures = 0;
	std::size_t Parts = 0;
	std::vector<int32> Absolute;
	std::vector<int32> Cloths;
	std::map<int16, int32> Scales;
	for (int32 Gear = 0; Gear < FGearFileNumbers::ModelCount; ++Gear)
	{
		INFO("gear model " << Gear);
		auto Files = ReadGearFiles(Disc->Image(), *Index, Gear);
		REQUIRE(Files);
		const FGearModelPack& Model = Files->Model;
		Bones += Model.Skeleton.size() + 1;
		Records += Files->Motion.BankEntries;
		Scripts += Files->Motion.ScriptEntries - 1;
		Textures += Model.Textures.size();
		Parts += Model.Model.Parts.size();
		CHECK_FALSE(Files->Motion.bHasSoundEffects);
		++Scales[Model.Header.Scale];
		if ((Model.Header.Flags & GearModelFlags::AbsoluteTextures) != 0)
		{
			Absolute.push_back(Gear);
		}
		for (std::size_t Cloth = 0; Cloth < Model.Cloths.size(); ++Cloth)
		{
			CHECK(MakeGearCloth(Model.Cloths[Cloth], Model.ClothMeshes[Cloth], 0x240, 0x100, 0, 0xFC));
			Cloths.push_back(Gear);
		}
	}
	CHECK(Bones == 4763);
	CHECK(Records == 689);
	CHECK(Scripts == 639);
	CHECK(Textures == 498);
	CHECK(Parts == 1786);
	CHECK(Absolute == std::vector<int32>{64, 69});
	CHECK(Cloths == std::vector<int32>{18, 18, 67});
	CHECK(Scales == std::map<int16, int32>{{260, 2}, {320, 63}, {350, 1}, {440, 1}, {480, 1}, {800, 1}, {960, 1}, {4096, 2}});
}

TEST_CASE("The second disc's gear models are the first's", "[Gears][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	const std::optional<FGameFileIndex> FirstIndex = ReadIndex(*First);
	const std::optional<FGameFileIndex> SecondIndex = ReadIndex(*Second);
	REQUIRE((FirstIndex && SecondIndex));
	for (int32 Gear = 0; Gear < FGearFileNumbers::ModelCount; ++Gear)
	{
		INFO("gear model " << Gear);
		const FGearFileNumbers Numbers = FGearFileNumbers::Of(Gear);
		for (const int32 Number : {Numbers.Motion, Numbers.Model})
		{
			const FIndexRecord* A = FirstIndex->Find(FGearFileNumbers::Directory, Number);
			const FIndexRecord* B = SecondIndex->Find(FGearFileNumbers::Directory, Number);
			REQUIRE((A != nullptr && B != nullptr));
			CHECK(*ReadFileSectors(First->Image(), *A) == *ReadFileSectors(Second->Image(), *B));
		}
	}
}
