#include "Graphics/SpriteCells.h"

#include "Game/PsxRandom.h"
#include "Graphics/SpriteFile.h"
#include "Graphics/SpriteSystem.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr uint32 HeaderEndOffset = 0x10;
		constexpr int32 MostParts = 0x3F;
	}

	int32 SpritePaletteSets(const FSpriteFile& File)
	{
		const FSpriteTables& Tables = File.Tables();
		const int32 Palettes = File.PalettesPerSet(Tables);
		const uint32 End = std::min<uint32>(File.Size(), static_cast<uint32>(File.Word(HeaderEndOffset) | File.Word(HeaderEndOffset + 2) << 16));
		const uint32 First = File.PaletteSet(Tables, 0);
		if (Palettes <= 0 || End <= First)
		{
			return 1;
		}
		return std::max<int32>(1, static_cast<int32>((End - First) / static_cast<uint32>(Palettes * 32)));
	}

	FVramImage SpritePaletteUpload(const FSpriteFile& File, const FSpriteFilePlace& Place, int32 PaletteSet)
	{
		const FSpriteTables& Tables = File.Tables();
		const int32 Colors = File.PalettesPerSet(Tables) * 16;
		FVramImage Upload{{static_cast<int16>(Place.ClutX), static_cast<int16>(Place.ClutY), static_cast<int16>(Colors), 1},
			std::vector<uint16>(static_cast<std::size_t>(Colors))};
		const uint32 First = File.PaletteSet(Tables, PaletteSet);
		for (int32 Color = 0; Color < Colors; ++Color)
		{
			Upload.Units[static_cast<std::size_t>(Color)] = File.Word(First + 2 * static_cast<uint32>(Color));
		}
		return Upload;
	}

	FTextureRegion SpritePartCell(const FSpritePart& Part)
	{
		const uint32 View = PackTextureView(FTexturePage::FromAttribute(Part.TexturePage), FClutPosition::FromAttribute(Part.Clut));
		const int32 LastU = std::min(255, Part.U + std::max<int32>(Part.Width, 1) - 1);
		const int32 LastV = std::min(255, Part.V + std::max<int32>(Part.Height, 1) - 1);
		return TextureRegionBetween(View, Part.U, Part.V, static_cast<uint8>(LastU), static_cast<uint8>(LastV));
	}

	void ForEachSpriteFrame(const FSpriteFile& File, const FSpriteFilePlace& Place, int32 PaletteSet, FVram& Vram,
		const std::function<void(std::span<const FSpritePart>)>& Visit, bool bOwnPalette)
	{
		FSpriteSystem Kernel;
		FPsxRandom Random;
		Kernel.Random = &Random;
		FSprite Sprite;
		Kernel.Create(Sprite, File, Place.ClutX, Place.ClutY, Place.TextureX, Place.TextureY, PaletteSet);
		if (!bOwnPalette)
		{
			Sprite.DrawFlags &= ~SpriteDrawFlags::PaletteUploadPending;
		}
		Kernel.SetPartCapacity(Sprite, MostParts);
		Kernel.BuildPendingFrames();
		Kernel.FlushUploads(Vram);
		const int32 Frames = File.FrameCount(File.Tables());
		for (int32 Frame = 1; Frame <= Frames; ++Frame)
		{
			Kernel.ClearFault();
			Kernel.SetFrame(Sprite, Frame);
			Kernel.BuildPendingFrames();
			Kernel.FlushUploads(Vram);
			if (Kernel.Fault() != ESpriteFault::None)
			{
				continue;
			}
			Visit(std::span<const FSpritePart>(Sprite.Parts.data(), static_cast<std::size_t>(Sprite.PartCount())));
		}
		Kernel.Free(Sprite);
	}
}
