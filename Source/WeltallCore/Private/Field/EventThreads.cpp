#include "Field/EventAssembly.h"

#include "EventAssemblyText.h"
#include "Field/EventInstructionSet.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		using namespace EventAssemblyText;

		constexpr std::size_t JumpBytes = 3;

		std::string At(std::size_t Offset, std::string_view Words)
		{
			std::string Problem;
			AppendHex(Problem, static_cast<uint32>(Offset), 4);
			Problem += ": ";
			Problem += Words;
			return Problem;
		}
	}

	FEventThreads FollowEventThreads(const FEventScripts& Scripts, std::optional<int32> Actor)
	{
		const std::span<const uint8> Code(Scripts.Code(), Scripts.CodeSize());
		// Each reached instruction's length at its offset.
		std::vector<uint8> Lengths(Code.size(), 0);
		std::vector<std::size_t> Pending;
		for (int32 Each = 0; Each < Scripts.ActorCount(); ++Each)
		{
			for (int32 Script = 0; Script < FEventScripts::ScriptsPerActor && (!Actor || *Actor == Each); ++Script)
			{
				// An entry point of 0 is an unused one, but for the first script of a bytecode that starts with it.
				const uint16 Entry = Scripts.EntryPoint(Each, Script);
				if (Entry != 0 || (Each == 0 && Script == 0 && !Code.empty() && Code[0] != EntranceMark))
				{
					Pending.push_back(Entry);
				}
			}
		}
		FEventThreads Threads;
		while (!Pending.empty())
		{
			std::size_t Offset = Pending.back();
			Pending.pop_back();
			for (;;)
			{
				if (Offset >= Code.size())
				{
					Threads.Problems.push_back(At(Offset, "a thread leaves the bytecode"));
					break;
				}
				if (Lengths[Offset] != 0)
				{
					break;
				}
				const std::optional<FEventInstruction> Instruction = ReadEventInstruction(Code, Offset);
				if (!Instruction)
				{
					Threads.Problems.push_back(At(Offset, "a thread reaches what is no instruction"));
					break;
				}
				Lengths[Offset] = static_cast<uint8>(Instruction->Length);
				for (int32 Operand = 0; Operand < Instruction->Operands->Count; ++Operand)
				{
					if (Instruction->Operands->Kinds[static_cast<std::size_t>(Operand)].Kind == EEventOperand::Jump)
					{
						Pending.push_back(static_cast<std::size_t>(Instruction->Values[static_cast<std::size_t>(Operand)]));
					}
				}
				Offset += Instruction->Length;
				if (Instruction->Shape->Opcode == EventOpcode::JumpTable)
				{
					for (std::size_t Entry = Offset; Entry + JumpBytes <= Code.size() && Code[Entry] == EventOpcode::Jump; Entry += JumpBytes)
					{
						Pending.push_back(Entry);
					}
				}
				if (Instruction->Shape->bEndsThread)
				{
					break;
				}
			}
		}
		std::size_t End = 0;
		std::size_t Last = 0;
		for (std::size_t Offset = 0; Offset < Code.size(); ++Offset)
		{
			if (Lengths[Offset] == 0)
			{
				continue;
			}
			if (Offset < End)
			{
				std::string Inside = "reached inside the instruction at ";
				AppendHex(Inside, static_cast<uint32>(Last), 4);
				Threads.Problems.push_back(At(Offset, Inside));
			}
			Threads.Reached.push_back(static_cast<uint16>(Offset));
			if (Offset + Lengths[Offset] > End)
			{
				End = Offset + Lengths[Offset];
				Last = Offset;
			}
		}
		std::sort(Threads.Problems.begin(), Threads.Problems.end());
		Threads.Problems.erase(std::unique(Threads.Problems.begin(), Threads.Problems.end()), Threads.Problems.end());
		return Threads;
	}
}
