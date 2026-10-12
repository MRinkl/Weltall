#include "Chd/ChdCrc16.h"

#include <array>

namespace Chd
{
	namespace
	{
		constexpr std::array<uint16, 256> MakeTable()
		{
			std::array<uint16, 256> Table{};
			for (uint32 Byte = 0; Byte < 256; ++Byte)
			{
				uint32 Value = Byte << 8;
				for (int Bit = 0; Bit < 8; ++Bit)
				{
					Value = (Value & 0x8000) != 0 ? Value << 1 ^ 0x1021 : Value << 1;
				}
				Table[Byte] = static_cast<uint16>(Value);
			}
			return Table;
		}

		constexpr std::array<uint16, 256> Table = MakeTable();
	}

	uint16 Crc16(std::span<const uint8> Bytes, uint16 Crc)
	{
		uint32 Value = Crc;
		for (const uint8 Byte : Bytes)
		{
			Value = (Value << 8 ^ Table[(Value >> 8 ^ Byte) & 0xFF]) & 0xFFFF;
		}
		return static_cast<uint16>(Value);
	}
}
