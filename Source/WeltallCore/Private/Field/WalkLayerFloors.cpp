#include "WalkLayerFloors.h"

#include "MovementRules.h"
#include "WalkmeshGeometry.h"

#include "Field/FieldWalkmesh.h"

#include <algorithm>
#include <utility>

namespace Weltall::WalkLayerFloors
{
	namespace
	{
		using namespace WalkmeshGeometry;
		using MovementRules::Add;

		constexpr int32 MaxTraceSteps = 32;

		/** The edge to cross toward the target when it lies outside the edges Outside marks, or -1 for none. */
		int32 EdgeToCross(uint32 Outside, uint32 A, uint32 B, uint32 C, uint32 Target, uint32 Start)
		{
			switch (Outside)
			{
				case 1:
					return 0;
				case 2:
					return 1;
				case 4:
					return 2;
				case 3:
					return PackedNclip(B, Target, Start) >= 0 ? 1 : 0;
				case 5:
					return PackedNclip(A, Target, Start) >= 0 ? 0 : 2;
				case 6:
					return PackedNclip(C, Target, Start) >= 0 ? 2 : 1;
				default:
					return -1;
			}
		}
	}

	ESearch FloorOnLayer(const FEventContext& Context, const FEventActorState& State, int32 Layer, FLayerFloors& Floors)
	{
		const FFieldWalkmesh& Walkmesh = *Context.Walkmesh;
		const FWalkmeshLayer& Mesh = Walkmesh.Layers[static_cast<std::size_t>(Layer)];
		int32 Triangle = State.FloorTriangles[static_cast<std::size_t>(Layer)];
		if (Triangle == -1)
		{
			return ESearch::None;
		}
		const int32 ToX = Add(State.Position[0], State.Step[0]) >> 16;
		const int32 ToZ = Add(State.Position[2], State.Step[2]) >> 16;
		const uint32 Target = PackPoint(ToX, ToZ);
		const uint32 Start = PackPoint(State.Position[0] >> 16, State.Position[2] >> 16);
		for (int32 Steps = 0;;)
		{
			if (Triangle < 0 || static_cast<std::size_t>(Triangle) >= Mesh.Triangles.size())
			{
				return ESearch::OutsideWalkmesh;
			}
			const FWalkmeshTriangle& Entered = Mesh.Triangles[static_cast<std::size_t>(Triangle)];
			if (std::any_of(Entered.Vertices.begin(), Entered.Vertices.end(), [&Mesh](uint16 Vertex) { return Vertex >= Mesh.Vertices.size(); }))
			{
				return ESearch::OutsideWalkmesh;
			}
			const uint32 A = PackVertex(Mesh.Vertices[Entered.Vertices[0]]);
			const uint32 B = PackVertex(Mesh.Vertices[Entered.Vertices[1]]);
			const uint32 C = PackVertex(Mesh.Vertices[Entered.Vertices[2]]);
			const uint32 Outside = (PackedNclip(A, B, Target) < 0 ? 1u : 0u) | (PackedNclip(B, C, Target) < 0 ? 2u : 0u) | (PackedNclip(C, A, Target) < 0 ? 4u : 0u);
			if (Outside == 0)
			{
				break;
			}
			const int32 Edge = EdgeToCross(Outside, A, B, C, Target, Start);
			Triangle = Edge < 0 ? -1 : Entered.Neighbours[static_cast<std::size_t>(Edge)];
			if (Triangle == -1 || ++Steps == MaxTraceSteps)
			{
				return ESearch::None;
			}
		}
		const FWalkmeshTriangle& Floor = Mesh.Triangles[static_cast<std::size_t>(Triangle)];
		if (Floor.Attribute >= Walkmesh.Attributes.size())
		{
			return ESearch::OutsideWalkmesh;
		}
		FGteLongVector& Normal = Floors.Normals[static_cast<std::size_t>(Layer)];
		const int16 Height = PlaneHeight(Mesh.Vertices[Floor.Vertices[0]], Mesh.Vertices[Floor.Vertices[1]], Mesh.Vertices[Floor.Vertices[2]], static_cast<int16>(ToX), static_cast<int16>(ToZ), Normal);
		Floors.Triangles[static_cast<std::size_t>(Layer)] = static_cast<int16>(Triangle);
		const bool bHonoursClosed = ((State.Flags1 >> ((Layer + EventActorFlags1::AttributesIgnoredShift) & 31)) & 1) == 0 && !Context.bPartyWalking;
		int32& Top = Floors.Floor[static_cast<std::size_t>(Layer)];
		int32& Underside = Floors.Underside[static_cast<std::size_t>(Layer)];
		if (bHonoursClosed && (Walkmesh.Attributes[Floor.Attribute] & FieldFloorAttribute::Closed) != 0)
		{
			Top = NoFloor;
			Underside = NoFloor;
			return ESearch::Found;
		}
		const bool bMoved = State.Step != std::array<int32, 3>{};
		Top = static_cast<int16>(State.Layer) == Layer && bMoved ? State.FloorHeight : Height;
		Underside = Top + std::max(0, Floor.Ceiling * 4);
		return ESearch::Found;
	}

	void SortFloors(FLayerFloors& Floors)
	{
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (std::size_t Rank = 0; Rank + 1 < SortedLayers; ++Rank)
			{
				if (Floors.Floor[Rank + 1] < Floors.Floor[Rank])
				{
					std::swap(Floors.Floor[Rank], Floors.Floor[Rank + 1]);
					std::swap(Floors.Underside[Rank], Floors.Underside[Rank + 1]);
					std::swap(Floors.Layer[Rank], Floors.Layer[Rank + 1]);
				}
			}
		}
	}
}
