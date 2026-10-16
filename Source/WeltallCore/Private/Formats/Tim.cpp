#include "Formats/Tim.h"

#include "Foundation/LittleEndian.h"

#include <algorithm>
#include <iterator>
#include <memory>
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

		/** A TIM's image as a picture: its texels' depth and the colours of its CLUT's first palette. None for 24-bit images. */
		std::optional<FVramPictureForm> PictureFormOf(uint8 BitsPerPixel, const std::optional<FVramImage>& Clut)
		{
			if (BitsPerPixel == 24)
			{
				return std::nullopt;
			}
			FVramPictureForm Form;
			Form.Depth = BitsPerPixel == 4 ? 0 : BitsPerPixel == 8 ? 1 : 2;
			if (BitsPerPixel <= 8 && Clut)
			{
				const std::size_t Colors = std::min<std::size_t>(Clut->Units.size(), std::size_t{1} << BitsPerPixel);
				Form.Palette = std::make_shared<const std::vector<uint16>>(Clut->Units.begin(), Clut->Units.begin() + static_cast<std::ptrdiff_t>(Colors));
			}
			return Form;
		}

		/** Whether a block at Offset is exactly what its header says, within VRAM: the scan's test, stricter than a load's. */
		bool IsExactBlock(std::span<const uint8> Bytes, std::size_t Offset, std::size_t& Length)
		{
			if (Bytes.size() < Offset + BlockHeaderSize)
			{
				return false;
			}
			Length = LittleEndian::ReadU32(Bytes, Offset);
			const uint16 X = LittleEndian::ReadU16(Bytes, Offset + 4);
			const uint16 Y = LittleEndian::ReadU16(Bytes, Offset + 6);
			const uint16 Width = LittleEndian::ReadU16(Bytes, Offset + 8);
			const uint16 Height = LittleEndian::ReadU16(Bytes, Offset + 10);
			return Width != 0 && Height != 0 && X < FVram::Width && Y < FVram::Height && Width <= FVram::Width && Height <= FVram::Height &&
				Length == BlockHeaderSize + std::size_t{Width} * Height * 2 && Length <= Bytes.size() - Offset;
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
		Tim.Image.Picture = PictureFormOf(Tim.BitsPerPixel, Tim.Clut);
		Tim.ByteSize = Offset + Length;
		return Tim;
	}

	std::vector<FTim> FindTims(std::span<const uint8> Bytes)
	{
		std::vector<FTim> Found;
		for (std::size_t Offset = 0; Offset + 8 <= Bytes.size(); Offset += 4)
		{
			if (LittleEndian::ReadU32(Bytes, Offset) != TimMagic)
			{
				continue;
			}
			const uint32 Flags = LittleEndian::ReadU32(Bytes, Offset + 4);
			const uint32 Mode = Flags & PixelModeMask;
			const bool bHasClut = (Flags & HasClutFlag) != 0;
			if ((Flags & ~(PixelModeMask | HasClutFlag)) != 0 || Mode > 2 || (Mode < 2 && !bHasClut))
			{
				continue;
			}
			std::size_t Length = 0;
			std::size_t Image = Offset + 8;
			if (bHasClut)
			{
				if (!IsExactBlock(Bytes, Image, Length))
				{
					continue;
				}
				Image += Length;
			}
			if (!IsExactBlock(Bytes, Image, Length))
			{
				continue;
			}
			TResult<FTim> Tim = ParseTim(Bytes.subspan(Offset));
			if (Tim)
			{
				Offset += (Tim->ByteSize & ~std::size_t{3}) - 4;
				Found.push_back(std::move(*Tim));
			}
		}
		return Found;
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
