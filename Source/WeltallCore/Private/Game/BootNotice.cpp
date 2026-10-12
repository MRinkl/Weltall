#include "Game/BootNotice.h"

#include "Formats/Lzss.h"
#include "Formats/Tim.h"
#include "Media/PsxExecutable.h"

namespace Weltall
{
	namespace
	{
		/** Where the executable carries the packed picture (both discs of the USA release). */
		constexpr uint32 PictureAddress = 0x8004EABC;
		/** The kernel puts the picture and its CLUT here, whatever the file says. */
		constexpr int16 PictureX = 640;
		constexpr int16 PictureY = 0;
		constexpr int16 ClutX = 0;
		constexpr int16 ClutY = 240;
		/** The default drawing environment's texture page: 4-bit at (640, 0). */
		constexpr uint16 DefaultTexturePage = 10;
		constexpr int32 SpriteX = 32;
		constexpr int32 SpriteY = 88;
		constexpr int32 FadeInVblanks = 16;
		constexpr int32 HoldVblanks = 110;
		constexpr uint32 SpriteBlendKey = 1;

		/** The brightness the kernel drew with by a vertical blank, from 1. */
		uint8 BrightnessAt(int32 Vblank)
		{
			if (Vblank <= FadeInVblanks)
			{
				return static_cast<uint8>((Vblank - 1) * 8);
			}
			if (Vblank <= FadeInVblanks + HoldVblanks)
			{
				return static_cast<uint8>((FadeInVblanks - 1) * 8);
			}
			return static_cast<uint8>(128 - (Vblank - FadeInVblanks - HoldVblanks - 1) * 8);
		}
	}

	TResult<FBootNotice> FBootNotice::Load(const FPsxExecutable& Executable)
	{
		if (!Executable.Contains(PictureAddress, 4))
		{
			return MakeError("the executable does not carry the boot notice");
		}
		const uint32 End = Executable.LoadAddress() + static_cast<uint32>(Executable.Body().size());
		const std::span<const uint8> Packed = Executable.Read(PictureAddress, End - PictureAddress);
		TResult<std::vector<uint8>> Unpacked = UnpackLzss(Packed);
		if (!Unpacked)
		{
			return Unpacked.Error();
		}
		TResult<FTim> Picture = ParseTim(*Unpacked);
		if (!Picture)
		{
			return Picture.Error();
		}
		if (!Picture->Clut)
		{
			return MakeError("the boot notice's picture has no CLUT");
		}
		FBootNotice Notice;
		FVramImage Clut = std::move(*Picture->Clut);
		Clut.Rect.X = ClutX;
		Clut.Rect.Y = ClutY;
		Notice.Screen.Upload(Clut);
		FVramImage Image = std::move(Picture->Image);
		Image.Rect.X = PictureX;
		Image.Rect.Y = PictureY;
		const int32 Width = Picture->PixelWidth();
		const int32 Height = Image.Rect.Height;
		Notice.Screen.Upload(Image);
		if (Width != 256 || Height != 48)
		{
			return MakeError("the boot notice's picture is not 256 x 48");
		}
		return Notice;
	}

	bool FBootNotice::Tick()
	{
		if (Vblank >= Vblanks)
		{
			return false;
		}
		++Vblank;
		return true;
	}

	FDrawTexture FBootNotice::Texture()
	{
		return {0, FTexturePage::FromAttribute(DefaultTexturePage), {ClutX, ClutY}};
	}

	FDrawList FBootNotice::DrawList() const
	{
		FDrawList List;
		DrawList(List);
		return List;
	}

	void FBootNotice::DrawList(FDrawList& Into) const
	{
		Into.Primitives.clear();
		if (Vblank == 0)
		{
			return;
		}
		const uint8 Level = BrightnessAt(Vblank);
		FDrawPrimitive Sprite = MakeSprite(SpriteX, SpriteY, 256, 48, Texture(), 0, 0, {Level, Level, Level});
		Sprite.BlendKey = SpriteBlendKey;
		Into.Primitives.push_back(Sprite);
	}
}
