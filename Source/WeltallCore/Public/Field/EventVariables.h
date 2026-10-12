#pragma once

#include "Field/EventScripts.h"
#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	class FGameState;

	/**
	 * The field's 1,024 event variables, 16 bits each. Scripts address a variable by its byte address,
	 * 0 to 0x7FE; an odd address names the variable below it. The first half (0x000-0x3FE) is the
	 * story's state and persists from map to map in the game state; the second half is the map's
	 * scratch, cleared whenever the field is entered.
	 */
	class FEventVariables
	{
	public:
		static constexpr int32 Count = 0x400;
		static constexpr int32 SavedCount = 0x200;
		/** One past the highest byte address. */
		static constexpr uint32 AddressEnd = Count * 2;

		/** The variable at byte address Address (below AddressEnd), widened by the map's signedness. */
		int32 Get(uint32 Address, const FEventScripts& Scripts) const
		{
			const uint16 Value = Values[Address >> 1];
			return Scripts.IsVariableUnsigned(Address) ? int32{Value} : int32{static_cast<int16>(Value)};
		}

		/** Stores the low 16 bits of Value. */
		void Set(uint32 Address, int32 Value) { Values[Address >> 1] = static_cast<uint16>(Value); }

		uint16 Raw(uint32 Address) const { return Values[Address >> 1]; }

		/** Field entry: the saved half from the game state, the map's half cleared. */
		WELTALLCORE_API void EnterField(const FGameState& State);

		/** Copies the saved half into the game state, as the field does every tick and before it exits. */
		WELTALLCORE_API void Save(FGameState& State) const;

	private:
		std::array<uint16, Count> Values{};
	};
}
