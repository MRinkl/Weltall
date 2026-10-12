#include "Support/MessageScenario.h"

#include <algorithm>

namespace Weltall::Testing
{
	namespace
	{
		// The opcodes the scenarios draw from, in the oracle's order.
		constexpr std::array<uint32, 24> Opcodes = {0xD2, 0xD2, 0xFC, 0xFC, 0xD4, 0xF5, 0xD3, 0x03, 0x9C, 0x9C, 0x9C, 0xA9,
			0xA9, 0xF4, 0xCF, 0xD0, 0xD6, 0xFE0D, 0x26, 0x27, 0x28, 0x29, 0xA0, 0x00};
		constexpr std::array<uint32, 6> MessageOpcodes = {0xD2, 0xF5, 0xD3, 0x03, 0xD4, 0xFC};
		constexpr std::array<uint32, 8> StyleBits = {0x1, 0x2, 0x4, 0x8, 0x40, 0x100, 0x400, 0x800};
		constexpr std::array<uint8, 3> Aliases = {0xFB, 0xFF, 0xFE};
		constexpr uint32 Portraits = 0x59;

		enum class EField : uint8
		{
			Byte,
			Word,
			/** The offset of the instruction with this index. */
			Jump,
		};

		struct FField
		{
			EField Kind;
			uint32 Value;
		};

		using FInstruction = std::vector<FField>;

		class FProgram
		{
		public:
			FProgram(FScenarioRandom& InRandom, uint32 InActors, std::vector<uint32> InMessages)
				: G(InRandom), Actors(InActors), Messages(std::move(InMessages))
			{
			}

			std::vector<FInstruction> Instructions;

			uint32 StyleByte()
			{
				if (G.Below(3) == 0)
				{
					return 0;
				}
				const uint32 Bits = G.Next() & 0xCF;
				return Bits | G.Below(3) << 4;
			}

			uint32 StyleWord()
			{
				uint32 Style = G.Below(3) << 4;
				for (const uint32 Bit : StyleBits)
				{
					if (G.Below(4) == 0)
					{
						Style |= Bit;
					}
				}
				return Style;
			}

			uint32 Actor()
			{
				const uint32 Pick = G.Below(Actors + static_cast<uint32>(Aliases.size()));
				return Pick < Actors ? Pick : Aliases[Pick - Actors];
			}

			FField Message() { return {EField::Word, Messages[G.Below(static_cast<uint32>(Messages.size()))]}; }

			FField Byte(uint32 Value) { return {EField::Byte, Value}; }
			FField Word(uint32 Value) { return {EField::Word, Value}; }

			void Add(uint32 Opcode)
			{
				FInstruction Fields;
				if (Opcode > 0xFF)
				{
					Fields = {Byte(0xFE), Byte(Opcode & 0xFF)};
				}
				else
				{
					Fields = {Byte(Opcode)};
				}
				const bool bMessage = std::find(MessageOpcodes.begin(), MessageOpcodes.end(), Opcode) != MessageOpcodes.end();
				switch (Opcode)
				{
				case 0xD2:
				case 0xF5:
				case 0xD3:
				case 0x03:
				{
					const FField Text = Message();
					Fields.insert(Fields.end(), {Text, Byte(StyleByte())});
					break;
				}
				case 0xD4:
				case 0xFC:
				{
					const FField Speaker = Byte(Actor());
					const FField Text = Message();
					Fields.insert(Fields.end(), {Speaker, Text, Byte(StyleByte())});
					break;
				}
				case 0xA9:
				{
					const uint32 First = G.Below(4) << 4;
					Fields.push_back(Byte(First | G.Below(6)));
					break;
				}
				case 0xF4:
					Fields.push_back(Byte(G.Below(2)));
					break;
				case 0xCF:
				{
					const uint32 X = G.Below(0x80);
					const uint32 Y = G.Below(0xC0);
					const uint32 Width = G.Below(20);
					Fields.insert(Fields.end(), {Byte(X), Byte(Y), Byte(Width), Byte(G.Below(6))});
					break;
				}
				case 0xD0:
				{
					const auto Either = [this](uint32 Limit) { return G.Below(2) == 0 ? 0u : G.Below(Limit); };
					const uint32 X = Either(0x100);
					const uint32 Y = Either(0xD0);
					const uint32 Width = Either(20);
					const uint32 Lines = G.Below(6);
					Fields.insert(Fields.end(), {Word(0x8000 | X), Word(0x8000 | Y), Word(0x8000 | Width), Word(0x8000 | Lines), Word(0x8000 | StyleWord())});
					break;
				}
				case 0xD6:
					Fields.push_back(Word(0x8000 | G.Below(4)));
					break;
				case 0xFE0D:
					Fields.push_back(Word(0x8000 | (G.Below(3) == 0 ? 0xFCu : G.Below(Portraits))));
					break;
				case 0x26:
					Fields.push_back(Word(0x8000 | (1 + G.Below(6))));
					break;
				case 0x27:
				case 0x28:
				case 0x29:
					Fields.push_back(Byte(Actor()));
					break;
				case 0xA0:
				{
					const uint32 Direction = G.Below(8);
					const uint32 Pitch = G.Below(90);
					Fields.insert(Fields.end(), {Word(0x8000 | Direction), Word(0x8000 | Pitch), Word(0x8000 | (0x100 + G.Below(0x300)))});
					break;
				}
				default:
					break;
				}
				Instructions.push_back(std::move(Fields));
				// Most messages are followed by a wait for their window, some by a choice first, as scripts do.
				if (bMessage && G.Below(4) != 0)
				{
					if (G.Below(3) == 0)
					{
						Add(0xA9);
					}
					Add(0x9C);
				}
			}

			std::vector<uint8> Layout(std::vector<uint32>& Offsets) const
			{
				uint32 Position = 0;
				for (const FInstruction& Fields : Instructions)
				{
					Offsets.push_back(Position);
					for (const FField& Field : Fields)
					{
						Position += Field.Kind == EField::Byte ? 1 : 2;
					}
				}
				std::vector<uint8> Code;
				for (const FInstruction& Fields : Instructions)
				{
					for (const FField& Field : Fields)
					{
						const uint32 Value = Field.Kind == EField::Jump ? Offsets[Field.Value] : Field.Value;
						Code.push_back(static_cast<uint8>(Value));
						if (Field.Kind != EField::Byte)
						{
							Code.push_back(static_cast<uint8>(Value >> 8));
						}
					}
				}
				return Code;
			}

		private:
			FScenarioRandom& G;
			uint32 Actors;
			std::vector<uint32> Messages;
		};

		void Put16(uint8* At, uint32 Value)
		{
			At[0] = static_cast<uint8>(Value);
			At[1] = static_cast<uint8>(Value >> 8);
		}

		void Put32(uint8* At, uint32 Value)
		{
			Put16(At, Value);
			Put16(At + 2, Value >> 16);
		}

		/** The messages a scenario opens: those whose layout has lines. */
		std::vector<uint32> UsableMessages(const FMessageTable& Messages)
		{
			std::vector<uint32> Usable;
			for (int32 Index = 0; Index < Messages.Count(); ++Index)
			{
				if (Messages.Lines(Index) != 0)
				{
					Usable.push_back(static_cast<uint32>(Index));
				}
			}
			return Usable;
		}
	}

	FMessageScenario GenerateMessageScenario(FScenarioRandom& Random, const FMessageTable& Messages)
	{
		FScenarioRandom& G = Random;
		FMessageScenario Scenario;
		const uint32 Actors = 1 + G.Below(5);
		const uint32 Planned = 40 + G.Below(24);
		FProgram Program(G, Actors, UsableMessages(Messages));
		for (uint32 Index = 0; Index < Planned; ++Index)
		{
			Program.Add(Opcodes[G.Below(static_cast<uint32>(Opcodes.size()))]);
		}
		Program.Instructions.push_back({{EField::Byte, 0x01}, {EField::Jump, 0}});
		std::vector<uint32> Offsets;
		const std::vector<uint8> Code = Program.Layout(Offsets);

		std::vector<uint8>& Section = Scenario.Section;
		Section.resize(32 * 4 + 4);
		for (uint32 Word = 0; Word < 32; ++Word)
		{
			Put32(&Section[Word * 4], G.Next());
		}
		Put32(&Section[32 * 4], Actors);
		for (uint32 Actor = 0; Actor < Actors; ++Actor)
		{
			std::array<uint8, 64> Entries{};
			Put16(&Entries[2], Offsets[G.Below(Planned)]);
			Section.insert(Section.end(), Entries.begin(), Entries.end());
		}
		Section.insert(Section.end(), Code.begin(), Code.end());

		for (uint32 Actor = 0; Actor < Actors; ++Actor)
		{
			std::array<uint8, EventStateBytes> State{};
			Put32(&State[0x04], G.Below(6) == 0 ? 0x200 : 0);
			State[0x80] = static_cast<uint8>(G.Below(2) == 0 ? 0xFF : G.Below(Portraits));
			State[0x81] = static_cast<uint8>(G.Below(0x100));
			if (G.Below(3) == 0)
			{
				State[0x82] = static_cast<uint8>(G.Below(20) * 3);
				State[0x83] = static_cast<uint8>(G.Below(6));
				const uint32 X = G.Below(0x100);
				Put16(&State[0x88], X);
				Put16(&State[0x8A], G.Below(0xD0));
			}
			Put32(&State[0x84], Program.StyleWord());
			const uint32 Direction = G.Next() & 0xFFFF;
			Put16(&State[0x104], Direction);
			Put16(&State[0x106], G.Next() & 0xFFFF);
			Put32(&State[0x12C], G.Below(8) << 9);
			for (uint32 Slot = 0; Slot < 8; ++Slot)
			{
				uint8* Thread = &State[0x8C + 8 * Slot];
				Put16(Thread, 0xFFFF);
				Thread[3] = 0xFF;
				Put32(Thread + 4, 0x3C0000 | 0xFFFF);
			}
			Scenario.States.push_back(State);

			std::array<uint8, 0x5C> Record{};
			for (uint32 Entry = 0; Entry < 9; ++Entry)
			{
				Put16(&Record[0x0C + 2 * Entry], G.Below(0x2000) - 0x1000);
			}
			for (uint32 Axis = 0; Axis < 3; ++Axis)
			{
				const uint32 Position = G.Below(0x800) - 0x400;
				Put32(&Record[0x20 + 4 * Axis], Position);
				Put32(&Record[0x40 + 4 * Axis], Position);
			}
			Put16(&Record[0x58], 0x200);
			Scenario.Records.push_back(Record);
		}
		for (uint16& Variable : Scenario.Variables)
		{
			Variable = static_cast<uint16>(G.Next());
		}
		Scenario.GameState.assign(0x2358, 0);
		for (uint32 Name = 0; Name < 31; ++Name)
		{
			const uint32 Length = 1 + G.Below(8);
			for (uint32 Glyph = 0; Glyph < Length; ++Glyph)
			{
				Scenario.GameState[20 * Name + Glyph] = static_cast<uint8>(0x20 + G.Below(0x37));
			}
		}
		for (uint32 Slot = 0; Slot < 3; ++Slot)
		{
			Scenario.GameState[0x1D34 + Slot] = static_cast<uint8>(G.Below(4) == 0 ? 0xFF : G.Below(12));
		}
		for (auto& Row : Scenario.View.M)
		{
			for (int16& Entry : Row)
			{
				Entry = static_cast<int16>(static_cast<int32>(G.Below(0x6000)) - 0x3000);
			}
		}
		Scenario.View.T[0] = static_cast<int32>(G.Below(0x800)) - 0x400;
		Scenario.View.T[1] = static_cast<int32>(G.Below(0x800)) - 0x400;
		Scenario.View.T[2] = 0x400 + static_cast<int32>(G.Below(0x1000));
		Scenario.ProjectionDistance = static_cast<uint16>(0x100 + G.Below(0x300));
		Scenario.GoalViewYaw = static_cast<int16>(G.Next());
		for (int32 Tick = 0; Tick < MessageScenarioTicks; ++Tick)
		{
			FMessageScenarioPads Pads;
			Pads.Pressed = G.Below(5) == 0 ? 0x20 : 0;
			const uint16 Down = G.Below(3) == 0 ? 0x4000 : 0;
			Pads.Repeat = static_cast<uint16>(Down | (G.Below(4) == 0 ? 0x1000 : 0));
			Scenario.Pads.push_back(Pads);
		}
		Scenario.EnteringTicks = G.Below(3) == 0 ? static_cast<int32>(G.Below(24)) : 0;
		Scenario.CloseAllTick = G.Below(3) == 0 ? static_cast<int32>(G.Below(MessageScenarioTicks)) : -1;
		return Scenario;
	}
}
