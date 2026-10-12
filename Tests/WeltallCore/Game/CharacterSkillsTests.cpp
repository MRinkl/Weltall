#include "Game/CharacterSkills.h"

#include "Foundation/LittleEndian.h"
#include "Foundation/Sha1.h"
#include "Game/GameState.h"
#include "Support/XorShift32.h"

#include <catch_amalgamated.hpp>

#include <vector>

using namespace Weltall;

namespace
{
	constexpr std::size_t CharactersOffset = 0x26C;
	constexpr std::size_t CharacterSize = 0xA4;
	constexpr std::size_t Counters = 0x90;

	std::vector<uint8> RandomBytes(Testing::FXorShift32& Random, std::size_t Size)
	{
		std::vector<uint8> Bytes(Size);
		for (std::size_t Offset = 0; Offset < Size; Offset += 4)
		{
			LittleEndian::WriteU32(Bytes, Offset, Random.Next());
		}
		return Bytes;
	}
}

// The Menu's own Deathblow percentage on random growth tables and states, under the oracle harness.
TEST_CASE("A Deathblow's learning percentage is the Menu's", "[Game][Menu]")
{
	Testing::FXorShift32 Random(0x0DEA7B10);
	std::vector<uint8> Growth = RandomBytes(Random, 11 * CharacterSkills::GrowthRecordSize);
	for (std::size_t Record = 0; Record < 11; ++Record)
	{
		for (std::size_t Word = 0; Word < 13 * 7; ++Word)
		{
			const uint32 Draw = Random.Next() % 8;
			const std::size_t Offset = Record * CharacterSkills::GrowthRecordSize + Word * 2;
			if (Draw == 0)
			{
				LittleEndian::WriteU16(Growth, Offset, 0);
			}
			else if (Draw == 1)
			{
				LittleEndian::WriteU16(Growth, Offset, 0xFFFF);
			}
		}
	}
	FSha1 Digest;
	FGameState State;
	for (int32 Case = 0; Case < 200; ++Case)
	{
		const std::vector<uint8> Block = RandomBytes(Random, FGameState::BlockSize);
		std::copy(Block.begin(), Block.end(), State.Block().begin());
		std::array<uint8, 3> Party{};
		for (uint8& Member : Party)
		{
			Member = static_cast<uint8>(Random.Next() % 11);
		}
		for (const uint8 Character : Party)
		{
			if (Random.Next() % 2 == 0)
			{
				for (std::size_t Counter = 0; Counter < 7; ++Counter)
				{
					LittleEndian::WriteU16(State.Block(), CharactersOffset + Character * CharacterSize + Counters + Counter * 2, static_cast<uint16>(Random.Next() % 300));
				}
			}
		}
		for (const uint8 Character : Party)
		{
			for (int32 Deathblow = 0; Deathblow < CharacterSkills::DeathblowCount; ++Deathblow)
			{
				std::array<uint8, 4> Value{};
				LittleEndian::WriteU32(Value, 0, DeathblowPercent(State, Character, Deathblow, Growth));
				Digest.Update(Value);
			}
		}
	}
	CHECK(FSha1::ToHex(Digest.Finish()) == "18f23e59bbc61fde8684da7ecf79f60b26d5ef66");
}

TEST_CASE("A Deathblow is learned by its bit in the first learned-skills word, the first Deathblow the highest", "[Game][Menu]")
{
	FGameState State;
	LittleEndian::WriteU16(State.Block(), 0x16C0 + 2 * 0x20, 0x8000 | 0x0400);
	CHECK(IsDeathblowLearned(State, 2, 0));
	CHECK(IsDeathblowLearned(State, 2, 5));
	CHECK_FALSE(IsDeathblowLearned(State, 2, 1));
	CHECK_FALSE(IsDeathblowLearned(State, 0, 0));
}
