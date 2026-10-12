#include "Field/FieldWalkTrace.h"

#include "MovementRules.h"
#include "WalkmeshGeometry.h"

#include "Field/FieldCamera.h"
#include "Field/FieldWalkmesh.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"

namespace Weltall
{
	namespace
	{
		using namespace MovementRules;
		using namespace WalkmeshGeometry;

		constexpr int32 MaxSteps = 32;
		constexpr int32 Reached = 0xFF;
		constexpr int16 Wall = -1;
		constexpr uint32 ClosedOnLayer0 = 0x800000;
		/** Within this of head-on (4096 a turn), a move into a wall stops instead of sliding. */
		constexpr uint32 HeadOn = 0x80;

		/** Which edges of a triangle have the point outside them, as bits 0-2. */
		uint32 OutsideEdges(uint32 A, uint32 B, uint32 C, uint32 Point)
		{
			return (PackedNclip(A, B, Point) < 0 ? 1u : 0u) | (PackedNclip(B, C, Point) < 0 ? 2u : 0u) | (PackedNclip(C, A, Point) < 0 ? 4u : 0u);
		}

		bool ClassesMeet(uint32 Flags0, uint32 Shift, uint32 Mask, uint32 Attributes, uint32 AttributeShift)
		{
			return (Flags0 >> Shift & Mask & Attributes >> AttributeShift) != 0;
		}

		/** The edge crossed out of a triangle through its side Side (1, 2 or 4): its two corners in order. */
		FWalkEdge CrossedEdge(const FWalkmeshLayer& Layer, const FWalkmeshTriangle& Triangle, uint32 Side)
		{
			const int32 First = Side == 1 ? 0 : Side == 2 ? 1 : 2;
			const int32 Second = (First + 1) % 3;
			return {Layer.Vertices[Triangle.Vertices[static_cast<std::size_t>(First)]], Layer.Vertices[Triangle.Vertices[static_cast<std::size_t>(Second)]]};
		}
	}

	bool TraceWalk(const FEventContext& Context, const FEventActorState& State, const std::array<int32, 3>& Delta, EWalkRule Rule, bool bProbe, FWalkTrace& Trace)
	{
		const FFieldWalkmesh& Walkmesh = *Context.Walkmesh;
		const int32 LayerIndex = static_cast<int16>(State.Layer);
		const FWalkmeshLayer& Layer = Walkmesh.Layers[static_cast<std::size_t>(LayerIndex)];
		int32 Triangle = State.FloorTriangles[static_cast<std::size_t>(LayerIndex)];
		if (Triangle == Wall)
		{
			return false;
		}
		const int32 TargetX = Add(State.Position[0], Delta[0]);
		const int32 TargetZ = Add(State.Position[2], Delta[2]);
		const uint32 Point = PackPoint(TargetX >> 16, TargetZ >> 16);
		const uint32 Start = PackPoint(State.Position[0] >> 16, State.Position[2] >> 16);
		Trace.Point = {static_cast<int16>(WholePart(TargetX)), 0, static_cast<int16>(WholePart(TargetZ))};
		const bool bAttributesSeen = (State.Flags1 >> ((LayerIndex + EventActorFlags1::AttributesIgnoredShift) & 31) & 1) == 0 && !Context.bPartyWalking;
		const uint32 Seen = bAttributesSeen ? ~0u : 0u;
		const bool bStartsSteep = (Walkmesh.Attributes[Layer.Triangles[static_cast<std::size_t>(Triangle)].Attribute] & Seen & FieldFloorAttribute::Steep) != 0;
		const int16 Feet = static_cast<int16>(WholePart(State.Position[1]));

		int32 Steps = 0;
		int32 From = Triangle;
		uint32 Side = 0;
		do
		{
			From = Triangle;
			const FWalkmeshTriangle& Current = Layer.Triangles[static_cast<std::size_t>(Triangle)];
			const uint32 A = PackVertex(Layer.Vertices[Current.Vertices[0]]);
			const uint32 B = PackVertex(Layer.Vertices[Current.Vertices[1]]);
			const uint32 C = PackVertex(Layer.Vertices[Current.Vertices[2]]);
			Side = OutsideEdges(A, B, C, Point);
			switch (Side)
			{
			case 0:
				Steps = Reached;
				break;
			case 3:
				Side = PackedNclip(B, Point, Start) >= 0 ? 2u : 1u;
				break;
			case 5:
				Side = PackedNclip(A, Point, Start) >= 0 ? 1u : 4u;
				break;
			case 6:
				Side = PackedNclip(C, Point, Start) >= 0 ? 4u : 2u;
				break;
			default:
				break;
			}
			if (Side == 1 || Side == 2 || Side == 4)
			{
				Triangle = Current.Neighbours[Side == 1 ? 0 : Side == 2 ? 1 : 2];
			}
			else if (Side == 7)
			{
				Triangle = Wall;
			}
			// A wall ends the trace whatever its attributes (the original reads them from before the table).
			if (Triangle == Wall)
			{
				break;
			}
			const FWalkmeshTriangle& Entered = Layer.Triangles[static_cast<std::size_t>(Triangle)];
			const uint32 Attributes = Walkmesh.Attributes[Entered.Attribute] & Seen;
			bool bClosed = ClassesMeet(State.Flags0, 8, 7, Attributes, 5) || ((Attributes & ClosedOnLayer0) != 0 && LayerIndex == 0);
			if (Rule == EWalkRule::Air)
			{
				bClosed = bClosed || ClassesMeet(State.Flags0, 9, 3, Attributes, 3);
			}
			else
			{
				Trace.Attributes = Attributes;
				if (!bClosed && (Attributes & FieldFloorAttribute::Steep) != 0 && !bStartsSteep)
				{
					FGteLongVector Normal{};
					Trace.Point[1] = PlaneHeight(Layer.Vertices[Entered.Vertices[0]], Layer.Vertices[Entered.Vertices[1]], Layer.Vertices[Entered.Vertices[2]], Trace.Point[0], Trace.Point[2], Normal);
					bClosed = Trace.Point[1] < Feet;
				}
			}
			if (bClosed)
			{
				Triangle = Wall;
				break;
			}
			++Steps;
		} while (Steps < MaxSteps);

		if (Triangle == Wall || Steps == MaxSteps)
		{
			if (Side == 1 || Side == 2 || Side == 4)
			{
				Trace.Edge = CrossedEdge(Layer, Layer.Triangles[static_cast<std::size_t>(From)], Side);
			}
			return false;
		}
		if (!bProbe)
		{
			const FWalkmeshTriangle& Floor = Layer.Triangles[static_cast<std::size_t>(Triangle)];
			FGteLongVector Normal{};
			Trace.Point[1] = PlaneHeight(Layer.Vertices[Floor.Vertices[0]], Layer.Vertices[Floor.Vertices[1]], Layer.Vertices[Floor.Vertices[2]], Trace.Point[0], Trace.Point[2], Normal);
		}
		return true;
	}

	void SlideAlongEdge(const FEventContext& Context, int32 Direction, const FWalkEdge& Edge, std::array<int32, 3>& Delta)
	{
		const int32 EdgeAngle = GteRatan2(Edge.To[2] - Edge.From[2], Edge.To[0] - Edge.From[0], *Context.Arctangents);
		const uint32 EdgeHeading = static_cast<uint32>(-EdgeAngle) & 0xFFF;
		const uint32 Relative = ((static_cast<uint32>(0xC00 - static_cast<int16>(Direction)) & 0xFFF) + EdgeHeading) & 0xFFF;
		if (Relative - HeadOn >= 0x1000 - 2 * HeadOn + 1)
		{
			Delta = {};
			return;
		}
		const bool bBackward = Relative < 0x800;
		const FGteLongVector Run = bBackward ? FGteLongVector{Edge.From[0] - Edge.To[0], 0, Edge.From[2] - Edge.To[2]}
											: FGteLongVector{Edge.To[0] - Edge.From[0], 0, Edge.To[2] - Edge.From[2]};
		const FGteLongVector Unit = VectorNormal(Run);
		const int32 Length = FieldLength2D(Delta[0] >> 12, Delta[2] >> 12);
		Delta = {Multiply(Unit[0], Length), 0, Multiply(Unit[2], Length)};
	}

	std::array<int32, 3> FloorPushVelocity(const FFieldMovement& Movement, int32 Speed, int32 Direction)
	{
		const int32 Scaled = Multiply(Speed * 16, Movement.FloorPushScale) >> 12;
		const int32 Angle = Direction & 0xFFF;
		return {Multiply(GteCos(Angle), Scaled), 0, Wrap(-int64{Multiply(GteSin(Angle), Scaled)})};
	}
}
