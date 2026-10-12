#pragma once

#include "Graphics/Vram.h"
#include "Menu/MenuPrimitive.h"

namespace Weltall::BattleHudGpu
{
	/** The codes libgpu's SetPoly*, SetLineF2 and friends give a primitive: the kind alone, opaque and modulated. */
	inline constexpr uint8 FlatQuad = 0x28;
	inline constexpr uint8 TexturedQuad = 0x2C;
	inline constexpr uint8 GouraudTriangle = 0x30;
	inline constexpr uint8 GouraudQuad = 0x38;
	inline constexpr uint8 GouraudTexturedQuad = 0x3C;
	inline constexpr uint8 FlatLine = 0x40;
	/** A texture page's bit that makes its semi-transparent pixels blend as B - F (with 0x20 clear) instead of (B + F) / 2. */
	inline constexpr uint16 PageSubtract = 0x40;
	inline constexpr uint16 PageAdd = 0x20;

	inline void Set(FMenuPrimitive& Primitive, uint8 Code)
	{
		Primitive.Code = Code;
		Primitive.LinePoints = Code == FlatLine ? 2 : 0;
	}

	inline void SetSemiTrans(FMenuPrimitive& Primitive, bool bOn)
	{
		Primitive.Code = static_cast<uint8>(bOn ? Primitive.Code | MenuGpu::SemiTransparent : Primitive.Code & ~MenuGpu::SemiTransparent);
	}

	inline void SetShadeTex(FMenuPrimitive& Primitive, bool bRaw)
	{
		Primitive.Code = static_cast<uint8>(bRaw ? Primitive.Code | MenuGpu::RawTexture : Primitive.Code & ~MenuGpu::RawTexture);
	}

	inline void SetColor(FMenuPrimitive& Primitive, int32 Corner, uint8 R, uint8 G, uint8 B)
	{
		Primitive.Corners[static_cast<std::size_t>(Corner)].Color = {R, G, B};
	}

	/** libgpu's texture window word (`SetDrawMode`'s second): the window's offset and mask in 8-texel steps. */
	constexpr uint32 TextureWindow(const FVramRect& Window)
	{
		const uint32 OffsetX = static_cast<uint32>(static_cast<uint8>(Window.X) >> 3);
		const uint32 OffsetY = static_cast<uint32>(static_cast<uint8>(Window.Y) >> 3);
		const uint32 MaskX = static_cast<uint32>((-Window.Width) & 0xFF) >> 3;
		const uint32 MaskY = static_cast<uint32>((-Window.Height) & 0xFF) >> 3;
		return 0xE2000000u | OffsetY << 15 | OffsetX << 10 | MaskY << 5 | MaskX;
	}

	/** libgpu's SetDrawMode without dithering or drawing into the display: the page and the texture window. */
	inline void SetDrawMode(FMenuPrimitive& Primitive, uint16 Page, const FVramRect& Window)
	{
		Primitive.Code = MenuGpu::DrawModeCommand;
		Primitive.Words = {MenuGpu::DrawMode(Page), TextureWindow(Window)};
	}

	/** A quad's four corners: top left, top right, bottom left, bottom right. */
	inline void SetCorners(FMenuPrimitive& Primitive, int32 X0, int32 Y0, int32 X1, int32 Y1, int32 X2, int32 Y2, int32 X3, int32 Y3)
	{
		const std::array<int32, 8> Points{X0, Y0, X1, Y1, X2, Y2, X3, Y3};
		for (std::size_t Corner = 0; Corner < 4; ++Corner)
		{
			Primitive.Corners[Corner].X = static_cast<int16>(Points[2 * Corner]);
			Primitive.Corners[Corner].Y = static_cast<int16>(Points[2 * Corner + 1]);
		}
	}

	inline void SetTexels(FMenuPrimitive& Primitive, int32 U0, int32 V0, int32 U1, int32 V1, int32 U2, int32 V2, int32 U3, int32 V3)
	{
		const std::array<int32, 8> Texels{U0, V0, U1, V1, U2, V2, U3, V3};
		for (std::size_t Corner = 0; Corner < 4; ++Corner)
		{
			Primitive.Corners[Corner].U = static_cast<uint8>(Texels[2 * Corner]);
			Primitive.Corners[Corner].V = static_cast<uint8>(Texels[2 * Corner + 1]);
		}
	}

	/** A quad over Width x Height texels from (U, V), placed at (X, Y); the texels wrap within their page. */
	inline void SetQuad(FMenuPrimitive& Quad, int16 X, int16 Y, uint8 U, uint8 V, int16 Width, int16 Height)
	{
		const int16 Right = static_cast<int16>(X + Width);
		const int16 Bottom = static_cast<int16>(Y + Height);
		SetCorners(Quad, X, Y, Right, Y, X, Bottom, Right, Bottom);
		SetTexels(Quad, U, V, U + Width, V, U, V + Height, U + Width, V + Height);
	}

	/** A text row's quad: Width texels of the 13-row text at (U, V), placed at (X, Y). */
	inline void SetTextQuad(FMenuPrimitive& Quad, int16 X, int16 Y, uint8 U, uint8 V, uint8 Width)
	{
		constexpr int16 Rows = 13;
		const int16 Right = static_cast<int16>(X + Width);
		const int16 Bottom = static_cast<int16>(Y + Rows);
		SetCorners(Quad, X, Y, Right, Y, X, Bottom, Right, Bottom);
		SetTexels(Quad, U, V, U + Width, V, U, V + Rows, U + Width, V + Rows);
	}
}
