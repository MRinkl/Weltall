#pragma once

#include "Field/EventContext.h"
#include "Support/FieldSongsModel.h"

#include <vector>

namespace Weltall::Testing
{
	/**
	 * The camera's words the scripts set, the field's settings and scenery and the music's globals,
	 * laid out as the event oracles read them from the original's memory
	 * (the camera, the field's state, the music, then the music's stops and the
	 * walkmesh's attribute words).
	 */
	std::vector<uint8> FieldStateBytes(const FEventContext& Context, const FFieldSongsModel& Songs);
}
