#pragma once

#include "Foundation/Types.h"

#include <array>
#include <span>

namespace Weltall
{
	/**
	 * The SPU's reverb unit: at half the sample rate it runs its reflections, combs and all-pass
	 * stages on a work area at the top of sound RAM, the input brought down and the output brought up
	 * through the hardware's 39-tap half-band filter.
	 */
	class FSpuReverb
	{
	public:
		static constexpr int32 RegisterCount = 32;

		/** A configuration register, 0 to 31 for 1F801DC0 to 1F801DFE. */
		void WriteRegister(int32 Index, uint16 Value) { Registers[static_cast<std::size_t>(Index)] = Value; }
		uint16 Register(int32 Index) const { return Registers[static_cast<std::size_t>(Index)]; }

		/** The work area's start (ESA, in 8-byte units); writing it moves the current address there too. */
		void SetStart(uint16 Start);
		uint16 Start() const { return StartUnits; }

		/**
		 * One sample: takes the mix of the voices sent to the reverb and gives its output, before the
		 * output volume. The work area is written only while Writes is set.
		 */
		std::array<int32, 2> Process(std::array<int32, 2> Input, std::span<uint8> Ram, bool bWrites);

	private:
		/** The resampling filters see this many samples back, kept twice over for straight windows. */
		static constexpr int32 HistorySize = 64;

		int32 RunSide(int32 Side, int32 Input, std::span<uint8> Ram, bool bWrites);
		int16 Read(std::span<const uint8> Ram, int32 Offset) const;
		void Write(std::span<uint8> Ram, int32 Offset, int32 Value) const;

		std::array<uint16, RegisterCount> Registers{};
		uint16 StartUnits = 0;
		/** Byte address of the current buffer position. */
		uint32 Current = 0;
		std::array<std::array<int16, 2 * HistorySize>, 2> Downsample{};
		std::array<std::array<int16, 2 * HistorySize>, 2> Upsample{};
		int32 Position = 0;
	};
}
