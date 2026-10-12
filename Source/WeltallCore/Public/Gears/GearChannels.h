#pragma once

#include "Foundation/Types.h"

#include <array>
#include <vector>

namespace Weltall
{
	struct FGearMotionPack;

	/** A place in a gear's motion pack: a script's instruction, a timeline's event, a channel's stream. */
	struct FGearCodeRef
	{
		FGearMotionPack* Pack = nullptr;
		uint32 Offset = 0;

		explicit operator bool() const { return Pack != nullptr; }
		bool operator==(const FGearCodeRef&) const = default;

		FGearCodeRef operator+(int32 Delta) const { return {Pack, Offset + static_cast<uint32>(Delta)}; }
	};

	/** A channel's kind byte: the kind in the low nibble; for the stream kinds, the axes the stream leaves out. */
	namespace GearChannelKind
	{
		/** A value per tick for each axis it holds, as they are. */
		constexpr uint8 Stream = 0;
		/** A signed byte added per tick, or 80h and the value itself in the next two bytes. */
		constexpr uint8 PackedDeltas = 1;
		/** A 16-bit step per tick, added; a translation's is turned by the bone's world matrix and scaled. */
		constexpr uint8 Deltas = 2;
		constexpr uint8 Linear = 3;
		/** A step of the rest of the way over the length, until the steps round to nothing. */
		constexpr uint8 Approach = 4;
		/** A velocity the acceleration grows each tick, added. */
		constexpr uint8 Accelerate = 5;
		constexpr uint8 Hold = 6;
		/** The yaw and pitch turned toward a point, never ending. */
		constexpr uint8 TurnToward = 7;
		constexpr uint8 TurnTowardYaw = 8;
		constexpr uint8 Mask = 0x0F;
		constexpr uint8 AbsentX = 0x10;
		constexpr uint8 AbsentY = 0x20;
		constexpr uint8 AbsentZ = 0x40;
	}

	namespace GearChannelTag
	{
		/** Kept when the others are freed or replaced. */
		constexpr uint8 Locked = 0xFF;
		/** The script's own turns of the root. */
		constexpr uint8 RootTurns = 0xFE;
	}

	/**
	 * One animated value of a bone, 14h bytes in the original's shared pool: a stream played from a
	 * record, or a move from one value to another, its tick and length. Words holds from (0-2) and to
	 * (3-5): a delta, a target or an acceleration by kind; a turn keeps its distance, gain and rate in
	 * from. A stream's start and read place alias the words in the original; each writer sets what its
	 * kind reads.
	 */
	struct FGearChannel
	{
		bool bUsed = false;
		uint8 Loop = 0;
		uint8 Kind = 0;
		uint8 Tag = 0;
		std::array<int16, 6> Words{};
		FGearCodeRef StreamStart;
		FGearCodeRef StreamRead;
		int16 Tick = 0;
		int16 Length = 0;

		uint8 KindNumber() const { return Kind & GearChannelKind::Mask; }
		bool IsStream() const { return KindNumber() <= GearChannelKind::Deltas; }
	};

	/**
	 * The channels every gear model of a screen shares: a fixed number, handed out lowest free first.
	 * When every one is in use a request gets none, and the bone asking stays as it is, unless the
	 * pool is told to grow (the original's pool of a map's init is too small for some maps' gears).
	 */
	class FGearChannelPool
	{
	public:
		static constexpr int32 None = -1;

		/** Capacity channels, none in use. */
		WELTALLCORE_API void Init(int32 Capacity, bool bGrowsWhenFull);
		/** The channels are given back: none in use, none to hand out. The capacity stays as it was. */
		WELTALLCORE_API void Release();
		/** The lowest free channel, or None; it counts as in use once its caller marks it. */
		WELTALLCORE_API int32 Allocate();
		WELTALLCORE_API void Free(int32 Index);

		bool HasEntries() const { return !Entries.empty(); }
		uint16 LowestFree() const { return Lowest; }
		uint16 Capacity() const { return Size; }
		FGearChannel& operator[](int32 Index) { return Entries[static_cast<std::size_t>(Index)]; }
		const FGearChannel& operator[](int32 Index) const { return Entries[static_cast<std::size_t>(Index)]; }

	private:
		std::vector<FGearChannel> Entries;
		uint16 Lowest = 0;
		uint16 Size = 0;
		bool bGrows = false;
	};
}
