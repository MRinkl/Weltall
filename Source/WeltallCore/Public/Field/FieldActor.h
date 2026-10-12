#pragma once

#include "Field/FieldArchive.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * An actor of a field map as the field keeps it while the map runs: its kind and drawing flags,
	 * its model's placement, and its integer position as the actors' updates leave it. Script actors
	 * also have an FEventActorState.
	 */
	struct FFieldActor
	{
		/** FieldActorFlag bits, changed by scripts. */
		uint16 Flags = 0;
		/** Where its model is drawn. */
		std::array<int32, 3> Translation{};
		/** Its integer position, refreshed by the actors' updates. */
		std::array<int32, 3> Position{};
		/** 4096 is a turn. */
		FGteVector Rotation{};
		/**
		 * Its model's rotation and scale (4.12) as the scripts last built them from Rotation and the
		 * actor's scale; the translation is Translation. Zero until the map's load or a script builds it.
		 */
		std::array<std::array<int16, 3>, 3> Matrix{};
		/**
		 * Its place in the world as the scenery's last update left it: Matrix under its parent's world
		 * matrix when it is attached to another actor's model, else Matrix itself; the translation is
		 * Position.
		 */
		std::array<std::array<int16, 3>, 3> WorldMatrix{};
		/**
		 * While bCarried, what the scenery's last update placed its model under: its parent's world block as it
		 * stood at this actor's turn, or the joint of the gear it rides. The drawing composes the view with it
		 * first, then with the model's own matrix.
		 */
		FGteMatrix Carrier{};
		bool bCarried = false;
		/**
		 * The vertices its model is drawn with when a MIMe animation moves them: the model's shared ones
		 * as the scenery's last update left them at this actor's turn. Empty when they never move.
		 */
		std::vector<FGteVector> DrawnVertices{};
		/** Its model's kernel draw mode (FieldModelDrawMode), as the field's last pass over the models left it. */
		uint8 DrawMode = 0;
		/** Its model's number in the map's list; meaningless without one. */
		uint16 Model = 0;

		/** Whether the event scripts see it at all: an actor whose kind bits are clear takes no part. */
		bool TakesPartInEvents() const { return (Flags & 0xF00) != 0; }
	};

	/**
	 * The map's actors as the field creates them from their definitions: an actor without a model is
	 * hidden and unrotated, and positions keep the definition's 16 bits.
	 */
	WELTALLCORE_API std::vector<FFieldActor> CreateFieldActors(std::span<const FFieldActorDefinition> Definitions);

	/**
	 * The field's pass over its models' draw modes, after the map's init and when the scripts turn
	 * fog on: every actor with a model takes the mode its flags and the fog give.
	 */
	WELTALLCORE_API void SetFieldModelDrawModes(std::span<FFieldActor> Actors, bool bFog);
}
