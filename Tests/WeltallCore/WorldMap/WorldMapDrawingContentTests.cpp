#include "Formats/Lzss.h"
#include "Formats/Tim.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "WorldMap/WorldMapDrawingTables.h"
#include "WorldMap/WorldMapObjectDrawing.h"
#include "WorldMap/WorldMapScene.h"
#include "WorldMap/WorldMapTables.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <string>

using namespace Weltall;

// What the world map's drawing reads from both discs: the program's drawing tables, and for every playable
// scene group its textures' archives, its place models' packets as the scene's start builds them, and the
// terrain's texture pages.
namespace
{
	/** The playable scene groups (the eras'), by their header files. */
	constexpr std::array<int32, 9> PlayableHeaders{43, 54, 65, 76, 87, 98, 109, 120, 131};

	void CheckDrawing(EDiscRelease Release)
	{
		const FIso9660* Disc = Testing::FindDisc(Release);
		if (Disc == nullptr)
		{
			SKIP("disc not found in Games/");
		}
		const auto Executable = ReadBootExecutable(*Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		const auto Tables = FWorldMapDrawingTables::Read(Disc->Image(), *Index, Release);
		REQUIRE(Tables);
		// The planes the program crosses: the left and right edges, the top, and the fourth that stands for the bottom.
		CHECK(Tables->FrustumPlanes[0] == std::array<int32, 3>{3474, 0, 2170});
		CHECK(Tables->FrustumPlanes[1] == std::array<int32, 3>{-3474, 0, 2170});
		CHECK(Tables->FrustumPlanes[2] == std::array<int32, 3>{0, 3474, 2170});
		CHECK(Tables->FrustumPlanes[3] == std::array<int32, 3>{0, 0, 2170});
		// The camera in its block's north-west quarter leaves out no quadrant of the nearest blocks, and the far row's.
		CHECK(Tables->QuadrantMasks[0][12] == std::array<uint16, 4>{0, 0, 0, 0});
		CHECK(Tables->QuadrantMasks[0][4] == std::array<uint16, 4>{0, 0xFFFF, 0, 0xFFFF});
		CHECK(Tables->QuadrantMasks[3][0] == std::array<uint16, 4>{0xFFFF, 0xFFFF, 0xFFFF, 0});
		CHECK(Tables->SkyCorners[0] == FGteVector{-4096, -768, 4096});
		CHECK(Tables->HorizonCorners[7] == FGteVector{4096, -640, 4032});
		CHECK(Tables->FogColor == std::array<uint8, 3>{192, 208, 240});
		CHECK(Tables->ObjectFogColor == std::array<uint8, 3>{224, 245, 255});
		CHECK(Tables->LightMatrix.M[0] == std::array<int16, 3>{0, -4096, 0});
		CHECK(Tables->ColorMatrix.M[1][0] == 1536);
		const std::array<int16, 10> Modes{4, 4, 5, 5, 0, 0, 2, 2, 3, 3};
		for (std::size_t Flags = 0; Flags < Modes.size(); ++Flags)
		{
			CHECK(Tables->DrawModes[Flags] == Modes[Flags]);
		}

		const auto Program = FWorldMapTables::Read(Disc->Image(), *Index, Release);
		REQUIRE(Program);
		// Every era's scene starts with exit code 2, for which the display's start sets the fog from B00h.
		for (int32 Entry = 1; Entry <= FWorldMapTables::EraCount; ++Entry)
		{
			CHECK(Program->Scenes[static_cast<std::size_t>(Entry)].StartExitCode == 2);
		}

		for (const int32 Header : PlayableHeaders)
		{
			INFO("group header " << Header);
			for (const int32 File : {WorldMapFiles::TerrainTextures(Header), WorldMapFiles::ObjectTextures(Header)})
			{
				const FIndexRecord* Record = Index->Find(WorldMapFiles::Directory, File);
				REQUIRE(Record != nullptr);
				const auto Packed = ReadFileSectors(Disc->Image(), *Record);
				REQUIRE(Packed);
				const auto Archive = UnpackLzss(*Packed);
				REQUIRE(Archive);
				const auto Images = ReadTimArchive(*Archive);
				INFO("file " << File << ": " << (Images ? std::string() : Images.Error().Message));
				CHECK(Images);
			}
			const auto Scene = ReadWorldMapScene(Disc->Image(), *Index, FWorldMapSceneEntry{static_cast<int16>(Header), 16, 16, 2});
			REQUIRE(Scene);
			const auto Packets = BuildWorldMapModelPackets(*Scene, *Tables);
			INFO((Packets ? std::string() : Packets.Error().Message));
			REQUIRE(Packets);
			CHECK(Packets->Objects.size() == Scene->Placements.size());
			// No cell names a texture page past the six the terrain uses (the program's table has seven).
			int32 PastPages = 0;
			for (int32 Block = 0; Block < Scene->Terrain.BlockCount(); ++Block)
			{
				for (int32 Quadrant = 0; Quadrant < 4; ++Quadrant)
				{
					for (int32 Row = 0; Row < 8; ++Row)
					{
						for (int32 Column = 0; Column < 8; ++Column)
						{
							PastPages += (Scene->Terrain.VertexWord(Block, Quadrant, Column, Row) >> 8 & 7) >= 6 ? 1 : 0;
						}
					}
				}
			}
			CHECK(PastPages == 0);
		}
	}
}

TEST_CASE("The world map's drawing reads its tables, textures and models from disc 1", "[Content]")
{
	CheckDrawing(EDiscRelease::UsaDisc1);
}

TEST_CASE("The world map's drawing reads its tables, textures and models from disc 2", "[Content]")
{
	CheckDrawing(EDiscRelease::UsaDisc2);
}
