#include "Chd/ChdZstd.h"

#include "Chd/ChdZstdBlock.h"

#include <array>
#include <bit>
#include <optional>

namespace Chd
{
	namespace
	{
		constexpr uint32 FrameMagic = 0xFD2FB528;
		/** Sixteen magic numbers mark frames of other data, to be passed over. */
		constexpr uint32 SkippableMagic = 0x184D2A50;
		constexpr uint32 MostBlockBytes = 128 * 1024;

		/** XXH64 with seed 0, of which a frame's checksum keeps the low 32 bits. */
		uint64 Xxh64(std::span<const uint8> Bytes)
		{
			constexpr uint64 Prime1 = 0x9E3779B185EBCA87ull;
			constexpr uint64 Prime2 = 0xC2B2AE3D27D4EB4Full;
			constexpr uint64 Prime3 = 0x165667B19E3779F9ull;
			constexpr uint64 Prime4 = 0x85EBCA77C2B2AE63ull;
			constexpr uint64 Prime5 = 0x27D4EB2F165667C5ull;
			const auto Round = [](uint64 Accumulator, uint64 Lane) { return std::rotl(Accumulator + Lane * Prime2, 31) * Prime1; };
			std::size_t At = 0;
			uint64 Hash = 0;
			if (Bytes.size() >= 32)
			{
				std::array<uint64, 4> Lanes{Prime1 + Prime2, Prime2, 0, 0 - Prime1};
				for (; At + 32 <= Bytes.size(); At += 32)
				{
					for (std::size_t Lane = 0; Lane < 4; ++Lane)
					{
						Lanes[Lane] = Round(Lanes[Lane], Zstd::ReadLittle(Bytes, At + Lane * 8, 8));
					}
				}
				Hash = std::rotl(Lanes[0], 1) + std::rotl(Lanes[1], 7) + std::rotl(Lanes[2], 12) + std::rotl(Lanes[3], 18);
				for (const uint64 Lane : Lanes)
				{
					Hash = (Hash ^ Round(0, Lane)) * Prime1 + Prime4;
				}
			}
			else
			{
				Hash = Prime5;
			}
			Hash += Bytes.size();
			for (; At + 8 <= Bytes.size(); At += 8)
			{
				Hash = std::rotl(Hash ^ Round(0, Zstd::ReadLittle(Bytes, At, 8)), 27) * Prime1 + Prime4;
			}
			if (At + 4 <= Bytes.size())
			{
				Hash = std::rotl(Hash ^ Zstd::ReadLittle(Bytes, At, 4) * Prime1, 23) * Prime2 + Prime3;
				At += 4;
			}
			for (; At < Bytes.size(); ++At)
			{
				Hash = std::rotl(Hash ^ Bytes[At] * Prime5, 11) * Prime1;
			}
			Hash = (Hash ^ Hash >> 33) * Prime2;
			Hash = (Hash ^ Hash >> 29) * Prime3;
			return Hash ^ Hash >> 32;
		}

		/** Decodes a frame after its magic number into Destination from Out on, moving Out past it; returns the bytes it took. */
		std::optional<std::size_t> Frame(Zstd::FBlockDecoder& Blocks, std::span<const uint8> Bytes, std::span<uint8> Destination, std::size_t& Out)
		{
			const uint32 Descriptor = Bytes[0];
			const uint32 SizeFlag = Descriptor >> 6;
			const bool bSingleSegment = (Descriptor >> 5 & 1) != 0;
			const bool bChecksum = (Descriptor >> 2 & 1) != 0;
			const uint32 DictionaryFlag = Descriptor & 3;
			if ((Descriptor >> 3 & 1) != 0)
			{
				return std::nullopt;
			}
			constexpr std::array<std::size_t, 4> DictionaryBytes{0, 1, 2, 4};
			const std::size_t ContentSizeBytes = SizeFlag == 0 ? (bSingleSegment ? 1 : 0) : std::size_t{1} << SizeFlag;
			std::size_t At = 1 + (bSingleSegment ? 0 : 1);
			if (Bytes.size() < At + DictionaryBytes[DictionaryFlag] + ContentSizeBytes)
			{
				return std::nullopt;
			}
			// No dictionary is known here: a frame made with one cannot be decoded.
			if (Zstd::ReadLittle(Bytes, At, DictionaryBytes[DictionaryFlag]) != 0)
			{
				return std::nullopt;
			}
			At += DictionaryBytes[DictionaryFlag];
			std::optional<uint64> ContentSize;
			if (ContentSizeBytes != 0)
			{
				ContentSize = Zstd::ReadLittle(Bytes, At, ContentSizeBytes) + (ContentSizeBytes == 2 ? 256 : 0);
				At += ContentSizeBytes;
			}

			Blocks.BeginFrame();
			Zstd::FFrameOutput Output(Destination, Out);
			for (bool bLast = false; !bLast;)
			{
				if (Bytes.size() - At < 3)
				{
					return std::nullopt;
				}
				const uint32 Header = static_cast<uint32>(Zstd::ReadLittle(Bytes, At, 3));
				At += 3;
				bLast = (Header & 1) != 0;
				const uint32 Type = Header >> 1 & 3;
				const uint32 Size = Header >> 3;
				const std::size_t Stored = Type == 1 ? 1 : Size;
				if (Size > MostBlockBytes || Type == 3 || Bytes.size() - At < Stored)
				{
					return std::nullopt;
				}
				const std::span<const uint8> Content = Bytes.subspan(At, Stored);
				const bool bDone = Type == 0 ? Output.Put(Content) : Type == 1 ? Output.Fill(Content[0], Size) : Blocks.Decode(Content, Output);
				if (!bDone)
				{
					return std::nullopt;
				}
				At += Stored;
			}
			if (ContentSize && *ContentSize != Output.Written().size())
			{
				return std::nullopt;
			}
			if (bChecksum)
			{
				if (Bytes.size() - At < 4 || Zstd::ReadLittle(Bytes, At, 4) != (Xxh64(Output.Written()) & 0xFFFFFFFF))
				{
					return std::nullopt;
				}
				At += 4;
			}
			Out = Output.End();
			return At;
		}
	}

	FZstdDecoder::FZstdDecoder() : Blocks(std::make_unique<Zstd::FBlockDecoder>())
	{
	}

	FZstdDecoder::~FZstdDecoder() = default;

	bool FZstdDecoder::Decode(std::span<const uint8> Packed, std::span<uint8> Destination)
	{
		std::size_t In = 0;
		std::size_t Out = 0;
		while (In < Packed.size())
		{
			if (Packed.size() - In < 8)
			{
				return false;
			}
			const uint32 Magic = static_cast<uint32>(Zstd::ReadLittle(Packed, In, 4));
			if ((Magic & 0xFFFFFFF0) == SkippableMagic)
			{
				const uint64 Skipped = Zstd::ReadLittle(Packed, In + 4, 4);
				if (Skipped > Packed.size() - In - 8)
				{
					return false;
				}
				In += 8 + static_cast<std::size_t>(Skipped);
				continue;
			}
			if (Magic != FrameMagic)
			{
				return false;
			}
			const auto Taken = Frame(*Blocks, Packed.subspan(In + 4), Destination, Out);
			if (!Taken)
			{
				return false;
			}
			In += 4 + *Taken;
		}
		return Out == Destination.size();
	}
}
