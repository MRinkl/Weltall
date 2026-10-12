#include "EventOperandText.h"

#include "EventAssemblyText.h"

namespace Weltall::EventAssemblyText
{
	namespace
	{
		constexpr std::string_view VariableOpens = "var[";

		void AppendVariable(std::string& Out, uint32 Address)
		{
			Out += VariableOpens;
			AppendHex(Out, Address, 3);
			Out += ']';
		}

		/** The address in "var[address]". */
		std::optional<uint32> VariableOf(std::string_view Word)
		{
			if (!Word.starts_with(VariableOpens) || !Word.ends_with(']'))
			{
				return std::nullopt;
			}
			return Hex(Word.substr(VariableOpens.size(), Word.size() - VariableOpens.size() - 1));
		}

		/** The number after Prefix, from 0 to High. */
		std::optional<int64> Suffixed(std::string_view Word, std::string_view Prefix, int64 High)
		{
			return Word.starts_with(Prefix) ? NumberWithin(Word.substr(Prefix.size()), 0, High) : std::nullopt;
		}

		std::optional<FOperandWord> Whole(std::optional<int64> Value)
		{
			return Value ? std::optional<FOperandWord>(FOperandWord{static_cast<uint32>(*Value)}) : std::nullopt;
		}
	}

	void AppendOperand(std::string& Out, FEventOperandKind Kind, int32 Stored, uint8 Flags)
	{
		const uint32 Value = static_cast<uint32>(Stored);
		switch (Kind.Kind)
		{
		case EEventOperand::Byte:
		case EEventOperand::Signed:
			Append(Out, Stored);
			break;
		case EEventOperand::Word:
		case EEventOperand::Long:
			Out += "0x";
			AppendHex(Out, Value, 1);
			break;
		case EEventOperand::Value:
			if ((Value & ConstantBit) != 0)
			{
				Out += '#';
				Append(Out, Value & ~uint32{ConstantBit});
			}
			else
			{
				AppendVariable(Out, Value);
			}
			break;
		case EEventOperand::Variable:
			AppendVariable(Out, Value);
			break;
		case EEventOperand::Actor:
			for (const FNamedActor& Named : NamedActors)
			{
				if (Named.Byte == Value)
				{
					Out += Named.Name;
					return;
				}
			}
			Out += "actor";
			Append(Out, Value);
			break;
		case EEventOperand::Script:
			Out += "script";
			Append(Out, Value & ((1u << ScriptBits) - 1));
			Out += '@';
			Append(Out, Value >> ScriptBits);
			break;
		case EEventOperand::Message:
			Out += "msg";
			Append(Out, Value);
			break;
		case EEventOperand::ByFlag:
			if ((Flags & Kind.FlagBit) != 0)
			{
				Out += '#';
				Append(Out, static_cast<int16>(Value));
			}
			else
			{
				AppendVariable(Out, Value);
			}
			break;
		case EEventOperand::Jump:
		case EEventOperand::Flags:
			break;
		}
	}

	std::optional<FOperandWord> OperandOfWord(FEventOperandKind Kind, std::string_view Word)
	{
		switch (Kind.Kind)
		{
		case EEventOperand::Byte:
			return Whole(NumberWithin(Word, 0, 0xFF));
		case EEventOperand::Word:
			return Whole(NumberWithin(Word, 0, 0xFFFF));
		case EEventOperand::Signed:
			if (const std::optional<int64> Value = NumberWithin(Word, -0x8000, 0x7FFF))
			{
				return FOperandWord{static_cast<uint32>(*Value) & 0xFFFF};
			}
			break;
		case EEventOperand::Long:
			return Whole(NumberWithin(Word, 0, 0xFFFFFFFFll));
		case EEventOperand::Value:
			if (const std::optional<int64> Value = Suffixed(Word, "#", ConstantBit - 1))
			{
				return FOperandWord{static_cast<uint32>(*Value) | ConstantBit};
			}
			if (const std::optional<uint32> Address = VariableOf(Word); Address && *Address < ConstantBit)
			{
				return FOperandWord{*Address};
			}
			break;
		case EEventOperand::Variable:
			if (const std::optional<uint32> Address = VariableOf(Word); Address && *Address <= 0xFFFF)
			{
				return FOperandWord{*Address};
			}
			break;
		case EEventOperand::Actor:
			for (const FNamedActor& Named : NamedActors)
			{
				if (Word == Named.Name)
				{
					return FOperandWord{Named.Byte};
				}
			}
			return Whole(Suffixed(Word, "actor", 0xFF));
		case EEventOperand::Script:
			if (const std::size_t Sign = Word.find('@'); Sign != std::string_view::npos)
			{
				const std::optional<int64> Script = Suffixed(Word.substr(0, Sign), "script", (1 << ScriptBits) - 1);
				const std::optional<int64> Priority = NumberWithin(Word.substr(Sign + 1), 0, (0x100 >> ScriptBits) - 1);
				if (Script && Priority)
				{
					return FOperandWord{static_cast<uint32>(*Priority << ScriptBits | *Script)};
				}
			}
			break;
		case EEventOperand::Message:
			return Whole(Suffixed(Word, "msg", 0xFFFF));
		case EEventOperand::ByFlag:
			if (Word.starts_with('#'))
			{
				if (const std::optional<int64> Value = NumberWithin(Word.substr(1), -0x8000, 0x7FFF))
				{
					return FOperandWord{static_cast<uint32>(*Value) & 0xFFFF, true};
				}
			}
			else if (const std::optional<uint32> Address = VariableOf(Word); Address && *Address <= 0xFFFF)
			{
				return FOperandWord{*Address};
			}
			break;
		case EEventOperand::Jump:
		case EEventOperand::Flags:
			break;
		}
		return std::nullopt;
	}
}
