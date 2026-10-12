#pragma once

#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	/**
	 * How a routine of the show goes on: back to its caller, into the routine it has just called, at once from
	 * the resume point it has just set (a jump within it), or at the next frame.
	 */
	enum class EShowFlow : uint8
	{
		Done,
		Call,
		Again,
		Frame,
	};

	/**
	 * One of the show's routines under way: which, where it goes on, and what it keeps between frames
	 * (the original's locals across its battle frames).
	 */
	struct FShowRoutine
	{
		uint8 Kind = 0;
		uint8 Resume = 0;
		int32 Value = 0;
		int32 Other = 0;
	};

	/**
	 * The routines the show runs across frames, innermost last: the original's nested calls that each
	 * wait for frames of their own (a playback's step inside a frame, the wait for a collapse inside it).
	 * Kept memory: routines nest a few deep at most.
	 */
	class FShowRoutineStack
	{
	public:
		static constexpr int32 Capacity = 16;

		bool IsEmpty() const { return Size == 0; }
		FShowRoutine& Top() { return Routines[static_cast<std::size_t>(Size - 1)]; }
		/** False when the stack is full (the caller faults). */
		bool Push(uint8 Kind, int32 Value = 0, int32 Other = 0)
		{
			if (Size >= Capacity)
			{
				return false;
			}
			Routines[static_cast<std::size_t>(Size++)] = {Kind, 0, Value, Other};
			return true;
		}
		void Pop() { --Size; }
		void Clear() { Size = 0; }

	private:
		std::array<FShowRoutine, Capacity> Routines{};
		int32 Size = 0;
	};
}
