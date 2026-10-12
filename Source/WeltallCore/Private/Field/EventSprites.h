#pragma once

#include "Field/EventContext.h"

namespace Weltall::EventOps
{
	/**
	 * Gives an actor a new sprite as the field does: the sprite, then the script state's record of it
	 * (its file or party slot, colour set, kind, and whether its first step waits), the actor's height,
	 * its animations switched off when the sprite says it has none, and the actor's record moved to
	 * its position. False, with a fault, when there is no sprite for the request.
	 */
	bool LoadActorSprite(FEventContext& Context, int32 Actor, EFieldSpriteKind Kind, int32 Slot, int32 PaletteSet, uint8 Id, bool bDeferred);

	/** Makes the running actor one that the sprite system draws. */
	void MarkSpriteActor(FEventContext& Context);

	/**
	 * Gives the running actor the map's first sprite, its first step not yet run, kept out of sight:
	 * the scripts show it later or never. False, with a fault, when the map has none.
	 */
	bool GiveHiddenMapSprite(FEventContext& Context);

	/** The party slot a character is in, or -1: the party list ends at its first empty slot. */
	int32 PartySlotOf(const FEventContext& Context, int32 Character);

	/** Moves the running actor's record and sprite to where its script state has it, the sprite at rest on its feet. */
	void SyncRecordPosition(FEventContext& Context);

	/** The actor's sprite, which placements move with the actor; null when it has none. */
	FSprite* ActorSprite(FEventContext& Context, int32 Actor);

	/**
	 * Moves the running actor onto the floor at (X, Z) of its layer, its sprite with it and at rest; or
	 * reports the walkmesh as unable to hold it, or the layer as one the map does not have, where the
	 * original has no floor to give it. With bAnyLayer such a layer is taken: the actor stands without a
	 * floor, as on the camera's.
	 */
	bool PlaceOnFloor(FEventContext& Context, int32 X, int32 Z, bool bAnyLayer = false);

	/** PlaceOnFloor for any actor. */
	bool PlaceActorOnFloor(FEventContext& Context, int32 Actor, int32 X, int32 Z, bool bAnyLayer = false);
}
