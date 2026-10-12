#include "Support/SortingDrawing.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <unordered_map>

namespace Weltall::Testing
{
	namespace
	{
		/** Nearer to the eye than this a polygon is clipped away. */
		constexpr float NearPlane = 1.0f;
		constexpr uint32 NoPiece = std::numeric_limits<uint32>::max();
		constexpr float NoDepth = std::numeric_limits<float>::infinity();
		/** The GPU's code bits: raw texture, semi-transparent. */
		constexpr uint32 RawCode = 1;
		constexpr uint32 SemiTransparentCode = 2;
		constexpr uint16 TexelBlends = 0x8000;

		enum class EFragment : uint8
		{
			None,
			Opaque,
			Blended,
		};

		/** The picture the audit draws into: Width x Height pixels over the screen, from its column Left. */
		struct FTarget
		{
			int32 Width = 0;
			int32 Height = 0;
			float Left = 0.0f;
			float Scale = 1.0f;
			float CenterX = 0.0f;
			float CenterY = 0.0f;
			float Projection = 1.0f;
		};

		struct FProjected
		{
			float X = 0.0f;
			float Y = 0.0f;
			float InverseZ = 0.0f;
			float UOverZ = 0.0f;
			float VOverZ = 0.0f;
			std::array<float, 3> Color{};
		};

		FSortingCorner Between(const FSortingCorner& A, const FSortingCorner& B, float T)
		{
			FSortingCorner Out;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Out.View[Axis] = A.View[Axis] + (B.View[Axis] - A.View[Axis]) * T;
				Out.Color[Axis] = A.Color[Axis] + (B.Color[Axis] - A.Color[Axis]) * T;
			}
			Out.U = A.U + (B.U - A.U) * T;
			Out.V = A.V + (B.V - A.V) * T;
			return Out;
		}

		FProjected Project(const FSortingCorner& Corner, const FTarget& Target)
		{
			FProjected Out;
			const float Z = Corner.View[2];
			Out.X = (Target.CenterX + Target.Projection * Corner.View[0] / Z - Target.Left) * Target.Scale;
			Out.Y = (Target.CenterY + Target.Projection * Corner.View[1] / Z) * Target.Scale;
			Out.InverseZ = 1.0f / Z;
			Out.UOverZ = Corner.U / Z;
			Out.VOverZ = Corner.V / Z;
			Out.Color = Corner.Color;
			return Out;
		}

		float EdgeOf(const FProjected& From, const FProjected& To, float X, float Y)
		{
			return (To.X - From.X) * (Y - From.Y) - (To.Y - From.Y) * (X - From.X);
		}

		int32 PixelBound(float At, int32 Limit)
		{
			return static_cast<int32>(std::clamp(At, -1.0f, static_cast<float>(Limit)));
		}

		/** Whether three corners in view space wind clockwise on the screen (x right, y down), as the display tests a one-sided triangle. */
		bool FacesTheEye(const FSortingCorner& A, const FSortingCorner& B, const FSortingCorner& C)
		{
			const std::array<float, 3>& P = A.View;
			const std::array<float, 3>& Q = B.View;
			const std::array<float, 3>& R = C.View;
			return P[0] * (Q[1] * R[2] - Q[2] * R[1]) + P[1] * (Q[2] * R[0] - Q[0] * R[2]) + P[2] * (Q[0] * R[1] - Q[1] * R[0]) > 0.0f;
		}

		/** Visits every pixel whose centre the triangle covers (its edges included) with the depth, texel coordinate and colour there. */
		template <typename TVisit>
		void DrawTriangle(const FProjected& A, const FProjected& B, const FProjected& C, const FTarget& Target, TVisit&& Visit)
		{
			const float Area = EdgeOf(A, B, C.X, C.Y);
			if (std::abs(Area) < 1e-6f)
			{
				return;
			}
			const int32 Left = std::max(0, PixelBound(std::floor(std::min({A.X, B.X, C.X})), Target.Width));
			const int32 Right = std::min(Target.Width - 1, PixelBound(std::ceil(std::max({A.X, B.X, C.X})), Target.Width));
			const int32 Top = std::max(0, PixelBound(std::floor(std::min({A.Y, B.Y, C.Y})), Target.Height));
			const int32 Bottom = std::min(Target.Height - 1, PixelBound(std::ceil(std::max({A.Y, B.Y, C.Y})), Target.Height));
			for (int32 Y = Top; Y <= Bottom; ++Y)
			{
				const float CentreY = static_cast<float>(Y) + 0.5f;
				for (int32 X = Left; X <= Right; ++X)
				{
					const float CentreX = static_cast<float>(X) + 0.5f;
					const float WeightA = EdgeOf(B, C, CentreX, CentreY) / Area;
					const float WeightB = EdgeOf(C, A, CentreX, CentreY) / Area;
					const float WeightC = EdgeOf(A, B, CentreX, CentreY) / Area;
					if (WeightA < 0.0f || WeightB < 0.0f || WeightC < 0.0f)
					{
						continue;
					}
					const float InverseZ = WeightA * A.InverseZ + WeightB * B.InverseZ + WeightC * C.InverseZ;
					const float U = (WeightA * A.UOverZ + WeightB * B.UOverZ + WeightC * C.UOverZ) / InverseZ;
					const float V = (WeightA * A.VOverZ + WeightB * B.VOverZ + WeightC * C.VOverZ) / InverseZ;
					std::array<float, 3> Color{};
					for (std::size_t Channel = 0; Channel < 3; ++Channel)
					{
						Color[Channel] = WeightA * A.Color[Channel] + WeightB * B.Color[Channel] + WeightC * C.Color[Channel];
					}
					Visit(static_cast<uint32>(Y * Target.Width + X), 1.0f / InverseZ, U, V, Color);
				}
			}
		}

		/** Visits every pixel the shape covers: a quad as the triangles 0-1-2 and 1-3-2, each clipped at the near plane. */
		template <typename TVisit>
		void DrawShape(const FSortingShape& Shape, const FTarget& Target, TVisit&& Visit)
		{
			static constexpr int32 Triangles[2][3] = {{0, 1, 2}, {1, 3, 2}};
			for (int32 Triangle = 0; Triangle < (Shape.Count == 4 ? 2 : Shape.Count == 3 ? 1 : 0); ++Triangle)
			{
				const auto CornerOf = [&Shape, Triangle](int32 Corner) -> const FSortingCorner&
				{
					return Shape.Corners[static_cast<std::size_t>(Triangles[Triangle][Corner % 3])];
				};
				if (Shape.bOneSided && !FacesTheEye(CornerOf(0), CornerOf(1), CornerOf(2)))
				{
					continue;
				}
				std::array<FSortingCorner, 4> Clipped{};
				int32 Kept = 0;
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					const FSortingCorner& From = CornerOf(Corner);
					const FSortingCorner& To = CornerOf(Corner + 1);
					const bool bFromIn = From.View[2] >= NearPlane;
					const bool bToIn = To.View[2] >= NearPlane;
					if (bFromIn)
					{
						Clipped[static_cast<std::size_t>(Kept++)] = From;
					}
					if (bFromIn != bToIn)
					{
						Clipped[static_cast<std::size_t>(Kept++)] = Between(From, To, (NearPlane - From.View[2]) / (To.View[2] - From.View[2]));
					}
				}
				if (Kept < 3)
				{
					continue;
				}
				std::array<FProjected, 4> Projected{};
				for (int32 Corner = 0; Corner < Kept; ++Corner)
				{
					Projected[static_cast<std::size_t>(Corner)] = Project(Clipped[static_cast<std::size_t>(Corner)], Target);
				}
				for (int32 Fan = 1; Fan + 1 < Kept; ++Fan)
				{
					DrawTriangle(Projected[0], Projected[static_cast<std::size_t>(Fan)], Projected[static_cast<std::size_t>(Fan + 1)], Target, Visit);
				}
			}
		}

		/** The point in view space a pixel's centre shows at a depth. */
		std::array<float, 3> ViewAt(const FTarget& Target, uint32 Pixel, float Depth)
		{
			const float X = (static_cast<float>(Pixel % static_cast<uint32>(Target.Width)) + 0.5f) / Target.Scale + Target.Left;
			const float Y = (static_cast<float>(Pixel / static_cast<uint32>(Target.Width)) + 0.5f) / Target.Scale;
			return {(X - Target.CenterX) * Depth / Target.Projection, (Y - Target.CenterY) * Depth / Target.Projection, Depth};
		}

		/** What a shape's pixel draws: nothing (a transparent texel), its own colour, or a blend; the texel read. */
		uint32 ColorOf(const FSortingShape& Shape, uint16 Texel, const std::array<float, 3>& Color)
		{
			std::array<float, 3> Channels = Color;
			if (Shape.bTextured)
			{
				for (std::size_t Channel = 0; Channel < 3; ++Channel)
				{
					const float Value = static_cast<float>((Texel >> (5 * Channel)) & 0x1F) * 8.0f;
					Channels[Channel] = Shape.bRaw ? Value : Value * Color[Channel] / 128.0f;
				}
			}
			uint32 Packed = 0;
			for (std::size_t Channel = 0; Channel < 3; ++Channel)
			{
				Packed |= static_cast<uint32>(std::clamp(Channels[Channel], 0.0f, 255.0f)) << (16 - 8 * Channel);
			}
			return Packed;
		}

		uint32 BlendOf(uint32 Back, uint32 Front, EBlendMode Mode)
		{
			uint32 Packed = 0;
			for (int32 Shift = 0; Shift <= 16; Shift += 8)
			{
				const int32 B = static_cast<int32>((Back >> Shift) & 0xFF);
				const int32 F = static_cast<int32>((Front >> Shift) & 0xFF);
				int32 Out = 0;
				switch (Mode)
				{
				case EBlendMode::Average:
					Out = (B + F) / 2;
					break;
				case EBlendMode::Add:
					Out = B + F;
					break;
				case EBlendMode::Subtract:
					Out = B - F;
					break;
				case EBlendMode::AddQuarter:
					Out = B + F / 4;
					break;
				}
				Packed |= static_cast<uint32>(std::clamp(Out, 0, 255)) << Shift;
			}
			return Packed;
		}

		/** Whether blending a colour changes what lies below it enough to be seen: adding or taking away black does nothing. */
		bool IsSeenBlending(uint32 Color, EBlendMode Mode)
		{
			const uint32 Brightest = std::max({Color >> 16 & 0xFF, Color >> 8 & 0xFF, Color & 0xFF});
			switch (Mode)
			{
			case EBlendMode::Average:
				return true;
			case EBlendMode::AddQuarter:
				return Brightest >= 32;
			default:
				return Brightest >= 8;
			}
		}

		/** A texel coordinate inside the shape's texels: wrapped into its texture window, then held within its bounds. */
		int32 TexelAt(float At, float Low, float High, uint8 WindowWidth)
		{
			if (WindowWidth != 0)
			{
				const float Width = static_cast<float>(WindowWidth);
				At -= std::floor(At / Width) * Width;
			}
			return static_cast<int32>(std::floor(std::clamp(At, Low, High)));
		}

		/**
		 * What a shape's pixel draws, and its colour: nothing where its texel is transparent or what it blends in is not
		 * seen, its own colour, or a blend.
		 */
		EFragment FragmentOf(const FSortingShape& Shape, const FVram& Vram, float U, float V, const std::array<float, 3>& Color, uint32& Drawn)
		{
			uint16 Texel = 0;
			EFragment Fragment = Shape.bSemiTransparent ? EFragment::Blended : EFragment::Opaque;
			if (Shape.bTextured)
			{
				const int32 TexelU = TexelAt(U, Shape.LowU, Shape.HighU, Shape.WindowWidth) + Shape.ScrollU;
				const int32 TexelV = TexelAt(V, Shape.LowV, Shape.HighV, 0) + Shape.ScrollV;
				Texel = FetchTexel(Vram, Shape.Page, Shape.Clut, static_cast<uint8>(TexelU & 0xFF), static_cast<uint8>(TexelV & 0xFF));
				if (Texel == 0)
				{
					return EFragment::None;
				}
				if ((Texel & TexelBlends) == 0)
				{
					Fragment = EFragment::Opaque;
				}
			}
			Drawn = ColorOf(Shape, Texel, Color);
			return Fragment == EFragment::Blended && !IsSeenBlending(Drawn, Shape.Blend) ? EFragment::None : Fragment;
		}

		bool IsSeenDifference(uint32 A, uint32 B)
		{
			for (int32 Shift = 0; Shift <= 16; Shift += 8)
			{
				const int32 Difference = static_cast<int32>(A >> Shift & 0xFF) - static_cast<int32>(B >> Shift & 0xFF);
				if (static_cast<uint32>(std::abs(Difference)) >= SortingSeenDifference)
				{
					return true;
				}
			}
			return false;
		}

		/** The conflicts the most pixels seen first, the marks renumbered to match. */
		void RankConflicts(FSortingFrameAudit& Audit)
		{
			std::vector<uint32> Ranked(Audit.Conflicts.size());
			for (uint32 Number = 0; Number < Ranked.size(); ++Number)
			{
				Ranked[Number] = Number;
			}
			std::stable_sort(Ranked.begin(), Ranked.end(), [&Audit](uint32 A, uint32 B)
			{
				const FSortingConflict& First = Audit.Conflicts[A];
				const FSortingConflict& Second = Audit.Conflicts[B];
				return First.Seen != Second.Seen ? First.Seen > Second.Seen : First.Pixels > Second.Pixels;
			});
			std::vector<FSortingConflict> Sorted;
			std::vector<uint32> RankOf(Ranked.size());
			for (uint32 Rank = 0; Rank < Ranked.size(); ++Rank)
			{
				Sorted.push_back(Audit.Conflicts[Ranked[Rank]]);
				RankOf[Ranked[Rank]] = Rank;
			}
			Audit.Conflicts = std::move(Sorted);
			for (uint32& Mark : Audit.Marks)
			{
				Mark = Mark == 0 ? 0 : RankOf[Mark - 1] + 1;
			}
		}
	}

	FSortingFrameAudit AuditSorting(std::vector<FSortingPiece> Pieces, const std::vector<FSortingShape>& Shapes, const FVram& Vram,
		const FSortingScreen& Screen, uint32 Background, float Margin, const ISortingJudge& Judge)
	{
		FSortingFrameAudit Audit;
		Audit.Pieces = std::move(Pieces);
		FTarget Target;
		Target.Width = Screen.Width * Screen.Scale;
		Target.Height = Screen.Lines * Screen.Scale;
		Target.Left = Screen.CenterX - static_cast<float>(Screen.Width) / 2.0f;
		Target.Scale = static_cast<float>(Screen.Scale);
		Target.CenterX = Screen.CenterX;
		Target.CenterY = Screen.CenterY;
		Target.Projection = Screen.Projection;

		const std::size_t PixelCount = static_cast<std::size_t>(Target.Width) * static_cast<std::size_t>(Target.Height);
		std::vector<uint32> Shown(PixelCount, NoPiece);
		std::vector<float> ShownDepth(PixelCount, NoDepth);
		std::vector<uint32> Nearest(PixelCount, NoPiece);
		std::vector<float> NearestDepth(PixelCount, NoDepth);
		Audit.PictureWidth = Target.Width;
		Audit.PictureHeight = Target.Height;
		Audit.Picture.assign(PixelCount, Background);
		Audit.DepthPicture = Audit.Picture;
		Audit.Marks.assign(PixelCount, 0);

		// The painter's order: the last opaque texel at each pixel is what shows, the nearest is what the depth would show.
		for (uint32 Number = 0; Number < Shapes.size(); ++Number)
		{
			const FSortingShape& Shape = Shapes[Number];
			const bool bBackground = Audit.Pieces[Number].Kind == ESortingPiece::Background;
			DrawShape(Shape, Target, [&](uint32 Pixel, float Depth, float U, float V, const std::array<float, 3>& Color)
			{
				uint32 Drawn = 0;
				const EFragment Fragment = FragmentOf(Shape, Vram, U, V, Color, Drawn);
				if (Fragment == EFragment::None)
				{
					return;
				}
				Audit.Picture[Pixel] = Fragment == EFragment::Opaque ? Drawn : BlendOf(Audit.Picture[Pixel], Drawn, Shape.Blend);
				if (bBackground)
				{
					// Behind everything: by depth it shows only where nothing stands.
					if (Nearest[Pixel] == NoPiece)
					{
						Audit.DepthPicture[Pixel] = Audit.Picture[Pixel];
					}
					return;
				}
				if (Fragment == EFragment::Opaque)
				{
					Shown[Pixel] = Number;
					ShownDepth[Pixel] = Depth;
					if (Depth < NearestDepth[Pixel])
					{
						Nearest[Pixel] = Number;
						NearestDepth[Pixel] = Depth;
						Audit.DepthPicture[Pixel] = Drawn;
					}
				}
			});
		}

		std::unordered_map<uint64, uint32> Pairs;
		// bSortable: a pair a depth test can put right; a blend drawn before what covers it is never one of them.
		const auto Count = [&](uint32 Over, uint32 Under, float Gap, uint32 Pixel, bool bSortable = true)
		{
			if (Judge.IsDecided(Audit.Pieces[Over], Audit.Pieces[Under], bSortable))
			{
				return;
			}
			const auto [Found, bNew] = Pairs.try_emplace(static_cast<uint64>(Over) << 32 | Under, static_cast<uint32>(Audit.Conflicts.size()));
			if (bNew)
			{
				Audit.Conflicts.push_back({Over, Under, 0, 0, 0.0f});
			}
			FSortingConflict& Conflict = Audit.Conflicts[Found->second];
			++Conflict.Pixels;
			Conflict.Gap = std::max(Conflict.Gap, Gap);
			Audit.Marks[Pixel] = Found->second + 1;
		};
		for (uint32 Pixel = 0; Pixel < PixelCount; ++Pixel)
		{
			if (Shown[Pixel] != Nearest[Pixel] && ShownDepth[Pixel] - NearestDepth[Pixel] > Margin)
			{
				Count(Shown[Pixel], Nearest[Pixel], ShownDepth[Pixel] - NearestDepth[Pixel], Pixel);
			}
		}
		// What blends: drawn after what shows there though farther, or drawn before it and hidden though nearer.
		for (uint32 Number = 0; Number < Shapes.size(); ++Number)
		{
			const FSortingShape& Shape = Shapes[Number];
			if (!Shape.bSemiTransparent || Audit.Pieces[Number].Kind == ESortingPiece::Background)
			{
				continue;
			}
			DrawShape(Shape, Target, [&](uint32 Pixel, float Depth, float U, float V, const std::array<float, 3>& Color)
			{
				uint32 Drawn = 0;
				if (FragmentOf(Shape, Vram, U, V, Color, Drawn) != EFragment::Blended)
				{
					return;
				}
				if (Depth < NearestDepth[Pixel])
				{
					Audit.DepthPicture[Pixel] = BlendOf(Audit.DepthPicture[Pixel], Drawn, Shape.Blend);
				}
				const uint32 Under = Shown[Pixel];
				if (Under == NoPiece)
				{
					return;
				}
				if (Number > Under)
				{
					const float Gap = Judge.BlendGap(Audit.Pieces[Number], Shape, Audit.Pieces[Under], ViewAt(Target, Pixel, ShownDepth[Pixel]),
						Depth - ShownDepth[Pixel]);
					if (Gap > Margin)
					{
						Count(Number, Under, Gap, Pixel);
					}
				}
				else if (Number < Under && ShownDepth[Pixel] - Depth > Margin)
				{
					Count(Under, Number, ShownDepth[Pixel] - Depth, Pixel, false);
				}
			});
		}

		for (uint32 Pixel = 0; Pixel < PixelCount; ++Pixel)
		{
			if (Audit.Marks[Pixel] != 0 && IsSeenDifference(Audit.Picture[Pixel], Audit.DepthPicture[Pixel]))
			{
				++Audit.Conflicts[Audit.Marks[Pixel] - 1].Seen;
			}
		}
		RankConflicts(Audit);
		return Audit;
	}

	void SetSortingTexture(FSortingShape& Shape, uint32 Code, uint16 Page, uint16 Clut, bool bTextured)
	{
		Shape.bTextured = bTextured;
		Shape.bSemiTransparent = (Code & SemiTransparentCode) != 0;
		Shape.bRaw = (Code & RawCode) != 0;
		Shape.Page = FTexturePage::FromAttribute(Page);
		Shape.Clut = FClutPosition::FromAttribute(Clut);
		Shape.Blend = Shape.Page.Blend;
	}

	void BoundSortingTexels(FSortingShape& Shape, bool bEdgeToEdge)
	{
		const auto Corners = std::span<const FSortingCorner>(Shape.Corners).first(static_cast<std::size_t>(Shape.Count));
		const auto [LowU, HighU] = std::minmax_element(Corners.begin(), Corners.end(), [](const FSortingCorner& A, const FSortingCorner& B) { return A.U < B.U; });
		const auto [LowV, HighV] = std::minmax_element(Corners.begin(), Corners.end(), [](const FSortingCorner& A, const FSortingCorner& B) { return A.V < B.V; });
		const float Hair = bEdgeToEdge ? 1.0f / 64.0f : 0.0f;
		Shape.LowU = LowU->U;
		Shape.HighU = std::max(LowU->U, HighU->U - Hair);
		Shape.LowV = LowV->V;
		Shape.HighV = std::max(LowV->V, HighV->V - Hair);
	}

	void SetSortingFloor(FSortingShape& Shape)
	{
		const auto Minus = [](const std::array<float, 3>& A, const std::array<float, 3>& B) { return std::array<float, 3>{A[0] - B[0], A[1] - B[1], A[2] - B[2]}; };
		const std::array<float, 3>& Origin = Shape.Corners[0].View;
		const std::array<float, 3> Across = Minus(Shape.Corners[1].View, Origin);
		const std::array<float, 3> Along = Minus(Shape.Corners[2].View, Origin);
		std::array<float, 3> Up = {Across[1] * Along[2] - Across[2] * Along[1], Across[2] * Along[0] - Across[0] * Along[2],
			Across[0] * Along[1] - Across[1] * Along[0]};
		const float Length = std::sqrt(Up[0] * Up[0] + Up[1] * Up[1] + Up[2] * Up[2]);
		if (Length == 0.0f)
		{
			return;
		}
		// The eye at the origin lies above the floor it looks down on.
		const float Side = -(Up[0] * Origin[0] + Up[1] * Origin[1] + Up[2] * Origin[2]) < 0.0f ? -1.0f : 1.0f;
		for (float& Axis : Up)
		{
			Axis *= Side / Length;
		}
		Shape.FloorPoint = Origin;
		Shape.FloorUp = Up;
	}

	float HeightAboveFloor(const FSortingShape& Shadow, const std::array<float, 3>& Point)
	{
		float Height = 0.0f;
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Height += (Point[Axis] - Shadow.FloorPoint[Axis]) * Shadow.FloorUp[Axis];
		}
		return Height;
	}

	const char* SortingPieceName(ESortingPiece Kind)
	{
		switch (Kind)
		{
		case ESortingPiece::Scenery:
			return "scenery";
		case ESortingPiece::Sprite:
			return "sprite";
		case ESortingPiece::Shadow:
			return "shadow";
		case ESortingPiece::Particle:
			return "particle";
		case ESortingPiece::Overlay:
			return "overlay";
		case ESortingPiece::GearPart:
			return "gear";
		case ESortingPiece::GearShadow:
			return "gear shadow";
		case ESortingPiece::SpriteModel:
			return "child model";
		case ESortingPiece::Ground:
			return "ground";
		case ESortingPiece::PlaceModel:
			return "place model";
		case ESortingPiece::Tree:
			return "tree";
		case ESortingPiece::Cloud:
			return "cloud";
		case ESortingPiece::Vehicle:
			return "ship";
		case ESortingPiece::Background:
			return "background";
		case ESortingPiece::Skipped:
			break;
		}
		return "skipped";
	}

	std::string SortingClass(ESortingPiece Over, ESortingPiece Under)
	{
		const auto Is = [Over, Under](ESortingPiece Kind) { return Over == Kind || Under == Kind; };
		if (Is(ESortingPiece::Sprite))
		{
			// The per-pixel sorting decides a sprite against everything by depth already.
			return std::string("sprite and ") + SortingPieceName(Over == ESortingPiece::Sprite ? Under : Over);
		}
		if (Is(ESortingPiece::Particle))
		{
			return std::string("particle and ") + SortingPieceName(Over == ESortingPiece::Particle ? Under : Over);
		}
		return std::string(SortingPieceName(Over)) + " over " + SortingPieceName(Under);
	}
}
