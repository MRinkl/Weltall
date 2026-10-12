#pragma once

#include "Field/FieldFrame.h"
#include "Field/FieldSceneFrame.h"
#include "Foundation/Result.h"
#include "Menu/MenuPrimitive.h"

#include <optional>

namespace Weltall
{
	class FGearShopModel;

	/** The gear shop's ordering table: 1024 entries, the depths taken whole (the kernel's shift 0). */
	inline constexpr int32 GearShopFarBucket = 0x3FF;
	inline constexpr int32 GearShopOrderingShift = 0;

	/**
	 * The gear shop's gear as a frame of the field's drawing, for the renderer's scene passes under the
	 * menu's own frame: the model evaluated as the field's gears are, through the shop's camera (its
	 * view, the menu's projection distance) and its one light, sorted as the shop's table sorts it,
	 * nothing else in the scene (black behind it). Tick counts the menu's frames: frames of consecutive
	 * ticks blend. A frame whose gear is not shown draws nothing.
	 */
	/** The gear shop's lights as the gears' drawing takes them: its one direction, its colours, its ambient. */
	WELTALLCORE_API const FFieldGearLights& GearShopLights();

	[[nodiscard]] WELTALLCORE_API std::optional<FError> EvaluateGearShopFrame(const FGearShopModel& Model, const FMenuGearView& Gear, uint32 Tick,
		EFieldCulling Culling, FFieldFrame& Frame);
}
