#include "Chd/ChdBytes.h"
#include "Chd/ChdCrc16.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <string_view>

using namespace Chd;

TEST_CASE("A CHD's integers are read big-endian", "[Chd]")
{
	constexpr std::array<uint8, 8> Bytes{0x80, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0xFF};
	const uint16 High = BigEndian::ReadU16(Bytes, 0);
	CHECK(High == 0x8001);
	CHECK((High & 0x8000) != 0);
	CHECK(BigEndian::ReadU24(Bytes, 0) == 0x800102);
	CHECK(BigEndian::ReadU32(Bytes, 4) == 0x040506FF);
	CHECK(BigEndian::ReadU48(Bytes, 2) == 0x0203040506FFull);
	CHECK(BigEndian::ReadU64(Bytes, 0) == 0x80010203040506FFull);
}

TEST_CASE("A CHD's CRC-16 is CCITT's from 0xFFFF and goes on across pieces", "[Chd]")
{
	constexpr std::string_view Check = "123456789";
	const std::span<const uint8> Bytes(reinterpret_cast<const uint8*>(Check.data()), Check.size());
	CHECK(Crc16(Bytes) == 0x29B1);
	CHECK(Crc16(Bytes.subspan(4), Crc16(Bytes.first(4))) == 0x29B1);
	CHECK(Crc16({}) == 0xFFFF);
}
