#include "Support/FieldScreenBytes.h"

#include <array>

namespace Weltall::Testing
{
	namespace
	{
		/** The GPU's codes and the library's CLUT and page words the field gives its markers and its cockpit's quads. */
		constexpr uint8 SpriteCode = 0x64;
		constexpr uint8 SemiTransparentQuadCode = 0x2E;
		constexpr uint16 MarkerClut = 0xF7 << 6 | 0x100 >> 4;
		constexpr uint16 HudClut = FFieldCockpitHud::ClutY << 6;
		constexpr uint16 HudPage = FFieldCockpitHud::ImageX >> 6;
		constexpr uint8 Neutral = 0x80;

		void Add(std::vector<uint8>& Bytes, uint32 Value, int32 Size)
		{
			for (int32 Byte = 0; Byte < Size; ++Byte)
			{
				Bytes.push_back(static_cast<uint8>(Value >> (8 * Byte)));
			}
		}

		void AddRect(std::vector<uint8>& Bytes, const FVramRect& Rect)
		{
			for (const int16 Value : {Rect.X, Rect.Y, Rect.Width, Rect.Height})
			{
				Add(Bytes, static_cast<uint16>(Value), 2);
			}
		}
	}

	std::vector<uint8> ScreenPieceBytes(const FFieldRenderState& Render, const FFieldCockpitHudLayout* Layout)
	{
		std::vector<uint8> Bytes;
		for (const FFieldVramOperation& Operation : Render.VramOperations)
		{
			switch (Operation.Kind)
			{
			case EFieldVramChange::Upload:
			{
				Bytes.push_back('U');
				AddRect(Bytes, Operation.Rect);
				const std::size_t Units = Operation.Rect.Width > 0 && Operation.Rect.Height > 0 ? static_cast<std::size_t>(Operation.Rect.Width * Operation.Rect.Height) : 0;
				for (std::size_t Unit = 0; Unit < Units; ++Unit)
				{
					Add(Bytes, Render.VramUnits[Operation.UnitsAt + Unit], 2);
				}
				break;
			}
			case EFieldVramChange::SaveClutRows:
				Bytes.push_back('D');
				AddRect(Bytes, Operation.Rect);
				break;
			case EFieldVramChange::FadeClutRow:
				Bytes.push_back('F');
				Add(Bytes, static_cast<uint32>(Operation.Row), 4);
				Add(Bytes, static_cast<uint32>(Operation.Factor), 4);
				break;
			default:
				break;
			}
		}
		if (Render.ClutFade.bLoaded)
		{
			Bytes.push_back('L');
			AddRect(Bytes, Render.ClutFade.Rect);
		}
		const FFieldScreenMarkers& Markers = Render.Markers;
		for (auto Placed = Markers.Placed.rbegin(); Placed != Markers.Placed.rend(); ++Placed)
		{
			const bool bBig = Placed->Marker == 0;
			const std::array<uint8, 2>& Texels = bBig ? FFieldScreenMarkers::BigTexels : FFieldScreenMarkers::SmallTexels;
			const std::array<uint8, 3>& Color = Markers.Colors[Placed->Marker];
			Bytes.insert(Bytes.end(), {'M', Color[0], Color[1], Color[2], SpriteCode});
			Add(Bytes, static_cast<uint16>(Placed->X), 2);
			Add(Bytes, static_cast<uint16>(Placed->Y), 2);
			Bytes.insert(Bytes.end(), {Texels[0], Texels[1]});
			Add(Bytes, MarkerClut, 2);
			Add(Bytes, bBig ? 16 : 8, 2);
			Add(Bytes, bBig ? 16 : 8, 2);
		}
		const FFieldCockpitHud& Hud = Render.CockpitHud;
		if (Hud.bOn && Layout != nullptr)
		{
			std::array<FFieldHudQuad, FFieldCockpitHudLayout::PieceCount> Quads;
			BuildCockpitHudQuads(*Layout, Hud, Quads);
			for (auto Quad = Quads.rbegin(); Quad != Quads.rend(); ++Quad)
			{
				if (!Quad->bDrawn)
				{
					continue;
				}
				Bytes.insert(Bytes.end(), {'H', Neutral, Neutral, Neutral, SemiTransparentQuadCode});
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					Add(Bytes, static_cast<uint16>(Quad->Corners[Corner][0]), 2);
					Add(Bytes, static_cast<uint16>(Quad->Corners[Corner][1]), 2);
					Bytes.insert(Bytes.end(), {Quad->Texels[Corner][0], Quad->Texels[Corner][1]});
				}
				Add(Bytes, HudClut, 2);
				Add(Bytes, static_cast<uint32>(Quad->BlendMode << 5 | HudPage), 2);
			}
			Bytes.push_back('C');
			Add(Bytes, Hud.Counter, 4);
			Add(Bytes, static_cast<uint32>(Hud.Blink), 4);
		}
		return Bytes;
	}
}
