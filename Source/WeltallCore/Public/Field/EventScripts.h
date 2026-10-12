#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	/** Where the party appears when it arrives on a map by one of its entrances. */
	struct FEventEntrance
	{
		int16 X = 0;
		int16 Z = 0;
		uint8 Layer = 0;
		/** A direction 0-7 for the camera, or KeepDirection: the one the previous map left. */
		uint8 CameraDirection = 0;
		/** A direction 0-7 the leader faces, or KeepDirection: the one it faced when it left the previous map. */
		uint8 Facing = 0;

		static constexpr uint8 KeepDirection = 0xFF;

		/** The camera's yaw (4096 a turn) for a direction 0-7; only the direction's low three bits count. */
		static uint16 CameraYaw(int32 Direction) { return static_cast<uint16>(((Direction + 4) & 7) << 9); }
		/** The leader's facing angle for a direction 0-7, marked as set. */
		static uint16 FacingAngle(int32 Direction) { return static_cast<uint16>(((Direction - 2) & 7) << 9 | 0x8000); }
	};

	/**
	 * A field map's event scripts: every script actor's 32 entry points into one bytecode, and which
	 * of the 1,024 event variables its scripts read as unsigned. Script 0 of an actor runs when the map
	 * loads, 1 is its default loop, 2 runs when the player talks to it and 3 when the player touches it;
	 * scripts start other scripts. An entry point of 0 is unused, except where the bytecode starts with
	 * a script.
	 */
	class FEventScripts
	{
	public:
		static constexpr int32 ScriptsPerActor = 32;
		static constexpr int32 VariableCount = 0x400;
		/** Bytes that may be read past the bytecode's end: an instruction's operands at its last byte. */
		static constexpr std::size_t ReadSlack = 0x20;

		/** Reads the map's script section. */
		WELTALLCORE_API static TResult<FEventScripts> Parse(std::span<const uint8> Section);

		int32 ActorCount() const { return static_cast<int32>(EntryPoints.size() / ScriptsPerActor); }

		uint16 EntryPoint(int32 Actor, int32 Script) const { return EntryPoints[static_cast<std::size_t>(Actor * ScriptsPerActor + Script)]; }

		/** Whether the scripts read the variable at byte address Address (0 to 0x7FF) as unsigned. */
		bool IsVariableUnsigned(uint32 Address) const { return (UnsignedBits[Address >> 6] >> (Address >> 1 & 31) & 1) != 0; }

		std::size_t CodeSize() const { return Size; }

		/** The bytecode, followed by ReadSlack zero bytes. */
		const uint8* Code() const { return Bytes.data(); }

		/**
		 * Entrance record Index, where the bytecode starts with the entrance table's mark. The table's
		 * length is not recorded: the game reads whatever record the entrance number names.
		 */
		WELTALLCORE_API std::optional<FEventEntrance> Entrance(int32 Index) const;

		/** Changes the bytecode's bytes at Offset from Was to Now; false, changing nothing, where they are not Was. */
		WELTALLCORE_API bool Replace(std::size_t Offset, std::span<const uint8> Was, std::span<const uint8> Now);

		/** Adds Code after the bytecode: its offset, none (adding nothing) where the scripts' 16-bit offsets could not reach its end. */
		WELTALLCORE_API std::optional<uint16> Append(std::span<const uint8> Code);

		/** Makes an actor's script start at Offset of the bytecode. */
		void SetEntryPoint(int32 Actor, int32 Script, uint16 Offset) { EntryPoints[static_cast<std::size_t>(Actor * ScriptsPerActor + Script)] = Offset; }

		/** Has the scripts read the variable at byte address Address (0 to 0x7FF) as unsigned. */
		void SetVariableUnsigned(uint32 Address) { UnsignedBits[Address >> 6] |= uint32{1} << (Address >> 1 & 31); }

	private:
		std::array<uint32, 32> UnsignedBits{};
		std::vector<uint16> EntryPoints;
		std::vector<uint8> Bytes;
		std::size_t Size = 0;
	};
}
