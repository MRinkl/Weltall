#include "Formats/Tim.h"

#include "Foundation/LittleEndian.h"

#include <iterator>
#include <string>
#include <utility>

namespace Weltall
{
	namespace
	{
		constexpr uint32 TimMagic = 0x10;
		constexpr uint32 HasClutFlag = 0x08;
		constexpr uint32 PixelModeMask = 0x07;
		constexpr std::size_t BlockHeaderSize = 12;

		/** A CLUT or image block: u32 length (including this header), a rectangle, then its units. */
		TResult<FVramImage> ReadBlock(std::span<const uint8> Bytes, std::size_t Offset, std::size_t& Length)
		{
			if (Bytes.size() < Offset + BlockHeaderSize)
			{
				return MakeError("block header is truncated");
			}
			Length = LittleEndian::ReadU32(Bytes, Offset);
			if (Length < BlockHeaderSize || Length > Bytes.size() - Offset)
			{
				return MakeError("block length is impossible");
			}
			FVramImage Block;
			Block.Rect = {LittleEndian::ReadS16(Bytes, Offset + 4), LittleEndian::ReadS16(Bytes, Offset + 6),
				LittleEndian::ReadS16(Bytes, Offset + 8), LittleEndian::ReadS16(Bytes, Offset + 10)};
			if (Block.Rect.Width <= 0 || Block.Rect.Height <= 0)
			{
				return MakeError("block has an empty rectangle");
			}
			const std::size_t Count = static_cast<std::size_t>(Block.Rect.Width) * static_cast<std::size_t>(Block.Rect.Height);
			if ((Length - BlockHeaderSize) / 2 < Count)
			{
				return MakeError("block is smaller than its rectangle");
			}
			Block.Units.resize(Count);
			for (std::size_t Unit = 0; Unit < Count; ++Unit)
			{
				Block.Units[Unit] = LittleEndian::ReadU16(Bytes, Offset + BlockHeaderSize + Unit * 2);
			}
			return Block;
		}
	}

	uint8 FTim::IndexAt(int32 X, int32 Y) const
	{
		const std::size_t Row = static_cast<std::size_t>(Y) * static_cast<std::size_t>(Image.Rect.Width);
		if (BitsPerPixel == 8)
		{
			const uint16 Unit = Image.Units[Row + static_cast<std::size_t>(X) / 2];
			return static_cast<uint8>(Unit >> (X % 2 * 8));
		}
		const uint16 Unit = Image.Units[Row + static_cast<std::size_t>(X) / 4];
		return static_cast<uint8>((Unit >> (X % 4 * 4)) & 0x0F);
	}

	TResult<FTim> ParseTim(std::span<const uint8> Bytes)
	{
		if (Bytes.size() < 8 || LittleEndian::ReadU32(Bytes, 0) != TimMagic)
		{
			return MakeError("not a TIM image");
		}
		static constexpr uint8 BitsForMode[] = {4, 8, 16, 24};
		const uint32 Flags = LittleEndian::ReadU32(Bytes, 4);
		if ((Flags & PixelModeMask) >= std::size(BitsForMode))
		{
			return MakeError("TIM pixel mode is not 4, 8, 16 or 24-bit");
		}
		FTim Tim;
		Tim.BitsPerPixel = BitsForMode[Flags & PixelModeMask];
		std::size_t Offset = 8;
		std::size_t Length = 0;
		if ((Flags & HasClutFlag) != 0)
		{
			auto Clut = ReadBlock(Bytes, Offset, Length);
			if (!Clut)
			{
				return MakeError("TIM CLUT " + Clut.Error().Message);
			}
			Tim.Clut = std::move(*Clut);
			Offset += Length;
		}
		auto Image = ReadBlock(Bytes, Offset, Length);
		if (!Image)
		{
			return MakeError("TIM image " + Image.Error().Message);
		}
		Tim.Image = std::move(*Image);
		Tim.ByteSize = Offset + Length;
		return Tim;
	}

	TResult<std::vector<FVramImage>> ReadTimSequence(std::span<const uint8> Bytes)
	{
		std::vector<FVramImage> Images;
		std::size_t Offset = 0;
		while (Offset + 8 <= Bytes.size() && LittleEndian::ReadU32(Bytes, Offset) == TimMagic)
		{
			const bool bHasClut = (LittleEndian::ReadU32(Bytes, Offset + 4) & HasClutFlag) != 0;
			Offset += 8;
			for (const bool bPresent : {bHasClut, true})
			{
				if (!bPresent)
				{
					continue;
				}
				std::size_t Length = 0;
				TResult<FVramImage> Block = ReadBlock(Bytes, Offset, Length);
				if (!Block)
				{
					return MakeError("TIM " + Block.Error().Message);
				}
				Images.push_back(std::move(*Block));
				Offset += Length & ~std::size_t{3};
			}
		}
		return Images;
	}

	TResult<std::vector<FVramImage>> ReadTimArchive(std::span<const uint8> Bytes)
	{
		if (Bytes.size() < 4)
		{
			return MakeError("a TIM archive without its count");
		}
		const uint32 Count = LittleEndian::ReadU32(Bytes, 0);
		if (Count > (Bytes.size() - 4) / 4)
		{
			return MakeError("a TIM archive's offsets run past it");
		}
		std::vector<FVramImage> Images;
		for (uint32 Index = Count; Index-- > 0;)
		{
			const std::size_t Offset = LittleEndian::ReadU32(Bytes, 4 + 4 * static_cast<std::size_t>(Index)) & ~3u;
			if (Offset > Bytes.size())
			{
				return MakeError("a TIM of an archive lies past it");
			}
			TResult<FTim> Tim = ParseTim(Bytes.subspan(Offset));
			if (!Tim)
			{
				return MakeError("TIM " + std::to_string(Index) + " of an archive: " + Tim.Error().Message);
			}
			if (Tim->Clut)
			{
				Images.push_back(std::move(*Tim->Clut));
			}
			Images.push_back(std::move(Tim->Image));
		}
		return Images;
	}
}
