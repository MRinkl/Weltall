#include "Presentation/OrderingTable.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	FDrawPrimitive Marked(uint32 Mark)
	{
		FDrawPrimitive Primitive;
		Primitive.BlendKey = Mark;
		return Primitive;
	}
}

TEST_CASE("An ordering table draws its highest entry first, and the latest added of an entry first", "[Presentation]")
{
	FOrderingTable Table(4);
	Table.Add(1, Marked(1));
	Table.Add(3, Marked(2));
	Table.Add(1, Marked(3));
	Table.Add(0, Marked(4));
	FDrawList List;
	Table.AppendTo(List);
	std::vector<uint32> Order;
	for (const FDrawPrimitive& Primitive : List.Primitives)
	{
		Order.push_back(Primitive.BlendKey);
	}
	CHECK(Order == std::vector<uint32>{2, 3, 1, 4});
	Table.Clear();
	FDrawList Empty;
	Table.AppendTo(Empty);
	CHECK(Empty.Primitives.empty());
}

TEST_CASE("An ordering table shows each entry's primitives in the order they were added", "[Presentation]")
{
	FOrderingTable Table(3);
	Table.Add(2, Marked(5));
	Table.Add(2, Marked(6));
	CHECK(Table.EntryCount() == 3);
	CHECK(Table.Entry(0).empty());
	REQUIRE(Table.Entry(2).size() == 2);
	CHECK(Table.Entry(2)[0].BlendKey == 5);
	CHECK(Table.Entry(2)[1].BlendKey == 6);
}
