#pragma once

#include "Foundation/Types.h"

#include <cstddef>
#include <span>
#include <type_traits>

namespace Weltall::BattleRecordBytes
{
	/** Reads a member of a game record from its little-endian bytes at Offset, at the member's width. */
	template <typename T>
	void Read(std::span<const uint8> Bytes, std::size_t Offset, T& Member)
	{
		static_assert(std::is_integral_v<T>);
		using FUnsigned = std::make_unsigned_t<T>;
		FUnsigned Value = 0;
		for (std::size_t Index = 0; Index < sizeof(T); ++Index)
		{
			Value = static_cast<FUnsigned>(Value | static_cast<FUnsigned>(Bytes[Offset + Index]) << (8 * Index));
		}
		Member = static_cast<T>(Value);
	}

	/** A visitor that fills every member it is shown from a record's bytes. */
	struct FReader
	{
		std::span<const uint8> Bytes;

		template <typename T>
		void operator()(std::size_t Offset, T& Member) const
		{
			Read(Bytes, Offset, Member);
		}
	};
}
