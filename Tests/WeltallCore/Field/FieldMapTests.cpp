#include "Field/FieldMap.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using Testing::FByteWriter;

namespace
{
	constexpr std::size_t SectorBytes = 2048;

	/** An LZSS stream of literals only, holding Extra junk bytes past Data like the game's section streams. */
	std::vector<uint8> Literals(std::vector<uint8> Data, std::size_t Extra = 0)
	{
		Data.resize(Data.size() + Extra, 0xEE);
		FByteWriter Stream;
		Stream.U32(static_cast<uint32>(Data.size()));
		for (std::size_t Index = 0; Index < Data.size(); ++Index)
		{
			if (Index % 8 == 0)
			{
				Stream.U8(0);
			}
			Stream.U8(Data[Index]);
		}
		return Stream.Bytes;
	}

	/** One part, three vertices, one static textured triangle, with or without its texture commands. */
	std::vector<uint8> TinyModel(bool bCommands)
	{
		FByteWriter Model;
		Model.U32(1).U32(0).U32(0).U32(0);
		Model.U16(0).U16(3).U16(1).U16(1).U32(0x48).U32(0x48).U32(0x60).U32(0x6C).U32(0).U32(0);
		Model.S16(-10).S16(-20).S16(-30).S16(0).S16(10).S16(20).S16(31).S16(0).U32(0).U32(0x20);
		Model.S16(-10).S16(-20).S16(-30).S16(0).S16(10).S16(20).S16(31).S16(0).S16(0).S16(0).S16(0).S16(0);
		Model.U8(5).U8(0x77).S16(1).U16(0).U16(1).U16(2).U16(0);
		if (bCommands)
		{
			Model.U32(0xC4000088).U32(0xC8007B40);
		}
		Model.U16(0x0100).U8(0).U8(0x25).U16(0).U16(1);
		return Model.Bytes;
	}

	std::vector<uint8> ModelList()
	{
		const std::vector<uint8> First = TinyModel(true);
		const std::vector<uint8> Second = TinyModel(false);
		FByteWriter List;
		List.U32(2).U32(12).U32(static_cast<uint32>(12 + First.size()));
		List.Bytes.insert(List.Bytes.end(), First.begin(), First.end());
		List.Bytes.insert(List.Bytes.end(), Second.begin(), Second.end());
		return List.Bytes;
	}

	/** One layer of one triangle. */
	std::vector<uint8> TinyWalkmesh()
	{
		FByteWriter Section;
		Section.U32(1).U32(14).U32(0).U32(0).U32(0).U32(0x20 + 16 + 24);
		Section.U32(0x20).U32(0x30);
		Section.S16(0).S16(1).S16(2).S16(-1).S16(-1).S16(-1).U8(0).U8(0).U16(0);
		Section.S16(0).S16(0).S16(0).S16(0).S16(64).S16(0).S16(0).S16(0).S16(0).S16(0).S16(64).S16(0);
		for (int32 Index = 0; Index < 256; ++Index)
		{
			Section.U32(0);
		}
		return Section.Bytes;
	}

	std::vector<uint8> Tims()
	{
		FByteWriter Section;
		Section.U32(1).U32(8);
		Section.U32(0x10).U32(0x08).U32(12 + 4).Rect(0, 481, 2, 1).U16(0x1234).U16(0);
		Section.U32(12 + 4).Rect(320, 0, 1, 1).U16(0x5678).U16(0);
		return Section.Bytes;
	}

	/** Two image sets: slot 0's two images, then slot 1's, which its slot takes from the stream instead. */
	std::vector<uint8> SpriteImages()
	{
		FByteWriter Section;
		Section.U32(2).U32(12).U32(12 + 12 + 8 + 6);
		Section.U32(2).U32(12).U32(12 + 8);
		Section.U16(2).U16(1).U16(0xAAAA).U16(0xBBBB);
		Section.U16(1).U16(1).U16(0xCCCC);
		Section.U32(1).U32(8).U16(1).U16(1).U16(0xDDDD);
		return Section.Bytes;
	}

	std::vector<uint8> Zones()
	{
		FByteWriter Section;
		for (int16 Corner = 0; Corner < 4; ++Corner)
		{
			Section.S16(static_cast<int16>(Corner * 100)).S16(-8).S16(static_cast<int16>(-Corner));
		}
		return Section.Bytes;
	}

	std::vector<uint8> Archive()
	{
		const std::vector<std::vector<uint8>> Sections = {Tims(), TinyWalkmesh(), ModelList(), {1, 2, 3}, SpriteImages(), {4, 5, 6, 7, 8}, {9}, {10, 11}, Zones()};
		FByteWriter Header;
		Header.S16(832).S16(0).S16(0x55).S16(0).S16(896).S16(0).S16(0).S16(1);
		Header.Bytes.resize(0x10C, 0);
		std::vector<std::vector<uint8>> Streams;
		for (std::size_t Section = 0; Section < Sections.size(); ++Section)
		{
			Streams.push_back(Literals(Sections[Section], Section % 8));
			Header.U32(static_cast<uint32>(Sections[Section].size()));
		}
		const uint32 ActorCount = 4;
		uint32 Offset = 0x190 + 16 * ActorCount;
		for (const std::vector<uint8>& Stream : Streams)
		{
			Header.U32(Offset);
			Offset += static_cast<uint32>(Stream.size());
		}
		Header.S16(-50).S16(500).S16(80).S16(0).S16(100).S16(101).S16(102).S16(0);
		Header.Bytes.resize(0x184, 0);
		Header.S16(50).S16(51).S16(52).S16(0).U32(0xABCD0000 | ActorCount);
		Header.U16(0x01E0).S16(0).S16(0).S16(0).S16(1).S16(2).S16(3).U16(7);
		Header.U16(0x0010).S16(0).S16(1024).S16(0).S16(10).S16(20).S16(30).U16(0);
		Header.U16(0x2004).S16(0).S16(0).S16(0).S16(0).S16(0).S16(0).U16(1);
		Header.U16(0x4000).S16(0).S16(0).S16(0).S16(-1).S16(0).S16(0).U16(1);
		for (const std::vector<uint8>& Stream : Streams)
		{
			Header.Bytes.insert(Header.Bytes.end(), Stream.begin(), Stream.end());
		}
		Header.Bytes.resize((Header.Size() + SectorBytes - 1) / SectorBytes * SectorBytes, 0);
		return Header.Bytes;
	}

	/** One CLUT block of one sector: four colours at (0, 480). */
	std::vector<uint8> TextureStream()
	{
		FByteWriter Stream;
		Stream.U32(0x1201).S16(0).S16(480).S16(0).S16(0).S16(4).S16(1).S16(1).S16(1).U32(1).U32(1).S16(1);
		Stream.Bytes.resize(SectorBytes, 0);
		Stream.U16(0x0000).U16(0x0001).U16(0x8000).U16(0x7FFF);
		Stream.Bytes.resize(2 * SectorBytes, 0);
		return Stream.Bytes;
	}
}

TEST_CASE("A field map gives its header, models, walkmesh, zones and raw sections", "[Field]")
{
	const auto Map = ParseFieldMap(Archive(), TextureStream());
	REQUIRE(Map);
	CHECK((Map->SpriteSlots[0].X == 832 && Map->SpriteSlots[0].bImagesInArchive));
	CHECK((Map->SpriteSlots[1].X == 896 && !Map->SpriteSlots[1].bImagesInArchive));
	CHECK(Map->Lights.Directional[0].Direction == FGteVector{-50, 500, 80});
	CHECK(Map->Lights.Directional[0].Color == FGteVector{100, 101, 102});
	CHECK(Map->Lights.Directional[1].Color == FGteVector{0, 0, 0});
	CHECK(Map->Lights.Ambient == FGteVector{50, 51, 52});

	// The count's high half is not read.
	REQUIRE(Map->Actors.size() == 4);
	const FFieldActorDefinition& Script = Map->Actors[0];
	CHECK((!Script.HasModel() && Script.IsHidden() && Script.Position == FGteVector{1, 2, 3}));
	const FFieldActorDefinition& Scenery = Map->Actors[1];
	CHECK((Scenery.HasModel() && Scenery.Rotation == FGteVector{0, 1024, 0} && Scenery.Position == FGteVector{10, 20, 30}));
	CHECK((Scenery.ModelDrawMode(false) == 2 && Scenery.ModelDrawMode(true) == 5));
	CHECK((Map->Actors[2].LightMode() == 1 && Map->Actors[2].ModelDrawMode(false) == 1 && Map->Actors[2].ModelDrawMode(true) == 4));
	CHECK((Map->Actors[3].ModelDrawMode(false) == 3 && Map->Actors[3].Position[0] == -1));

	REQUIRE(Map->Models.size() == 2);
	REQUIRE(Map->ModelObjects.size() == 3);
	const FFieldModelObject& First = Map->ModelObjects[0];
	CHECK((First.Actor == 1 && First.TextureAtBuild == FMdlTextureState{}));
	CHECK((First.Bounds.Centre == FGteVector{0, 0, 0} && First.Bounds.Radius == 123));
	// Model 1 has no texture commands: its builds inherit model 0's page and CLUT, then keep them.
	CHECK(Map->ModelObjects[1].TextureAtBuild == FMdlTextureState{0x88, 0x7B40});
	CHECK(Map->ModelObjects[2].TextureAtBuild == FMdlTextureState{0x88, 0x7B40});
	CHECK(Map->Models[1].Parts[0].Primitives[0].Texture(Map->ModelObjects[1].TextureAtBuild) == FMdlTextureState{0x88, 0x7B40});

	REQUIRE(Map->Walkmesh.Layers.size() == 1);
	CHECK(Map->Walkmesh.Layers[0].Vertices[2] == FGteVector{0, 0, 64});
	REQUIRE(Map->Zones.size() == 1);
	CHECK(Map->Zones[0].Corners[3] == FGteVector{300, -8, -3});

	// Sections end at their recorded size, whatever their streams hold past it.
	CHECK(Map->SpriteSets == std::vector<uint8>{1, 2, 3});
	CHECK(Map->EventScripts == std::vector<uint8>{4, 5, 6, 7, 8});
	CHECK(Map->Encounters == std::vector<uint8>{9});
	CHECK(Map->Messages == std::vector<uint8>{10, 11});
}

TEST_CASE("A field map's textures go up in the field's order, and brightening lifts its palettes", "[Field]")
{
	const auto Map = ParseFieldMap(Archive(), TextureStream());
	REQUIRE(Map);
	const std::vector<FVramImage>& Uploads = Map->TextureUploads;
	REQUIRE(Uploads.size() == 5);
	CHECK((Uploads[0].Rect.Y == 481 && Uploads[1].Rect.X == 320));
	CHECK((Uploads[2].Rect.X == 832 && Uploads[2].Rect.Width == 2 && Uploads[2].Units == std::vector<uint16>{0xAAAA, 0xBBBB}));
	CHECK((Uploads[3].Rect.X == 896 && Uploads[3].Units == std::vector<uint16>{0xCCCC}));
	CHECK((Uploads[4].Rect.Y == 480 && Uploads[4].Rect.Width == 4));

	FVram Plain;
	UploadFieldTextures(*Map, Plain, false);
	CHECK(Plain.Revision() == 5);
	CHECK((Plain.At(1, 480) == 0x0001 && Plain.At(0, 481) == 0x1234));

	FVram Bright;
	UploadFieldTextures(*Map, Bright, true);
	CHECK(Bright.Revision() == 6);
	CHECK(Bright.At(0, 480) == 0x0000);
	CHECK(Bright.At(1, 480) == 0x0C63);
	CHECK(Bright.At(2, 480) == 0x8C63);
	CHECK(Bright.At(3, 480) == 0x7FFF);
	CHECK(Bright.At(0, 481) == 0x1E77);
	CHECK(Bright.At(1, 481) == 0x0000);
	// Only the palette rows change.
	CHECK(Bright.At(320, 0) == 0x5678);
}

TEST_CASE("A field map whose actors place a missing model is rejected", "[Field]")
{
	std::vector<uint8> Bytes = Archive();
	// Actor 3's model number.
	Bytes[0x190 + 3 * 16 + 14] = 2;
	CHECK_FALSE(ParseFieldMap(Bytes, TextureStream()));
	CHECK_FALSE(ParseFieldMap(std::vector<uint8>(0x100, 0), TextureStream()));
}
