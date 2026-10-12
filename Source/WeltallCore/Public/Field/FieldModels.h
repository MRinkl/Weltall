#pragma once

#include "Field/EventContext.h"
#include "Foundation/Result.h"

#include <optional>

namespace Weltall
{
	/**
	 * The map's MIMe animations as its load makes them, once the actors are created: each model an
	 * actor with the animation flag places gets vertices of its own (MimeVertices), which every actor
	 * placing it draws (DrawnVertices). An error for a MIMe that moves its part's normals, or one with
	 * more keys than a script actor's part values, which no map has.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> SetUpFieldMime(FEventContext& Context);

	/**
	 * What the field's drawing of its models leaves for the next tick, as it runs once a tick: every
	 * actor's world matrix and position taken from its own (a model attached to another actor's under
	 * that actor's world matrix, in actor order, so a parent later in the list lends last tick's), then
	 * the drifting models moved by the scenery's drift step and the drift's totals advanced; each shown
	 * model with a MIMe animation updates its shared vertices (a script actor's keys weighed by its part
	 * values, another's by 0), and each shown actor whose model's vertices move takes them as they
	 * stand. A model riding a gear's joint is placed by the gears' module, which the port does not have
	 * yet: an error.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> UpdateFieldModels(FEventContext& Context);
}
