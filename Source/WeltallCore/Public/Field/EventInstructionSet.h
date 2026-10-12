#pragma once

#include "Foundation/Types.h"

#include <array>
#include <optional>
#include <span>
#include <string_view>

namespace Weltall
{
	/** What an operand of an event instruction holds, which gives its size and how it is written. */
	enum class EEventOperand : uint8
	{
		/** One byte. */
		Byte,
		/** Two bytes, unsigned. */
		Word,
		/** Two bytes, signed. */
		Signed,
		/** Four bytes. */
		Long,
		/** Two bytes: a constant of 15 bits where bit 15 is set, else a variable's address. */
		Value,
		/** Two bytes: a variable's address. */
		Variable,
		/** One byte: an actor's number, a party slot's actor or the running actor. */
		Actor,
		/** One byte: a script's number in its low five bits, a priority above them. */
		Script,
		/** Two bytes: an offset in the bytecode. */
		Jump,
		/** Two bytes: a message's number in the map's table. */
		Message,
		/** One byte: which of the instruction's ByFlag operands are constants, and whatever else it keeps there. */
		Flags,
		/** Two bytes: a signed constant where its bit of the flags byte is set, else a variable's address. */
		ByFlag,
	};

	struct FEventOperandKind
	{
		EEventOperand Kind = EEventOperand::Byte;
		/** A ByFlag operand's bit of the flags byte. */
		uint8 FlagBit = 0;

		WELTALLCORE_API int32 Size() const;
	};

	inline constexpr int32 MaxEventOperands = 12;

	/** An instruction's operands in the order they follow its opcode. */
	struct FEventOperandList
	{
		std::array<FEventOperandKind, MaxEventOperands> Kinds{};
		int32 Count = 0;
		/** The bytes they take together. */
		int32 Bytes = 0;
		/** The flags byte's bits its ByFlag operands answer to. */
		uint8 FlagBits = 0;
	};

	/** One instruction of the field's event scripts. */
	struct FEventInstructionShape
	{
		/** 00 to FF, or FE00 to FEFF for the two-byte instructions FE begins. */
		uint16 Opcode = 0;
		std::string_view Name;
		FEventOperandList Operands;
		/** A few instructions' first operand byte selects their operands: Operands or OtherOperands (EventOperandsOf). */
		bool bFirstOperandSelects = false;
		FEventOperandList OtherOperands;
		/** A thread never runs on into the instruction after this one: it ends, jumps, restarts or stays. */
		bool bEndsThread = false;

		int32 OpcodeBytes() const { return Opcode > 0xFF ? 2 : 1; }
	};

	namespace EventOpcode
	{
		inline constexpr uint16 Jump = 0x01;
		inline constexpr uint16 If = 0x02;
		/** Jumps to one of the Jump instructions that follow it. */
		inline constexpr uint16 JumpTable = 0xA6;
		inline constexpr uint8 ExtendedPrefix = 0xFE;
	}

	/** The instruction of an opcode, none where the scripts have none there. */
	WELTALLCORE_API const FEventInstructionShape* FindEventInstruction(uint16 Opcode);
	WELTALLCORE_API const FEventInstructionShape* FindEventInstruction(std::string_view Name);

	/** The operands an instruction has when its first operand byte is First: a few instructions' depend on it. */
	WELTALLCORE_API const FEventOperandList& EventOperandsOf(const FEventInstructionShape& Shape, uint8 First);

	/** An instruction as it lies in bytecode. */
	struct FEventInstruction
	{
		const FEventInstructionShape* Shape = nullptr;
		const FEventOperandList* Operands = nullptr;
		std::size_t Offset = 0;
		std::size_t Length = 0;
		/** Each operand as stored, a Signed one sign-extended. */
		std::array<int32, MaxEventOperands> Values{};

		/** The flags byte, 0 without one. */
		WELTALLCORE_API uint8 Flags() const;
	};

	/** The instruction at Offset; none where no instruction has that opcode or its operands end past the code. */
	WELTALLCORE_API std::optional<FEventInstruction> ReadEventInstruction(std::span<const uint8> Code, std::size_t Offset);

	/**
	 * How the If instruction's sixteen tests are written, by the flags byte's low four bits: the six
	 * comparisons, "&" (a bit in common), "|" (either not zero) and "~&" (a bit of the second the first
	 * lacks); "test7" and "test9" repeat "!=" and "&", and "test11" to "test15" never hold.
	 */
	WELTALLCORE_API std::string_view EventTestName(int32 Test);
	WELTALLCORE_API std::optional<int32> EventTestOfName(std::string_view Name);
}
