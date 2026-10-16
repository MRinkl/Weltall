#include "Battling/BattlingArenaData.h"

#include "Battling/BattlingData.h"
#include "BattlingFiles.h"
#include "Formats/Tim.h"
#include "Foundation/LittleEndian.h"
#include "Media/GameFileIndex.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		/** Where a release's Battling program (unpacked) keeps the arena's drawing tables, as offsets in it. */
		struct FBattlingArenaLayout
		{
			EDiscRelease Release;
			std::size_t FirstColumns;
			std::size_t LastColumns;
			std::size_t CellTexels;
			std::size_t Lighting;
			std::size_t HudPoints;
			std::size_t NamePalette;
			/** The effects' frame tables: two of columns and three of rows, alternating, 16 bytes each. */
			std::size_t EffectTexels;
			std::size_t FirePalette;
		};

		constexpr std::array<FBattlingArenaLayout, 2> ArenaLayouts{{
			{EDiscRelease::UsaDisc1, 0x21D44, 0x21DC4, 0x21E44, 0x21C9C, 0x21D04, 0x21D24, 0x216E8, 0x221F0},
			{EDiscRelease::UsaDisc2, 0x21D44, 0x21DC4, 0x21E44, 0x21C9C, 0x21D04, 0x21D24, 0x216E8, 0x221F0},
		}};

		/** The portraits: a gear's two sectors, its palette then its picture; seven gears to a column of VRAM. */
		constexpr int32 PortraitFile = 6;
		constexpr std::size_t PortraitBytes = 0x1000;
		constexpr std::size_t PortraitPaletteBytes = 0x100;
		constexpr int32 PortraitsPerColumn = 7;
		constexpr FVramRect FirstPortrait{0x1A0, 0, 0x1E, 0x40};
		constexpr int16 PortraitColumnStep = 0x20;
		constexpr FVramRect FirstPortraitPalette{0x200, 0x1FF, 0x80, 1};

		/** The fire's palette: loaded with every colour blending, all but its last. */
		constexpr FVramRect FirePalettePlace{0, 0x1FD, 0xFF, 1};

		constexpr std::size_t LightingSize = 17;
		constexpr std::size_t CellTexelsSize = 0x20;
		constexpr std::size_t EffectTableSize = 16;

		constexpr int32 TextureArchive = 5;
		constexpr uint32 TextureCount = 36;
		/** The effects' pictures: the first sprite set's frames, the others' (by set: the second and fourth, the third, the fifth), the trails'. */
		constexpr uint32 FirstSetTextures = 12;
		constexpr std::array<uint32, 3> SetTextures{12, 15, 25};
		constexpr uint32 TrailTexture = 16;
		constexpr uint32 ShadowTexture = 13;
		constexpr uint32 FirstTerrainPage = 17;
		constexpr uint32 WallTexture = 26;
		constexpr uint32 FontTexture = 14;
		constexpr uint32 HudTubesTexture = 21;
		constexpr uint32 HudHeatTexture = 22;
		constexpr uint32 PointerTexture = 23;
		constexpr uint32 FlashTexture = 24;

		/** A colour a loader sets in a picture's palette before uploading it. */
		struct FPaletteChange
		{
			uint32 Texture;
			std::size_t Colour;
			uint16 Value;
		};

		/**
		 * The loaders' changes: the font's, the HUD's tubes', the tutorial's pointer's
		 * and the screen flash's bar's, each first colour made clear.
		 */
		constexpr std::array<FPaletteChange, 9> PaletteChanges{{
			{FontTexture, 0, 0}, {FontTexture, 2, 0x9063}, {FontTexture, 3, 0xFFFF},
			{HudTubesTexture, 0, 0}, {HudTubesTexture, 1, 0x8000},
			{PointerTexture, 0, 0}, {PointerTexture, 2, 0x8000}, {PointerTexture, 3, 0xFFFF},
			{FlashTexture, 0, 0},
		}};
		constexpr uint16 SemiTransparentBit = 0x8000;
		constexpr std::size_t WallColours = 256;
		/** The pictures whose 16-colour palette the loaders make clear in its first colour and blending in the others: the shadow's, the effects'. */
		constexpr std::array<uint32, 4> BlendedPalettes{12, ShadowTexture, 15, TrailTexture};
		constexpr std::size_t BlendedColours = 16;

		/** The effects' sprite sets by number: the texels a frame covers, half the square it is drawn over, whether its texels are drawn raw. */
		struct FSpriteSetShape
		{
			uint8 Size;
			int16 Half;
			bool bRaw;
		};

		constexpr std::array<FSpriteSetShape, 5> SpriteSetShapes{{{0x26, 0x60, true}, {0x1F, 0x60, true}, {0xF, 0x48, false}, {0x1F, 0x60, true}, {0xF, 0x20, true}}};
		/** The fourth set is the second's picture drawn subtracting from the screen. */
		constexpr uint16 SubtractBlend = 2 << 5;
		constexpr uint16 BlendBits = 3 << 5;

		/** The trails' palette: the gears' colours, then the texels that pick each (the first colour's index, the second's). */
		constexpr uint16 TrailTexelsPickingColours = 0x1111;

		/** libgpu's GetTPage and GetClut. */
		constexpr uint16 TexturePageOf(int32 Depth, int32 Blend, int32 X, int32 Y)
		{
			return static_cast<uint16>((Depth & 3) << 7 | (Blend & 3) << 5 | (Y & 0x100) >> 4 | (X & 0x3FF) >> 6 | (Y & 0x200) << 2);
		}

		constexpr uint16 ClutOf(int32 X, int32 Y)
		{
			return static_cast<uint16>(Y << 6 | (X >> 4 & 0x3F));
		}

		/** The image list's blocks: a kind, a place the side's origins replace, an offset from it, the size, then the units. */
		constexpr uint32 ImageBlock = 0x1100;
		constexpr uint32 ClutBlock = 0x1101;
		constexpr std::size_t BlockHeader = 16;
		constexpr std::array<int16, 2> GearPageX{0x3C0, 0x380};
		constexpr std::array<int16, 2> GearClutY{0x1FF, 0x1FE};

		FDrawColor ColourAt(std::span<const uint8> Bytes, std::size_t Offset)
		{
			return {Bytes[Offset], Bytes[Offset + 1], Bytes[Offset + 2]};
		}
	}

	TResult<FBattlingArenaTables> FBattlingArenaTables::Read(const FCdImage& Image, const FGameFileIndex& Index, EDiscRelease Release)
	{
		const FBattlingArenaLayout* Layout = nullptr;
		for (const FBattlingArenaLayout& Candidate : ArenaLayouts)
		{
			if (Candidate.Release == GameplayRelease(Release))
			{
				Layout = &Candidate;
			}
		}
		if (Layout == nullptr)
		{
			return MakeError("no Battling drawing tables are known for this release");
		}
		const TResult<std::vector<uint8>> Unpacked = BattlingFiles::ReadPacked(Image, Index, BattlingFiles::ProgramDirectory, BattlingFiles::ProgramFile);
		if (!Unpacked)
		{
			return Unpacked.Error();
		}
		const std::span<const uint8> Program = *Unpacked;
		if (Program.size() < Layout->CellTexels + CellTexelsSize || Program.size() < Layout->FirePalette + 2 * FBattlingArenaTables::FireColours)
		{
			return MakeError("the Battling program is too short to hold the arena's drawing tables");
		}
		FBattlingArenaTables Tables;
		for (std::size_t Row = 0; Row < Tables.FirstColumn.size(); ++Row)
		{
			Tables.FirstColumn[Row] = Program[Layout->FirstColumns + Row];
			Tables.LastColumn[Row] = Program[Layout->LastColumns + Row];
		}
		for (std::size_t Mirror = 0; Mirror < Tables.CellTexels.size(); ++Mirror)
		{
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				Tables.CellTexels[Mirror][Corner] = ReadU16(Program, Layout->CellTexels + 8 * Mirror + 2 * Corner);
			}
		}
		for (std::size_t Number = 0; Number < Tables.Lighting.size(); ++Number)
		{
			const std::size_t At = Layout->Lighting + LightingSize * Number;
			Tables.Lighting[Number] = {ColourAt(Program, At), ColourAt(Program, At + 4), ColourAt(Program, At + 8), ColourAt(Program, At + 12), Program[At + 16] != 0};
		}
		for (std::size_t Point = 0; Point < Tables.HudPoints.size(); ++Point)
		{
			Tables.HudPoints[Point] = {ReadS16(Program, Layout->HudPoints + 4 * Point), ReadS16(Program, Layout->HudPoints + 4 * Point + 2)};
		}
		for (std::size_t Colour = 0; Colour < Tables.NamePalette.size(); ++Colour)
		{
			Tables.NamePalette[Colour] = ReadU16(Program, Layout->NamePalette + 2 * Colour);
		}
		for (std::size_t Frame = 0; Frame < EffectTableSize; ++Frame)
		{
			const std::size_t At = Layout->EffectTexels + Frame;
			Tables.EffectColumns[0][Frame] = Program[At];
			Tables.EffectRows[0][Frame] = Program[At + EffectTableSize];
			Tables.EffectColumns[1][Frame] = Program[At + 2 * EffectTableSize];
			Tables.EffectRows[1][Frame] = Program[At + 3 * EffectTableSize];
			Tables.EffectRows[2][Frame] = Program[At + 4 * EffectTableSize];
		}
		for (std::size_t Colour = 0; Colour < Tables.FirePalette.size(); ++Colour)
		{
			Tables.FirePalette[Colour] = ReadU16(Program, Layout->FirePalette + 2 * Colour);
		}
		return Tables;
	}

	TResult<std::vector<FVramImage>> ReadBattlingGearTextures(const FBattlingGearFile& File, int32 Side)
	{
		std::vector<FVramImage> Uploads;
		const std::span<const uint8> Bytes = File.Bytes;
		if (File.Textures == 0)
		{
			return Uploads;
		}
		if (File.Textures + 4 > Bytes.size())
		{
			return MakeError("a gear file's textures lie past it");
		}
		const uint32 Count = ReadU32(Bytes, File.Textures);
		std::size_t At = File.Textures + 4 * (static_cast<std::size_t>(Count) + 1);
		for (uint32 Block = 0; Block < Count; ++Block)
		{
			if (At + BlockHeader > Bytes.size())
			{
				return MakeError("a gear file's texture " + std::to_string(Block) + " lies past it");
			}
			const uint32 Kind = ReadU32(Bytes, At);
			if (Kind != ImageBlock && Kind != ClutBlock)
			{
				break;
			}
			FVramImage Upload;
			const bool bClut = Kind == ClutBlock;
			Upload.Rect.X = static_cast<int16>((bClut ? 0 : GearPageX[static_cast<std::size_t>(Side)]) + ReadS16(Bytes, At + 8));
			Upload.Rect.Y = static_cast<int16>((bClut ? GearClutY[static_cast<std::size_t>(Side)] : 0) + ReadS16(Bytes, At + 10));
			Upload.Rect.Width = ReadS16(Bytes, At + 12);
			Upload.Rect.Height = ReadS16(Bytes, At + 14);
			const std::size_t Units = static_cast<std::size_t>(Upload.Rect.Width) * static_cast<std::size_t>(Upload.Rect.Height);
			if (Upload.Rect.Width < 0 || Upload.Rect.Height < 0 || At + BlockHeader + 2 * Units > Bytes.size())
			{
				return MakeError("a gear file's texture " + std::to_string(Block) + " runs past it");
			}
			Upload.Units.resize(Units);
			for (std::size_t Unit = 0; Unit < Units; ++Unit)
			{
				Upload.Units[Unit] = ReadU16(Bytes, At + BlockHeader + 2 * Unit);
			}
			At += BlockHeader + 2 * Units;
			Uploads.push_back(std::move(Upload));
		}
		return Uploads;
	}

	TResult<FBattlingArenaTextures> FBattlingArenaTextures::Read(const FCdImage& Image, const FGameFileIndex& Index)
	{
		const TResult<std::vector<uint8>> Unpacked = BattlingFiles::ReadPacked(Image, Index, BattlingFiles::DataDirectory, TextureArchive);
		if (!Unpacked)
		{
			return Unpacked.Error();
		}
		const std::span<const uint8> Archive = *Unpacked;
		if (Archive.size() < 4 * (TextureCount + 1) || ReadU32(Archive, 0) < TextureCount)
		{
			return MakeError("Battling's picture archive holds fewer pictures than the arena draws with");
		}
		FBattlingArenaTextures Textures;
		for (uint32 Number = 0; Number < TextureCount; ++Number)
		{
			const std::size_t Offset = ReadU32(Archive, 4 + 4 * Number);
			if (Offset >= Archive.size())
			{
				return MakeError("Battling's picture " + std::to_string(Number) + " lies past its archive");
			}
			TResult<FTim> Tim = ParseTim(Archive.subspan(Offset));
			if (!Tim)
			{
				return Tim.Error();
			}
			if (!Tim->Clut)
			{
				return MakeError("Battling's picture " + std::to_string(Number) + " has no palette");
			}
			FVramImage& Clut = *Tim->Clut;
			const FVramRect& Pixels = Tim->Image.Rect;
			if (std::find(BlendedPalettes.begin(), BlendedPalettes.end(), Number) != BlendedPalettes.end())
			{
				for (std::size_t Colour = 0; Colour < BlendedColours && Colour < Clut.Units.size(); ++Colour)
				{
					Clut.Units[Colour] = Colour == 0 ? uint16{0} : static_cast<uint16>(Clut.Units[Colour] | SemiTransparentBit);
				}
			}
			for (std::size_t Set = 0; Set < SetTextures.size(); ++Set)
			{
				if (SetTextures[Set] == Number)
				{
					Textures.SetPages[Set] = TexturePageOf(0, 1, Pixels.X, Pixels.Y);
					Textures.SetCluts[Set] = ClutOf(Clut.Rect.X, Clut.Rect.Y);
					Textures.SetRows[Set] = static_cast<uint8>(Pixels.Y);
				}
			}
			if (Number < FirstSetTextures)
			{
				Textures.FirstSetTexels[Number] = static_cast<uint16>(static_cast<uint8>(Pixels.X << 2) | static_cast<uint8>(Pixels.Y) << 8);
				Textures.FirstSetCluts[Number] = ClutOf(Clut.Rect.X, Clut.Rect.Y);
				Textures.FirstSetPage = TexturePageOf(0, 1, Pixels.X, Pixels.Y);
			}
			else if (Number == TrailTexture)
			{
				// The palette's row read as 4-bit texels: its third unit's first texel for player 1, its fourth's for player 2.
				const int16 Row = static_cast<int16>(Clut.Rect.Y + 1);
				Textures.TrailPage = TexturePageOf(0, 1, Pixels.X, Pixels.Y);
				Textures.TrailClut = ClutOf(Clut.Rect.X, Row);
				for (std::size_t Side = 0; Side < Textures.TrailTexels.size(); ++Side)
				{
					const uint8 U = static_cast<uint8>((Clut.Rect.X << 2) + 4 * (2 + Side));
					Textures.TrailTexels[Side] = static_cast<uint16>(U | static_cast<uint8>(Row) << 8);
				}
				Textures.TrailPalette = {Clut.Rect.X, Row, 4, 1};
			}
			else if (Number >= FirstTerrainPage && Number < FirstTerrainPage + Textures.TerrainPages.size())
			{
				Textures.TerrainPages[Number - FirstTerrainPage] = TexturePageOf(1, 1, Pixels.X, Pixels.Y);
				Textures.TerrainCluts[Number - FirstTerrainPage] = ClutOf(Clut.Rect.X, Clut.Rect.Y);
			}
			else if (Number == WallTexture)
			{
				// Every colour of the wall takes the blend bit.
				for (std::size_t Colour = 0; Colour < WallColours && Colour < Clut.Units.size(); ++Colour)
				{
					Clut.Units[Colour] |= SemiTransparentBit;
				}
				Textures.WallPage = TexturePageOf(1, 0, Pixels.X, Pixels.Y);
				Textures.WallClut = ClutOf(Clut.Rect.X, Clut.Rect.Y);
				Textures.WallV = static_cast<uint8>(Pixels.Y);
			}
			else if (Number == ShadowTexture)
			{
				Textures.ShadowPage = TexturePageOf(0, 2, Pixels.X, Pixels.Y);
				Textures.ShadowClut = ClutOf(Clut.Rect.X, Clut.Rect.Y);
				Textures.ShadowU = static_cast<uint8>(Pixels.X << 2);
				Textures.ShadowV = static_cast<uint8>(Pixels.Y);
			}
			else if (Number == HudTubesTexture)
			{
				Textures.HudTubes = Pixels;
				Textures.HudClut = ClutOf(Clut.Rect.X, Clut.Rect.Y);
			}
			else if (Number == PointerTexture)
			{
				Textures.PointerTexel = static_cast<uint16>(static_cast<uint8>(Pixels.X << 2) | static_cast<uint8>(Pixels.Y) << 8);
				Textures.PointerClut = ClutOf(Clut.Rect.X, Clut.Rect.Y);
				Textures.PointerPage = TexturePageOf(0, 1, Pixels.X, Pixels.Y);
			}
			else if (Number == FontTexture)
			{
				Textures.FontPage = TexturePageOf(0, 1, Pixels.X, Pixels.Y);
				Textures.FontClut = ClutOf(Clut.Rect.X, Clut.Rect.Y);
			}
			else if (Number == HudHeatTexture)
			{
				// Drawn with the tubes' palette: its own is never uploaded.
				Textures.HudHeat = Pixels;
				Textures.Uploads.push_back(std::move(Tim->Image));
				continue;
			}
			for (const FPaletteChange& Change : PaletteChanges)
			{
				if (Change.Texture == Number && Change.Colour < Clut.Units.size())
				{
					Clut.Units[Change.Colour] = Change.Value;
				}
			}
			Textures.Uploads.push_back(std::move(Clut));
			Textures.Uploads.push_back(std::move(Tim->Image));
		}
		return Textures;
	}

	FVramImage BattlingNamePaletteUpload(const FBattlingArenaTables& Tables)
	{
		return {BattlingNamePalettePlace, std::vector<uint16>(Tables.NamePalette.begin(), Tables.NamePalette.end())};
	}

	std::array<FBattlingSpriteSet, 5> MakeBattlingSpriteSets(const FBattlingArenaTables& Tables, const FBattlingArenaTextures& Textures)
	{
		std::array<FBattlingSpriteSet, 5> Sets;
		for (std::size_t Number = 0; Number < Sets.size(); ++Number)
		{
			FBattlingSpriteSet& Set = Sets[Number];
			Set.Size = SpriteSetShapes[Number].Size;
			Set.Half = SpriteSetShapes[Number].Half;
			Set.bRaw = SpriteSetShapes[Number].bRaw;
		}
		std::copy(Textures.FirstSetTexels.begin(), Textures.FirstSetTexels.end(), Sets[0].Texels.begin());
		std::copy(Textures.FirstSetCluts.begin(), Textures.FirstSetCluts.end(), Sets[0].Cluts.begin());
		Sets[0].Page = Textures.FirstSetPage;
		// The others' frames by the program's tables, each set's rows from its picture's first; the fourth is the second's.
		const auto FromPicture = [&](FBattlingSpriteSet& Set, std::size_t Picture, std::size_t Columns, std::size_t Rows) {
			for (std::size_t Frame = 0; Frame < Set.Texels.size(); ++Frame)
			{
				const uint8 V = static_cast<uint8>(Tables.EffectRows[Rows][Frame] + Textures.SetRows[Picture]);
				Set.Texels[Frame] = static_cast<uint16>(Tables.EffectColumns[Columns][Frame] | V << 8);
			}
			Set.Cluts.fill(Textures.SetCluts[Picture]);
			Set.Page = Textures.SetPages[Picture];
		};
		FromPicture(Sets[1], 0, 0, 0);
		FromPicture(Sets[2], 1, 1, 1);
		FromPicture(Sets[3], 0, 0, 0);
		Sets[3].Page = static_cast<uint16>((Sets[3].Page & ~BlendBits) | SubtractBlend);
		FromPicture(Sets[4], 2, 1, 2);
		return Sets;
	}

	FVramRect BattlingPortraitPlace(int32 Gear)
	{
		return {static_cast<int16>(FirstPortrait.X + PortraitColumnStep * (Gear / PortraitsPerColumn)),
			static_cast<int16>(FirstPortrait.Height * (Gear % PortraitsPerColumn)), FirstPortrait.Width, FirstPortrait.Height};
	}

	FVramRect BattlingPortraitPalettePlace(int32 Gear)
	{
		return {FirstPortraitPalette.X, static_cast<int16>(FirstPortraitPalette.Y - Gear), FirstPortraitPalette.Width, FirstPortraitPalette.Height};
	}

	TResult<std::vector<FVramImage>> ReadBattlingPortraitUploads(const FCdImage& Image, const FGameFileIndex& Index, std::span<const uint8> Gears)
	{
		const FIndexRecord* Record = Index.Find(BattlingFiles::DataDirectory, PortraitFile);
		if (Record == nullptr || !Record->IsFile())
		{
			return MakeError("the disc lacks Battling's portraits");
		}
		const TResult<std::vector<uint8>> Sectors = ReadFileSectors(Image, *Record);
		if (!Sectors)
		{
			return Sectors.Error();
		}
		std::vector<FVramImage> Uploads;
		for (const uint8 Gear : Gears)
		{
			const std::size_t At = static_cast<std::size_t>(Gear) * PortraitBytes;
			if (Sectors->size() < At + PortraitBytes)
			{
				return MakeError("Battling's portraits hold none of gear " + std::to_string(Gear));
			}
			const std::span<const uint8> Portrait = std::span<const uint8>(*Sectors).subspan(At, PortraitBytes);
			for (const bool bPicture : {false, true})
			{
				FVramImage& Upload = Uploads.emplace_back(FVramImage{bPicture ? BattlingPortraitPlace(Gear) : BattlingPortraitPalettePlace(Gear), {}});
				const std::size_t From = bPicture ? PortraitPaletteBytes : 0;
				const std::size_t Units = static_cast<std::size_t>(Upload.Rect.Width) * static_cast<std::size_t>(Upload.Rect.Height);
				for (std::size_t Unit = 0; Unit < Units; ++Unit)
				{
					Upload.Units.push_back(ReadU16(Portrait, From + 2 * Unit));
				}
			}
		}
		return Uploads;
	}

	FVramImage BattlingFirePaletteUpload(const FBattlingArenaTables& Tables)
	{
		FVramImage Upload{FirePalettePlace, {}};
		for (std::size_t Colour = 0; Colour < static_cast<std::size_t>(FirePalettePlace.Width); ++Colour)
		{
			Upload.Units.push_back(static_cast<uint16>(Tables.FirePalette[Colour] | SemiTransparentBit));
		}
		return Upload;
	}

	FVramImage BattlingFirePictureUpload(std::span<const uint8> Rows)
	{
		FVramImage Upload{BattlingFirePicturePlace, {}};
		const std::size_t Units = static_cast<std::size_t>(BattlingFirePicturePlace.Width) * static_cast<std::size_t>(BattlingFirePicturePlace.Height);
		for (std::size_t Unit = 0; Unit < Units && 2 * Unit + 1 < Rows.size(); ++Unit)
		{
			Upload.Units.push_back(static_cast<uint16>(Rows[2 * Unit] | Rows[2 * Unit + 1] << 8));
		}
		return Upload;
	}

	FVramImage BattlingTrailPaletteUpload(const FBattlingArenaTextures& Textures, const std::array<uint8, 3>& Player1Colour,
		const std::array<uint8, 3>& Player2Colour)
	{
		// Each channel above the middle, to five bits, blending.
		const auto Colour = [](const std::array<uint8, 3>& Channels) {
			const auto Bits = [](uint8 Channel) { return static_cast<uint16>(std::max(Channel - 0x80, 0) >> 2 & 0x1F); };
			return static_cast<uint16>(Bits(Channels[0]) | Bits(Channels[1]) << 5 | Bits(Channels[2]) << 10 | SemiTransparentBit);
		};
		return {Textures.TrailPalette, {Colour(Player1Colour), Colour(Player2Colour), 0, TrailTexelsPickingColours}};
	}

	std::array<FVramImage, 3> BattlingGearHudUploads(const FBattlingGearFile& File, int32 Side)
	{
		const std::span<const uint8> Pictures = std::span<const uint8>(File.Bytes).subspan(File.HudPictures, BattlingGears::HudPicturesSize);
		std::array<FVramImage, 3> Uploads;

		FVramImage& Palette = Uploads[0];
		Palette.Rect = {0, static_cast<int16>(BattlingIconPaletteRow + Side), 0x100, 1};
		for (std::size_t Colour = 0; Colour < 0x100; ++Colour)
		{
			Palette.Units.push_back(ReadU16(Pictures, 2 * Colour));
		}

		// Two 8-bit pixels a unit; player 1's rows are reversed.
		constexpr std::size_t Size = BattlingGears::IconSize;
		FVramImage& Icon = Uploads[1];
		Icon.Rect = {static_cast<int16>(BattlingIconsPlace.X + 0x10 * Side), BattlingIconsPlace.Y, static_cast<int16>(Size / 2), static_cast<int16>(Size)};
		for (std::size_t Row = 0; Row < Size; ++Row)
		{
			const std::span<const uint8> Pixels = Pictures.subspan(BattlingGears::HudPaletteSize + Size * Row, Size);
			for (std::size_t Unit = 0; Unit < Size / 2; ++Unit)
			{
				const std::size_t Left = 2 * Unit;
				const uint8 First = Side == 0 ? Pixels[Size - 1 - Left] : Pixels[Left];
				const uint8 Second = Side == 0 ? Pixels[Size - 2 - Left] : Pixels[Left + 1];
				Icon.Units.push_back(static_cast<uint16>(First | Second << 8));
			}
		}

		FVramImage& Name = Uploads[2];
		Name.Rect = {BattlingNamesPlace.X, static_cast<int16>(BattlingNamesPlace.Y + 8 * Side), 0x10, 8};
		for (std::size_t Unit = 0; Unit < 0x10 * 8; ++Unit)
		{
			Name.Units.push_back(ReadU16(Pictures, BattlingGears::HudNameOffset + 2 * Unit));
		}
		return Uploads;
	}
}
