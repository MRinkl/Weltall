#include "Field/FieldCollisions.h"

#include "EventOps.h"
#include "WalkmeshGeometry.h"

#include "Graphics/Gte.h"
#include "Graphics/MdlModel.h"

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

namespace Weltall
{
	namespace
	{
		using namespace WalkmeshGeometry;

		constexpr uint32 ExtraRotationAxis = 0x3;
		constexpr uint8 NoParent = 0xFF;

		FGteMatrix Identity()
		{
			FGteMatrix Matrix;
			Matrix.M = {{{FGteMatrix::One, 0, 0}, {0, FGteMatrix::One, 0}, {0, 0, FGteMatrix::One}}};
			return Matrix;
		}

		/**
		 * Where the platform's model is placed: its own matrix turned by its extra rotation when it has
		 * one, else under the world's rotation as last built and, when it is attached to another actor, that actor's
		 * place in the world as the scenery's last update left it.
		 */
		FGteMatrix PlacePlatform(const FEventContext& Context, const FFieldActor& Record, const FEventActorState& State)
		{
			FGteMatrix Own;
			Own.M = Record.Matrix;
			Own.T = Record.Translation;
			const uint32 Axis = State.Status & ExtraRotationAxis;
			if (Axis != 0)
			{
				FGteVector Angles{};
				Angles[Axis - 1] = State.ExtraAngle;
				FGteMatrix Turned = MulMatrix(Own, RotationXyz(Angles));
				Turned.T = Own.T;
				return CompMatrix(Identity(), Turned);
			}
			FGteMatrix Scene = CompMatrix(Identity(), Context.Camera.WorldRotation);
			if (State.Parent != NoParent)
			{
				const FFieldActor& Parent = Context.Actors[State.Parent];
				FGteMatrix World;
				World.M = Parent.WorldMatrix;
				World.T = Parent.Position;
				Scene = CompMatrix(Scene, World);
			}
			return CompMatrix(Scene, Own);
		}

		/** A vertex placed by the matrix, kept to 16 bits a component as the GTE saturates it. */
		FGteVector Place(const FGteMatrix& Matrix, const FGteVector& Vertex)
		{
			const FGteLongVector Placed = RotTrans(Matrix, Vertex);
			FGteVector Short{};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Short[Axis] = static_cast<int16>(std::clamp<int32>(Placed[Axis], std::numeric_limits<int16>::min(), std::numeric_limits<int16>::max()));
			}
			return Short;
		}
	}

	std::optional<FPlatformFloor> PlatformFloor(FEventContext& Context, int32 Platform, int32 X, int32 Z)
	{
		const FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Platform)];
		const FEventActorState& State = Context.States[static_cast<std::size_t>(Platform)];
		const bool bHasModel = (Record.Flags & FieldActorFlag::NoModel) == 0 && Record.Model < Context.MapModels.size() && !Context.MapModels[Record.Model].Parts.empty();
		if (!bHasModel || ((State.Status & ExtraRotationAxis) == 0 && State.Parent != NoParent && State.Parent >= Context.Actors.size()))
		{
			EventOps::Fail(Context, EEventFault::PlatformModel, static_cast<uint32>(Platform));
			return std::nullopt;
		}
		const FGteMatrix Matrix = PlacePlatform(Context, Record, State);
		const FMdlPart& Part = Context.MapModels[Record.Model].Parts.front();
		// A model a MIMe animates is stood on as the scenery's last update left its vertices.
		const bool bMoving = Record.Model < Context.MimeVertices.size() && !Context.MimeVertices[Record.Model].empty();
		const std::vector<FGteVector>& Vertices = bMoving ? Context.MimeVertices[Record.Model] : Part.Vertices;
		const uint32 Point = PackPoint(X, Z);
		const int16 PointX = static_cast<int16>(X);
		const int16 PointZ = static_cast<int16>(Z);
		int32 Highest = std::numeric_limits<int32>::max();
		FGteLongVector Normal{};
		for (const FMdlPrimitive& Primitive : Part.Primitives)
		{
			const bool bQuad = Primitive.Traits().Corners == 4;
			std::array<FGteVector, 4> Corners{};
			std::array<uint32, 4> Packed{};
			for (std::size_t Corner = 0; Corner < (bQuad ? 4u : 3u); ++Corner)
			{
				Corners[Corner] = Place(Matrix, Vertices[Primitive.Vertices[Corner]]);
				Packed[Corner] = PackVertex(Corners[Corner]);
			}
			// A quad is the triangles 0-1-2 and 1-3-2: its outline runs 0, 1, 3, 2.
			const std::array<std::size_t, 4> Outline = bQuad ? std::array<std::size_t, 4>{0, 1, 3, 2} : std::array<std::size_t, 4>{0, 1, 2, 0};
			const std::size_t Sides = bQuad ? 4 : 3;
			bool bHolds = true;
			for (std::size_t Side = 0; Side < Sides && bHolds; ++Side)
			{
				bHolds = PackedNclip(Packed[Outline[Side]], Packed[Outline[(Side + 1) % Sides]], Point) >= 0;
			}
			if (!bHolds || PackedNclip(Packed[0], Packed[1], Packed[2]) < 0)
			{
				continue;
			}
			const bool bSecondHalf = bQuad && PackedNclip(Packed[1], Packed[2], Point) < 0;
			const int32 Height = bSecondHalf ? PlaneHeight(Corners[1], Corners[3], Corners[2], PointX, PointZ, Normal) : PlaneHeight(Corners[0], Corners[1], Corners[2], PointX, PointZ, Normal);
			Highest = std::min(Highest, Height);
		}
		if (Highest == std::numeric_limits<int32>::max())
		{
			return std::nullopt;
		}
		return FPlatformFloor{Highest, Normal};
	}
}
