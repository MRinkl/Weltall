#include "Field/EventPlacement.h"

#include "Field/FieldCameraFloor.h"

#include <array>

namespace Weltall
{
	namespace
	{
		/** An actor with this record flag keeps its definition's height when the map places it. */
		constexpr uint16 KeepsDefinitionHeight = 0x80;
		constexpr int32 MaxLayers = 4;

		using FFloors = std::array<FFieldFloor, MaxLayers>;

		/**
		 * The floor under (X, Z) on every walking layer (all but the last, the camera's), recorded as
		 * the actor's triangles. A script's placement on the camera's layer, or on one of the four the map
		 * does not have, gets no floor there, as when no triangle holds the point: the original takes it
		 * from stack it never wrote. False when there are too many layers or the actor's own layer is not
		 * one it can be placed on.
		 */
		bool FindFloors(FEventActorState& State, const FFieldWalkmesh& Walkmesh, int32 X, int32 Z, FFloors& Floors, bool bFloorlessLayersAllowed)
		{
			const int32 WalkingLayers = static_cast<int32>(Walkmesh.Layers.size()) - 1;
			if (WalkingLayers > MaxLayers)
			{
				return false;
			}
			for (int32 Layer = 0; Layer < WalkingLayers; ++Layer)
			{
				const FFieldFloor& Floor = Floors[static_cast<std::size_t>(Layer)] = FindFieldFloor(Walkmesh.Layers[static_cast<std::size_t>(Layer)], X, Z);
				State.FloorTriangles[static_cast<std::size_t>(Layer)] = static_cast<int16>(Floor.Triangle);
			}
			const int32 Own = static_cast<int16>(State.Layer);
			const int32 Highest = bFloorlessLayersAllowed ? MaxLayers - 1 : WalkingLayers - 1;
			return Own >= 0 && Own <= Highest;
		}
	}

	std::optional<uint32> ActorFloorAttributes(const FEventActorState& State, const FFieldWalkmesh& Walkmesh)
	{
		const int32 Layer = static_cast<int16>(State.Layer);
		if (Layer < 0 || Layer >= MaxLayers || static_cast<std::size_t>(Layer) >= Walkmesh.Layers.size())
		{
			return std::nullopt;
		}
		// Bits 3-5 of the second flag word: the actor ignores that layer's attributes.
		if ((State.Flags1 >> ((Layer + 3) & 31) & 1) != 0)
		{
			return 0u;
		}
		const int32 Triangle = State.FloorTriangles[static_cast<std::size_t>(Layer)];
		const std::vector<FWalkmeshTriangle>& Triangles = Walkmesh.Layers[static_cast<std::size_t>(Layer)].Triangles;
		if (Triangle < 0 || static_cast<std::size_t>(Triangle) >= Triangles.size() || Triangles[static_cast<std::size_t>(Triangle)].Attribute >= Walkmesh.Attributes.size())
		{
			return std::nullopt;
		}
		return Walkmesh.Attributes[Triangles[static_cast<std::size_t>(Triangle)].Attribute];
	}

	bool PlaceActorOnLoad(FEventActorState& State, FFieldActor& Record, const FFieldWalkmesh& Walkmesh)
	{
		FFloors Floors{};
		if (!FindFloors(State, Walkmesh, static_cast<int16>(Record.Translation[0]), static_cast<int16>(Record.Translation[2]), Floors, false))
		{
			return false;
		}
		const std::optional<uint32> Attributes = ActorFloorAttributes(State, Walkmesh);
		if (!Attributes)
		{
			return false;
		}
		State.FloorAttributes = *Attributes;
		const FFieldFloor& Floor = Floors[State.Layer];
		State.FloorNormal = Floor.Normal;
		if ((Record.Flags & KeepsDefinitionHeight) == 0)
		{
			Record.Translation[1] = Floor.Height;
		}
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			State.Position[Axis] = static_cast<int32>(static_cast<uint32>(Record.Translation[Axis]) << 16);
		}
		State.FloorHeight = static_cast<int16>(Record.Translation[1]);
		return true;
	}

	bool PlaceActor(FEventActorState& State, FFieldActor& Record, const FFieldWalkmesh& Walkmesh, int32 X, int32 Z)
	{
		FFloors Floors{};
		if (!FindFloors(State, Walkmesh, X, Z, Floors, true))
		{
			return false;
		}
		// A layer the map does not have has no floor, and so no attributes.
		const std::optional<uint32> Attributes = State.Layer < Walkmesh.Layers.size() ? ActorFloorAttributes(State, Walkmesh) : std::optional<uint32>(0u);
		if (!Attributes)
		{
			return false;
		}
		State.FloorAttributes = *Attributes;
		const FFieldFloor& Floor = Floors[State.Layer];
		State.FloorNormal = Floor.Normal;
		Record.Translation = {X, Floor.Height, Z};
		Record.Position = Record.Translation;
		State.Position = {static_cast<int32>(static_cast<uint32>(X) << 16), Floor.Height * 0x10000, static_cast<int32>(static_cast<uint32>(Z) << 16)};
		State.Velocity = {};
		State.Step = {};
		State.MoveWork = {};
		State.Word62 = 0;
		State.InteractX = 0;
		State.InteractZ = 0;
		State.WordF0 = 0;
		State.TargetHeight = 0;
		State.FloorHeight = Floor.Height;
		State.Flags0 = (State.Flags0 & ~EventActorFlags0::FixedHeight) | EventActorFlags0::FollowsFloor;
		return true;
	}
}
