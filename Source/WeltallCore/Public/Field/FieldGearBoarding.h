#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>
#include <optional>

namespace Weltall
{
	struct FEventContext;

	/**
	 * A party slot's member boards the slot's gear (the scripts' FE 1F, the buttons, the menu): the
	 * member's actor takes the gear's sprite where the member stands and becomes the gear's pilot, the
	 * gear's actor is hidden, the game state has the member riding, both actors land and play their idle
	 * animations again, the gear actor's effects stop emitting, and the slot's parked-gear variables
	 * take the member's place. False, with the context's fault and nothing changed, when the slot has no
	 * member's actor or no gear actor, or there is no game state.
	 */
	WELTALLCORE_API bool BoardFieldGear(FEventContext& Context, int32 Slot);

	/**
	 * A party slot's member leaves the slot's gear (the scripts' FE 20, the buttons, the menu): the game
	 * state has the member walking, the two actors take their own sprites back, the gear's actor stands
	 * where the member stands, facing as the member faces, shown and unfrozen, playing its standing
	 * animation (6), the member is a party member again playing its idle animation, both land, the slot's
	 * parked-gear variables take the place and the gear actor's effects stop emitting. False, with the
	 * context's fault and nothing changed, as BoardFieldGear.
	 */
	WELTALLCORE_API bool LeaveFieldGear(FEventContext& Context, int32 Slot);

	/**
	 * The slot's parked-gear variables: this map with the member's walkmesh layer in bits 14-15, and the
	 * member's x and z. Nothing for a slot without a member's actor.
	 */
	WELTALLCORE_API void ParkFieldGear(FEventContext& Context, int32 Slot);

	/**
	 * The field's boarding of the given party slots, in gear mode only (the map's pilot sprites): by the
	 * buttons (bByButtons) a slot whose member rides leaves its gear and one whose member walks boards
	 * it; after the menu, which has set who rides, a slot whose member now rides boards and one who
	 * walks leaves. Slots without a member's actor are passed over. An error when a slot has no gear
	 * actor, where the original writes through the table's empty entry.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> BoardOrLeaveFieldGears(FEventContext& Context, const std::array<bool, 3>& Slots, bool bByButtons);
}
