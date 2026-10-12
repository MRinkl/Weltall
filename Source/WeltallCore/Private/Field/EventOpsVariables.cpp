#include "EventOps.h"

#include "Game/PsxRandom.h"

#include <optional>

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint32 AccumulatorAddress = 0x000;
		constexpr uint8 FlagOperandBit = 0x40;

		/** The instructions that combine a variable with an operand (35, 38-40, DE, DF): variable, operand, flags. */
		int32 Operand(FEventContext& Context)
		{
			return ReadOperand(Context, 3, CodeByte(Context, 5), FlagOperandBit);
		}

		template <int32 Value>
		void SetConstant(FEventContext& Context)
		{
			SetVar(Context, ReadU16(Context, 1), Value);
			Advance(Context, 3);
		}

		void Set(FEventContext& Context)
		{
			const uint16 Address = ReadU16(Context, 1);
			SetVar(Context, Address, Operand(Context));
			Advance(Context, 6);
		}

		/** Combines the variable with the operand and stores the low 16 bits. */
		template <int32 (*Combine)(int32, int32)>
		void Update(FEventContext& Context)
		{
			const uint16 Address = ReadU16(Context, 1);
			const int32 Value = GetVar(Context, Address);
			SetVar(Context, Address, Combine(Value, Operand(Context)));
			Advance(Context, 6);
		}

		// Unsigned arithmetic gives the processor's wrap-around; only the low 16 bits are kept.
		int32 Add(int32 A, int32 B) { return static_cast<int32>(static_cast<uint32>(A) + static_cast<uint32>(B)); }
		int32 Subtract(int32 A, int32 B) { return static_cast<int32>(static_cast<uint32>(A) - static_cast<uint32>(B)); }
		int32 Multiply(int32 A, int32 B) { return static_cast<int32>(static_cast<uint32>(A) * static_cast<uint32>(B)); }
		int32 SetBit(int32 A, int32 B) { return static_cast<int32>(static_cast<uint32>(A) | 1u << (B & 31)); }
		int32 ClearBit(int32 A, int32 B) { return static_cast<int32>(static_cast<uint32>(A) & ~(1u << (B & 31))); }
		int32 And(int32 A, int32 B) { return A & B; }
		int32 Or(int32 A, int32 B) { return A | B; }
		int32 Xor(int32 A, int32 B) { return A ^ B; }
		/** A zero divisor counts as 1. */
		int32 Divide(int32 A, int32 B) { return A / (B == 0 ? 1 : B); }

		template <int32 Step>
		void Increment(FEventContext& Context)
		{
			const uint16 Address = ReadU16(Context, 1);
			SetVar(Context, Address, GetVar(Context, Address) + Step);
			Advance(Context, 3);
		}

		/** 41: the shift count is a value operand; only its low 5 bits count. */
		void ShiftLeft(FEventContext& Context)
		{
			const uint16 Address = ReadU16(Context, 1);
			const int32 Value = GetVar(Context, Address);
			const int32 Count = ReadValue(Context, 3) & 31;
			SetVar(Context, Address, static_cast<int32>(static_cast<uint32>(Value) << Count));
			Advance(Context, 5);
		}

		/** 42: shifts the widened variable arithmetically: a signed variable keeps its sign. */
		void ShiftRight(FEventContext& Context)
		{
			const uint16 Address = ReadU16(Context, 1);
			const int32 Value = GetVar(Context, Address);
			const int32 Count = ReadValue(Context, 3) & 31;
			SetVar(Context, Address, Value >> Count);
			Advance(Context, 5);
		}

		void Random(FEventContext& Context)
		{
			const uint16 Address = ReadU16(Context, 1);
			SetVar(Context, Address, Context.Random->Next());
			Advance(Context, 3);
		}

		/** A8: a random number from 0 to the operand. */
		void RandomUpTo(FEventContext& Context)
		{
			const int32 Number = Context.Random->Next();
			const int32 Highest = ReadValue(Context, 3);
			SetVar(Context, ReadU16(Context, 1), Multiply(Number, Highest + 1) >> 15);
			Advance(Context, 5);
		}

		/** The byte of the bytecode a table read names: the table's offset plus the index, in 16 bits. */
		std::optional<uint16> TableOffset(FEventContext& Context, int32 Width)
		{
			const uint16 Offset = static_cast<uint16>(ReadU16(Context, 1) + ReadValue(Context, 5));
			if (Offset + static_cast<std::size_t>(Width) > Context.Scripts->CodeSize())
			{
				Fail(Context, EEventFault::ReadOutsideCode, Offset);
				return std::nullopt;
			}
			return Offset;
		}

		/** 48: a byte of a table in the bytecode. */
		void ReadScriptByte(FEventContext& Context)
		{
			const std::optional<uint16> Offset = TableOffset(Context, 1);
			if (!Offset)
			{
				return;
			}
			SetVar(Context, ReadU16(Context, 3), Context.Scripts->Code()[*Offset]);
			Advance(Context, 7);
		}

		/** 49: a 16-bit word of a table in the bytecode, at any byte. */
		void ReadScriptWord(FEventContext& Context)
		{
			const std::optional<uint16> Offset = TableOffset(Context, 2);
			if (!Offset)
			{
				return;
			}
			const uint8* At = Context.Scripts->Code() + *Offset;
			SetVar(Context, ReadU16(Context, 3), At[0] | At[1] << 8);
			Advance(Context, 8);
		}

		/** 87: sets variable 0, the story's progress; costs no budget. */
		void SetAccumulator(FEventContext& Context)
		{
			Context.Run.Budget += 0x20;
			SetVar(Context, AccumulatorAddress, ReadValue(Context, 1));
			Advance(Context, 3);
		}

		void StoreAccumulator(FEventContext& Context)
		{
			const uint16 Address = ReadU16(Context, 1);
			SetVar(Context, Address, GetVar(Context, AccumulatorAddress));
			Advance(Context, 3);
		}

		/** DC: when both name the same variable, it keeps its value. */
		void Swap(FEventContext& Context)
		{
			const int32 First = GetVar(Context, ReadU16(Context, 1));
			const int32 Second = GetVar(Context, ReadU16(Context, 3));
			SetVar(Context, ReadU16(Context, 3), First);
			SetVar(Context, ReadU16(Context, 1), Second);
			Advance(Context, 5);
		}

		/** 94: sets the field clock to minutes and seconds, stopped. */
		void SetClock(FEventContext& Context)
		{
			Context.Clock.Ticks = 0;
			Context.Clock.Mode = 0xFF;
			const int32 Minutes = ReadValue(Context, 1);
			const int32 Seconds = ReadValue(Context, 3);
			SetVar(Context, FFieldClock::Address, (Minutes & 0xFF) << 8 | (Seconds & 0xFF));
			Advance(Context, 5);
		}

		/** 95: starts the clock in a mode: counting down or up, or stopped. */
		void StartClock(FEventContext& Context)
		{
			const uint8 Mode = CodeByte(Context, 1);
			Advance(Context, 2);
			Context.Clock.Mode = Mode;
		}

		void StopClock(FEventContext& Context)
		{
			Context.Clock.Ticks = 0;
			Context.Clock.Mode = 0xFF;
			Advance(Context, 1);
		}

		/** A flag is a bit of a variable: the operand is the variable's address times 16 plus the bit. */
		struct FFlag
		{
			uint32 Address;
			uint32 Mask;
		};

		FFlag ReadFlag(FEventContext& Context)
		{
			const uint16 Operand = ReadU16(Context, 1);
			return {static_cast<uint32>(Operand >> 4), 1u << (Operand & 0xF)};
		}

		/** FB: goes on when the flag is set, else jumps. */
		void IfFlag(FEventContext& Context)
		{
			const FFlag Flag = ReadFlag(Context);
			Branch(Context, (static_cast<uint32>(GetVar(Context, Flag.Address)) & Flag.Mask) != 0, 5, 3);
		}

		void SetFlag(FEventContext& Context)
		{
			const FFlag Flag = ReadFlag(Context);
			SetVar(Context, Flag.Address, static_cast<int32>(static_cast<uint32>(GetVar(Context, Flag.Address)) | Flag.Mask));
			Advance(Context, 3);
		}

		void ClearFlag(FEventContext& Context)
		{
			const FFlag Flag = ReadFlag(Context);
			SetVar(Context, Flag.Address, static_cast<int32>(static_cast<uint32>(GetVar(Context, Flag.Address)) & ~Flag.Mask));
			Advance(Context, 3);
		}
	}

	void RegisterVariables(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0x35] = &Set;
		Base[0x36] = &SetConstant<1>;
		Base[0x37] = &SetConstant<0>;
		Base[0x38] = &Update<&Add>;
		Base[0x39] = &Update<&Subtract>;
		Base[0x3A] = &Update<&SetBit>;
		Base[0x3B] = &Update<&ClearBit>;
		Base[0x3C] = &Increment<1>;
		Base[0x3D] = &Increment<-1>;
		Base[0x3E] = &Update<&And>;
		Base[0x3F] = &Update<&Or>;
		Base[0x40] = &Update<&Xor>;
		Base[0x41] = &ShiftLeft;
		Base[0x42] = &ShiftRight;
		Base[0x43] = &Random;
		Base[0x48] = &ReadScriptByte;
		Base[0x49] = &ReadScriptWord;
		Base[0x87] = &SetAccumulator;
		Base[0x94] = &SetClock;
		Base[0x95] = &StartClock;
		Base[0x96] = &StopClock;
		Base[0x88] = &StoreAccumulator;
		Base[0xA8] = &RandomUpTo;
		Base[0xDC] = &Swap;
		Base[0xDE] = &Update<&Multiply>;
		Base[0xDF] = &Update<&Divide>;
		Base[0xFB] = &IfFlag;
		Extended[0x0A] = &SetFlag;
		Extended[0x0B] = &ClearFlag;
	}
}
