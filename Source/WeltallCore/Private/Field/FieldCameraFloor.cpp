#include "Field/FieldCameraFloor.h"

#include "Field/FieldWalkmesh.h"
#include "Foundation/ProcessorDivide.h"
#include "WalkmeshGeometry.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		using WalkmeshGeometry::PackedNclip;
		using WalkmeshGeometry::PackPoint;
		using WalkmeshGeometry::PackVertex;

		int32 Wrap(uint32 Value)
		{
			return static_cast<int32>(Value);
		}

		/** The way into the area gives up after this many steps, at the edge it last crossed. */
		constexpr int32 MaxAreaSteps = 0xF0;

		bool IsOpenToCamera(const FFieldWalkmesh& Walkmesh, const FWalkmeshTriangle& Triangle)
		{
			return (Walkmesh.Attributes[Triangle.Attribute] & FieldFloorAttribute::InCameraArea) != 0;
		}

		/**
		 * Where the line from the point toward the way's start crosses an edge (from, to), both
		 * directions normalized by the library; parallel lines leave the point where it is.
		 */
		FFieldCameraAreaPoint BackToEdge(const FGteVector& From, const FGteVector& To, int16 X, int16 Z, int16 StartX, int16 StartZ)
		{
			const FGteLongVector Along = VectorNormal({To[0] - From[0], 0, To[2] - From[2]});
			const FGteLongVector Back = VectorNormal({StartX - X, 0, StartZ - Z});
			const int32 Crossing = Wrap(static_cast<uint32>(Back[0]) * static_cast<uint32>(Along[2]) - static_cast<uint32>(Back[2]) * static_cast<uint32>(Along[0])) >> 12;
			int32 Distance = 0;
			if (Crossing != 0)
			{
				const uint32 Across = static_cast<uint32>(Z - From[2]) * static_cast<uint32>(Along[0]) - static_cast<uint32>(X - From[0]) * static_cast<uint32>(Along[2]);
				Distance = ProcessorDivide(Wrap(Across), Crossing);
			}
			FFieldCameraAreaPoint Point;
			Point.bInside = false;
			Point.X = static_cast<int16>(X + (Wrap(static_cast<uint32>(Distance) * static_cast<uint32>(Back[0])) >> 12));
			Point.Z = static_cast<int16>(Z + (Wrap(static_cast<uint32>(Distance) * static_cast<uint32>(Back[2])) >> 12));
			return Point;
		}
	}

	int16 WalkmeshGeometry::PlaneHeight(const FGteVector& V0, const FGteVector& V1, const FGteVector& V2, int16 X, int16 Z, FGteLongVector& Normal)
	{
		const FGteLongVector First = VectorNormal({V1[0] - V0[0], V1[1] - V0[1], V1[2] - V0[2]});
		const FGteLongVector Second = VectorNormal({V2[0] - V0[0], V2[1] - V0[1], V2[2] - V0[2]});
		Normal = OuterProduct12(First, Second);
		if (Normal[1] == 0)
		{
			return 0;
		}
		const uint32 Along = 0u - static_cast<uint32>(Normal[2]) * static_cast<uint32>(Z - V0[2]) - static_cast<uint32>(Normal[0]) * static_cast<uint32>(X - V0[0]);
		return static_cast<int16>(V0[1] + static_cast<int16>(static_cast<int32>(Along) / Normal[1]));
	}

	FFieldFloor FindFieldFloor(const FWalkmeshLayer& Layer, int32 X, int32 Z)
	{
		const uint32 Point = PackPoint(X, Z);
		for (std::size_t Index = 0; Index < Layer.Triangles.size(); ++Index)
		{
			const FWalkmeshTriangle& Triangle = Layer.Triangles[Index];
			const FGteVector& V0 = Layer.Vertices[Triangle.Vertices[0]];
			const FGteVector& V1 = Layer.Vertices[Triangle.Vertices[1]];
			const FGteVector& V2 = Layer.Vertices[Triangle.Vertices[2]];
			const uint32 A = PackVertex(V0);
			const uint32 B = PackVertex(V1);
			const uint32 C = PackVertex(V2);
			if (PackedNclip(A, B, Point) >= 0 && PackedNclip(B, C, Point) >= 0 && PackedNclip(C, A, Point) >= 0)
			{
				FFieldFloor Floor;
				Floor.Triangle = static_cast<int32>(Index);
				Floor.Height = WalkmeshGeometry::PlaneHeight(V0, V1, V2, static_cast<int16>(X), static_cast<int16>(Z), Floor.Normal);
				return Floor;
			}
		}
		return {};
	}

	FFieldPoint KeepEyeAboveCameraLayer(const FFieldWalkmesh& Walkmesh, const FFieldPoint& Eye)
	{
		const FFieldFloor Floor = FindFieldFloor(Walkmesh.CameraLayer(), static_cast<int16>(Eye[0] >> 16), static_cast<int16>(Eye[2] >> 16));
		FFieldPoint Kept = Eye;
		if (Floor.Height < static_cast<int16>(Eye[1] >> 16))
		{
			Kept[1] = Floor.Height * 0x10000;
		}
		return Kept;
	}

	FFieldCameraAreaPoint FindFieldCameraAreaPoint(const FFieldWalkmesh& Walkmesh, const std::array<int16, 4>& Bounds, int16 X, int16 Z)
	{
		const FWalkmeshLayer& Layer = Walkmesh.CameraLayer();
		// The way starts from the point kept in the bounds rectangle: x, z, width and minus the depth.
		const int32 StartX = X < Bounds[0] ? Bounds[0] : std::min<int32>(X, Bounds[0] + Bounds[2]);
		const int32 StartZ = Z > Bounds[1] ? Bounds[1] : std::max<int32>(Z, Bounds[1] + Bounds[3]);
		const uint32 Point = PackPoint(X, Z);
		const uint32 Start = PackPoint(StartX, StartZ);
		int32 Current = FindFieldFloor(Layer, StartX, StartZ).Triangle;
		for (int32 Steps = 0;;)
		{
			const FWalkmeshTriangle& Triangle = Layer.Triangles[static_cast<std::size_t>(Current)];
			std::array<uint32, 3> Corners;
			for (std::size_t Corner = 0; Corner < 3; ++Corner)
			{
				Corners[Corner] = PackVertex(Layer.Vertices[Triangle.Vertices[Corner]]);
			}
			// Bit n: the point lies outside edge n (corner n to the next).
			uint32 Outside = (PackedNclip(Corners[0], Corners[1], Point) < 0 ? 1u : 0u) | (PackedNclip(Corners[1], Corners[2], Point) < 0 ? 2u : 0u)
				| (PackedNclip(Corners[2], Corners[0], Point) < 0 ? 4u : 0u);
			// Outside two edges, the way leaves by the one on the start's side of the line from their corner.
			if (Outside == 3)
			{
				Outside = PackedNclip(Corners[1], Point, Start) >= 0 ? 2u : 1u;
			}
			else if (Outside == 5)
			{
				Outside = PackedNclip(Corners[0], Point, Start) >= 0 ? 1u : 4u;
			}
			else if (Outside == 6)
			{
				Outside = PackedNclip(Corners[2], Point, Start) >= 0 ? 4u : 2u;
			}
			// Inside a triangle the point is in the area: the way enters open triangles only, so only
			// the one it starts in can be closed. There, and outside all three edges of a triangle,
			// the original crosses no edge and clamps to one from memory it never wrote; the port
			// keeps the point.
			if (Outside == 0 || Outside == 7)
			{
				return {};
			}
			const std::size_t Crossed = Outside == 1 ? 0 : Outside == 2 ? 1 : 2;
			const int32 Next = Triangle.Neighbours[Crossed];
			if (Next < 0 || !IsOpenToCamera(Walkmesh, Layer.Triangles[static_cast<std::size_t>(Next)]) || ++Steps == MaxAreaSteps)
			{
				const FGteVector& From = Layer.Vertices[Triangle.Vertices[Crossed]];
				const FGteVector& To = Layer.Vertices[Triangle.Vertices[(Crossed + 1) % 3]];
				return BackToEdge(From, To, X, Z, static_cast<int16>(StartX), static_cast<int16>(StartZ));
			}
			Current = Next;
		}
	}
}
