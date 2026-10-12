#include "Foundation/Sha1.h"
#include "Graphics/SpriteFile.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "WorldMap/WorldMapTables.h"
#include "WorldMap/WorldMapScene.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <string>

using namespace Weltall;

namespace
{
	/** What an independent reader counts in each scene group, the same on both discs. */
	struct FGroupFacts
	{
		int32 Group;
		int32 Header;
		std::size_t Entrances;
		std::array<std::size_t, 3> Triggers;
		std::size_t Parts;
		std::size_t Placements;
		std::size_t Billboards;
		int32 Messages;
		std::size_t Regions;
		const char* TerrainSha1;
	};

	constexpr std::array<FGroupFacts, WorldMapFiles::GroupCount> Groups{{
		{0, 43, 1, {2, 0, 0}, 16, 18, 1756, 41, 16, "ae5368a80b54332984a6773272df059b4a269f88"},
		{1, 54, 15, {6, 0, 0}, 14, 16, 1756, 41, 16, "fe9fcf5bf97daa1520a74d1106a2e3acb41f8ceb"},
		{2, 65, 15, {14, 14, 4}, 36, 49, 1756, 41, 16, "fe9fcf5bf97daa1520a74d1106a2e3acb41f8ceb"},
		{3, 76, 15, {13, 13, 3}, 51, 69, 1756, 41, 16, "fe9fcf5bf97daa1520a74d1106a2e3acb41f8ceb"},
		{4, 87, 21, {15, 15, 3}, 71, 90, 1756, 41, 16, "082d13270fee4c282e2440d1c6fa09f07cbe100b"},
		{5, 98, 23, {15, 15, 1}, 64, 80, 1756, 41, 16, "082d13270fee4c282e2440d1c6fa09f07cbe100b"},
		{6, 109, 22, {16, 16, 2}, 65, 81, 1697, 41, 16, "a18bbbf109c2a8ebe0bb2a9146b51e45b5042f5c"},
		{7, 120, 22, {16, 16, 2}, 64, 80, 1697, 41, 16, "0fc28f68097a1b0dc7d7b552ebd5cbec34d9f908"},
		{8, 131, 12, {10, 10, 1}, 65, 76, 2352, 41, 16, "655314effd8f79daa57952ca60b97172b977701d"},
		{9, 142, 10, {11, 0, 0}, 7, 17, 1756, 41, 0, "d6862b5c0278133c4cf6518c77df83b235635026"},
		{10, 153, 10, {11, 0, 0}, 4, 12, 1756, 41, 0, "d6862b5c0278133c4cf6518c77df83b235635026"},
		{11, 164, 10, {11, 0, 0}, 10, 17, 1756, 41, 0, "082d13270fee4c282e2440d1c6fa09f07cbe100b"},
		{12, 175, 10, {11, 0, 0}, 13, 19, 1756, 41, 0, "a18bbbf109c2a8ebe0bb2a9146b51e45b5042f5c"},
		{13, 186, 10, {11, 0, 0}, 2, 3, 1697, 41, 0, "a18bbbf109c2a8ebe0bb2a9146b51e45b5042f5c"},
		{14, 197, 26, {24, 24, 25}, 3, 3, 1697, 41, 16, "2b14d3b00a5fad396382742a5239a532e4b32822"},
		{15, 208, 26, {23, 24, 24}, 69, 86, 2352, 40, 16, "655314effd8f79daa57952ca60b97172b977701d"},
		{16, 219, 27, {23, 23, 24}, 8, 9, 2471, 40, 16, "655314effd8f79daa57952ca60b97172b977701d"},
	}};

	struct FDisc
	{
		FPsxExecutable Executable;
		FGameFileIndex Index;
	};

	TResult<FDisc> OpenDisc(const FIso9660& Disc)
	{
		TResult<FPsxExecutable> Executable = ReadBootExecutable(Disc);
		if (!Executable)
		{
			return Executable.Error();
		}
		TResult<FGameFileIndex> Index = FGameFileIndex::Read(*Executable);
		if (!Index)
		{
			return Index.Error();
		}
		return FDisc{std::move(*Executable), std::move(*Index)};
	}

	std::string TerrainSha1(const FWorldMapTerrain& Terrain)
	{
		FSha1 Hash;
		for (int32 Block = 0; Block < Terrain.BlockCount(); ++Block)
		{
			for (int32 Quadrant = 0; Quadrant < 4; ++Quadrant)
			{
				for (int32 Word = 0; Word < FWorldMapTerrain::QuadrantSide * FWorldMapTerrain::QuadrantSide; ++Word)
				{
					const uint32 Value = Terrain.VertexWord(Block, Quadrant, Word % FWorldMapTerrain::QuadrantSide, Word / FWorldMapTerrain::QuadrantSide);
					const std::array<uint8, 4> Bytes{static_cast<uint8>(Value), static_cast<uint8>(Value >> 8), static_cast<uint8>(Value >> 16),
						static_cast<uint8>(Value >> 24)};
					Hash.Update(Bytes);
				}
			}
			for (int32 Cell = 0; Cell < 256; ++Cell)
			{
				const uint16 Value = Terrain.CellAttribute(Block, Cell % 16, Cell / 16);
				const std::array<uint8, 2> Bytes{static_cast<uint8>(Value), static_cast<uint8>(Value >> 8)};
				Hash.Update(Bytes);
			}
		}
		return FSha1::ToHex(Hash.Finish());
	}

	void CheckProgram(EDiscRelease Release)
	{
		const FIso9660* Disc = Testing::FindDisc(Release);
		if (Disc == nullptr)
		{
			SKIP("disc not found in Games/");
		}
		const TResult<FDisc> Opened = OpenDisc(*Disc);
		REQUIRE(Opened);
		const TResult<FWorldMapTables> Program = FWorldMapTables::Read(Disc->Image(), Opened->Index, Release);
		REQUIRE(Program);
		CHECK(Program->EraThresholds == std::vector<uint16>{24, 54, 135, 149, 186, 198, 204, 237});
		CHECK(Program->BandThresholds == std::vector<uint16>{54, 201, 340});
		for (int32 Era = 0; Era < FWorldMapTables::EraCount; ++Era)
		{
			CHECK(Program->Scenes[static_cast<std::size_t>(Era + 1)].GroupHeader == Groups[static_cast<std::size_t>(Era)].Header);
			CHECK(Program->Scenes[static_cast<std::size_t>(Era + 1)].BlocksX == 16);
			CHECK(Program->Scenes[static_cast<std::size_t>(Era + 1)].StartExitCode == 2);
		}
		CHECK(Program->WalkTable[1] == std::array<int16, 8>{1, 0, 0, 1, 1, 0, 0, 0});
		CHECK(Program->WalkTable[2] == std::array<int16, 8>{1, 0, 0, 1, 0, 0, 0, 0});
		CHECK(Program->WalkTable[5] == std::array<int16, 8>{0, 0, 1, 1, 0, 0, 0, 0});
		CHECK(Program->WalkTable[7] == std::array<int16, 8>{1, 1, 1, 1, 1, 0, 0, 0});
		CHECK(Program->DiagonalWeights == std::array<int32, 4>{2896, -2896, 2896, 2896});
		CHECK(Program->SplitLines[8].NormalX == 2896);
		CHECK(Program->SplitLines[0].PointX == 0xFFFF);
		CHECK(Program->SlideDirections[11] == std::array<int32, 2>{4096, 0});
		CHECK(Program->ForestRegions[5] == 3);
		CHECK(Program->ForestRegions[6] == 10);
		CHECK(Program->ColliderPasses == std::array<int16, 6>{1, 1, 0, 1, 1, 1});
		CHECK(Program->Camera.Distances == std::array<int32, 4>{640 << 12, 880 << 12, 1120 << 12, 1360 << 12});
		CHECK(Program->Camera.Pitches == std::array<int16, 4>{-341, -455, -512, -568});
		CHECK(Program->Camera.FlyingPitches[0] == -227);
		CHECK(Program->Camera.Clearances == std::array<int16, 4>{-256, -480, -696, -936});
		CHECK(Program->Camera.FlyingClearances[0] == -176);
		CHECK(Program->GearSprites[2].TextureX == 640);
		CHECK(Program->GearSprites[0].ClutY == 509);
		CHECK((Program->ThamesPlace == FWorldMapTrigger{0, 0, 0, 0, 312, 1, 14, 0}));
		CHECK((Program->ShevatPlace == FWorldMapTrigger{0, 0, 0, 0, 440, 1, 29, 0}));
		CHECK((Program->InteriorPlace == FWorldMapTrigger{0, 0, 0, 0, 290, 3, -1, 0}));
		CHECK((Program->CloudPlaces == std::array<std::array<int32, 2>, 5>{{{192, 192}, {576, 832}, {1216, 320}, {1472, 1088}, {704, 1600}}}));
		CHECK(Program->EmptySlotGear == 0x07);
		CHECK((Program->VillageVoyage == std::array<std::array<int16, 2>, 4>{{{19968, 11392}, {18944, 10944}, {17920, 11264}, {17920, 11636}}}));
		CHECK((Program->FactoryVoyage[0] == std::array<int16, 2>{8951, 19484}));
		CHECK((Program->FactoryVoyage[6] == std::array<int16, 2>{8028, 16652}));
		CHECK((Program->ThamesObjects == std::array<uint16, 9>{0, 0, 0, 46, 51, 51, 51, 51, 0}));
		CHECK((Program->ShevatObjects == std::array<uint16, 9>{0, 0, 0, 47, 52, 52, 52, 52, 0}));
		CHECK((Program->SpinnerPairObjects[4] == std::array<uint16, 2>{39, 42}));
		CHECK((Program->FastSpinnerPairObjects[8] == std::array<uint16, 2>{44, 45}));
		CHECK((Program->LandmarkAObjects == std::array<uint16, 9>{0, 0, 0, 0, 0, 0, 67, 79, 64}));
		CHECK((Program->LandmarkCObjects == std::array<uint16, 9>{0, 0, 0, 0, 43, 43, 43, 43, 43}));
		CHECK((Program->ThamesWaypoints[0] == std::array<uint16, 2>{23296, 23736}));
		CHECK((Program->ThamesWaypoints[7] == std::array<uint16, 2>{25620, 16928}));
		CHECK(Program->ShevatAttachments.size() == 14);
		CHECK((Program->ShevatAttachments[4] == std::array<int16, 2>{5, 7}));
		CHECK((Program->StoryObjects == std::vector<int16>{67, 68, 70, 71, 72, 73, 74, 75, 76}));

		const TResult<FWorldMapSceneChoice> Era2 = ChooseWorldMapScene(*Program, 1, 60);
		REQUIRE(Era2);
		CHECK(*Era2->Era == 2);
		CHECK(Era2->Scene.GroupHeader == 65);
		const TResult<FWorldMapSceneChoice> Scene15 = ChooseWorldMapScene(*Program, 15, 0);
		REQUIRE(Scene15);
		CHECK(!Scene15->Era.has_value());
		CHECK(Scene15->Scene.GroupHeader == 175);
		CHECK(!ChooseWorldMapScene(*Program, 19, 0));
	}

	void CheckGroups(EDiscRelease Release)
	{
		const FIso9660* Disc = Testing::FindDisc(Release);
		if (Disc == nullptr)
		{
			SKIP("disc not found in Games/");
		}
		const TResult<FDisc> Opened = OpenDisc(*Disc);
		REQUIRE(Opened);
		for (const FGroupFacts& Facts : Groups)
		{
			INFO("group " << Facts.Group);
			const TResult<FWorldMapScene> Scene = ReadWorldMapScene(Disc->Image(), Opened->Index, FWorldMapSceneEntry{static_cast<int16>(Facts.Header), 16, 16, 2});
			REQUIRE(Scene);
			CHECK(Scene->Entrances.size() == Facts.Entrances);
			for (std::size_t List = 0; List < Facts.Triggers.size(); ++List)
			{
				REQUIRE(Scene->TriggerLists[List].has_value());
				CHECK(Scene->TriggerLists[List]->size() == Facts.Triggers[List]);
			}
			CHECK(Scene->Models.Parts.size() == Facts.Parts);
			CHECK(Scene->Meshes.size() == Facts.Parts);
			CHECK(Scene->Placements.size() == Facts.Placements);
			std::size_t Billboards = 0;
			for (const std::vector<FWorldMapBillboard>& List : Scene->Billboards)
			{
				Billboards += List.size();
			}
			CHECK(Billboards == Facts.Billboards);
			const TResult<FMessageTable> Messages = Scene->Messages();
			REQUIRE(Messages);
			CHECK(Messages->Count() == Facts.Messages);
			CHECK(Scene->Regions.size() == Facts.Regions);
			CHECK(TerrainSha1(Scene->Terrain) == Facts.TerrainSha1);

			// The second terrain file holds the same blocks column by column.
			const FIndexRecord* Columns = Opened->Index.Find(WorldMapFiles::Directory, WorldMapFiles::TerrainByColumns(Facts.Header));
			REQUIRE(Columns != nullptr);
			const TResult<std::vector<uint8>> ColumnBytes = ReadFileSectors(Disc->Image(), *Columns);
			REQUIRE(ColumnBytes);
			const TResult<FWorldMapTerrain> ByColumns = FWorldMapTerrain::Parse(*ColumnBytes, 16, 16);
			REQUIRE(ByColumns);
			for (int32 X = 0; X < 16; ++X)
			{
				for (int32 Z = 0; Z < 16; ++Z)
				{
					CHECK(Scene->Terrain.VertexWord(Z * 16 + X, 3, 8, 8) == ByColumns->VertexWord(X * 16 + Z, 3, 8, 8));
					CHECK(Scene->Terrain.CellAttribute(Z * 16 + X, 15, 15) == ByColumns->CellAttribute(X * 16 + Z, 15, 15));
				}
			}
		}
	}

	void CheckSprites(EDiscRelease Release)
	{
		const FIso9660* Disc = Testing::FindDisc(Release);
		if (Disc == nullptr)
		{
			SKIP("disc not found in Games/");
		}
		const TResult<FDisc> Opened = OpenDisc(*Disc);
		REQUIRE(Opened);
		int32 Parsed = 0;
		for (int32 File = WorldMapFiles::CharacterSpriteBase; File < WorldMapFiles::CharacterSpriteBase + 16; ++File)
		{
			const FIndexRecord* Record = Opened->Index.Find(WorldMapFiles::Directory, File);
			REQUIRE(Record != nullptr);
			const TResult<std::vector<uint8>> Bytes = ReadFileSectors(Disc->Image(), *Record);
			REQUIRE(Bytes);
			const TResult<FSpriteFile> Sprite = FSpriteFile::Parse(std::span<const uint8>(*Bytes).first(static_cast<std::size_t>(Record->Size)));
			INFO("file " << File);
			CHECK(Sprite);
			Parsed += Sprite ? 1 : 0;
		}
		for (int32 File = WorldMapFiles::GearSpriteBase; File < WorldMapFiles::GearSpriteBase + 17; ++File)
		{
			const FIndexRecord* Record = Opened->Index.Find(WorldMapFiles::Directory, File);
			REQUIRE(Record != nullptr);
			const TResult<std::vector<uint8>> Bytes = ReadFileSectors(Disc->Image(), *Record);
			REQUIRE(Bytes);
			const TResult<FSpriteFile> Sprite = FSpriteFile::Parse(std::span<const uint8>(*Bytes).first(static_cast<std::size_t>(Record->Size)));
			INFO("file " << File);
			CHECK(Sprite);
			Parsed += Sprite ? 1 : 0;
		}
		CHECK(Parsed == 33);
	}
}

TEST_CASE("The world map program's tables read from disc 1", "[Content]")
{
	CheckProgram(EDiscRelease::UsaDisc1);
}

TEST_CASE("The world map program's tables read from disc 2", "[Content]")
{
	CheckProgram(EDiscRelease::UsaDisc2);
}

TEST_CASE("Every world map scene group of disc 1 reads as an independent reader counts it", "[Content]")
{
	CheckGroups(EDiscRelease::UsaDisc1);
}

TEST_CASE("Every world map scene group of disc 2 reads as an independent reader counts it", "[Content]")
{
	CheckGroups(EDiscRelease::UsaDisc2);
}

TEST_CASE("The world map's party and gear sprites parse on disc 1", "[Content]")
{
	CheckSprites(EDiscRelease::UsaDisc1);
}

TEST_CASE("The world map's party and gear sprites parse on disc 2", "[Content]")
{
	CheckSprites(EDiscRelease::UsaDisc2);
}
