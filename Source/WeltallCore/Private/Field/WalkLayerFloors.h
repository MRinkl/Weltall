#pragma once

#include "Field/EventContext.h"
#include "Graphics/GteVectors.h"

#include <array>

// The floors under an actor's step on every walking layer, as the layer step finds and sorts them.
namespace Weltall::WalkLayerFloors
{
	inline constexpr int32 NoFloor = 0x7FFFFFFF;
	inline constexpr int32 LayerSlots = 4;
	/** The floors are sorted by two passes over their first three: every walking layer a map can have. */
	inline constexpr int32 SortedLayers = 3;

	enum class ESearch : uint8
	{
		Found,
		None,
		OutsideWalkmesh,
	};

	/** Every walking layer's floor under the step, kept by layer (triangles, normals) and sorted by height (the rest). */
	struct FLayerFloors
	{
		std::array<int32, LayerSlots> Floor{NoFloor, NoFloor, NoFloor, NoFloor};
		/** The floor's underside: a triangle's positive ceiling byte gives its floor a thickness of 4 units a step. */
		std::array<int32, LayerSlots> Underside{NoFloor, NoFloor, NoFloor, NoFloor};
		std::array<int32, LayerSlots> Layer{0, 1, 2, 3};
		std::array<int16, LayerSlots> Triangles{};
		std::array<FGteLongVector, LayerSlots> Normals{};
	};

	/**
	 * The floor under the actor's position plus its step on one layer, traced from its triangle there
	 * through neighbours only, without the attributes' rules. A floor closed to the actor (0x800000,
	 * unless it ignores the layer's attributes or the party walks) is none. The actor's own layer takes
	 * the height its move found, unless it did not move.
	 */
	ESearch FloorOnLayer(const FEventContext& Context, const FEventActorState& State, int32 Layer, FLayerFloors& Floors);

	/** Two passes of a bubble sort over the first three floors, which carries each floor's layer along. */
	void SortFloors(FLayerFloors& Floors);
}
