#pragma once

#include "Foundation/Types.h"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	namespace StrSector
	{
		/** A video sector's user data: a 32-byte header, then its share of the frame. */
		inline constexpr std::size_t HeaderBytes = 32;
		inline constexpr std::size_t PayloadBytes = 2016;
	}

	/** The header of one sector of a streamed MDEC video frame. */
	struct FStrSectorHeader
	{
		/** This sector's place in the frame, from 0, and the frame's sector count. */
		uint16 SectorIndex = 0;
		uint16 SectorCount = 0;
		/** Counted from 1 within the movie. */
		uint32 FrameNumber = 0;
		/** The frame's compressed bytes over all its sectors. */
		uint32 FrameBytes = 0;
		uint16 Width = 0;
		uint16 Height = 0;
	};

	/** The header of a video sector's user data, or nothing when the sector holds no MDEC video. */
	WELTALLCORE_API std::optional<FStrSectorHeader> ParseStrSector(std::span<const uint8> UserData);

	/** A compressed video frame put together from its sectors. */
	struct FStrFrame
	{
		uint32 FrameNumber = 0;
		uint16 Width = 0;
		uint16 Height = 0;
		std::vector<uint8> Bitstream;
	};

	/**
	 * Collects video sectors in disc order into whole frames. A frame whose sectors arrive incomplete
	 * or out of order is dropped, as a player drops a frame it could not read.
	 */
	class FStrFrameAssembler
	{
	public:
		/** Adds a video sector's user data; returns the frame once its last sector is in. */
		WELTALLCORE_API std::optional<FStrFrame> Add(std::span<const uint8> UserData);

	private:
		FStrFrame Pending;
		uint16 NextSector = 0;
		uint16 SectorCount = 0;
		uint32 FrameBytes = 0;
	};
}
