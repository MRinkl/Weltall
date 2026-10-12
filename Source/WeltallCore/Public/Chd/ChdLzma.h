#pragma once

#include "Chd/ChdTypes.h"

#include <array>
#include <vector>

namespace Chd
{
	/**
	 * Unpacks raw LZMA streams: no header, the properties known beforehand, the size known and an end mark optional.
	 * A CHD's lzma codec packs each hunk so, with lc=3, lp=0, pb=2.
	 */
	class FLzmaDecoder
	{
	public:
		/** Literal context bits, literal position bits and position bits, as an LZMA stream's properties name them. */
		FLzmaDecoder(uint32 InLc, uint32 InLp, uint32 InPb);

		/** Unpacks Packed, which must be a whole stream of exactly Destination's size; false when it is not. */
		bool Decode(std::span<const uint8> Packed, std::span<uint8> Destination);

	private:
		static constexpr uint32 States = 12;
		static constexpr uint32 MostPositionStates = 16;

		/** The probabilities of a match length's code: short, medium and long lengths, the first two per position. */
		struct FLengthModel
		{
			uint16 Choice;
			uint16 Choice2;
			std::array<std::array<uint16, 8>, MostPositionStates> Low;
			std::array<std::array<uint16, 8>, MostPositionStates> Mid;
			std::array<uint16, 256> High;
		};

		/** Every probability the stream's bits are coded with, all reset for each stream. */
		struct FModel
		{
			std::array<uint16, States * MostPositionStates> IsMatch;
			std::array<uint16, States> IsRep;
			std::array<uint16, States> IsRepG0;
			std::array<uint16, States> IsRepG1;
			std::array<uint16, States> IsRepG2;
			std::array<uint16, States * MostPositionStates> IsRep0Long;
			std::array<std::array<uint16, 64>, 4> DistanceSlot;
			std::array<uint16, 115> DistanceMiddle;
			std::array<uint16, 16> DistanceAlign;
			FLengthModel Length;
			FLengthModel RepLength;
		};

		uint32 Lc;
		uint32 Lp;
		uint32 Pb;
		FModel Model{};
		std::vector<uint16> Literals;
	};
}
