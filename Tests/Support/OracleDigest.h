#pragma once

#include "Foundation/Sha1.h"
#include "Foundation/Types.h"
#include "Support/XorShift32.h"

#include <span>
#include <string>
#include <vector>

namespace Weltall::Testing
{
	/** The SHA-1 of values written little-endian at the width the original writes them, as the oracle scripts take it. */
	class FOracleDigest
	{
	public:
		template <typename... TValues>
		void I16(TValues... Values)
		{
			(Bytes<2>(static_cast<uint32>(Values)), ...);
		}

		template <typename... TValues>
		void I32(TValues... Values)
		{
			(Bytes<4>(static_cast<uint32>(Values)), ...);
		}

		template <typename... TValues>
		void U8(TValues... Values)
		{
			(Bytes<1>(static_cast<uint32>(Values)), ...);
		}

		/** Bytes as they lie in memory. */
		void Raw(std::span<const uint8> Values)
		{
			Hash.Update(Values);
			if (Kept != nullptr)
			{
				Kept->insert(Kept->end(), Values.begin(), Values.end());
			}
		}

		/** Every byte digested from here on is also appended to Bytes (none: no longer), for comparing a digest's parts with the oracle's. */
		void KeepIn(std::vector<uint8>* Bytes) { Kept = Bytes; }

		std::string Hex() { return FSha1::ToHex(Hash.Finish()); }

		/** The digest's own bytes, for a digest of digests. */
		FSha1::FDigest Finish() { return Hash.Finish(); }

	private:
		template <int32 Count>
		void Bytes(uint32 Value)
		{
			uint8 Little[Count];
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Little[Index] = static_cast<uint8>(Value >> (8 * Index));
			}
			Raw(Little);
		}

		FSha1 Hash;
		std::vector<uint8>* Kept = nullptr;
	};

	/** A signed value of Bits bits from the generator, as the oracle scripts draw them. */
	inline int32 DrawSigned(FXorShift32& Random, int32 Bits)
	{
		const uint32 Value = Bits == 32 ? Random.Next() : Random.Next() & ((1u << Bits) - 1);
		const bool bNegative = ((Value >> (Bits - 1)) & 1) != 0;
		return bNegative ? static_cast<int32>(static_cast<int64>(Value) - (int64{1} << Bits)) : static_cast<int32>(Value);
	}

	/** A signed value of Low to High bits (the count drawn first). */
	inline int32 DrawSignedBits(FXorShift32& Random, int32 Low, int32 High)
	{
		const int32 Bits = Low + static_cast<int32>(Random.Next() % static_cast<uint32>(High - Low + 1));
		return DrawSigned(Random, Bits);
	}
}
