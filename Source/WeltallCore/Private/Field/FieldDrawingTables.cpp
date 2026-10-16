#include "Field/FieldDrawingTables.h"

#include "Formats/Lzss.h"
#include "Foundation/LittleEndian.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		/** Where a release keeps the tables: offsets in its Field program (directory 1, unpacked) and the kernel's address of the font's CLUTs. */
		struct FDrawingTablesLayout
		{
			EDiscRelease Release;
			int32 ProgramFile;
			std::size_t SystemImages;
			std::size_t FramePieces;
			std::size_t ChoiceCursor;
			std::size_t MoreCursor;
			std::size_t PortraitCells;
			std::size_t MessageText;
			std::size_t PortraitFiles;
			std::size_t FacePlaces;
			std::size_t ParticleSprites;
			uint32 TextClut;
			/** The compass's tables: its directions' bits, its letters' places, its quads' corners' x, z, u and v, its textures, its shadow's corners and texels. */
			std::size_t CompassDirections;
			std::size_t CompassLetters;
			std::size_t CompassX;
			std::size_t CompassZ;
			std::size_t CompassU;
			std::size_t CompassV;
			std::size_t CompassTextures;
			std::size_t CompassShadow;
			std::size_t CompassShadowTexels;
			/** The kernel's address of the pause's picture, an LZSS image list. */
			uint32 PauseImage;
		};

		constexpr int32 ProgramDirectory = 1;
		/** Two faces' files for each portrait number a script can give. */
		constexpr std::size_t PortraitFileBytes = 2 * 0x100;
		/** A particle's picture: half its width and height and its centre (in steps of 16 view units), then its corners' texels. */
		constexpr std::size_t ParticleSpriteBytes = 0x18;
		/** The effects' texture page's rows the particles' pictures start at. */
		constexpr int16 ParticleRows = 0x40;
		/**
		 * The compass's quads: the dial's 4 x 4 (a column of the corner tables each way) from the first
		 * four rows, the letters' four and the needle, one row of them each.
		 */
		constexpr std::size_t CompassLetterRow = 4;
		constexpr std::size_t CompassNeedleRow = 8;
		/** The shadow's texels lie from this row of its page on; its page and CLUT are the code's own. */
		constexpr int32 ShadowRows = 0xC0;
		constexpr FFieldCompassLayout::FTexture ShadowTexture{0, 640, 256, 256, 242};
		constexpr std::array<FDrawingTablesLayout, 2> Layouts{{
			{EDiscRelease::UsaDisc1, 14, 0x3E154, 0x3E3AC, 0x3E3EC, 0x3E414, 0x3E444, 0x3E464, 0x3E6F0, 0x3EFF4, 0x3F78C, 0x80050190, 0x3E134, 0x3E144, 0x3E1F0,
				0x3E238, 0x3E280, 0x3E2C8, 0x3E310, 0x3E340, 0x3E380, 0x8004FBD8},
			{EDiscRelease::UsaDisc2, 14, 0x3E154, 0x3E3AC, 0x3E3EC, 0x3E414, 0x3E444, 0x3E464, 0x3E6F0, 0x3EFF4, 0x3F78C, 0x80050190, 0x3E134, 0x3E144, 0x3E1F0,
				0x3E238, 0x3E280, 0x3E2C8, 0x3E310, 0x3E340, 0x3E380, 0x8004FBD8},
		}};

		/** The kernel's packed image lists are short: enough of the executable to hold one. */
		constexpr std::size_t PackedImageBytes = 0x800;
		/** An image list's block of pixels: its type word, then x, y, the offset, width and height, then the pixels. */
		constexpr uint32 ImageBlock = 0x1100;
		constexpr std::size_t ImageBlockHeader = 16;

		/** The first image of a kernel image list (a count, as many words, then blocks), at its offset. */
		TResult<FVramImage> ReadImageList(std::span<const uint8> List)
		{
			if (List.size() < 4)
			{
				return MakeError("an image list is cut short");
			}
			const std::size_t At = 4 + 4 * static_cast<std::size_t>(ReadU32(List, 0));
			if (List.size() < At + ImageBlockHeader || ReadU32(List, At) != ImageBlock)
			{
				return MakeError("an image list does not start with an image");
			}
			FVramImage Image;
			Image.Rect = {ReadS16(List, At + 8), ReadS16(List, At + 10), ReadS16(List, At + 12), ReadS16(List, At + 14)};
			const std::size_t Units = static_cast<std::size_t>(std::max(0, Image.Rect.Width * Image.Rect.Height));
			if (List.size() < At + ImageBlockHeader + 2 * Units)
			{
				return MakeError("an image list's pixels are cut short");
			}
			Image.Units.resize(Units);
			for (std::size_t Unit = 0; Unit < Units; ++Unit)
			{
				Image.Units[Unit] = ReadU16(List, At + ImageBlockHeader + 2 * Unit);
			}
			return Image;
		}

		/** A texture coordinate as the compass's builder stores it: clamped to 0-255. */
		uint8 Texel(int32 Value)
		{
			return static_cast<uint8>(std::clamp(Value, 0, 0xFF));
		}

		/** Row Column's and row Row's corners of the compass's quad tables: (x, z) from the first, (u, v) from the second. */
		FFieldCompassLayout::FQuad CompassQuad(std::span<const uint8> Program, const FDrawingTablesLayout& Layout, std::size_t Column, std::size_t Row)
		{
			FFieldCompassLayout::FQuad Quad;
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				const std::size_t Across = (Column * 4 + Corner) * 2;
				const std::size_t Down = (Row * 4 + Corner) * 2;
				Quad.Corners[Corner] = {ReadS16(Program, Layout.CompassX + Across), ReadS16(Program, Layout.CompassZ + Down)};
				Quad.Texels[Corner] = {Texel(ReadS16(Program, Layout.CompassU + Across)), Texel(ReadS16(Program, Layout.CompassV + Down))};
			}
			return Quad;
		}

		FFieldCompassLayout ReadCompass(std::span<const uint8> Program, const FDrawingTablesLayout& Layout)
		{
			FFieldCompassLayout Compass;
			for (std::size_t Row = 0; Row < 4; ++Row)
			{
				for (std::size_t Column = 0; Column < 4; ++Column)
				{
					Compass.Dial[Row * 4 + Column] = CompassQuad(Program, Layout, Column, Row);
				}
			}
			for (std::size_t Letter = 0; Letter < Compass.LetterQuads.size(); ++Letter)
			{
				Compass.LetterQuads[Letter] = CompassQuad(Program, Layout, CompassLetterRow + Letter, CompassLetterRow + Letter);
				Compass.LetterPlaces[Letter] = {ReadS16(Program, Layout.CompassLetters + Letter * 4), ReadS16(Program, Layout.CompassLetters + Letter * 4 + 2)};
			}
			Compass.Needle = CompassQuad(Program, Layout, CompassNeedleRow, CompassNeedleRow);
			for (std::size_t Texture = 0; Texture < Compass.Textures.size(); ++Texture)
			{
				const auto Word = [&Program, &Layout, Texture](std::size_t Number) { return ReadS16(Program, Layout.CompassTextures + Texture * 12 + Number * 2); };
				// A texture page starts on a 64-unit column and a 256-row half of VRAM.
				Compass.Textures[Texture] = {Word(0), static_cast<int16>(Word(2) & 0x3C0), static_cast<int16>(Word(3) & 0x100), Word(4), Word(5)};
			}
			for (std::size_t Direction = 0; Direction < Compass.DirectionBits.size(); ++Direction)
			{
				Compass.DirectionBits[Direction] = ReadU16(Program, Layout.CompassDirections + Direction * 2);
			}
			for (std::size_t Quad = 0; Quad < Compass.Shadow.size(); ++Quad)
			{
				FFieldCompassLayout::FQuad& Shadow = Compass.Shadow[Quad];
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					const std::size_t At = Quad * 8 + Corner * 2;
					Shadow.Corners[Corner] = {ReadS16(Program, Layout.CompassShadow + At * 2), ReadS16(Program, Layout.CompassShadow + At * 2 + 2)};
					Shadow.Texels[Corner] = {Texel(Program[Layout.CompassShadowTexels + At]), Texel(Program[Layout.CompassShadowTexels + At + 1] + ShadowRows)};
				}
			}
			Compass.ShadowTexture = ShadowTexture;
			return Compass;
		}

		template <std::size_t Count>
		void ReadWindows(std::span<const uint8> Program, std::size_t Offset, std::array<FFieldTexelWindow, Count>& Windows)
		{
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				const std::size_t At = Offset + Index * 8;
				Windows[Index] = {ReadS16(Program, At), ReadS16(Program, At + 2), ReadS16(Program, At + 4), ReadS16(Program, At + 6)};
			}
		}
	}

	std::optional<std::array<int32, 2>> FFieldDrawingTables::FaceFiles(int32 Portrait) const
	{
		const std::size_t First = static_cast<std::size_t>(Portrait) * 2;
		if (Portrait < 0 || First + 1 >= PortraitFiles.size())
		{
			return std::nullopt;
		}
		return std::array<int32, 2>{FirstFaceFile + PortraitFiles[First], FirstFaceFile + PortraitFiles[First + 1]};
	}

	TResult<FFieldDrawingTables> FFieldDrawingTables::Read(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Executable,
		EDiscRelease Release)
	{
		const FDrawingTablesLayout* Layout = nullptr;
		for (const FDrawingTablesLayout& Candidate : Layouts)
		{
			if (Candidate.Release == GameplayRelease(Release))
			{
				Layout = &Candidate;
			}
		}
		if (Layout == nullptr)
		{
			return MakeError("no field drawing tables are known for this release");
		}
		const FIndexRecord* Record = Index.Find(ProgramDirectory, Layout->ProgramFile);
		if (Record == nullptr)
		{
			return MakeError("the disc lacks the Field program");
		}
		const TResult<std::vector<uint8>> Sectors = ReadFileSectors(Image, *Record);
		if (!Sectors)
		{
			return Sectors.Error();
		}
		const TResult<std::vector<uint8>> Unpacked = UnpackLzss(*Sectors);
		if (!Unpacked)
		{
			return Unpacked.Error();
		}
		const std::span<const uint8> Program = *Unpacked;
		if (Program.size() < Layout->FacePlaces + 2 * FaceSlots * 8 || Program.size() < Layout->PortraitFiles + PortraitFileBytes
			|| Program.size() < Layout->ParticleSprites + ParticleSpriteCount * ParticleSpriteBytes || Program.size() < Layout->CompassShadowTexels + 32
			|| !Executable.Contains(Layout->TextClut, 64) || !Executable.Contains(Layout->PauseImage, PackedImageBytes))
		{
			return MakeError("the Field program or the kernel is too short to hold the field's drawing tables");
		}
		FFieldDrawingTables Tables;
		for (std::size_t Place = 0; Place < SystemImageCount; ++Place)
		{
			const std::size_t At = Layout->SystemImages + Place * 12;
			Tables.SystemImages[Place] = {ReadS16(Program, At), ReadS16(Program, At + 2), ReadS16(Program, At + 4), ReadS16(Program, At + 6),
				ReadS16(Program, At + 8), ReadS16(Program, At + 10)};
		}
		for (std::size_t Window = 0; Window < Tables.MessageText.size(); ++Window)
		{
			Tables.MessageText[Window] = {ReadS16(Program, Layout->MessageText + Window * 4), ReadS16(Program, Layout->MessageText + Window * 4 + 2)};
		}
		ReadWindows(Program, Layout->FramePieces, Tables.FramePieces);
		ReadWindows(Program, Layout->ChoiceCursor, Tables.ChoiceCursor);
		ReadWindows(Program, Layout->MoreCursor, Tables.MoreCursor);
		for (std::size_t Cell = 0; Cell < Tables.PortraitCells.size(); ++Cell)
		{
			Tables.PortraitCells[Cell] = {Program[Layout->PortraitCells + Cell * 4], Program[Layout->PortraitCells + Cell * 4 + 2]};
		}
		for (std::size_t Place = 0; Place < Tables.FacePlaces.size(); ++Place)
		{
			const std::size_t At = Layout->FacePlaces + Place * 8;
			Tables.FacePlaces[Place] = {ReadS16(Program, At), ReadS16(Program, At + 2), ReadS16(Program, At + 4), ReadS16(Program, At + 6), 0x100, 1};
		}
		Tables.PortraitFiles.assign(Program.begin() + static_cast<std::ptrdiff_t>(Layout->PortraitFiles),
			Program.begin() + static_cast<std::ptrdiff_t>(Layout->PortraitFiles + PortraitFileBytes));
		const std::span<const uint8> Clut = Executable.Read(Layout->TextClut, 64);
		for (std::size_t Color = 0; Color < Tables.TextClut.size(); ++Color)
		{
			Tables.TextClut[Color] = ReadU16(Clut, Color * 2);
		}
		for (std::size_t Sprite = 0; Sprite < Tables.ParticleSprites.size(); ++Sprite)
		{
			const std::size_t At = Layout->ParticleSprites + Sprite * ParticleSpriteBytes;
			const auto Word = [&Program, At](std::size_t Number) { return ReadS16(Program, At + 2 * Number); };
			const int16 Left = static_cast<int16>(16 * (Word(2) - Word(0)));
			const int16 Right = static_cast<int16>(16 * (Word(0) + Word(2)));
			const int16 Top = static_cast<int16>(16 * (Word(3) - Word(1)));
			const int16 Bottom = static_cast<int16>(16 * (Word(1) + Word(3)));
			FFieldParticleSprite& Picture = Tables.ParticleSprites[Sprite];
			Picture.Corners = {{{Left, Top}, {Right, Top}, {Left, Bottom}, {Right, Bottom}}};
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				Picture.Texels[Corner] = {Word(4 + 2 * Corner), static_cast<int16>(Word(5 + 2 * Corner) + ParticleRows)};
			}
		}
		Tables.Compass = ReadCompass(Program, *Layout);
		const TResult<std::vector<uint8>> PauseList = UnpackLzss(Executable.Read(Layout->PauseImage, PackedImageBytes));
		if (!PauseList)
		{
			return PauseList.Error();
		}
		TResult<FVramImage> Pause = ReadImageList(*PauseList);
		if (!Pause)
		{
			return MakeError("the pause's picture: " + Pause.Error().Message);
		}
		Tables.PauseImage = std::move(*Pause);
		TResult<FFieldCockpitHudLayout> CockpitHud = FFieldCockpitHudLayout::FromProgram(Program, Release);
		if (!CockpitHud)
		{
			return CockpitHud.Error();
		}
		Tables.CockpitHud = std::move(*CockpitHud);
		return Tables;
	}
}
