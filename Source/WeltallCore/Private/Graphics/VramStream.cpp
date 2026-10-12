#include "Graphics/VramStream.h"

#include "Foundation/LittleEndian.h"

namespace Weltall
{
	namespace
	{
		constexpr std::size_t SectorBytes = 2048;
		constexpr uint32 ImageBlock = 0x1200;
		constexpr uint32 ClutBlock = 0x1201;
		constexpr std::size_t RowsOffset = 28;
	}

	TResult<std::vector<FVramImage>> DecodeVramStream(std::span<const uint8> Sectors)
	{
		using namespace LittleEndian;
		const std::size_t SectorCount = Sectors.size() / SectorBytes;
		std::vector<FVramImage> Uploads;
		int32 BlocksLeft = 0;
		std::size_t Sector = 0;
		while (Sector < SectorCount)
		{
			const std::span<const uint8> Header = Sectors.subspan(Sector * SectorBytes, SectorBytes);
			++Sector;
			const uint32 Kind = ReadU32(Header, 0);
			if (Kind != ImageBlock && Kind != ClutBlock)
			{
				break;
			}
			const int16 X = static_cast<int16>(ReadS16(Header, 4) + ReadS16(Header, 8));
			int16 Y = static_cast<int16>(ReadS16(Header, 6) + ReadS16(Header, 10));
			const int16 Width = ReadS16(Header, 12);
			if (BlocksLeft == 0)
			{
				BlocksLeft = static_cast<int32>(ReadU32(Header, 20));
			}
			const int32 DataSectors = static_cast<int32>(ReadU32(Header, 24));
			if (DataSectors == 0)
			{
				continue;
			}
			if (Width <= 0 || Width > FVram::Width)
			{
				return MakeError("VRAM stream block is " + std::to_string(Width) + " units wide");
			}
			if (DataSectors < 0 || RowsOffset + 2 * static_cast<std::size_t>(DataSectors) > SectorBytes)
			{
				return MakeError("VRAM stream block's row counts do not fit in its header");
			}
			for (int32 Index = 0; Index < DataSectors; ++Index)
			{
				if (Sector >= SectorCount)
				{
					return Uploads;
				}
				const int16 Rows = ReadS16(Header, RowsOffset + 2 * static_cast<std::size_t>(Index));
				const std::size_t Units = static_cast<std::size_t>(Width) * static_cast<std::size_t>(Rows < 0 ? 0 : Rows);
				if (Rows < 0 || Units * 2 > SectorBytes)
				{
					return MakeError("VRAM stream sector holds " + std::to_string(Rows) + " rows of " + std::to_string(Width) + " units");
				}
				if (Rows > 0)
				{
					FVramImage Upload{{X, Y, Width, Rows}, std::vector<uint16>(Units)};
					const std::size_t Start = Sector * SectorBytes;
					for (std::size_t Unit = 0; Unit < Units; ++Unit)
					{
						Upload.Units[Unit] = ReadU16(Sectors, Start + Unit * 2);
					}
					Uploads.push_back(std::move(Upload));
				}
				Y = static_cast<int16>(Y + Rows);
				++Sector;
			}
			if (--BlocksLeft < 1)
			{
				break;
			}
		}
		return Uploads;
	}
}
