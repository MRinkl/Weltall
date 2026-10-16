#include "Formats/SectionContainer.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using Testing::FByteWriter;

namespace
{
	/** An LZSS stream of one group of eight literal bytes, from First on: 13 bytes. */
	void Literals(FByteWriter& Into, uint8 First)
	{
		Into.U32(8).U8(0);
		for (uint8 Byte = First; Byte < First + 8; ++Byte)
		{
			Into.U8(Byte);
		}
	}
}

TEST_CASE("A container's sections unpack by their 1-based numbers", "[Formats]")
{
	FByteWriter File;
	File.U32(2).U32(12).U32(25);
	Literals(File, 1);
	Literals(File, 9);
	const auto First = UnpackContainerSection(File.Bytes, 1);
	REQUIRE(First);
	CHECK(*First == std::vector<uint8>{1, 2, 3, 4, 5, 6, 7, 8});
	const auto Second = UnpackContainerSection(File.Bytes, 2);
	REQUIRE(Second);
	CHECK(Second->front() == 9);
	CHECK_FALSE(UnpackContainerSection(File.Bytes, 0));
	CHECK_FALSE(UnpackContainerSection(File.Bytes, 3));
}

TEST_CASE("A file has a container's shape only with a small count of rising offsets within it", "[Formats]")
{
	FByteWriter File;
	File.U32(2).U32(12).U32(25);
	Literals(File, 1);
	Literals(File, 9);
	CHECK(ContainerSectionCount(File.Bytes) == 2u);

	FByteWriter Falling;
	Falling.U32(2).U32(20).U32(12).U32(0).U32(0).U32(0);
	CHECK_FALSE(ContainerSectionCount(Falling.Bytes));
	FByteWriter Inside;
	Inside.U32(2).U32(4).U32(12).U32(0);
	CHECK_FALSE(ContainerSectionCount(Inside.Bytes));
	FByteWriter Past;
	Past.U32(1).U32(64);
	CHECK_FALSE(ContainerSectionCount(Past.Bytes));
	FByteWriter Many;
	Many.U32(65);
	for (int32 Offset = 0; Offset < 70; ++Offset)
	{
		Many.U32(300);
	}
	CHECK_FALSE(ContainerSectionCount(Many.Bytes));
	CHECK_FALSE(ContainerSectionCount(std::vector<uint8>{0, 0, 0, 0, 1, 2}));
}
