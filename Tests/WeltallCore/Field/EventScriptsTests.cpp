#include "Field/EventScripts.h"
#include "Field/EventVariables.h"
#include "Game/GameState.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using Weltall::Testing::FByteWriter;

namespace
{
	/** A script section: the signedness bitmap, the actor count, 32 entry points per actor, the bytecode. */
	std::vector<uint8> Section(const std::array<uint32, 32>& Bitmap, const std::vector<std::array<uint16, 32>>& Entries, std::initializer_list<uint8> Code)
	{
		FByteWriter Writer;
		for (const uint32 Word : Bitmap)
		{
			Writer.U32(Word);
		}
		Writer.U32(static_cast<uint32>(Entries.size()));
		for (const auto& Actor : Entries)
		{
			for (const uint16 Entry : Actor)
			{
				Writer.U16(Entry);
			}
		}
		Writer.U8s(Code);
		return Writer.Bytes;
	}
}

TEST_CASE("An event script section gives each actor's entry points and the bytecode", "[Field]")
{
	std::array<uint16, 32> First{};
	First[0] = 0;
	First[1] = 2;
	std::array<uint16, 32> Second{};
	Second[31] = 3;
	const auto Scripts = FEventScripts::Parse(Section({}, {First, Second}, {0x00, 0xC3, 0x00, 0x01, 0x02, 0x00}));
	REQUIRE(Scripts);
	CHECK(Scripts->ActorCount() == 2);
	CHECK(Scripts->EntryPoint(0, 1) == 2);
	CHECK(Scripts->EntryPoint(1, 31) == 3);
	CHECK(Scripts->CodeSize() == 6);
	CHECK(Scripts->Code()[1] == 0xC3);
	// Past the end, an instruction's operands read zeros.
	CHECK(Scripts->Code()[6 + FEventScripts::ReadSlack - 1] == 0);
}

TEST_CASE("A script section too short for its actors is refused", "[Field]")
{
	FByteWriter Writer;
	for (int32 Word = 0; Word < 32; ++Word)
	{
		Writer.U32(0);
	}
	Writer.U32(3).U16(0);
	const auto Scripts = FEventScripts::Parse(Writer.Bytes);
	REQUIRE_FALSE(Scripts);
	CHECK(Scripts.Error().Message.find("3 actors") != std::string::npos);
	CHECK_FALSE(FEventScripts::Parse(std::vector<uint8>(0x40)));
}

TEST_CASE("The entrance table's records are where the game reads them", "[Field]")
{
	// FF, then two 7-byte records: x, z, layer, camera direction, facing.
	const auto Scripts = FEventScripts::Parse(Section({}, {{}}, {0xFF, 0x10, 0x00, 0xF0, 0xFF, 0x01, 0x02, 0x06, 0x34, 0x12, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00}));
	REQUIRE(Scripts);
	const auto First = Scripts->Entrance(0);
	REQUIRE(First);
	CHECK(First->X == 0x10);
	CHECK(First->Z == -0x10);
	CHECK(First->Layer == 1);
	CHECK(First->CameraDirection == 2);
	CHECK(First->Facing == 6);
	const auto Second = Scripts->Entrance(1);
	REQUIRE(Second);
	CHECK(Second->X == 0x1234);
	CHECK(Second->CameraDirection == FEventEntrance::KeepDirection);
	CHECK_FALSE(Scripts->Entrance(2));
	// Direction 2 turns the camera to 6 eighths of a turn; facing 6 is 4 eighths, marked as set.
	CHECK(FEventEntrance::CameraYaw(2) == 0xC00);
	CHECK(FEventEntrance::FacingAngle(6) == (0x800 | 0x8000));
	CHECK(FEventEntrance::FacingAngle(0) == (0xC00 | 0x8000));

	const auto Plain = FEventScripts::Parse(Section({}, {{}}, {0x00, 0x10, 0x00}));
	REQUIRE(Plain);
	CHECK_FALSE(Plain->Entrance(0));
}

TEST_CASE("Event variables are addressed by byte, an odd address naming the even one below", "[Field]")
{
	std::array<uint32, 32> Bitmap{};
	// Variable 0x042 (bit 0x21 of the bitmap: word 1, bit 1) and 0x7FE (word 31, bit 31) read unsigned.
	Bitmap[1] = 1u << 1;
	Bitmap[31] = 1u << 31;
	const auto Scripts = FEventScripts::Parse(Section(Bitmap, {}, {0x00}));
	REQUIRE(Scripts);
	FEventVariables Variables;
	Variables.Set(0x41, 0x12345);
	CHECK(Variables.Raw(0x40) == 0x2345);
	Variables.Set(0x42, -1);
	Variables.Set(0x44, -1);
	CHECK(Variables.Get(0x42, *Scripts) == 0xFFFF);
	CHECK(Variables.Get(0x43, *Scripts) == 0xFFFF);
	CHECK(Variables.Get(0x44, *Scripts) == -1);
	Variables.Set(0x7FE, 0x8000);
	CHECK(Variables.Get(0x7FE, *Scripts) == 0x8000);
	CHECK(Scripts->IsVariableUnsigned(0x42));
	CHECK_FALSE(Scripts->IsVariableUnsigned(0x40));
}

TEST_CASE("Entering the field loads the story's variables and clears the map's", "[Field]")
{
	FGameState State;
	State.SetSavedEventVariable(0, 42);
	State.SetSavedEventVariable(0x1FF, 7);
	FEventVariables Variables;
	Variables.Set(0x400, 99);
	Variables.EnterField(State);
	CHECK(Variables.Raw(0x000) == 42);
	CHECK(Variables.Raw(0x3FE) == 7);
	CHECK(Variables.Raw(0x400) == 0);
	Variables.Set(0x002, 5);
	Variables.Set(0x402, 6);
	Variables.Save(State);
	CHECK(State.SavedEventVariable(1) == 5);
	CHECK(State.SavedEventVariable(0) == 42);
}
