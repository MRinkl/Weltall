#pragma once

#include "Foundation/Types.h"
#include "Graphics/GteMatrix.h"
#include "Support/EventScenarioProgram.h"
#include "Support/EventStateLayout.h"
#include "Text/MessageTable.h"

#include <array>
#include <vector>

namespace Weltall::Testing
{
	/** The maps whose messages the message scenarios open. */
	inline constexpr std::array<int32, 11> MessageScenarioMaps = {5, 50, 100, 120, 180, 250, 300, 350, 400, 529, 600};
	inline constexpr int32 MessageScenarioTicks = 200;

	/** A tick's buttons: the field's pressed word and its repeat word. */
	struct FMessageScenarioPads
	{
		uint16 Pressed = 0;
		uint16 Repeat = 0;
	};

	/**
	 * A random map for the message windows: actors whose default scripts run a random program of the
	 * message opcodes on the map's messages, with random states where the windows read them, random
	 * records, variables, names and party, a random view and projection distance, and each tick's
	 * buttons. It is drawn from XorShift32 exactly as the oracle that ran the original windows
	 * on the same scenarios draws it.
	 */
	struct FMessageScenario
	{
		std::vector<uint8> Section;
		std::vector<std::array<uint8, 0x5C>> Records;
		std::vector<std::array<uint8, EventStateBytes>> States;
		std::array<uint16, 0x400> Variables{};
		std::vector<uint8> GameState;
		FGteMatrix View;
		uint16 ProjectionDistance = 0;
		int16 GoalViewYaw = 0;
		std::vector<FMessageScenarioPads> Pads;
		/** The ticks the map's entry lasts, when no window opens. */
		int32 EnteringTicks = 0;
		/** The tick after whose windows' update every window closes, as a map change closes them; -1 none. */
		int32 CloseAllTick = -1;
	};

	/** The rest of a scenario once its map is drawn (MessageScenarioMaps[Random.Below(size)]): its messages are the map's. */
	FMessageScenario GenerateMessageScenario(FScenarioRandom& Random, const FMessageTable& Messages);
}
