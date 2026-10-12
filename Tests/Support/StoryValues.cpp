#include "Support/StoryValues.h"

#include "Field/EventAssembly.h"
#include "Field/EventInstructionSet.h"
#include "Field/FieldMap.h"

#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>

namespace Weltall::Testing
{
	namespace
	{
		/** If's operand kinds, in its flags: its second operand a constant, or its first. */
		constexpr uint8 KindBits = 0xF0;
		constexpr uint8 SecondConstant = 0x40;
		constexpr uint8 FirstConstant = 0x80;
		/** The story's progress is the variable at byte address 0 of 7FFh; an odd address names the variable below it. */
		constexpr int32 LastAddress = 0x7FF;
		constexpr int32 AddressBits = 0x7FE;
		/** In a value operand: a constant in its low bits rather than a variable's address. */
		constexpr int32 ConstantValue = 0x8000;
		constexpr int32 MapNumberBits = 0x3FF;
		/**
		 * The saved variables the story's scripts keep their own state in: past those the field itself writes (the
		 * entrance, the map left, the leader's place, the party) and below the scratch every map's entry clears.
		 */
		constexpr int32 FirstStoryVariable = 0x48;
		constexpr int32 FirstScratchVariable = 0x400;
		/** A flag's number holds its variable's address above its bit. */
		constexpr int32 FlagAddressShift = 4;

		bool IsStoryVariable(int32 Address)
		{
			return Address >= FirstStoryVariable && Address < FirstScratchVariable;
		}

		std::optional<int32> Constant(int32 Value)
		{
			return (Value & ConstantValue) != 0 ? std::optional<int32>(Value & ~ConstantValue) : std::nullopt;
		}

		bool IsProgress(int32 Address)
		{
			return Address >= 0 && Address <= LastAddress && (Address & AddressBits) == 0;
		}

		bool IsNamed(const FEventInstruction& Instruction, std::initializer_list<std::string_view> Names)
		{
			for (const std::string_view Name : Names)
			{
				if (Instruction.Shape->Name == Name)
				{
					return true;
				}
			}
			return false;
		}
	}

	FStoryReferences FindStoryReferences(const FEventScripts& Scripts)
	{
		FStoryReferences Found;
		const std::span<const uint8> Code(Scripts.Code(), Scripts.CodeSize() + FEventScripts::ReadSlack);
		for (const uint16 Offset : FollowEventThreads(Scripts).Reached)
		{
			const auto Instruction = ReadEventInstruction(Code, Offset);
			if (!Instruction)
			{
				continue;
			}
			std::optional<int32> Compared;
			std::optional<int32> Neighbour;
			if (IsNamed(*Instruction, {"IfAccLess", "IfAccGreater", "IfAccEqual"}))
			{
				Compared = Constant(Instruction->Values[0] & 0xFFFF);
			}
			else if (IsNamed(*Instruction, {"IfFlag"}))
			{
				const int32 Address = (Instruction->Values[0] & 0xFFFF) >> FlagAddressShift & AddressBits;
				if (IsStoryVariable(Address))
				{
					Found.Tested.insert(static_cast<uint16>(Address));
				}
			}
			else if (Instruction->Shape->Opcode == EventOpcode::If)
			{
				const uint8 Kinds = Instruction->Flags() & KindBits;
				for (const int32 Operand : {0, 1})
				{
					const int32 Address = Instruction->Values[static_cast<std::size_t>(Operand)] & AddressBits;
					if ((Kinds & (Operand == 0 ? FirstConstant : SecondConstant)) == 0 && IsStoryVariable(Address))
					{
						Found.Tested.insert(static_cast<uint16>(Address));
					}
				}
				if (Kinds == SecondConstant && IsProgress(Instruction->Values[0]))
				{
					Compared = static_cast<int16>(Instruction->Values[1]);
				}
				else if (Kinds == FirstConstant && IsProgress(Instruction->Values[1]))
				{
					Compared = static_cast<int16>(Instruction->Values[0]);
				}
			}
			else if (IsNamed(*Instruction, {"ChangeMap", "ChangeMapTransition"}))
			{
				Neighbour = Constant(Instruction->Values[0] & 0xFFFF);
			}
			else if (IsNamed(*Instruction, {"EnterDoor", "DoorExit"}))
			{
				Neighbour = Constant(Instruction->Values[1] & 0xFFFF);
			}
			else if (IsNamed(*Instruction, {"SetWalkAttrByte", "OrWalkAttrByte", "AndWalkAttrByte"}))
			{
				Found.WalkRecords.insert(Instruction->Values[0]);
			}
			if (Compared && *Compared >= 0)
			{
				Found.Compared.insert(static_cast<uint16>(*Compared));
			}
			if (Neighbour && (*Neighbour & MapNumberBits) < FieldMapCount)
			{
				Found.Neighbours.insert(*Neighbour & MapNumberBits);
			}
		}
		return Found;
	}

	std::vector<uint16> StoryValuesToldApart(const std::set<uint16>& Compared)
	{
		std::set<uint16> Values;
		if (*Compared.begin() > 0)
		{
			Values.insert(static_cast<uint16>(*Compared.begin() - 1));
		}
		for (const uint16 Value : Compared)
		{
			Values.insert(Value);
			Values.insert(static_cast<uint16>(Value + 1));
		}
		return {Values.begin(), Values.end()};
	}
}
