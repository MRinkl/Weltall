#include "Graphics/SpriteDrawing.h"

#include "Graphics/Gte.h"
#include "Graphics/GteVectors.h"
#include "Graphics/Sprite.h"

#include <utility>

namespace Weltall
{
	namespace
	{
		/** A part of anchor group g is linked g entries nearer. */
		constexpr uint32 GroupsSortNearer = 0x8000000;
		constexpr uint32 PartGroupMask = 0x7;
		constexpr uint32 PartFlipsTexelsAcross = 0x10;
		constexpr uint32 PartFlipsTexelsDown = 0x20;
		constexpr uint32 NoGroup = 0xFFFFFFFF;

		using FCorner = std::array<int16, 2>;

		/** A value times a 4.12 scale, rounded toward zero as the kernel rounds it. */
		int32 AtScale(int32 Value, int32 Scale)
		{
			int32 Product = Value * Scale;
			if (Product < 0)
			{
				Product += 0xFFF;
			}
			return Product >> 12;
		}

		FGteVector InPlane(const FCorner& Corner)
		{
			return {Corner[0], Corner[1], 0};
		}

		/**
		 * The kernel's corners v0 to v3 go to the GPU as its corners 0, 1, 3 and 2, with the texels'
		 * first row at v0 and v1 and their first column at v0 and v3.
		 */
		void AddQuad(std::vector<FSpriteQuad>& Quads, const FGteMatrix& Matrix, const std::array<FCorner, 4>& Corners, const FSpritePart& Part,
			int32 FirstRow, int32 Rows, int32 OtIndex)
		{
			FSpriteQuad& Quad = Quads.emplace_back();
			Quad.Matrix = Matrix;
			Quad.Corners = {InPlane(Corners[0]), InPlane(Corners[1]), InPlane(Corners[3]), InPlane(Corners[2])};
			Quad.U0 = Part.U;
			Quad.U1 = static_cast<int16>(Part.U + Part.Width);
			Quad.V0 = static_cast<int16>(FirstRow);
			Quad.V1 = static_cast<int16>(FirstRow + Rows);
			Quad.CellV0 = Part.V;
			Quad.CellV1 = static_cast<int16>(Part.V + Part.Height);
			Quad.TexturePage = Part.TexturePage;
			Quad.Clut = Part.Clut;
			Quad.ColorCode = Part.ColorCode;
			Quad.OtIndex = OtIndex;
		}

		/** The matrix of a part's anchor group: the sprite's, or turned and moved by the group's anchor. */
		FGteMatrix GroupMatrix(const FSprite& Sprite, const FGteMatrix& Matrix, uint32 Group)
		{
			if (!Sprite.bHasAnchors)
			{
				return Matrix;
			}
			const FSpriteAnchor& Anchor = Sprite.Anchors[Group];
			if (Anchor.OffsetX == 0 && Anchor.OffsetY == 0 && Anchor.RotationZ == 0)
			{
				// An anchor turned only about x or y is drawn with the sprite's own matrix.
				return Matrix;
			}
			const bool bMirrored = (Sprite.DrawFlags & SpriteDrawFlags::Mirrored) != 0;
			const int32 Shift = Sprite.SizeShift();
			int32 OffsetX = static_cast<int32>(static_cast<int8>(Anchor.OffsetX)) << Shift;
			if (bMirrored)
			{
				OffsetX = -OffsetX;
			}
			const int32 OffsetY = static_cast<int32>(static_cast<int8>(Anchor.OffsetY)) << Shift;
			const FGteVector Angles = {Anchor.RotationX, Anchor.RotationY, static_cast<int16>(bMirrored ? -Anchor.RotationZ : Anchor.RotationZ)};
			FGteMatrix Result = MulMatrix(Matrix, RotationXyz(Angles));
			Result.T = {Matrix.T[0] + AtScale(OffsetX, Sprite.Scale), Matrix.T[1] + AtScale(OffsetY, Sprite.Scale), Matrix.T[2]};
			return Result;
		}

		/** A part's quad across: its left and right x, mirrored with the sprite and swapped by its own flip. */
		struct FAcross
		{
			int16 First = 0;
			int16 Second = 0;
		};

		FAcross Across(const FSprite& Sprite, const FSpritePart& Part, int16 Width)
		{
			int16 Left = static_cast<int16>(Part.X << Sprite.SizeShift());
			if ((Sprite.DrawFlags & SpriteDrawFlags::Mirrored) != 0)
			{
				Width = static_cast<int16>(-Width);
				Left = static_cast<int16>(-Left);
			}
			const int16 Right = static_cast<int16>(Left + Width);
			return (Part.Flags & PartFlipsTexelsAcross) == 0 ? FAcross{Left, Right} : FAcross{Right, Left};
		}
	}

	FGteMatrix SpriteDrawMatrix(const FSprite& Sprite, const FGteMatrix& Rotation, const FGteMatrix& View)
	{
		const std::array<int32, 2> Pivot = SpritePivotOffset(Sprite);
		const FGteVector Position = {static_cast<int16>(Sprite.Position[0] >> 16), static_cast<int16>(Sprite.Position[1] >> 16),
			static_cast<int16>(Sprite.Position[2] >> 16)};
		const FGteLongVector Turned = ApplyMatrix(View, Position);
		FGteMatrix Result;
		Result.M = Rotation.M;
		Result.T = {View.T[0] + Turned[0] + Pivot[0], View.T[1] + Turned[1] + Pivot[1], View.T[2] + Turned[2]};
		return Result;
	}

	std::array<int32, 2> SpritePivotOffset(const FSprite& Sprite)
	{
		const int32 Shift = Sprite.SizeShift();
		int32 PivotX = static_cast<int32>(Sprite.PivotX) << Shift;
		if (Sprite.IsMirroredFacing())
		{
			PivotX = -PivotX;
		}
		const int32 PivotY = static_cast<int32>(Sprite.PivotY) << Shift;
		return {AtScale(PivotX, Sprite.Scale), AtScale(PivotY, Sprite.Scale)};
	}

	FGteMatrix SpriteModelMatrix(const FSprite& Sprite)
	{
		const std::array<int16, 3>& Scale = Sprite.TransformScale;
		const FGteMatrix Rotation = RotationXyz({Sprite.Rotation[0], Sprite.Rotation[1], Sprite.Rotation[2]});
		FGteMatrix Result = (Sprite.BuildFlags & SpriteBuildFlags::ScaleFirst) != 0
			? MulMatrix(Rotation, ScaleRows(FGteMatrix::Identity(), {Scale[0], Scale[1], Scale[2]}))
			: ScaleRows(Rotation, {Scale[0], Scale[1], Scale[2]});
		if (Sprite.SizeFactor != 0)
		{
			const int32 Half = Sprite.SizeFactor >> 1;
			Result = ScaleColumns(Result, {Half, Half, Half});
		}
		Result.T = {static_cast<int16>(Sprite.Position[0] >> 16), static_cast<int16>(Sprite.Position[1] >> 16), static_cast<int16>(Sprite.Position[2] >> 16)};
		return Result;
	}

	FGteMatrix DrawSpriteParts(const FSprite& Sprite, const FGteMatrix& Matrix, int32 OtIndex, std::vector<FSpriteQuad>* Quads)
	{
		const int32 Shift = Sprite.SizeShift();
		int16 PivotX = static_cast<int16>(Sprite.PivotX << Shift);
		if (Sprite.IsMirroredFacing())
		{
			PivotX = static_cast<int16>(-PivotX);
		}
		const int16 PivotY = static_cast<int16>(Sprite.PivotY << Shift);
		const uint32 HiddenGroups = Sprite.DrawFlags >> SpriteDrawFlags::HiddenGroupsShift & 0xFF;
		const bool bFlipped = (Sprite.DrawFlags & SpriteDrawFlags::FlippedVertically) != 0;
		FGteMatrix Current = Matrix;
		uint32 Group = NoGroup;
		bool bShown = false;
		for (int32 Index = 0; Index < Sprite.PartCount(); ++Index)
		{
			const FSpritePart& Part = Sprite.Parts[static_cast<std::size_t>(Index)];
			const uint32 PartGroup = Part.Flags & PartGroupMask;
			if (PartGroup != Group)
			{
				Group = PartGroup;
				bShown = (1u << Group & HiddenGroups) == 0;
				Current = GroupMatrix(Sprite, Matrix, Group);
			}
			if (!bShown || Quads == nullptr)
			{
				continue;
			}
			const int16 Width = static_cast<int16>(static_cast<int16>(Part.Width + static_cast<int8>(Part.GrowWidth)) << Shift);
			int16 Height = static_cast<int16>(static_cast<int16>(Part.Height + static_cast<int8>(Part.GrowHeight)) << Shift);
			int16 Top = static_cast<int16>(Part.Y << Shift);
			if (bFlipped)
			{
				Height = static_cast<int16>(-Height);
				Top = static_cast<int16>(-Top);
			}
			int16 FirstY = Top;
			int16 SecondY = static_cast<int16>(Top + Height);
			if ((Part.Flags & PartFlipsTexelsDown) != 0)
			{
				std::swap(FirstY, SecondY);
			}
			const FAcross X = Across(Sprite, Part, Width);
			const auto At = [PivotX, PivotY](int16 CornerX, int16 CornerY)
			{
				return FCorner{static_cast<int16>(CornerX - PivotX), static_cast<int16>(CornerY - PivotY)};
			};
			const int32 Ot = (Sprite.DrawFlags & GroupsSortNearer) != 0 ? OtIndex - static_cast<int32>(Group) : OtIndex;
			AddQuad(*Quads, Current, {At(X.First, FirstY), At(X.Second, FirstY), At(X.Second, SecondY), At(X.First, SecondY)}, Part, Part.V, Part.Height, Ot);
		}
		return Current;
	}

	void DrawSpriteShadow(const FSprite& Sprite, const FGteMatrix& View, int32 OtIndex, std::vector<FSpriteQuad>& Quads)
	{
		constexpr uint32 ShadowCode = 0x2C000000;
		FGteMatrix Matrix = ScaleRows(View, {Sprite.Scale, Sprite.Scale / 2, 0});
		const FGteLongVector Floor = ApplyMatrix(View, {static_cast<int16>(Sprite.Position[0] >> 16), Sprite.FloorY, static_cast<int16>(Sprite.Position[2] >> 16)});
		Matrix.T = {View.T[0] + Floor[0], View.T[1] + Floor[1], View.T[2] + Floor[2]};
		const int32 Shift = Sprite.SizeShift();
		const uint32 HiddenGroups = Sprite.DrawFlags >> SpriteDrawFlags::HiddenGroupsShift & 0xFF;
		const bool bMirrored = (Sprite.DrawFlags & SpriteDrawFlags::Mirrored) != 0;
		const bool bFlipped = (Sprite.DrawFlags & SpriteDrawFlags::FlippedVertically) != 0;
		for (int32 Index = 0; Index < Sprite.PartCount(); ++Index)
		{
			const FSpritePart& Part = Sprite.Parts[static_cast<std::size_t>(Index)];
			if ((1u << (Part.Flags & PartGroupMask) & HiddenGroups) != 0)
			{
				continue;
			}
			int16 Width = static_cast<int16>(static_cast<int16>(Part.Width + static_cast<int8>(Part.GrowWidth)) << Shift);
			int16 Height = static_cast<int16>(static_cast<int16>(Part.Height + static_cast<int8>(Part.GrowHeight)) << Shift);
			int16 Left = static_cast<int16>(Part.X << Shift);
			int16 Near = static_cast<int16>(Part.Y << Shift);
			if (bMirrored)
			{
				Width = static_cast<int16>(-Width);
				Left = static_cast<int16>(-Left);
			}
			// The sprite's vertical flip and the part's own cancel each other out.
			if (bFlipped != ((Part.Flags & PartFlipsTexelsDown) != 0))
			{
				Height = static_cast<int16>(-Height);
				Near = static_cast<int16>(-Near);
			}
			const int16 Right = static_cast<int16>(Left + Width);
			const int16 Far = static_cast<int16>(Near + Height);
			const bool bAcross = (Part.Flags & PartFlipsTexelsAcross) != 0;
			const bool bDown = (Part.Flags & PartFlipsTexelsDown) != 0;
			const int16 FirstX = bAcross ? Right : Left;
			const int16 SecondX = bAcross ? Left : Right;
			const int16 FirstZ = bDown ? Far : Near;
			const int16 SecondZ = bDown ? Near : Far;
			FSpriteQuad& Quad = Quads.emplace_back();
			Quad.Matrix = Matrix;
			Quad.Corners = {FGteVector{FirstX, 0, FirstZ}, FGteVector{SecondX, 0, FirstZ}, FGteVector{FirstX, 0, SecondZ}, FGteVector{SecondX, 0, SecondZ}};
			Quad.U0 = Part.U;
			Quad.U1 = static_cast<int16>(Part.U + Part.Width);
			Quad.V0 = Part.V;
			Quad.V1 = static_cast<int16>(Part.V + Part.Height);
			Quad.TexturePage = Part.TexturePage;
			Quad.Clut = Part.Clut;
			Quad.ColorCode = ShadowCode;
			Quad.OtIndex = OtIndex;
		}
	}

	void DrawSpritePartsClipped(const FSprite& Sprite, const FGteMatrix& Matrix, int32 OtIndex, ESpriteClip Clip, int32 Line, std::vector<FSpriteQuad>& Quads)
	{
		const int32 Shift = Sprite.SizeShift();
		const int32 LineY = Line << Shift;
		const bool bFlipped = (Sprite.DrawFlags & SpriteDrawFlags::FlippedVertically) != 0;
		for (int32 Index = 0; Index < Sprite.PartCount(); ++Index)
		{
			const FSpritePart& Part = Sprite.Parts[static_cast<std::size_t>(Index)];
			const int16 Width = static_cast<int16>((Part.Width + static_cast<int8>(Part.GrowWidth)) << Shift);
			int32 Height = (Part.Height + static_cast<int8>(Part.GrowHeight)) << Shift;
			int32 Top = static_cast<int32>(Part.Y) << Shift;
			if (bFlipped)
			{
				Height = -Height;
				Top = -Top;
			}
			const int32 Low = Height < 1 ? Top + Height : Top;
			const int32 High = Height < 1 ? Top : Top + Height;
			// The line cuts the quad and its texels at whole texels; a quad wholly on the other side is left out.
			int16 FirstY = 0;
			int16 SecondY = 0;
			int32 FirstRow = Part.V;
			int32 Rows = Part.Height;
			if (Clip == ESpriteClip::KeepAbove)
			{
				if (Low > LineY)
				{
					continue;
				}
				const int32 Cut = LineY < High ? High - LineY : 0;
				Height -= Cut;
				if (Height < 0)
				{
					Top -= Cut;
				}
				FirstY = static_cast<int16>(Top);
				SecondY = static_cast<int16>(FirstY + static_cast<int16>(Height));
				const int32 CutRows = static_cast<int8>(Cut >> Shift);
				Rows = static_cast<int8>(Part.Height) - CutRows;
				if (Height < 1)
				{
					FirstRow = static_cast<uint8>(Part.V - CutRows);
				}
			}
			else
			{
				if (LineY > High)
				{
					continue;
				}
				const int32 Cut = Low < LineY ? LineY - Low : 0;
				const int32 Kept = Height + (Height > 0 ? -Cut : Cut);
				FirstY = static_cast<int16>(static_cast<int16>(Top) + static_cast<int16>(Cut));
				SecondY = static_cast<int16>(FirstY + static_cast<int16>(Kept));
				const int32 CutRows = static_cast<int8>(Cut >> Shift);
				FirstRow = static_cast<uint8>(Kept < 1 ? Part.V - CutRows : Part.V + CutRows);
				Rows = static_cast<int8>(Part.Height) - CutRows;
			}
			if ((Part.Flags & PartFlipsTexelsDown) != 0)
			{
				std::swap(FirstY, SecondY);
			}
			const FAcross X = Across(Sprite, Part, Width);
			AddQuad(Quads, Matrix, {FCorner{X.First, FirstY}, FCorner{X.Second, FirstY}, FCorner{X.Second, SecondY}, FCorner{X.First, SecondY}}, Part, FirstRow,
				Rows, OtIndex);
		}
	}
}
