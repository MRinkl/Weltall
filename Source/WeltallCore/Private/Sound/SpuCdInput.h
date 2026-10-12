#pragma once

#include "Foundation/Types.h"

#include <algorithm>
#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	/** The SPU's CD input: the stereo frames the drive has delivered and the SPU has not taken yet. */
	class FSpuCdInput
	{
	public:
		explicit FSpuCdInput(std::size_t InCapacity)
			: Ring(InCapacity * 2), Capacity(InCapacity)
		{
		}

		/** Appends interleaved frames; returns how many fitted. */
		std::size_t Queue(std::span<const int16> Stereo)
		{
			const std::size_t Frames = std::min(Stereo.size() / 2, Capacity - Queued);
			for (std::size_t Frame = 0; Frame < Frames; ++Frame)
			{
				const std::size_t At = (Next + Queued + Frame) % Capacity;
				Ring[At * 2] = Stereo[Frame * 2];
				Ring[At * 2 + 1] = Stereo[Frame * 2 + 1];
			}
			Queued += Frames;
			return Frames;
		}

		std::size_t QueuedFrames() const { return Queued; }

		void Clear() { Queued = 0; }

		/** The next frame, or silence when none is left. */
		std::array<int32, 2> Take()
		{
			if (Queued == 0)
			{
				return {0, 0};
			}
			const std::array<int32, 2> Frame{Ring[Next * 2], Ring[Next * 2 + 1]};
			Next = (Next + 1) % Capacity;
			--Queued;
			return Frame;
		}

	private:
		std::vector<int16> Ring;
		std::size_t Capacity;
		std::size_t Next = 0;
		std::size_t Queued = 0;
	};
}
