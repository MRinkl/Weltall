#include "Field/FieldScreenInVram.h"

#include "Field/FieldRenderState.h"
#include "Field/FieldVramChanges.h"

#include <algorithm>
#include <array>

namespace Weltall
{
	namespace
	{
		constexpr int32 FullBrightness = 128;

		struct FUnitRect
		{
			int32 Left = 0;
			int32 Top = 0;
			int32 Right = 0;
			int32 Bottom = 0;

			bool IsEmpty() const { return Right <= Left || Bottom <= Top; }
		};

		FUnitRect ToUnits(const FVramRect& Rect)
		{
			return {Rect.X, Rect.Y, Rect.X + Rect.Width, Rect.Y + Rect.Height};
		}

		FUnitRect Intersect(const FUnitRect& A, const FUnitRect& B)
		{
			return {std::max(A.Left, B.Left), std::max(A.Top, B.Top), std::min(A.Right, B.Right), std::min(A.Bottom, B.Bottom)};
		}

		/** The part of a region lying in Piece, its screen offset moved with it. */
		FFieldScreenInVram Part(const FFieldScreenInVram& Region, const FUnitRect& Piece)
		{
			FFieldScreenInVram Result = Region;
			Result.Rect = {static_cast<int16>(Piece.Left), static_cast<int16>(Piece.Top), static_cast<int16>(Piece.Right - Piece.Left),
				static_cast<int16>(Piece.Bottom - Piece.Top)};
			Result.ScreenX = static_cast<int16>(Region.ScreenX + Piece.Left - Region.Rect.X);
			Result.ScreenY = static_cast<int16>(Region.ScreenY + Piece.Top - Region.Rect.Y);
			return Result;
		}

		/** A rectangle written as an upload writes it: from its place within VRAM, wrapping past the right and bottom edges. */
		int32 WrappedPieces(const FVramRect& Written, std::array<FUnitRect, 4>& Pieces)
		{
			const int32 Left = Written.X & (FVram::Width - 1);
			const int32 Top = Written.Y & (FVram::Height - 1);
			const int32 Width = std::max<int32>(0, Written.Width);
			const int32 Height = std::max<int32>(0, Written.Height);
			int32 Count = 0;
			for (const auto& [Y0, Y1] : {std::pair{Top, std::min(Top + Height, FVram::Height)}, std::pair{0, Top + Height - FVram::Height}})
			{
				for (const auto& [X0, X1] : {std::pair{Left, std::min(Left + Width, FVram::Width)}, std::pair{0, Left + Width - FVram::Width}})
				{
					const FUnitRect Piece{X0, Y0, X1, Y1};
					if (!Piece.IsEmpty())
					{
						Pieces[static_cast<std::size_t>(Count++)] = Piece;
					}
				}
			}
			return Count;
		}
	}

	void WriteOverScreenInVram(std::vector<FFieldScreenInVram>& Regions, const FVramRect& Written)
	{
		std::array<FUnitRect, 4> Pieces{};
		const int32 PieceCount = WrappedPieces(Written, Pieces);
		for (int32 Piece = 0; Piece < PieceCount; ++Piece)
		{
			const FUnitRect& Over = Pieces[static_cast<std::size_t>(Piece)];
			std::vector<FFieldScreenInVram> Left;
			for (const FFieldScreenInVram& Region : Regions)
			{
				const FUnitRect Whole = ToUnits(Region.Rect);
				const FUnitRect Hit = Intersect(Whole, Over);
				if (Hit.IsEmpty())
				{
					Left.push_back(Region);
					continue;
				}
				// What lies above and below the written rows, then beside them within those rows.
				const FUnitRect Rest[4] = {{Whole.Left, Whole.Top, Whole.Right, Hit.Top}, {Whole.Left, Hit.Bottom, Whole.Right, Whole.Bottom},
					{Whole.Left, Hit.Top, Hit.Left, Hit.Bottom}, {Hit.Right, Hit.Top, Whole.Right, Hit.Bottom}};
				for (const FUnitRect& Kept : Rest)
				{
					if (!Kept.IsEmpty())
					{
						Left.push_back(Part(Region, Kept));
					}
				}
			}
			Regions = std::move(Left);
		}
	}

	void CarryScreenInVram(std::span<const FFieldScreenInVram> Regions, const FFieldVramOperation& Copy, const FFieldCover& Cover,
		std::vector<FFieldScreenInVram>& Carried)
	{
		const FUnitRect Source = ToUnits(Copy.Rect);
		const auto Carry = [&](const FFieldScreenInVram& Region)
		{
			const FUnitRect Hit = Intersect(ToUnits(Region.Rect), Source);
			if (Hit.IsEmpty())
			{
				return;
			}
			FFieldScreenInVram Moved = Part(Region, Hit);
			Moved.Rect.X = static_cast<int16>(Copy.ToX + Hit.Left - Source.Left);
			Moved.Rect.Y = static_cast<int16>(Copy.ToY + Hit.Top - Source.Top);
			Moved.Brightness = static_cast<uint8>(std::min(0xFF, Region.Brightness * Copy.Brightness / FullBrightness));
			Carried.push_back(Moved);
		};
		for (const FFieldScreenInVram& Region : Regions)
		{
			Carry(Region);
		}
		if (Cover.bScreen)
		{
			Carry({FFieldCover::PictureArea, 0, 0, FullBrightness, Cover.Taken});
		}
	}
}
