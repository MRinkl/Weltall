#include "Gears/GearCloth.h"

#include "Gears/GearFiles.h"
#include "Graphics/GteVectors.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The 32-bit products the original's cloth arithmetic keeps. */
		int32 Times(int32 A, int32 B)
		{
			return static_cast<int32>(static_cast<uint32>(A) * static_cast<uint32>(B));
		}

		/** A division by 1 << Shift that rounds toward zero, as the compiler divides a signed value by a power of two. */
		int32 ShiftTowardZero(int32 Value, int32 Shift)
		{
			return (Value < 0 ? Value + ((1 << Shift) - 1) : Value) >> Shift;
		}

		int32 Root(int32 X, int32 Y, int32 Z)
		{
			return SquareRoot0(static_cast<int32>(static_cast<uint32>(Times(X, X)) + static_cast<uint32>(Times(Y, Y)) + static_cast<uint32>(Times(Z, Z))));
		}

		/** Point Next set on the link from Point toward Target (relative to Point): its direction at 256, its length times the scale. */
		void Pull(const FGearClothPoint& Point, FGearClothPoint& Next, std::array<int32, 3> Toward, int32 Scale)
		{
			const int32 Length = Root(Toward[0], Toward[1], Toward[2]);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Toward[Axis] = Length == 0 ? 0 : Times(Toward[Axis], 256) / Length;
				Next.Position[Axis] = static_cast<int16>(Point.Position[Axis] + static_cast<int16>(ShiftTowardZero(Times(Times(Point.Length, Toward[Axis]), Scale), 20)));
			}
		}

		void Link(FGearCloth& Cloth, std::size_t At, const std::array<int16, 3>& Wind, int32 Scale, int16 Floor)
		{
			const FGearClothPoint& Point = Cloth.Points[At];
			FGearClothPoint& Next = Cloth.Points[At + 1];
			Pull(Point, Next, {Next.Position[0] - Point.Position[0] + Wind[0], Next.Position[1] - Point.Position[1] + Wind[1] + Point.Pull, Next.Position[2] - Point.Position[2] + Wind[2]}, Scale);
			if (Floor < Next.Position[1])
			{
				Next.Position[1] = Floor;
			}
			for (const FGearClothSphereState& Sphere : Cloth.Spheres)
			{
				std::array<int32, 3> Away{};
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Away[Axis] = Next.Position[Axis] - Sphere.World[Axis];
				}
				const int32 Distance = Root(Away[0], Away[1], Away[2]);
				const int32 Radius = static_cast<int16>(ShiftTowardZero(Times(Sphere.Radius, Scale), 12));
				if (Distance >= Radius)
				{
					continue;
				}
				std::array<int32, 3> Surface{};
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Surface[Axis] = Sphere.World[Axis] + (Distance == 0 ? 0 : Times(Radius, Away[Axis]) / Distance) - Point.Position[Axis];
				}
				Pull(Point, Next, Surface, Scale);
			}
		}
	}

	std::optional<FGearCloth> MakeGearCloth(const FGearClothSpec& Spec, const FGearClothMesh& Mesh, int16 TextureX, int16 TextureY, int16 ClutX, int16 ClutY)
	{
		FGearCloth Cloth;
		Cloth.Bone = Spec.Bone;
		Cloth.Front = Spec.Front;
		Cloth.Back = Spec.Back;
		const std::size_t Columns = Mesh.Anchors.size();
		for (const std::array<uint16, 3>& Anchor : Mesh.Anchors)
		{
			FGteVector Placed{};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Placed[Axis] = static_cast<int16>(ShiftTowardZero(Times(Anchor[Axis] + Spec.Offset[Axis], Spec.Scale), 12));
			}
			Cloth.Anchors.push_back(Placed);
		}
		std::size_t Link = 0;
		for (std::size_t Column = 0; Column < Columns; ++Column)
		{
			Cloth.ColumnStarts.push_back(static_cast<uint16>(Cloth.Points.size()));
			for (uint16 Point = 0; Point < Mesh.PointCounts[Column]; ++Point, ++Link)
			{
				Cloth.Points.push_back({static_cast<int16>(ShiftTowardZero(Times(Mesh.LinkLengths[Link], Spec.Scale), 12)),
					static_cast<int16>(Mesh.Pulls[Link] + Spec.PullBase), Cloth.Anchors[Column], 0, {}});
			}
			Cloth.Points.push_back({0, 0, Cloth.Anchors[Column], 0, {}});
		}
		if (Columns < 2)
		{
			return std::nullopt;
		}
		const uint8 UBase = static_cast<uint8>(TextureX * 4);
		const uint8 VBase = static_cast<uint8>(TextureY);
		const int32 UStep = static_cast<int16>(Spec.USpan / static_cast<int32>(Columns - 1));
		for (std::size_t Column = 0; Column + 1 < Columns; ++Column)
		{
			const uint16 Rows = std::min(Mesh.PointCounts[Column], Mesh.PointCounts[Column + 1]);
			if (Rows == 0)
			{
				return std::nullopt;
			}
			const uint8 VStep = static_cast<uint8>(Spec.VSpan / static_cast<int32>(Rows));
			const uint8 ULeft = static_cast<uint8>(UBase + static_cast<uint8>(static_cast<int32>(Column) * UStep));
			const uint8 URight = static_cast<uint8>(UBase + static_cast<uint8>(static_cast<int32>(Column + 1) * UStep));
			for (uint16 Row = 0; Row < Rows; ++Row)
			{
				const uint16 Left = static_cast<uint16>(Cloth.ColumnStarts[Column] + Row);
				const uint16 Right = static_cast<uint16>(Left + Mesh.PointCounts[Column] + 1);
				const uint8 Top = static_cast<uint8>(VBase + VStep * Row);
				const uint8 Bottom = static_cast<uint8>(VBase + VStep * (Row + 1));
				Cloth.Triangles.push_back({{Left, Right, static_cast<uint16>(Left + 1)}, {{{ULeft, Top}, {URight, Top}, {ULeft, Bottom}}}});
				Cloth.Triangles.push_back({{Right, static_cast<uint16>(Right + 1), static_cast<uint16>(Left + 1)}, {{{URight, Top}, {URight, Bottom}, {ULeft, Bottom}}}});
			}
		}
		if (Cloth.Triangles.size() != 2 * std::size_t{Mesh.Rows})
		{
			return std::nullopt;
		}
		const int32 PageX = (TextureX < 0 ? TextureX + 0x3F : TextureX) >> 6;
		const int32 PageY = (TextureY < 0 ? TextureY + 0xFF : TextureY) >> 8;
		// A 4-bit page with blend mode 1, as the kernel's GetTPage packs it.
		Cloth.TexturePage = static_cast<uint16>(1 << 5 | (PageY & 1) << 4 | (PageX & 0xF) | (PageY & 2) << 10);
		Cloth.Clut = static_cast<uint16>(ClutY << 6 | ((ClutX >> 4) & 0x3F));
		for (const FGearClothSphere& Sphere : Spec.Spheres)
		{
			Cloth.Spheres.push_back({Sphere.Center, Sphere.Bone, {}, Sphere.Radius});
		}
		return Cloth;
	}

	bool StepGearCloth(FGearCloth& Cloth, const std::array<int16, 3>& Wind, int32 Scale, int16 Floor)
	{
		for (const uint16 Start : Cloth.ColumnStarts)
		{
			for (std::size_t At = Start; Cloth.Points[At].Length != 0; ++At)
			{
				Link(Cloth, At, Wind, Scale, Floor);
			}
		}
		for (FGearClothPoint& Point : Cloth.Points)
		{
			Point.Normals = 0;
			Point.Normal = {};
		}
		for (const FGearClothTriangle& Triangle : Cloth.Triangles)
		{
			const FGteVector& A = Cloth.Points[Triangle.Points[0]].Position;
			const FGteVector& B = Cloth.Points[Triangle.Points[1]].Position;
			const FGteVector& C = Cloth.Points[Triangle.Points[2]].Position;
			FGteLongVector Face = OuterProduct0({A[0] - B[0], A[1] - B[1], A[2] - B[2]}, {A[0] - C[0], A[1] - C[1], A[2] - C[2]});
			for (int32& Component : Face)
			{
				Component = ShiftTowardZero(Component, 3);
			}
			const FGteLongVector Normal = VectorNormal(Face);
			for (const uint16 Corner : Triangle.Points)
			{
				FGearClothPoint& Point = Cloth.Points[Corner];
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Point.Normal[Axis] = static_cast<int32>(static_cast<uint32>(Point.Normal[Axis]) + static_cast<uint32>(Normal[Axis]));
				}
				++Point.Normals;
			}
		}
		for (FGearClothPoint& Point : Cloth.Points)
		{
			if (Point.Normals == 0)
			{
				return false;
			}
			for (int32& Component : Point.Normal)
			{
				Component /= Point.Normals;
			}
		}
		return true;
	}

	void DrawGearCloth(const FGearCloth& Cloth, const FGte& Gte, EGearClothCulling Culling, int32 FarBucket, int32 OrderingShift, uint8 Slot,
		uint8 ClothNumber, std::vector<FGearDrawnClothTriangle>& Drawn)
	{
		const FGteColor Front{Cloth.Front[0], Cloth.Front[1], Cloth.Front[2], GearClothCode};
		const FGteColor Back{Cloth.Back[0], Cloth.Back[1], Cloth.Back[2], GearClothCode};
		for (std::size_t Number = 0; Number < Cloth.Triangles.size(); ++Number)
		{
			const FGearClothTriangle& Triangle = Cloth.Triangles[Number];
			std::array<FGteProjection, 3> Corners{};
			uint32 Flag = 0;
			for (std::size_t Corner = 0; Corner < 3; ++Corner)
			{
				Corners[Corner] = Gte.Rtps(Cloth.Points[Triangle.Points[Corner]].Position);
				Flag |= Corners[Corner].Flag;
			}
			const bool bProjected = (Flag & GteFlag::DepthSaturated) == 0;
			if (!bProjected && Culling == EGearClothCulling::Original)
			{
				break;
			}
			// A triangle the GTE cannot project takes its side from view space, as its screen corners are saturated.
			const bool bFront = bProjected
				? Nclip(Corners[0].ScreenX, Corners[0].ScreenY, Corners[1].ScreenX, Corners[1].ScreenY, Corners[2].ScreenX, Corners[2].ScreenY) < 0
				: !WindsClockwiseFromEye(Corners[0], Corners[1], Corners[2]);
			const uint16 Bucket = static_cast<uint16>(Gte.AverageDepth3(Corners[0].ScreenZ, Corners[1].ScreenZ, Corners[2].ScreenZ) >> OrderingShift);
			if (Bucket > FarBucket)
			{
				continue;
			}
			FGearDrawnClothTriangle& Out = Drawn.emplace_back();
			for (std::size_t Corner = 0; Corner < 3; ++Corner)
			{
				const uint16 Point = Triangle.Points[Corner];
				const std::array<int32, 3>& Normal = Cloth.Points[Point].Normal;
				// The drawing reads the averaged normal's low halves, and turns it round for the back.
				FGteVector Facing{};
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Facing[Axis] = static_cast<int16>(bFront ? Normal[Axis] : -Normal[Axis]);
				}
				const FGteColor Lit = Gte.NormalColorColor(Facing, bFront ? Front : Back);
				Out.Points[Corner] = Cloth.Points[Point].Position;
				Out.Colors[Corner] = {Lit.R, Lit.G, Lit.B};
				Out.Uvs[Corner] = Triangle.Uvs[Corner];
				Out.PointKeys[Corner] = static_cast<uint32>(Slot) << 24 | static_cast<uint32>(ClothNumber) << 16 | Point;
			}
			Out.Slot = Slot;
			Out.Cloth = ClothNumber;
			Out.Triangle = static_cast<uint16>(Number);
			Out.TexturePage = Cloth.TexturePage;
			Out.Clut = Cloth.Clut;
			Out.Bucket = Bucket;
		}
	}
}
