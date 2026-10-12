#include "Field/EventAssembly.h"

#include "EventAssemblyText.h"
#include "EventOperandText.h"
#include "Field/EventInstructionSet.h"
#include "Foundation/LittleEndian.h"

#include <algorithm>
#include <map>

namespace Weltall
{
	namespace
	{
		using namespace EventAssemblyText;

		constexpr std::size_t BytecodeReach = 0x10000;
		constexpr int32 MaxActors = 0x100;
		constexpr uint32 VariableBytes = FEventScripts::VariableCount * 2;
		constexpr std::string_view FlagsPrefix = "flags=";

		class FAssembler
		{
		public:
			explicit FAssembler(const FEventAssemblyContext& Context)
			{
				Result.Origin = Context.Origin;
				for (const auto& [Name, Operand] : Context.Definitions)
				{
					Definitions.emplace(Name, Operand);
				}
			}

			TResult<FEventAssembly> Assemble(std::string_view Text)
			{
				for (int32 Number = 1; !Text.empty() && Error.empty(); ++Number)
				{
					const std::size_t End = std::min(Text.find('\n'), Text.size());
					Line = Number;
					ReadLine(WordsOf(Text.substr(0, End)));
					Text.remove_prefix(std::min(End + 1, Text.size()));
				}
				for (std::size_t Item = 0; Item < Instructions.size() && Error.empty(); ++Item)
				{
					Line = Instructions[Item].Line;
					Encode(Instructions[Item]);
				}
				for (std::size_t Entry = 0; Entry < NamedEntries.size() && Error.empty(); ++Entry)
				{
					Line = NamedEntries[Entry].Line;
					if (const std::optional<uint16> Target = TargetOf(NamedEntries[Entry].Target))
					{
						AddEntry(NamedEntries[Entry].Actor, NamedEntries[Entry].Script, *Target);
					}
				}
				if (!Error.empty())
				{
					return MakeError(std::move(Error));
				}
				Result.Labels.assign(Labels.begin(), Labels.end());
				return std::move(Result);
			}

		private:
			struct FInstruction
			{
				int32 Line = 0;
				const FEventInstructionShape* Shape = nullptr;
				const FEventOperandList* Operands = nullptr;
				/** Where its operands begin in the code. */
				std::size_t At = 0;
				std::vector<std::string> Tokens;
			};

			struct FNamedEntry
			{
				int32 Line = 0;
				int32 Actor = 0;
				int32 Script = 0;
				std::string Target;
			};

			bool Fail(std::string_view Words)
			{
				if (Error.empty())
				{
					Error = "line ";
					Append(Error, Line);
					Error += ": ";
					Error += Words;
				}
				return false;
			}

			bool Fail(std::string_view Words, std::string_view Token)
			{
				std::string Message(Words);
				Message += " '";
				Message += Token;
				Message += '\'';
				return Fail(Message);
			}

			uint16 Here() const { return static_cast<uint16>(Result.Origin + Result.Code.size()); }

			bool Emit(std::size_t Bytes)
			{
				if (Result.Origin + Result.Code.size() + Bytes > BytecodeReach)
				{
					return Fail("the bytecode outgrows what a 16-bit offset reaches");
				}
				Result.Code.resize(Result.Code.size() + Bytes, 0);
				return true;
			}

			/** A token as the text means it: what a definition says it stands for, else itself. */
			std::string_view Meaning(std::string_view Token) const
			{
				const auto Defined = Definitions.find(Token);
				return Defined != Definitions.end() ? std::string_view(Defined->second) : Token;
			}

			void ReadLine(const std::vector<std::string_view>& Tokens)
			{
				if (Tokens.empty())
				{
					return;
				}
				const std::string_view First = Tokens.front();
				if (Tokens.back().ends_with(':'))
				{
					ReadLabel(Tokens);
				}
				else if (First == "actors")
				{
					ReadActors(Tokens);
				}
				else if (First == "unsigned")
				{
					ReadUnsigned(Tokens);
				}
				else if (First == "entrances")
				{
					if (Tokens.size() != 1 || Here() != 0)
					{
						Fail("'entrances' stands alone, where the bytecode begins");
					}
					else if (Emit(1))
					{
						Result.Code.back() = EntranceMark;
					}
				}
				else if (First == "entrance")
				{
					ReadEntrance(Tokens);
				}
				else if (First == "entry")
				{
					ReadEntry(Tokens);
				}
				else if (First == "define")
				{
					ReadDefinition(Tokens);
				}
				else if (First == "data")
				{
					ReadData(Tokens);
				}
				else
				{
					ReadInstruction(Tokens);
				}
			}

			void ReadLabel(const std::vector<std::string_view>& Tokens)
			{
				if (Result.Origin + Result.Code.size() >= BytecodeReach)
				{
					Fail("a label past what a 16-bit offset reaches");
					return;
				}
				if (Tokens.size() == 4 && Tokens[0] == "actor" && Tokens[2] == "script")
				{
					const std::optional<int64> Actor = NumberWithin(Tokens[1], 0, MaxActors - 1);
					const std::optional<int64> Script = NumberWithin(Tokens[3].substr(0, Tokens[3].size() - 1), 0, FEventScripts::ScriptsPerActor - 1);
					if (!Actor || !Script)
					{
						Fail("a script's line reads 'actor N script N:'");
						return;
					}
					AddEntry(static_cast<int32>(*Actor), static_cast<int32>(*Script), Here());
					return;
				}
				const std::string_view Name = Tokens[0].substr(0, Tokens[0].size() - 1);
				if (Tokens.size() != 1 || !IsName(Name))
				{
					Fail("a label is one name and a colon");
				}
				else if (Definitions.contains(Name) || !Labels.emplace(Name, Here()).second)
				{
					Fail("a name defined twice:", Name);
				}
			}

			void AddEntry(int32 Actor, int32 Script, uint16 Offset)
			{
				const bool bTwice = std::any_of(Result.EntryPoints.begin(), Result.EntryPoints.end(),
					[Actor, Script](const FEventEntryPoint& Entry) { return Entry.Actor == Actor && Entry.Script == Script; });
				if (bTwice)
				{
					Fail("a script given twice");
					return;
				}
				Result.EntryPoints.push_back({Actor, Script, Offset});
			}

			void ReadActors(const std::vector<std::string_view>& Tokens)
			{
				const std::optional<int64> Count = Tokens.size() == 2 ? NumberWithin(Tokens[1], 0, MaxActors) : std::nullopt;
				if (!Count || Result.Actors)
				{
					Fail("'actors' takes the actor count, once");
					return;
				}
				Result.Actors = static_cast<int32>(*Count);
			}

			void ReadUnsigned(const std::vector<std::string_view>& Tokens)
			{
				for (std::size_t Token = 1; Token < Tokens.size(); ++Token)
				{
					const std::size_t Dash = Tokens[Token].find('-');
					const std::optional<uint32> First = Hex(Tokens[Token].substr(0, Dash));
					const std::optional<uint32> Last = Dash == std::string_view::npos ? First : Hex(Tokens[Token].substr(Dash + 1));
					if (!First || !Last || *First > *Last || *Last >= VariableBytes || ((*First | *Last) & 1) != 0)
					{
						Fail("not a variable's address or a range of them:", Tokens[Token]);
						return;
					}
					for (uint32 Address = *First; Address <= *Last; Address += 2)
					{
						Result.UnsignedVariables.push_back(static_cast<uint16>(Address));
					}
				}
			}

			std::optional<int64> DirectionOf(std::string_view Token) const
			{
				return Token == Keep ? std::optional<int64>(KeepDirection) : NumberWithin(Token, 0, 0xFF);
			}

			void ReadEntrance(const std::vector<std::string_view>& Tokens)
			{
				constexpr std::array<std::string_view, 5> Words = {"x", "z", "layer", "camera", "facing"};
				bool bWords = Tokens.size() == 1 + 2 * Words.size();
				for (std::size_t Word = 0; bWords && Word < Words.size(); ++Word)
				{
					bWords = Tokens[1 + 2 * Word] == Words[Word];
				}
				if (!bWords)
				{
					Fail("an entrance reads 'entrance x N z N layer N camera N facing N'");
					return;
				}
				const std::optional<int64> X = NumberWithin(Tokens[2], -0x8000, 0x7FFF);
				const std::optional<int64> Z = NumberWithin(Tokens[4], -0x8000, 0x7FFF);
				const std::optional<int64> Layer = NumberWithin(Tokens[6], 0, 0xFF);
				const std::optional<int64> Camera = DirectionOf(Tokens[8]);
				const std::optional<int64> Facing = DirectionOf(Tokens[10]);
				if (!X || !Z || !Layer || !Camera || !Facing)
				{
					Fail("an entrance's number is out of range");
					return;
				}
				const std::size_t At = Result.Code.size();
				if (Emit(EntranceBytes))
				{
					LittleEndian::WriteU16(Result.Code, At, static_cast<uint16>(*X));
					LittleEndian::WriteU16(Result.Code, At + 2, static_cast<uint16>(*Z));
					Result.Code[At + 4] = static_cast<uint8>(*Layer);
					Result.Code[At + 5] = static_cast<uint8>(*Camera);
					Result.Code[At + 6] = static_cast<uint8>(*Facing);
				}
			}

			void ReadEntry(const std::vector<std::string_view>& Tokens)
			{
				const std::optional<int64> Actor = Tokens.size() == 4 ? NumberWithin(Tokens[1], 0, MaxActors - 1) : std::nullopt;
				const std::optional<int64> Script = Tokens.size() == 4 ? NumberWithin(Tokens[2], 0, FEventScripts::ScriptsPerActor - 1) : std::nullopt;
				if (!Actor || !Script)
				{
					Fail("an entry point reads 'entry actor script target'");
					return;
				}
				NamedEntries.push_back({Line, static_cast<int32>(*Actor), static_cast<int32>(*Script), std::string(Tokens[3])});
			}

			void ReadDefinition(const std::vector<std::string_view>& Tokens)
			{
				if (Tokens.size() != 3 || !IsName(Tokens[1]))
				{
					Fail("a definition reads 'define Name operand'");
				}
				else if (Labels.contains(Tokens[1]) || !Definitions.emplace(Tokens[1], Meaning(Tokens[2])).second)
				{
					Fail("a name defined twice:", Tokens[1]);
				}
			}

			void ReadData(const std::vector<std::string_view>& Tokens)
			{
				for (std::size_t Token = 1; Token < Tokens.size(); ++Token)
				{
					const std::optional<uint32> Byte = Tokens[Token].size() == 2 ? Hex(Tokens[Token]) : std::nullopt;
					if (!Byte)
					{
						Fail("data is bytes of two hexadecimal digits, not", Tokens[Token]);
						return;
					}
					if (Emit(1))
					{
						Result.Code.back() = static_cast<uint8>(*Byte);
					}
				}
			}

			/** Writes the opcode and leaves room for the operands, which are encoded once every label has its place. */
			void ReadInstruction(const std::vector<std::string_view>& Tokens)
			{
				const FEventInstructionShape* Shape = FindEventInstruction(Tokens.front());
				if (Shape == nullptr)
				{
					Fail("no instruction is named", Tokens.front());
					return;
				}
				FInstruction Instruction;
				Instruction.Line = Line;
				Instruction.Shape = Shape;
				for (std::size_t Token = 1; Token < Tokens.size(); ++Token)
				{
					Instruction.Tokens.emplace_back(Meaning(Tokens[Token]));
				}
				// The operands of a few instructions follow from their first one, a byte.
				uint8 First = 0;
				if (Shape->bFirstOperandSelects)
				{
					const std::optional<int64> Selector = Instruction.Tokens.empty() ? std::nullopt : NumberWithin(Instruction.Tokens.front(), 0, 0xFF);
					if (!Selector)
					{
						Fail("the instruction's first operand is a byte that selects the rest");
						return;
					}
					First = static_cast<uint8>(*Selector);
				}
				Instruction.Operands = &EventOperandsOf(*Shape, First);
				const std::size_t Start = Result.Code.size();
				if (!Emit(static_cast<std::size_t>(Shape->OpcodeBytes() + Instruction.Operands->Bytes)))
				{
					return;
				}
				Result.Code[Start] = static_cast<uint8>(Shape->Opcode > 0xFF ? EventOpcode::ExtendedPrefix : Shape->Opcode);
				Result.Code[Start + static_cast<std::size_t>(Shape->OpcodeBytes()) - 1] = static_cast<uint8>(Shape->Opcode);
				Instruction.At = Start + static_cast<std::size_t>(Shape->OpcodeBytes());
				Instructions.push_back(std::move(Instruction));
			}

			/** A jump's or an entry point's place: "@offset" or a label. */
			std::optional<uint16> TargetOf(std::string_view Token)
			{
				if (Token.starts_with('@'))
				{
					const std::optional<int64> Offset = NumberWithin(Token.substr(1), 0, 0xFFFF);
					if (!Offset)
					{
						Fail("not an offset:", Token);
						return std::nullopt;
					}
					return static_cast<uint16>(*Offset);
				}
				const auto Label = Labels.find(Token);
				if (Label == Labels.end())
				{
					Fail("no label is named", Token);
					return std::nullopt;
				}
				return Label->second;
			}

			/** An operand's stored word: a jump's from the labels, any other from its own spelling. */
			std::optional<FOperandWord> OperandOf(FEventOperandKind Kind, std::string_view Token)
			{
				if (Kind.Kind == EEventOperand::Jump)
				{
					const std::optional<uint16> Target = TargetOf(Token);
					return Target ? std::optional<FOperandWord>(FOperandWord{*Target}) : std::nullopt;
				}
				std::optional<FOperandWord> Operand = OperandOfWord(Kind, Token);
				if (!Operand)
				{
					Fail("not what the instruction takes there:", Token);
				}
				return Operand;
			}

			void Encode(const FInstruction& Instruction)
			{
				const FEventOperandList& Operands = *Instruction.Operands;
				const bool bIf = Instruction.Shape->Opcode == EventOpcode::If;
				std::vector<std::string_view> Tokens(Instruction.Tokens.begin(), Instruction.Tokens.end());
				uint8 Flags = 0;
				uint8 Named = Operands.FlagBits;
				if (bIf)
				{
					// "If a test b else target": the test's number goes into the flags, the rest are the operands.
					const std::optional<int32> Test = Tokens.size() >= 5 && Tokens[3] == "else" ? EventTestOfName(Tokens[1]) : std::nullopt;
					if (!Test)
					{
						Fail("If reads 'If a test b else label'");
						return;
					}
					Flags = static_cast<uint8>(*Test);
					Named |= TestBits;
					Tokens.erase(Tokens.begin() + 3);
					Tokens.erase(Tokens.begin() + 1);
				}
				const bool bOtherFlags = !Tokens.empty() && Tokens.back().starts_with(FlagsPrefix);
				if (bOtherFlags)
				{
					const std::optional<int64> Others = NumberWithin(Tokens.back().substr(FlagsPrefix.size()), 0, 0xFF);
					if (!Others || (*Others & Named) != 0)
					{
						Fail("flags= gives the flags byte's bits the operands do not");
						return;
					}
					Flags |= static_cast<uint8>(*Others);
					Tokens.pop_back();
				}
				std::size_t At = Instruction.At;
				std::size_t FlagsAt = Result.Code.size();
				std::size_t Token = 0;
				uint8 Decided = 0;
				for (int32 Operand = 0; Operand < Operands.Count; ++Operand)
				{
					const FEventOperandKind Kind = Operands.Kinds[static_cast<std::size_t>(Operand)];
					if (Kind.Kind == EEventOperand::Flags)
					{
						FlagsAt = At++;
						continue;
					}
					if (Token == Tokens.size())
					{
						Fail("the instruction takes more operands");
						return;
					}
					const std::optional<FOperandWord> Encoded = OperandOf(Kind, Tokens[Token++]);
					if (!Encoded)
					{
						return;
					}
					if (Kind.Kind == EEventOperand::ByFlag)
					{
						// Two operands can answer to one bit: they are then constants or variables together.
						const uint8 Bit = Encoded->bConstant ? Kind.FlagBit : uint8{0};
						if ((Decided & Kind.FlagBit) != 0 && (Flags & Kind.FlagBit) != Bit)
						{
							Fail("these two operands are constants or variables together");
							return;
						}
						Decided |= Kind.FlagBit;
						Flags |= Bit;
					}
					for (int32 Byte = 0; Byte < Kind.Size(); ++Byte)
					{
						Result.Code[At++] = static_cast<uint8>(Encoded->Stored >> (8 * Byte));
					}
				}
				if (Token != Tokens.size())
				{
					Fail("the instruction takes fewer operands");
					return;
				}
				if (FlagsAt < Result.Code.size())
				{
					Result.Code[FlagsAt] = Flags;
				}
				else if (bOtherFlags)
				{
					Fail("the instruction has no flags byte");
				}
			}

			FEventAssembly Result;
			std::map<std::string, std::string, std::less<>> Definitions;
			std::map<std::string, uint16, std::less<>> Labels;
			std::vector<FInstruction> Instructions;
			std::vector<FNamedEntry> NamedEntries;
			std::string Error;
			int32 Line = 0;
		};
	}

	TResult<FEventAssembly> AssembleEventText(std::string_view Text, const FEventAssemblyContext& Context)
	{
		return FAssembler(Context).Assemble(Text);
	}
}
