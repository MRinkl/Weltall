#include "Field/EventInstructionSet.h"

#include "EventInstructionRows.h"

#include <algorithm>
#include <vector>

namespace Weltall
{
	namespace
	{
		constexpr std::size_t BaseCount = 0x100;
		constexpr std::size_t ShapeCount = 2 * BaseCount;

		/** An instruction whose first operand byte selects between two operand lists. */
		struct FVariant
		{
			uint16 Opcode = 0;
			std::string_view OtherOperands;
			bool (*SelectsOther)(uint8 First) = nullptr;
		};

		constexpr FVariant Variants[] = {
			// A move is two instructions of one opcode: its set-up, begun by a zero byte, then its run.
			{0x0010, "b", [](uint8 Marker) { return Marker != 0; }},
			{0x0011, "b v", [](uint8 Marker) { return Marker != 0; }},
			// A jump's set-up, then its flight.
			{0x0057, "b", [](uint8 Mode) { return (Mode & 3) == 3; }},
			{0x0073, "b v v v", [](uint8 Preset) { return Preset == 1; }},
			{0xFE27, "b v", [](uint8 Control) { return Control == 0; }},
			{0xFE5C, "b v", [](uint8 Control) { return Control == 2; }},
			{0xFE77, "b w o40 o20 o10 f", [](uint8 Mode) { return Mode == 1; }},
			{0xFEB0, "b v v", [](uint8 Mode) { return Mode != 1; }},
			{0xFED4, "b v v v v", [](uint8 Mode) { return Mode == 1 || Mode == 3; }},
			{0xFEDD, "b v v", [](uint8 Mode) { return Mode == 0 || Mode == 1; }},
		};

		constexpr uint16 ThreadEnds[] = {0x00, 0x01, 0x04, 0x0C, 0x0D, 0x5B, 0x92, 0xD1, 0xE4, 0xFE00, 0xFE78, 0xFE79, 0xFE7A, 0xFE7B, 0xFE7C, 0xFE7D, 0xFE7E};

		constexpr std::array<std::string_view, 16> TestNames = {
			"==", "!=", ">", "<", ">=", "<=", "&", "test7", "|", "test9", "~&", "test11", "test12", "test13", "test14", "test15",
		};

		uint8 HexDigit(char Digit)
		{
			return static_cast<uint8>(Digit <= '9' ? Digit - '0' : Digit - 'A' + 10);
		}

		FEventOperandKind KindOfLetters(std::string_view Letters)
		{
			switch (Letters.front())
			{
			case 'w':
				return {EEventOperand::Word};
			case 'h':
				return {EEventOperand::Signed};
			case 'l':
				return {EEventOperand::Long};
			case 'v':
				return {EEventOperand::Value};
			case 'V':
				return {EEventOperand::Variable};
			case 'a':
				return {EEventOperand::Actor};
			case 's':
				return {EEventOperand::Script};
			case 'j':
				return {EEventOperand::Jump};
			case 'm':
				return {EEventOperand::Message};
			case 'f':
				return {EEventOperand::Flags};
			case 'o':
				return {EEventOperand::ByFlag, static_cast<uint8>(HexDigit(Letters[1]) << 4 | HexDigit(Letters[2]))};
			default:
				return {EEventOperand::Byte};
			}
		}

		FEventOperandList ListOfLetters(std::string_view Letters)
		{
			FEventOperandList List;
			while (!Letters.empty())
			{
				const std::size_t End = std::min(Letters.find(' '), Letters.size());
				const FEventOperandKind Kind = KindOfLetters(Letters.substr(0, End));
				List.Kinds[static_cast<std::size_t>(List.Count++)] = Kind;
				List.Bytes += Kind.Size();
				List.FlagBits |= Kind.FlagBit;
				Letters.remove_prefix(std::min(End + 1, Letters.size()));
			}
			return List;
		}

		struct FInstructionSet
		{
			std::array<FEventInstructionShape, ShapeCount> Shapes{};
			std::array<bool (*)(uint8), ShapeCount> SelectsOther{};
			/** The shapes that are instructions, by name. */
			std::vector<const FEventInstructionShape*> ByName;

			static std::size_t Slot(uint16 Opcode) { return Opcode > 0xFF ? BaseCount + (Opcode & 0xFF) : Opcode; }

			FInstructionSet()
			{
				for (const std::span<const FEventInstructionRow> Rows : {BaseEventInstructionRows(), ExtendedEventInstructionRows()})
				{
					for (const FEventInstructionRow& Row : Rows)
					{
						FEventInstructionShape& Shape = Shapes[Slot(Row.Opcode)];
						Shape.Opcode = Row.Opcode;
						Shape.Name = Row.Name;
						Shape.Operands = ListOfLetters(Row.Operands);
						Shape.OtherOperands = Shape.Operands;
						ByName.push_back(&Shape);
					}
				}
				for (const FVariant& Variant : Variants)
				{
					Shapes[Slot(Variant.Opcode)].bFirstOperandSelects = true;
					Shapes[Slot(Variant.Opcode)].OtherOperands = ListOfLetters(Variant.OtherOperands);
					SelectsOther[Slot(Variant.Opcode)] = Variant.SelectsOther;
				}
				for (const uint16 Opcode : ThreadEnds)
				{
					Shapes[Slot(Opcode)].bEndsThread = true;
				}
				std::sort(ByName.begin(), ByName.end(), [](const FEventInstructionShape* A, const FEventInstructionShape* B) { return A->Name < B->Name; });
			}
		};

		const FInstructionSet& InstructionSet()
		{
			static const FInstructionSet Instance;
			return Instance;
		}
	}

	int32 FEventOperandKind::Size() const
	{
		switch (Kind)
		{
		case EEventOperand::Byte:
		case EEventOperand::Actor:
		case EEventOperand::Script:
		case EEventOperand::Flags:
			return 1;
		case EEventOperand::Long:
			return 4;
		default:
			return 2;
		}
	}

	const FEventInstructionShape* FindEventInstruction(uint16 Opcode)
	{
		if (Opcode > 0xFF && Opcode >> 8 != EventOpcode::ExtendedPrefix)
		{
			return nullptr;
		}
		const FEventInstructionShape& Shape = InstructionSet().Shapes[FInstructionSet::Slot(Opcode)];
		return Shape.Name.empty() ? nullptr : &Shape;
	}

	const FEventInstructionShape* FindEventInstruction(std::string_view Name)
	{
		const std::vector<const FEventInstructionShape*>& ByName = InstructionSet().ByName;
		const auto Found = std::lower_bound(ByName.begin(), ByName.end(), Name, [](const FEventInstructionShape* Shape, std::string_view Wanted) { return Shape->Name < Wanted; });
		return Found != ByName.end() && (*Found)->Name == Name ? *Found : nullptr;
	}

	const FEventOperandList& EventOperandsOf(const FEventInstructionShape& Shape, uint8 First)
	{
		const auto Selects = InstructionSet().SelectsOther[FInstructionSet::Slot(Shape.Opcode)];
		return Selects != nullptr && Selects(First) ? Shape.OtherOperands : Shape.Operands;
	}

	uint8 FEventInstruction::Flags() const
	{
		for (int32 Operand = 0; Operand < Operands->Count; ++Operand)
		{
			if (Operands->Kinds[static_cast<std::size_t>(Operand)].Kind == EEventOperand::Flags)
			{
				return static_cast<uint8>(Values[static_cast<std::size_t>(Operand)]);
			}
		}
		return 0;
	}

	std::optional<FEventInstruction> ReadEventInstruction(std::span<const uint8> Code, std::size_t Offset)
	{
		if (Offset >= Code.size())
		{
			return std::nullopt;
		}
		uint16 Opcode = Code[Offset];
		std::size_t At = Offset + 1;
		if (Opcode == EventOpcode::ExtendedPrefix)
		{
			if (At >= Code.size())
			{
				return std::nullopt;
			}
			Opcode = static_cast<uint16>(Opcode << 8 | Code[At++]);
		}
		const FEventInstructionShape* Shape = FindEventInstruction(Opcode);
		if (Shape == nullptr)
		{
			return std::nullopt;
		}
		FEventInstruction Instruction;
		Instruction.Shape = Shape;
		Instruction.Operands = &EventOperandsOf(*Shape, At < Code.size() ? Code[At] : uint8{0});
		Instruction.Offset = Offset;
		if (Code.size() - At < static_cast<std::size_t>(Instruction.Operands->Bytes))
		{
			return std::nullopt;
		}
		for (int32 Operand = 0; Operand < Instruction.Operands->Count; ++Operand)
		{
			const FEventOperandKind Kind = Instruction.Operands->Kinds[static_cast<std::size_t>(Operand)];
			uint32 Stored = 0;
			for (int32 Byte = 0; Byte < Kind.Size(); ++Byte)
			{
				Stored |= static_cast<uint32>(Code[At++]) << (8 * Byte);
			}
			Instruction.Values[static_cast<std::size_t>(Operand)] = Kind.Kind == EEventOperand::Signed ? static_cast<int16>(Stored) : static_cast<int32>(Stored);
		}
		Instruction.Length = At - Offset;
		return Instruction;
	}

	std::string_view EventTestName(int32 Test)
	{
		return TestNames[static_cast<std::size_t>(Test) & 0xF];
	}

	std::optional<int32> EventTestOfName(std::string_view Name)
	{
		const auto Found = std::find(TestNames.begin(), TestNames.end(), Name);
		return Found != TestNames.end() ? std::optional<int32>(static_cast<int32>(Found - TestNames.begin())) : std::nullopt;
	}
}
