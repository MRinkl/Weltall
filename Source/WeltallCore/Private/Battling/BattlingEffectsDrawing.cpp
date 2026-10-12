#include "Battling/BattlingDrawing.h"

#include "Battling/BattlingFight.h"
#include "Graphics/Gte.h"
#include "Graphics/GteVectors.h"
#include "Graphics/SpriteDrawing.h"

#include <algorithm>
#include <cmath>

namespace Weltall
{
	namespace
	{
		constexpr int32 BucketShift = 4;
		/** A sprite is linked this much nearer than its depth, once it lies that deep. */
		constexpr int32 SpriteNearer = 0x40;
		/** A trail's or a band's brightness, which fades by 8 a frame. */
		constexpr int32 TrailBrightness = 0x40;
		constexpr int32 TrailFade = 8;
		constexpr FDrawColor DustColour{0xF0, 0xB0, 0x80};
		constexpr FDrawColor SplashColour{0xA0, 0xD0, 0xFF};
		constexpr FDrawColor SparkColour{0x80, 0x80, 0xC0};
		/** The dust and the splashes are projected three at a time: the last one or two of a frame are not drawn. */
		constexpr int32 ProjectedTogether = 3;

		/** A point about the camera's target, 16 bits a component as the program keeps it. */
		FGteVector AboutTarget(const std::array<int16, 3>& Point, const FBattlingPoint& Target)
		{
			return {static_cast<int16>(Point[0] - Target[0]), static_cast<int16>(Point[1] - Target[1]), static_cast<int16>(Point[2] - Target[2])};
		}

		/** A point moved by an offset the GTE worked out, 16 bits of which the program keeps. */
		FGteVector Moved(const FGteVector& Point, const FGteLongVector& Offset)
		{
			return {static_cast<int16>(Point[0] + Offset[0]), static_cast<int16>(Point[1] + Offset[1]), static_cast<int16>(Point[2] + Offset[2])};
		}

		/** The R3000's division, which a divisor of 0 does not stop: it gives -1 for a dividend of 0 or more, else 1. */
		int32 Divide(int32 Dividend, int32 Divisor)
		{
			if (Divisor == 0)
			{
				return Dividend >= 0 ? -1 : 1;
			}
			return Dividend / Divisor;
		}

		/** The low 32 bits of a product, as the R3000's mflo keeps them. */
		int32 Low32(int64 Value)
		{
			return static_cast<int32>(static_cast<uint32>(static_cast<uint64>(Value)));
		}

		/** Each effect as a polygon of the frame: where the view puts its corners and where the GTE put them on the original's screen. */
		class FEffectsDraw
		{
		public:
			FEffectsDraw(FWorldMapFrameBuilder& InBuilder, const FGte& InView)
				: Builder(InBuilder), View(InView), PerPixel(1.0f / static_cast<float>(std::max<int32>(InView.ProjectionDistance, 1)))
			{
			}

			FWorldMapPolygon& Add(int32 Depth)
			{
				FWorldMapPolygon& Polygon = Builder.Add(static_cast<uint16>(Depth >> BucketShift));
				Polygon.Layer = EWorldMapLayer::Effect;
				Polygon.CornerCount = 4;
				return Polygon;
			}

			std::array<float, 3> ViewOf(const FGteVector& Point) const { return ViewPoint(View.Transform, Point); }

			/** A view point moved across the original's screen by pixels at its own depth. */
			std::array<float, 3> Across(const std::array<float, 3>& Point, float Right, float Down) const
			{
				return {Point[0] + Right * Point[2] * PerPixel, Point[1] + Down * Point[2] * PerPixel, Point[2]};
			}

			/** A tile: a rectangle of the screen hanging from the point's pixel. */
			void Rectangle(const FGteVector& Point, int16 Width, int16 Height, const FDrawColor& Colour, bool bSemiTransparent)
			{
				const FGteProjection At = View.Rtps(Point);
				FWorldMapPolygon& Polygon = Add(At.ScreenZ);
				Polygon.Primitive = EWorldMapPrimitive::Rectangle;
				Polygon.bSemiTransparent = bSemiTransparent;
				Polygon.Colors.fill(Colour);
				const std::array<float, 3> Corner = ViewOf(Point);
				for (std::size_t Index = 0; Index < 4; ++Index)
				{
					const int16 Right = (Index & 1) != 0 ? Width : int16{0};
					const int16 Down = (Index & 2) != 0 ? Height : int16{0};
					Polygon.ScreenX[Index] = static_cast<int16>(At.ScreenX + Right);
					Polygon.ScreenY[Index] = static_cast<int16>(At.ScreenY + Down);
					Polygon.View[Index] = Across(Corner, Right, Down);
				}
			}

			/** A line a pixel wide, linked by its second end's depth. */
			void Line(const FGteVector& From, const FGteVector& To, const FDrawColor& Colour)
			{
				const FGteProjection AtFrom = View.Rtps(From);
				const FGteProjection AtTo = View.Rtps(To);
				FWorldMapPolygon& Polygon = Add(AtTo.ScreenZ);
				Polygon.Primitive = EWorldMapPrimitive::Line;
				Polygon.Colors.fill(Colour);
				Polygon.ScreenX = {AtFrom.ScreenX, AtTo.ScreenX, AtFrom.ScreenX, AtTo.ScreenX};
				Polygon.ScreenY = {AtFrom.ScreenY, AtTo.ScreenY, AtFrom.ScreenY, AtTo.ScreenY};
				// On the screen: half a pixel to either side of it and past either end.
				const std::array<float, 3> Start = ViewOf(From);
				const std::array<float, 3> End = ViewOf(To);
				float AlongX = 1.0f;
				float AlongY = 0.0f;
				if (Start[2] > 0.0f && End[2] > 0.0f)
				{
					const float X = End[0] / End[2] - Start[0] / Start[2];
					const float Y = End[1] / End[2] - Start[1] / Start[2];
					const float Length = std::sqrt(X * X + Y * Y);
					if (Length > 0.0f)
					{
						AlongX = X / Length;
						AlongY = Y / Length;
					}
				}
				const float SideX = -AlongY * 0.5f;
				const float SideY = AlongX * 0.5f;
				Polygon.View = {Across(Start, -AlongX * 0.5f - SideX, -AlongY * 0.5f - SideY), Across(Start, -AlongX * 0.5f + SideX, -AlongY * 0.5f + SideY),
					Across(End, AlongX * 0.5f - SideX, AlongY * 0.5f - SideY), Across(End, AlongX * 0.5f + SideX, AlongY * 0.5f + SideY)};
			}

			FWorldMapFrameBuilder& Builder;
			const FGte& View;
			float PerPixel;
		};

		/** A quad blending onto the screen with one texel of the trails' picture across it. */
		void SetTrailTexel(FWorldMapPolygon& Polygon, const FBattlingArenaTextures& Textures, uint32 Side, int32 Frame)
		{
			Polygon.bTextured = true;
			Polygon.bSemiTransparent = true;
			Polygon.TexturePage = Textures.TrailPage;
			Polygon.Clut = Textures.TrailClut;
			const uint16 Texel = Textures.TrailTexels[Side & 1];
			const uint8 U = static_cast<uint8>(Texel);
			const uint8 V = static_cast<uint8>(Texel >> 8);
			Polygon.U.fill(U);
			Polygon.V.fill(V);
			Polygon.TexCoords.fill({static_cast<float>(U) + 0.5f, static_cast<float>(V) + 0.5f});
			Polygon.TexelMin = {U, V};
			Polygon.TexelMax = {U, V};
			const uint8 Brightness = static_cast<uint8>(TrailBrightness - TrailFade * Frame);
			Polygon.Colors.fill({Brightness, Brightness, Brightness});
		}
	}

	void FBattlingDrawing::DrawEffects(const FBattlingFight& Fight, const FGte& View, bool bSparksAndLinesAlone)
	{
		const FBattlingEffects& Effects = Fight.Effects;
		const FBattlingPoint& Target = Fight.Target;
		const FGteMatrix Facing = Transpose(View.Transform);
		FEffectsDraw Draw(Builder, View);
		const auto DrawLines = [&]() {
			for (int32 Number = 0; Number < Effects.LinesShown; ++Number)
			{
				const FBattlingLine& Line = Effects.Lines[static_cast<std::size_t>(Number)];
				Draw.Line(AboutTarget(Line.From, Target), AboutTarget(Line.To, Target), {Line.Colour[0], Line.Colour[1], Line.Colour[2]});
			}
		};
		// The hit sparks, each a small tile blending onto the screen.
		const auto DrawSparks = [&]() {
			for (std::size_t Number = 0; Number < Effects.Sparks.size(); ++Number)
			{
				if (Effects.SparksDrawn[Number])
				{
					const FBattlingSpark& Spark = Effects.Sparks[Number];
					Draw.Rectangle(AboutTarget(Spark.Position, Target), Spark.Width, Spark.Height, SparkColour, true);
				}
			}
		};
		if (bSparksAndLinesAlone)
		{
			DrawSparks();
			DrawLines();
			return;
		}

		// The sprites: squares facing the screen about their points, by their set's size.
		for (const FBattlingEffect& Slot : Effects.Slots)
		{
			if (!Slot.bActive || Slot.Kind != EBattlingEffectKind::Sprite)
			{
				continue;
			}
			const FBattlingSpriteSet& Set = SpriteSets[Slot.Set];
			const FGteVector Point = AboutTarget(Slot.Position, Target);
			const FGteVector First = Moved(Point, ApplyMatrix(Facing, {static_cast<int16>(-Set.Half), static_cast<int16>(-Set.Half), 0}));
			const FGteVector Last = Moved(Point, ApplyMatrix(Facing, {Set.Half, Set.Half, 0}));
			const FGteProjection AtFirst = View.Rtps(First);
			const FGteProjection AtLast = View.Rtps(Last);
			const int32 Depth = AtLast.ScreenZ > SpriteNearer ? AtLast.ScreenZ - SpriteNearer : AtLast.ScreenZ;
			FWorldMapPolygon& Polygon = Draw.Add(Depth);
			Polygon.bTextured = true;
			Polygon.bSemiTransparent = true;
			Polygon.bRaw = Set.bRaw;
			// Effects are added before the tick counts their frames on: the first frame drawn is 1.
			const std::size_t Frame = static_cast<std::size_t>(Slot.Frame - 1) % Set.Texels.size();
			Polygon.TexturePage = Set.Page;
			Polygon.Clut = Set.Cluts[Frame];
			Polygon.Colors.fill(Set.bRaw ? FDrawColor{0x80, 0x80, 0x80} : FDrawColor{Slot.Colour[0], Slot.Colour[1], Slot.Colour[2]});
			const uint8 U = static_cast<uint8>(Set.Texels[Frame]);
			const uint8 V = static_cast<uint8>(Set.Texels[Frame] >> 8);
			const std::array<float, 3> TopLeft = Draw.ViewOf(First);
			std::array<float, 3> BottomRight = Draw.ViewOf(Last);
			if (TopLeft[2] > 0.0f && BottomRight[2] > 0.0f)
			{
				// Both corners at the first's depth, where the screen shows them.
				BottomRight = {BottomRight[0] * TopLeft[2] / BottomRight[2], BottomRight[1] * TopLeft[2] / BottomRight[2], TopLeft[2]};
			}
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				const bool bRight = (Corner & 1) != 0;
				const bool bDown = (Corner & 2) != 0;
				Polygon.ScreenX[Corner] = bRight ? AtLast.ScreenX : AtFirst.ScreenX;
				Polygon.ScreenY[Corner] = bDown ? AtLast.ScreenY : AtFirst.ScreenY;
				Polygon.View[Corner] = {bRight ? BottomRight[0] : TopLeft[0], bDown ? BottomRight[1] : TopLeft[1], TopLeft[2]};
				Polygon.U[Corner] = static_cast<uint8>(U + (bRight ? Set.Size : 0));
				Polygon.V[Corner] = static_cast<uint8>(V + (bDown ? Set.Size : 0));
				Polygon.TexCoords[Corner] = {static_cast<float>(U) + (bRight ? Set.Size : 0), static_cast<float>(V) + (bDown ? Set.Size : 0)};
			}
			Polygon.TexelMin = {U, V};
			Polygon.TexelMax = {static_cast<uint8>(U + Set.Size - 1), static_cast<uint8>(V + Set.Size - 1)};
		}

		// The trails: each tick's segment a short bar across the point's path on the screen, drawn on to the segment before.
		for (std::size_t Index = 0; Index < Effects.Slots.size(); ++Index)
		{
			const FBattlingEffect& Slot = Effects.Slots[Index];
			if (!Slot.bActive || Slot.Kind != EBattlingEffectKind::Trail)
			{
				continue;
			}
			FTrailMark& Mark = TrailMarks[Index];
			const FGteVector Point = AboutTarget(Slot.Position, Target);
			const FGteProjection AtPoint = View.Rtps(Point);
			Mark.Point = {AtPoint.ScreenX, AtPoint.ScreenY};
			FGteVector Across{0, static_cast<int16>(-Slot.Length), 0};
			if (Slot.Link >= 0)
			{
				// Across the screen from the segment before: the bar is as long as the trail's length on either side.
				const FTrailMark& Before = TrailMarks[static_cast<std::size_t>(Slot.Link)];
				const int32 X = Mark.Point[0] - Before.Point[0];
				const int32 Y = Mark.Point[1] - Before.Point[1];
				const int32 Distance = SquareRoot0(X * X + Y * Y);
				const int16 AcrossX = static_cast<int16>(Divide(X * Slot.Length, Distance));
				const int16 AcrossY = static_cast<int16>(Divide(Y * Slot.Length, Distance));
				Across = {AcrossY, static_cast<int16>(-AcrossX), 0};
			}
			const FGteVector Back{static_cast<int16>(-Across[0]), static_cast<int16>(-Across[1]), 0};
			const FGteVector First = Moved(Point, ApplyMatrix(Facing, Across));
			const FGteVector Second = Moved(Point, ApplyMatrix(Facing, Back));
			const FGteProjection AtFirst = View.Rtps(First);
			const FGteProjection AtSecond = View.Rtps(Second);
			const FTrailMark Previous = Slot.Link >= 0 ? TrailMarks[static_cast<std::size_t>(Slot.Link)] : FTrailMark{};
			Mark.Corners = {{{AtFirst.ScreenX, AtFirst.ScreenY}, {AtSecond.ScreenX, AtSecond.ScreenY}}};
			Mark.View = {Draw.ViewOf(First), Draw.ViewOf(Second)};
			if (Slot.Link < 0)
			{
				continue;
			}
			// The segment before's corners in the order that keeps the quad from crossing itself: by the side of the line
			// through both points each first corner lies on (in the R3000's 32 bits).
			const auto Side = [&](const std::array<int16, 2>& Corner) {
				return Low32(int64{Previous.Point[1] - Mark.Point[1]} * Corner[0] + int64{Mark.Point[0] - Previous.Point[0]} * Corner[1]
					+ int64{Previous.Point[0]} * Mark.Point[1] - int64{Mark.Point[0]} * Previous.Point[1]);
			};
			const bool bOpposite = Low32(int64{Side(Mark.Corners[0])} * Side(Previous.Corners[0])) < 1;
			const std::size_t Third = bOpposite ? 1 : 0;
			FWorldMapPolygon& Polygon = Draw.Add(AtSecond.ScreenZ);
			SetTrailTexel(Polygon, Textures, Slot.Side, Slot.Frame);
			Polygon.ScreenX = {AtFirst.ScreenX, AtSecond.ScreenX, Previous.Corners[Third][0], Previous.Corners[1 - Third][0]};
			Polygon.ScreenY = {AtFirst.ScreenY, AtSecond.ScreenY, Previous.Corners[Third][1], Previous.Corners[1 - Third][1]};
			Polygon.View = {Mark.View[0], Mark.View[1], Previous.View[Third], Previous.View[1 - Third]};
		}

		// The bands: the quad between this tick's two points and the tick before's.
		for (const FBattlingEffect& Slot : Effects.Slots)
		{
			if (!Slot.bActive || Slot.Kind != EBattlingEffectKind::Swept || Slot.Link < 0)
			{
				continue;
			}
			const FBattlingEffect& Before = Effects.Slots[static_cast<std::size_t>(Slot.Link)];
			const std::array<FGteVector, 4> Corners{AboutTarget(Slot.Position, Target), AboutTarget(Slot.End, Target), AboutTarget(Before.Position, Target),
				AboutTarget(Before.End, Target)};
			std::array<FGteProjection, 4> At;
			for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
			{
				At[Corner] = View.Rtps(Corners[Corner]);
			}
			FWorldMapPolygon& Polygon = Draw.Add(At[2].ScreenZ);
			SetTrailTexel(Polygon, Textures, Slot.Side, Slot.Frame);
			for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
			{
				Polygon.ScreenX[Corner] = At[Corner].ScreenX;
				Polygon.ScreenY[Corner] = At[Corner].ScreenY;
				Polygon.View[Corner] = Draw.ViewOf(Corners[Corner]);
			}
		}

		// The dust and the splashes as the drawing's steps left them, a pixel and a few pixels across.
		for (int32 Grain = 0; Grain < Effects.DrawnDustCount - Effects.DrawnDustCount % ProjectedTogether; ++Grain)
		{
			Draw.Rectangle(AboutTarget(Effects.DrawnDust[static_cast<std::size_t>(Grain)], Target), 1, 1, DustColour, false);
		}
		for (int32 Drop = 0; Drop < Effects.DrawnSplashCount - Effects.DrawnSplashCount % ProjectedTogether; ++Drop)
		{
			const FBattlingSplash& Tile = Effects.Splashes[static_cast<std::size_t>(Drop)];
			Draw.Rectangle(AboutTarget(Effects.DrawnSplashes[static_cast<std::size_t>(Drop)], Target), Tile.Width, Tile.Height, SplashColour, false);
		}
		DrawLines();
		DrawSparks();
	}
}
