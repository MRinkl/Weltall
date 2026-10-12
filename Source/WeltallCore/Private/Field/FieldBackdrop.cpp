#include "Field/FieldBackdrop.h"

#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"

#include <algorithm>
#include <cmath>

namespace Weltall
{
	namespace
	{
		/** The panorama's screen: 320 x 240, whatever the field's. */
		constexpr int32 ScreenHeight = 240;
		/** At most this many quads a strip across the screen, as the panorama keeps them. */
		constexpr int32 StripQuads = 8;
		/** A continuation of the strip past the screen stops after this many quads. */
		constexpr int32 MoreQuads = 64;
		/** The zoom's unit: 256 draws the strip texel for pixel. */
		constexpr int32 ZoomUnit = 0x100;

		/** A signed division by 2^Shift rounded toward zero, as the compiled code divides. */
		int32 Divide(int32 Value, int32 Shift)
		{
			return (Value < 0 ? Value + ((1 << Shift) - 1) : Value) >> Shift;
		}

		int32 Wrap(int64 Value)
		{
			return static_cast<int32>(static_cast<uint32>(static_cast<uint64>(Value)));
		}

		/** libgpu's GetTPage. */
		uint16 TexturePage(int32 Depth, int32 X, int32 Y)
		{
			return static_cast<uint16>((Depth & 3) << 7 | (Y & 0x100) >> 4 | (X & 0x3FF) >> 6 | (Y & 0x200) << 2);
		}

		/** A strip being laid across the screen: where the next quad starts on the screen and in the strip. */
		struct FStripCursor
		{
			int32 X = 0;
			int32 U = 0;
		};

		/**
		 * The kernel's strip from Cursor rightward to Limit, at most MaxQuads quads, each within one
		 * texture page's reach of 256 texels and ending where the strip wraps; Cursor is left where the
		 * next would start. Returns false when the limit was not reached.
		 */
		bool LinkStrip(const FFieldBackdrop& Backdrop, int32 Top, int32 Limit, int32 MaxQuads, FStripCursor& Cursor, std::vector<FFieldBackdropQuad>& Quads)
		{
			const FFieldPanorama& Panorama = Backdrop.Panorama;
			const int32 Width = Panorama.Width;
			const int32 Depth = Panorama.TexturePage;
			const int32 Scale = Backdrop.Zoom + ZoomUnit;
			const int32 PageColumn = Panorama.VramX - Divide(Panorama.VramX, 6) * 64;
			const int32 PageTexels = (0x100 >> (Depth & 0x1F)) - 1;
			const uint8 Row = static_cast<uint8>(Panorama.VramY - Divide(Panorama.VramY, 8) * 0x100);
			const uint8 LastRow = static_cast<uint8>(Row + static_cast<uint8>(Panorama.Height));
			const uint16 Clut = static_cast<uint16>(Panorama.ClutY << 6 | (Panorama.ClutX >> 4 & 0x3F));
			for (int32 Quad = 0; Quad < MaxQuads; ++Quad)
			{
				const int16 First = static_cast<int16>(Cursor.U);
				const uint32 U = static_cast<uint32>(Cursor.U + (PageColumn << ((2 - Depth) & 0x1F) & 0xFFFF)) & static_cast<uint32>(PageTexels);
				int32 Texels = 0x100 - static_cast<int32>(U);
				if (Width < First + static_cast<int16>(Texels))
				{
					Texels = static_cast<uint16>(Width) - Cursor.U;
				}
				int16 Columns = static_cast<int16>(Texels);
				int32 Pixels = (static_cast<int16>(Texels) << 8) / Scale;
				if (Limit < static_cast<int16>(Cursor.X) + static_cast<int16>(Pixels))
				{
					Pixels = Limit - Cursor.X;
					Columns = static_cast<int16>(Divide(static_cast<int16>(Pixels) * Scale, 8));
				}
				const int32 Next = static_cast<int16>(First + Columns) % Width;
				const int32 Right = Cursor.X + Pixels;
				FFieldBackdropQuad& Out = Quads.emplace_back();
				Out.Kind = FFieldBackdropQuad::EKind::Textured;
				const int16 Left = static_cast<int16>(Cursor.X);
				const int16 Bottom = static_cast<int16>(Backdrop.HorizonY);
				const int16 TopY = static_cast<int16>(Backdrop.HorizonY - Top);
				Out.Corners = {{{Left, TopY}, {static_cast<int16>(Right), TopY}, {Left, Bottom}, {static_cast<int16>(Right), Bottom}}};
				const uint8 FirstU = static_cast<uint8>(U);
				const uint8 LastU = static_cast<uint8>(Columns + FirstU - 1);
				Out.Texels = {{{FirstU, Row}, {LastU, Row}, {FirstU, LastRow}, {LastU, LastRow}}};
				const int32 PageX = Divide(static_cast<int16>(Panorama.VramX + (First >> ((2 - Depth) & 0x1F))), 6) << 6;
				Out.TexturePage = TexturePage(Depth, PageX, Divide(Panorama.VramY, 8) << 8);
				Out.Clut = Clut;
				Cursor.X = Right;
				Cursor.U = Next;
				if (static_cast<int16>(Right) >= Limit)
				{
					return true;
				}
			}
			return false;
		}

		FFieldBackdropQuad Band(int32 Left, int32 Right, int32 Top, int32 Bottom, const std::array<uint8, 3>& Upper, const std::array<uint8, 3>& Lower,
			bool bGraded)
		{
			FFieldBackdropQuad Quad;
			Quad.Kind = bGraded ? FFieldBackdropQuad::EKind::Graded : FFieldBackdropQuad::EKind::Flat;
			const int16 X0 = static_cast<int16>(Left);
			const int16 X1 = static_cast<int16>(Right);
			Quad.Corners = {{{X0, static_cast<int16>(Top)}, {X1, static_cast<int16>(Top)}, {X0, static_cast<int16>(Bottom)}, {X1, static_cast<int16>(Bottom)}}};
			Quad.Colors = {Upper, Upper, Lower, Lower};
			return Quad;
		}
	}

	FFieldBackdrop PlacePanorama(const FFieldPanorama& Made, const FGteVector& From, const FGteVector& To, const FGte& Gte, const FGteArctangents& Arctangents)
	{
		FFieldBackdrop Backdrop;
		Backdrop.bShown = true;
		Backdrop.Panorama = Made;
		const int32 AcrossX = To[0] - From[0];
		const int32 AcrossZ = To[2] - From[2];
		const FGteVector Heading = VectorNormalS({AcrossX, 0, AcrossZ});
		const int32 Distance = static_cast<int16>(Made.Position[2]);
		const int16 Height = static_cast<int16>(Made.Position[1]);
		// The horizon's point: Distance ahead of the target along the view's heading, at its height.
		const int32 AheadX = Divide(Heading[0] * Distance, 12);
		const int32 AheadZ = Divide(Heading[2] * Distance, 12);
		Backdrop.HorizonY = Gte.Rtps({static_cast<int16>(static_cast<int16>(AheadX) + To[0]), Height, static_cast<int16>(static_cast<int16>(AheadZ) + To[2])}).ScreenY;
		if (Made.DepthDivisor != 0)
		{
			const int32 AcrossY = To[1] - From[1];
			const int32 Length = SquareRoot0(Wrap(static_cast<int64>(AcrossX) * AcrossX + static_cast<int64>(AcrossY) * AcrossY + static_cast<int64>(AcrossZ) * AcrossZ));
			const int16 Zoom = static_cast<int16>((Length - Made.DepthOffset) / Made.DepthDivisor);
			Backdrop.Zoom = std::clamp<int32>(Zoom, 0, ZoomUnit);
		}
		const int32 Turn = GteRatan2(AcrossX, AcrossZ, Arctangents) & 0xFFF;
		const int32 Scroll = Made.Position[2] < 0 ? -Made.Scroll : Made.Scroll;
		Backdrop.ScrollU = Divide(Wrap(static_cast<int64>(Made.Width) * Scroll * Turn), 12);
		// The graded band ends at the horizon's point scaled by the band scale (8.8).
		const FGteVector BandPoint{static_cast<int16>(static_cast<int16>(Divide(AheadX * Made.BandScale, 8)) + To[0]),
			static_cast<int16>(Divide(Height * Made.BandScale, 8)), static_cast<int16>(static_cast<int16>(Divide(AheadZ * Made.BandScale, 8)) + To[2])};
		Backdrop.BandY = Gte.Rtps(BandPoint).ScreenY;
		return Backdrop;
	}

	FFieldBackdrop PlaceFieldBackdrop(const FFieldPanorama& Made, bool bHidden, const FFieldPoint& Eye, const FFieldPoint& Target, const FFieldView& View,
		const FGteArctangents& Arctangents)
	{
		if (!Made.bEnabled || bHidden)
		{
			return FFieldBackdrop{};
		}
		// The camera's whole units, as the kernel takes its points.
		FGteVector From{}, To{};
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			From[Axis] = static_cast<int16>(Eye[Axis] >> 16);
			To[Axis] = static_cast<int16>(Target[Axis] >> 16);
		}
		FGte Gte;
		Gte.OffsetX = FieldScreenCenterX << 16;
		Gte.OffsetY = FieldScreenCenterY << 16;
		Gte.ProjectionDistance = View.ProjectionDistance;
		Gte.Transform = View.View;
		return PlacePanorama(Made, From, To, Gte, Arctangents);
	}

	FFieldBackdrop BlendFieldBackdrop(const FFieldBackdrop& Before, const FFieldBackdrop& Latest, float Alpha)
	{
		FFieldBackdrop Backdrop = Latest;
		if (!Before.bShown || !Latest.bShown)
		{
			return Backdrop;
		}
		const int32 Width = std::max<int32>(Latest.Panorama.Width, 1);
		int32 Turn = (Latest.ScrollU - Before.ScrollU) % Width;
		Turn += Turn > Width / 2 ? -Width : Turn < -Width / 2 ? Width : 0;
		const auto Between = [Alpha](int32 From, int32 To)
		{ return static_cast<int32>(std::lround(static_cast<float>(From) + static_cast<float>(To - From) * Alpha)); };
		Backdrop.ScrollU = Before.ScrollU + static_cast<int32>(std::lround(static_cast<float>(Turn) * Alpha));
		Backdrop.HorizonY = Between(Before.HorizonY, Latest.HorizonY);
		Backdrop.BandY = Between(Before.BandY, Latest.BandY);
		Backdrop.Zoom = Between(Before.Zoom, Latest.Zoom);
		return Backdrop;
	}

	void DrawFieldBackdrop(const FFieldBackdrop& Backdrop, int32 Left, int32 Right, std::vector<FFieldBackdropQuad>& Quads)
	{
		Quads.clear();
		if (!Backdrop.bShown)
		{
			return;
		}
		const FFieldPanorama& Panorama = Backdrop.Panorama;
		const int32 Width = Panorama.Width;
		const int32 Scale = Backdrop.Zoom + ZoomUnit;
		// The strip, centred when the zoom leaves it narrower than the screen, and linked first.
		const int32 Centring = (FieldBackdropWidth - (Width << 8) / Scale) / 2;
		const int32 Start = static_cast<int16>(Backdrop.ScrollU - (Centring + Divide(Centring * Backdrop.Zoom, 8)));
		int32 FirstU = Start % Width;
		if (static_cast<int16>(FirstU) < 0)
		{
			FirstU += static_cast<uint16>(Width);
		}
		int32 Top = 0;
		const int16 Y = static_cast<int16>(Backdrop.HorizonY);
		if (Y >= 0 && Y <= Panorama.Height + ScreenHeight)
		{
			Top = (Panorama.Height << 8) / Scale;
		}
		if (static_cast<int16>(Top) > 0)
		{
			FStripCursor Cursor{0, FirstU};
			const bool bAcross = LinkStrip(Backdrop, Top, FieldBackdropWidth, StripQuads, Cursor, Quads);
			// A strip the view turns goes round with it; one fixed to the screen is a picture of the screen alone.
			const bool bGoesOn = Panorama.Scroll != 0;
			if (bGoesOn && Right > FieldBackdropWidth)
			{
				// Past the screen the strip goes on from where the screen's column would take it.
				if (!bAcross)
				{
					Cursor.U = static_cast<int32>((Cursor.U + (((FieldBackdropWidth - Cursor.X) * Scale) >> 8)) % Width);
					Cursor.X = FieldBackdropWidth;
				}
				LinkStrip(Backdrop, Top, Right, MoreQuads, Cursor, Quads);
			}
			if (bGoesOn && Left < 0)
			{
				const int32 Texels = (-Left * Scale + ZoomUnit - 1) >> 8;
				FStripCursor Before{Left, ((FirstU - Texels) % Width + Width) % Width};
				LinkStrip(Backdrop, Top, 0, MoreQuads, Before, Quads);
			}
		}
		if (!Panorama.bBands)
		{
			std::reverse(Quads.begin(), Quads.end());
			return;
		}
		// Then the bands: the flat one above the strip, the graded one from the horizon down to its end,
		// and the flat one below that.
		const int32 Upper = std::min<int32>(static_cast<int16>(Backdrop.HorizonY - Panorama.Height), ScreenHeight);
		if (Upper > 0)
		{
			Quads.push_back(Band(Left, Right, 0, Upper, Panorama.Colors[0], Panorama.Colors[0], false));
		}
		const int32 Horizon = static_cast<int16>(Backdrop.HorizonY);
		int32 End = static_cast<int16>(Backdrop.BandY);
		if (End - Horizon > ScreenHeight)
		{
			End = static_cast<int16>(Horizon + ScreenHeight);
		}
		int32 Lower = 0;
		if (End >= 0)
		{
			if (Horizon < ScreenHeight)
			{
				Quads.push_back(Band(Left, Right, Horizon, End, Panorama.Colors[1], Panorama.Colors[2], true));
			}
			Lower = End;
		}
		if (Lower < ScreenHeight)
		{
			Quads.push_back(Band(Left, Right, Lower, ScreenHeight, Panorama.Colors[2], Panorama.Colors[2], false));
		}
		// Each was linked at the head of the entry: the last is drawn first.
		std::reverse(Quads.begin(), Quads.end());
	}
}
