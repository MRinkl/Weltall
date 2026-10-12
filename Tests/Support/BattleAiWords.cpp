#include "Support/BattleAiWords.h"

#include <algorithm>
#include <initializer_list>

namespace Weltall::Testing::BattleAiWords
{
	namespace
	{
		constexpr std::array<uint8, 4> NoHandlerConditions{0x9C, 0xA0, 0xC4, 0xEF};
		constexpr std::array<uint8, 99> Plain{
			0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
			0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
			0x28, 0x29, 0x2E, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3E, 0x3F,
			0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x51, 0x53, 0x54, 0x55,
			0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x69,
			0x6A, 0x6B, 0x6C, 0x6D, 0x70, 0x71, 0x72, 0x73, 0x74};
		constexpr std::array<uint8, 11> Guarded{0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x4E, 0x4F, 0x50, 0x67, 0x68};
		constexpr std::array<uint8, 27> Conditions{0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D,
			0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x9A, 0x9B};

		/** The word fields a script may write: every one but the status and the gear's buffs. */
		std::array<uint8, 22> WritableWords()
		{
			std::array<uint8, 22> Words{};
			std::size_t Count = 0;
			for (uint8 Field = 0; Field < FieldCount; ++Field)
			{
				if (Field != 2 && Field != 17)
				{
					Words[Count++] = Field;
				}
			}
			return Words;
		}

		const std::array<uint8, 22> Writable = WritableWords();




		uint32 Flag(FXorShift32& G)
		{
			return G.Next() % 2 != 0 ? 0 : G.Next() & 0xFF;
		}

		FWord Immediate(FXorShift32& G, uint32 Op, uint32 A)
		{
			const uint32 Value = G.Next() % 4 != 0 ? G.Next() & 0xFFFF : 0;
			return Word(Op, A, Value, Value >> 8);
		}

		bool In(uint32 Op, std::initializer_list<uint32> Ops)
		{
			return std::find(Ops.begin(), Ops.end(), Op) != Ops.end();
		}

	}

	FWord Condition(FXorShift32& G, uint32 E)
	{
		uint32 Op = 0;
		if (G.Next() % 8 != 0)
		{
			Op = Conditions[G.Next() % Conditions.size()];
		}
		else
		{
			Op = NoHandlerConditions[G.Next() % 4];
		}
		if (In(Op, {0x81, 0x83, 0x85, 0x8B, 0x8F}))
		{
			const uint32 A = Bv(G, E);
			return Word(Op, A, G.Next());
		}
		if (In(Op, {0x82, 0x84, 0x86, 0x8C, 0x90}))
		{
			return Immediate(G, Op, Wv(G, E));
		}
		if (In(Op, {0x87, 0x89, 0x8D, 0x91}))
		{
			const uint32 A = Bv(G, E);
			return Word(Op, A, Bv(G, E));
		}
		if (In(Op, {0x88, 0x8A, 0x8E, 0x92}))
		{
			const uint32 A = Wv(G, E);
			return Word(Op, A, Wv(G, E));
		}
		if (In(Op, {0x93, 0x94}))
		{
			const uint32 A = Lv(G, E);
			return Word(Op, A, Lv(G, E));
		}
		if (Op == 0x95)
		{
			return Word(Op, G.Next() % 11);
		}
		if (Op == 0x96)
		{
			return Word(Op, G.Next() % 32);
		}
		const uint32 A = G.Next();
		const uint32 B = G.Next();
		return Word(Op, A, B, G.Next());
	}

	FWord PlainCommand(FXorShift32& G, uint32 E, bool bEventWork)
	{
		uint32 Op = Plain[G.Next() % Plain.size()];
		if (Op == 0x70 && !bEventWork)
		{
			Op = 0x62;
		}
		if (In(Op, {0x04, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x3E}))
		{
			const uint32 A = Bv(G, E);
			return Word(Op, A, G.Next() % 4 != 0 ? G.Next() : 0);
		}
		if (In(Op, {0x05, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x3F}))
		{
			return Immediate(G, Op, Wv(G, E));
		}
		if (Op == 0x06)
		{
			return Immediate(G, Op, Lv(G, E));
		}
		if (Op == 0x07)
		{
			const uint32 A = G.Next() % 16;
			return Word(Op, A, G.Next());
		}
		if (Op >= 0x18 && Op <= 0x1F)
		{
			const uint32 A = Bv(G, E);
			const uint32 B = Bv(G, E);
			return Word(Op, A, B, Bv(G, E));
		}
		if (Op >= 0x20 && Op <= 0x27)
		{
			const uint32 A = Wv(G, E);
			const uint32 B = Wv(G, E);
			return Word(Op, A, B, Wv(G, E));
		}
		if (Op == 0x28 || Op == 0x29)
		{
			const uint32 A = Op == 0x28 ? G.Next() % FieldCount : WritableWordField(G);
			const uint32 B = G.Next();
			return Word(Op, A, B, G.Next());
		}
		if (Op == 0x2E)
		{
			const uint32 A = Lv(G, E);
			const uint32 B = G.Next() % 2 != 0 ? 2 : 3 + G.Next() % 10;
			return Word(Op, A, B, Wv(G, E));
		}
		if (Op == 0x30 || Op == 0x31)
		{
			const uint32 A = Wv(G, E);
			return Word(Op, A, G.Next() % 16);
		}
		if (Op == 0x32)
		{
			const uint32 A = Bv(G, E);
			return Word(Op, A, Bv(G, E));
		}
		if (Op == 0x33)
		{
			const uint32 A = Wv(G, E);
			return Word(Op, A, Wv(G, E));
		}
		if (Op == 0x34)
		{
			const uint32 A = Lv(G, E);
			return Word(Op, A, Lv(G, E));
		}
		if (Op == 0x35)
		{
			const uint32 A = Bv(G, E);
			return Word(Op, A, Wv(G, E));
		}
		if (Op == 0x36)
		{
			const uint32 A = Wv(G, E);
			return Word(Op, A, Lv(G, E));
		}
		if (In(Op, {0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x56, 0x5A, 0x5B}))
		{
			const uint32 A = Wv(G, E);
			return Word(Op, A, Flag(G));
		}
		if (Op == 0x4C || Op == 0x4D)
		{
			const uint32 A = Bv(G, E);
			return Word(Op, A, G.Next() % 9);
		}
		if (Op == 0x51)
		{
			const uint32 A = Bv(G, E);
			return Word(Op, A, G.Next() % 49);
		}
		if (Op == 0x53 || Op == 0x55)
		{
			return Word(Op, Lv(G, E));
		}
		if (Op == 0x54)
		{
			const uint32 A = Wv(G, E);
			return Word(Op, A, G.Next() % 12);
		}
		if (In(Op, {0x57, 0x64, 0x73}))
		{
			return Word(Op, Wv(G, E));
		}
		if (Op == 0x58 || Op == 0x59)
		{
			return Word(Op, Bv(G, E));
		}
		if (Op >= 0x5C && Op <= 0x61)
		{
			const uint32 A = G.Next() % FieldCount;
			const uint32 B = Wv(G, E);
			return Word(Op, A, B, Wv(G, E));
		}
		if (Op == 0x63)
		{
			const std::array<uint32, 4> Choices{0, 0x80, 1, G.Next()};
			return Word(Op, Choices[G.Next() % 4]);
		}
		if (Op == 0x65 || Op == 0x66)
		{
			const uint32 A = Wv(G, E);
			return Word(Op, A, G.Next() % 4);
		}
		if (Op == 0x6A || Op == 0x6B)
		{
			const uint32 A = Lv(G, E);
			const uint32 B = Lv(G, E);
			return Word(Op, A, B, Lv(G, E));
		}
		if (Op == 0x6C || Op == 0x6D)
		{
			const uint32 A = Lv(G, E);
			return Word(Op, A, G.Next() % 4 != 0 ? G.Next() : 0);
		}
		if (Op == 0x71)
		{
			const uint32 A = G.Next() % 9;
			return Word(Op, A, G.Next() % 2);
		}
		if (Op == 0x72)
		{
			const uint32 A = G.Next() % 8;
			const uint32 B = G.Next() % 8;
			return Word(Op, A, B, G.Next());
		}
		const uint32 A = G.Next();
		const uint32 B = G.Next();
		return Word(Op, A, B, G.Next());
	}

	/** A command naming a slot by a mask, and the mask's own W. */
	std::pair<FWord, uint32> GuardedCommand(FXorShift32& G, uint32 E)
	{
		const uint32 Op = Guarded[G.Next() % Guarded.size()];
		const uint32 Mask = G.Next() % 8;
		if (Op == 0x2A || Op == 0x2B)
		{
			const uint32 A = Bv(G, E);
			return {Word(Op, A, G.Next() % FieldCount, Mask), Mask};
		}
		if (Op == 0x2C)
		{
			const uint32 A = Wv(G, E);
			return {Word(Op, A, G.Next() % FieldCount, Mask), Mask};
		}
		if (Op == 0x2D)
		{
			const uint32 A = Wv(G, E);
			return {Word(Op, A, WritableWordField(G), Mask), Mask};
		}
		if (Op == 0x2E)
		{
			return {Word(Op, Lv(G, E), 1, Mask), Mask};
		}
		if (Op == 0x2F)
		{
			const uint32 A = Lv(G, E);
			return {Word(Op, A, G.Next() % 2, Mask), Mask};
		}
		if (Op == 0x4E || Op == 0x4F || Op == 0x50)
		{
			return {Word(Op, Bv(G, E), Mask), Mask};
		}
		return {Word(Op, Wv(G, E), Mask), Mask};
	}

	uint32 WritableWordField(FXorShift32& G)
	{
		return Writable[G.Next() % Writable.size()];
	}
}
