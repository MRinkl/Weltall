#include "Formats/Lzss.h"

#include "Foundation/LittleEndian.h"

#include <string>

namespace Weltall
{
	namespace
	{
		/** The game unpacks into the console's 2 MB of RAM, so no real stream is larger. */
		constexpr uint32 MaxUnpackedSize = 0x200000;
		constexpr std::size_t SizeFieldBytes = 4;
		constexpr int32 MinimumLength = 3;

		enum class EStop : uint8
		{
			/** Only between groups, where the game compares the output with the stream's size. */
			BetweenGroups,
			/** As soon as the output holds the bytes asked for, even inside a reference. */
			AtLimit,
		};

		TResult<uint32> ReadSize(std::span<const uint8> Stream)
		{
			if (Stream.size() < SizeFieldBytes)
			{
				return MakeError("LZSS stream has no size");
			}
			const uint32 Size = LittleEndian::ReadU32(Stream, 0);
			if (Size > MaxUnpackedSize)
			{
				return MakeError("LZSS stream claims " + std::to_string(Size) + " bytes, more than the console's RAM");
			}
			return Size;
		}

		TResult<std::vector<uint8>> Decode(std::span<const uint8> Stream, uint32 Limit, EStop Stop)
		{
			std::vector<uint8> Output;
			Output.reserve(Limit);
			std::size_t Position = SizeFieldBytes;
			const auto Truncated = [] { return MakeError("LZSS stream ends before its data does"); };
			while (Output.size() < Limit)
			{
				if (Position >= Stream.size())
				{
					return Truncated();
				}
				const uint8 Flags = Stream[Position++];
				for (int32 Bit = 0; Bit < 8; ++Bit)
				{
					if (Stop == EStop::AtLimit && Output.size() == Limit)
					{
						break;
					}
					if ((Flags >> Bit & 1) == 0)
					{
						if (Position >= Stream.size())
						{
							return Truncated();
						}
						Output.push_back(Stream[Position++]);
						continue;
					}
					if (Position + 2 > Stream.size())
					{
						return Truncated();
					}
					const uint8 Low = Stream[Position];
					const uint8 High = Stream[Position + 1];
					Position += 2;
					const std::size_t Distance = Low | (High & 0x0F) << 8;
					// The game would copy whatever memory lies there; none of its streams does either.
					if (Distance == 0 || Distance > Output.size())
					{
						return MakeError("LZSS reference outside the output written so far");
					}
					const int32 Length = (High >> 4) + MinimumLength;
					const std::size_t Start = Output.size() - Distance;
					for (int32 Index = 0; Index < Length; ++Index)
					{
						if (Stop == EStop::AtLimit && Output.size() == Limit)
						{
							break;
						}
						// Copied a byte at a time, so a reference may overlap the bytes it produces.
						const uint8 Byte = Output[Start + static_cast<std::size_t>(Index)];
						Output.push_back(Byte);
					}
				}
			}
			if (Output.size() > Limit)
			{
				return MakeError("LZSS stream passes its size inside a group");
			}
			return Output;
		}
	}

	TResult<std::vector<uint8>> UnpackLzss(std::span<const uint8> Stream)
	{
		const TResult<uint32> Size = ReadSize(Stream);
		if (!Size)
		{
			return Size.Error();
		}
		return Decode(Stream, *Size, EStop::BetweenGroups);
	}

	TResult<std::vector<uint8>> UnpackLzssPrefix(std::span<const uint8> Stream, uint32 Count)
	{
		const TResult<uint32> Size = ReadSize(Stream);
		if (!Size)
		{
			return Size.Error();
		}
		if (Count > *Size)
		{
			return MakeError("LZSS stream holds " + std::to_string(*Size) + " bytes, fewer than the " + std::to_string(Count) + " asked for");
		}
		return Decode(Stream, Count, EStop::AtLimit);
	}
}
