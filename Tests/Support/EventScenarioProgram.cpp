#include "Support/EventScenarioProgram.h"

#include <algorithm>
#include <span>

namespace Weltall::Testing
{
	namespace
	{
		using namespace EventScenarioShape;

		// The opcodes the scenarios draw from, in the oracle's order.
		constexpr std::array<uint32, 368> Opcodes = {
			0x00, 0x01, 0x02, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0D, 0x26, 0x31, 0x32, 0x33,
			0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43,
			0x48, 0x49, 0x6D, 0x6E, 0x84, 0x85, 0x86, 0x87, 0x88, 0x92, 0xA6, 0xA8, 0xC3, 0xC6, 0xC9,
			0xCA, 0xCB, 0xCC, 0xDC, 0xDE, 0xDF, 0xE2, 0xE3, 0xFB,
			0xFE00, 0xFE05, 0xFE06, 0xFE0A, 0xFE0B, 0xFE28, 0xFE29, 0xFE2A, 0xFE2B, 0xFE2C, 0xFE2D, 0xFE2E,
			0xFE2F, 0xFE30, 0xFE31, 0xFE32, 0xFE33, 0xFE34, 0xFE35, 0xFE36, 0xFE37, 0xFE72, 0xFE73, 0xFE76,
			0xFED3,
			0x18, 0x1F, 0x20, 0x22, 0x23, 0x24, 0x25, 0x28, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x46,
			0x5F, 0x67, 0x69, 0x6B, 0x6C, 0x6F, 0x70, 0x89, 0x94, 0x95, 0x96, 0xCD, 0xCE, 0xD7, 0xD8, 0xD9,
			0xF8, 0xF9, 0xFE01, 0xFE07, 0xFE09, 0xFE0D, 0xFE17, 0xFE38, 0xFE71, 0xFE75,
			0x34, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0xB9, 0xBA,
			0xBB, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0xFE1C,
			0x0E, 0x0F, 0x13, 0xD1, 0xE4, 0xFD, 0xFF, 0xFE74, 0xFE79, 0xFE7A, 0xFED2,
			0xFE3A, 0xFE3B, 0xFE69, 0xFE6B, 0xFE8B, 0xFE9F, 0xFE41, 0xFE42,
			0x9D, 0xA0, 0xA1, 0xA4, 0xB6,
			0x21, 0x27, 0x29, 0x99, 0xE6, 0xE7, 0xF7, 0xFE51, 0xFE52, 0xFE53, 0xFE54, 0xFE8E, 0x72, 0x75,
			0xFEB8, 0xFEB7, 0xF1, 0x9B, 0x80, 0xFE25, 0xFE4F, 0xFE50,
			0xE5, 0xFE3D, 0xFE3E, 0xFE3F, 0xFE6A, 0xFE70, 0xFE80, 0xFE81, 0xFE82,
			0x58, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xFA, 0xFE03, 0xFE08, 0xFE47, 0xFE5F, 0xFEC3, 0xFEC4,
			0xF6, 0xFE04, 0xFE0C, 0xFE13, 0xFE14,
			0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0xA3, 0xAB, 0xAC,
			0x0C, 0xA7, 0x14, 0x15, 0xD5, 0xFE43, 0xFEAE, 0xFED9,
			0x59, 0x5A, 0x5B, 0xEF, 0xFE99,
			0xE1, 0x74, 0xFE65, 0xFE66,
			0xFE8F, 0xFE90, 0xFE91, 0xFE92, 0xFE93, 0xFE94, 0xFE95, 0xFE97, 0xFEA5, 0xFEBD, 0xFEC2, 0xFEC8, 0xFEC9,
			0xFE0E, 0xFE0F, 0xFE10, 0xFE11, 0xFE12, 0xFEA2,
			0xA2, 0xFEA8, 0xFEA9, 0xFEAA, 0xFE45,
			0xC4, 0xC5, 0xE8, 0xE9,
			0xDB, 0xDD, 0xE0, 0xFE46, 0xFE5B, 0xFEA4, 0xB5, 0x76, 0x77, 0xB7, 0xB8, 0xFE22, 0xAF, 0xB0, 0xB1, 0xF2, 0xFE48,
			0x9A, 0xEB, 0xEC, 0xED, 0xEE, 0xF0, 0xF3, 0xA5,
			0xDA, 0xFE40, 0xFE26, 0xFE27,
			0x17, 0xFE16, 0xFEB5, 0xFEB6, 0xFE23,
			0xFE5D, 0xFE62, 0xFE63, 0xFE64, 0xFE8C, 0xFE8A, 0xFE8D, 0xFE98,
			0x5D, 0x5E, 0x68, 0x6A, 0xAA, 0x7F, 0xFE39, 0xFE9A,
			0xAD, 0xAE, 0xC7, 0xC8, 0xFE49, 0xFE6F, 0xFE9E,
			0xFEBF, 0xFE83, 0xFE7F, 0xFEBB, 0xFEBC, 0xFEE0, 0xFE55, 0xFE56, 0xFE57, 0xFE58, 0xFE59, 0xFE5A, 0xFECF, 0xFEDA,
			0xFE87, 0xFE60, 0xFE61, 0xFE67, 0xFEA0, 0xFECB, 0xFECC, 0xFE85, 0xFE86, 0xFE9B, 0xFE9C, 0xFE9D,
			0xFE88, 0xFE89,
			0x98, 0x12, 0x71, 0xFE84, 0x56, 0xFEDF, 0xFEAD, 0xFEB4, 0xFED5,
		};
		// The moves' scenarios: the walks and glides with the instructions that change their pace, their
		// actors' flags and their course; the party's walks as a group, the jump and the leader's walk.
		constexpr std::array<uint32, 22> MoveOpcodes = {0x44, 0x45, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x21, 0xF8, 0x01,
			0xFE23, 0xFE24, 0xFEB5, 0x57, 0xFE68};
		// The jumps' scenarios: jumps, the party's walks (FE B5 ends them at the next tick), the leader's walk
		// (to where its record stands, half the time) and the movement bounds, with the pace (21) and the course (01).
		constexpr std::array<uint32, 12> JumpOpcodes = {0x57, 0x57, 0x57, 0xFE23, 0xFE24, 0xFEB5, 0xFEB5, 0xFE68, 0x21, 0x01, 0x17, 0xFE16};
		constexpr std::array<uint32, 5> IfKinds = {0x00, 0x40, 0x80, 0xC0, 0x10};
		constexpr std::array<uint32, 4> Characters = {0xFC, 0xFD, 0xFE, 0xFF};

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

		bool IsOneOf(uint32 Opcode, std::initializer_list<uint32> Candidates)
		{
			return std::find(Candidates.begin(), Candidates.end(), Opcode) != Candidates.end();
		}

		class FProgram
		{
		public:
			FProgram(FScenarioRandom& InGenerator, uint32 InActors, uint32 InPlanned, EEventScenarioFamily InFamily)
				: G(InGenerator), Actors(InActors), Planned(InPlanned), Family(InFamily)
			{
			}

			std::vector<FInstruction> Instructions;

			FField Variable() { return {EField::Word, G.Below(0x800)}; }

			FField Value()
			{
				if (G.Below(4) == 0)
				{
					return Variable();
				}
				return {EField::Word, 0x8000 | (G.Next() & 0x7FFF)};
			}

			FField SmallValue(uint32 Limit)
			{
				if (G.Below(4) == 0)
				{
					return Variable();
				}
				return {EField::Word, 0x8000 | G.Below(Limit)};
			}

			FField Target() { return {EField::Jump, G.Below(Planned)}; }

			uint32 Actor(bool bMustExist)
			{
				const uint32 Pick = G.Below(Actors + 4);
				if (Pick < Actors)
				{
					return Pick;
				}
				constexpr std::array<uint32, 4> Aliases = {0xFB, 0xFF, 0xFE, 0xFD};
				const uint32 Alias = Aliases[Pick - Actors];
				return bMustExist && Alias == 0xFD ? 0xFB : Alias;
			}

			FField Flagged(uint32 Flags, uint32 Bit)
			{
				if ((Flags & Bit) != 0)
				{
					return {EField::Word, G.Next() & 0xFFFF};
				}
				return Variable();
			}

			/** A flags byte, then an operand for each bit, then the byte itself. */
			void FlaggedRun(FInstruction& Body, std::initializer_list<uint32> Bits)
			{
				const uint32 Flags = G.Next() & 0xFF;
				for (const uint32 Bit : Bits)
				{
					Body.push_back(Flagged(Flags, Bit));
				}
				Body.push_back({EField::Byte, Flags});
			}

			/** A constant character: a party alias (FF/FE/FD) or none (FC), or an id below the limit. */
			FField Character(uint32 Limit)
			{
				const bool bAlias = G.Below(2) == 0;
				const uint32 Value = bAlias ? Characters[G.Below(4)] : G.Below(Limit);
				return {EField::Word, 0x8000 | Value};
			}

			/** A ground coordinate: a constant on or somewhat off the walkmesh's grid, or a variable's address. */
			FField Ground(uint32 Flags, uint32 Bit)
			{
				if ((Flags & Bit) != 0)
				{
					return {EField::Word, (G.Below(0xC00) - 0x600) & 0xFFFF};
				}
				return Variable();
			}

			/** Constant coordinates small enough that their squared differences add up within 31 bits. */
			void Coordinates(FInstruction& Body, std::initializer_list<uint32> Bits)
			{
				uint32 Flags = G.Next() & 0xFF;
				for (const uint32 Bit : Bits)
				{
					Flags |= Bit;
				}
				for (std::size_t Operand = 0; Operand < Bits.size(); ++Operand)
				{
					Body.push_back({EField::Word, (G.Below(0x4000) - 0x2000) & 0xFFFF});
				}
				Body.push_back({EField::Byte, Flags});
			}

			/** Constant coordinates within the walkmesh's square (the flags make each a constant): a move's distance stays far from 31 bits. */
			std::vector<FField> Near(std::initializer_list<uint32> Bits, uint32& Flags)
			{
				Flags = G.Next() & 0xFF;
				for (const uint32 Bit : Bits)
				{
					Flags |= Bit;
				}
				std::vector<FField> Operands;
				for (std::size_t Operand = 0; Operand < Bits.size(); ++Operand)
				{
					Operands.push_back({EField::Word, (G.Below(0xC00) - 0x600) & 0xFFFF});
				}
				return Operands;
			}

			/** A height offset of a glide: a constant within 100h either way, its flags byte making it one. */
			FField Height() { return {EField::Word, (G.Below(0x200) - 0x100) & 0xFFFF}; }

			void AddMove(uint32 Opcode, FInstruction& Body)
			{
				uint32 Flags = 0;
				if (IsOneOf(Opcode, {0x4A, 0x4B, 0x4E, 0x4F}))
				{
					const std::vector<FField> Operands = Near({0x80, 0x40}, Flags);
					Body.insert(Body.end(), Operands.begin(), Operands.end());
					Body.push_back({EField::Byte, Flags});
					if (IsOneOf(Opcode, {0x4B, 0x4F}))
					{
						Body.push_back(SmallValue(24));
					}
				}
				else if (IsOneOf(Opcode, {0x4C, 0x4D, 0x50, 0x51}))
				{
					const std::vector<FField> Operands = Near({0x80, 0x40, 0x20}, Flags);
					Body.push_back(Operands[0]);
					Body.push_back(Operands[1]);
					Body.push_back({EField::Byte, Flags});
					Body.push_back(Operands[2]);
					if (IsOneOf(Opcode, {0x4D, 0x51}))
					{
						Body.push_back(SmallValue(24));
					}
				}
				else if (Opcode == 0x44)
				{
					Body.push_back(Value());
					Body.push_back(SmallValue(24));
				}
				else if (Opcode == 0x45)
				{
					Body.push_back(Value());
					Body.push_back(Height());
					Body.push_back(SmallValue(24));
					Body.push_back({EField::Byte, (G.Next() & 0xFF) | 0x80});
				}
				else if (IsOneOf(Opcode, {0x52, 0x53}))
				{
					// Never an empty party slot: that ends the walk at once with its stage left, and the next
					// move would take an actor from its own operands.
					Body.push_back({EField::Byte, Actor(true)});
					if (Opcode == 0x53)
					{
						Body.push_back(SmallValue(24));
					}
				}
				else if (Opcode == 0x57)
				{
					AddJump(Body);
				}
				else if (Opcode == 0xFE68 && Family == EEventScenarioFamily::Jumps && G.Below(2) == 0)
				{
					// The leader's record's place (x, z, y) read into variables just before (2D FF): the walk ends
					// at once where the record and the actor agree.
					const FField X = Variable();
					const FField Z = Variable();
					const FField Y = Variable();
					Body.insert(Body.begin(), {{EField::Byte, 0x2D}, {EField::Byte, 0xFF}, X, Z, Y});
					Body.insert(Body.end(), {X, Z, {EField::Byte, G.Next() & 0x3F}});
				}
				else if (Opcode == 0xFE68)
				{
					const std::vector<FField> Operands = Near({0x80, 0x40}, Flags);
					Body.insert(Body.end(), Operands.begin(), Operands.end());
					Body.push_back({EField::Byte, Flags});
				}
				else if (IsOneOf(Opcode, {0x54, 0x55}))
				{
					// The byte indexes the records directly: a party alias would read past them.
					Body.push_back({EField::Byte, G.Below(Actors)});
					Body.push_back(Height());
					Body.push_back({EField::Byte, (G.Next() & 0xFF) | 0x80});
					if (Opcode == 0x55)
					{
						Body.push_back(SmallValue(24));
					}
				}
			}

			/**
			 * 57: a setup and its flight as one instruction (nothing lands on the flight alone), or the snap. A
			 * layer to land on is a constant one of the walkmesh's; a speed's distance stays within 31 bits.
			 */
			void AddJump(FInstruction& Body)
			{
				const uint32 Kind = G.Below(4);
				if (Kind == 3)
				{
					Body.push_back({EField::Byte, 0x0F});
					return;
				}
				const uint32 Mode = Kind | (G.Below(3) == 0 ? 0x80u : 0u);
				const uint32 Flags = (G.Next() & 0xFF) | 0xE0;
				const FField X{EField::Word, (G.Below(0xC00) - 0x600) & 0xFFFF};
				const FField Z{EField::Word, (G.Below(0xC00) - 0x600) & 0xFFFF};
				const FField Y = (Mode & 0x80) != 0 ? FField{EField::Word, G.Below(WalkingLayers + 1)} : FField{EField::Word, (G.Below(0x200) - 0x100) & 0xFFFF};
				const FField Ticks = (Flags & 0x10) != 0 ? FField{EField::Word, (G.Below(24) - 2) & 0xFFFF} : Variable();
				Body.insert(Body.end(), {{EField::Byte, Mode}, X, Z, Y, Ticks, {EField::Byte, Flags}, {EField::Byte, 0x57}});
				Body.push_back({EField::Byte, (G.Next() & 0xF0) | 0x03});
			}

			/**
			 * FE 23: 7FFFh stops the party where it stands (the other operands are not read; the walk needs the
			 * actors' sprites, which only the moves' scenarios give them), or the walk, each direction within
			 * the table's sixteen or FF to keep the facing.
			 */
			void AddPartyWalk(FInstruction& Body)
			{
				if (Family == EEventScenarioFamily::Machine || G.Below(4) == 0)
				{
					const uint32 Flags = (G.Next() & 0xFF) | 0x80;
					Body.push_back({EField::Word, 0x7FFF});
					for (int32 Operand = 0; Operand < 5; ++Operand)
					{
						Body.push_back({EField::Word, G.Next() & 0xFFFF});
					}
					Body.push_back({EField::Byte, Flags});
					for (int32 Operand = 0; Operand < 3; ++Operand)
					{
						Body.push_back({EField::Word, G.Next() & 0xFFFF});
					}
					return;
				}
				uint32 Flags = 0;
				const std::vector<FField> Operands = Near({0x80, 0x40, 0x20, 0x10, 0x08, 0x04}, Flags);
				Body.insert(Body.end(), Operands.begin(), Operands.end());
				Body.push_back({EField::Byte, Flags});
				for (int32 Operand = 0; Operand < 3; ++Operand)
				{
					const bool bKeep = G.Below(4) == 0;
					Body.push_back({EField::Word, 0x8000 | (bKeep ? 0xFFu : G.Below(16))});
				}
			}

			/** The operands of the instructions that ask the field's main loop for programs, menus, movies and dissolves; false for any other. */
			bool AddRequest(uint32 Opcode, FInstruction& Body)
			{
				if (IsOneOf(Opcode, {0xFE7F, 0xFE55, 0xFE57, 0xFEDA, 0xFE87, 0xFE61, 0xFECB, 0xFECC}))
				{
				}
				else if (Opcode == 0xFEBF)
				{
					for (int32 Operand = 0; Operand < 6; ++Operand)
					{
						Body.push_back(Value());
					}
				}
				else if (IsOneOf(Opcode, {0xFE83, 0xFE56, 0xFE58, 0xFE59, 0xFE5A, 0xFE9B, 0xFE9C, 0xFE9D}))
				{
					Body.push_back(Value());
				}
				else if (IsOneOf(Opcode, {0xFEBB, 0xFE85}))
				{
					Body.push_back(Variable());
				}
				else if (IsOneOf(Opcode, {0xFEAD, 0xFEB4}))
				{
					Body.push_back(Variable());
					Body.push_back({EField::Byte, G.Below(3)});
				}
				else if (Opcode == 0xFED5)
				{
					Body.push_back(Variable());
					Body.push_back(Variable());
				}
				else if (Opcode == 0xFEBC)
				{
					FlaggedRun(Body, {0x80});
				}
				else if (IsOneOf(Opcode, {0xFEE0, 0xFE86}))
				{
					Body.push_back({EField::Byte, G.Below(2)});
				}
				else if (Opcode == 0xFECF)
				{
					Body.push_back(Value());
					Body.push_back(Value());
				}
				else if (Opcode == 0xFE60)
				{
					// The kinds 0-3, with the skip and sound flags.
					Body.push_back(Value());
					Body.push_back(Value());
					Body.push_back(Value());
					const uint32 Kind = G.Below(4);
					Body.push_back({EField::Word, 0x8000 | Kind | G.Below(4) << 6});
				}
				else if (Opcode == 0xFE67)
				{
					// FEh has the movie start again at its end.
					uint32 Flags = 0xFE;
					if (G.Below(4) != 0)
					{
						const uint32 Kind = G.Below(4);
						Flags = Kind | G.Below(2) << 6;
					}
					for (int32 Operand = 0; Operand < 4; ++Operand)
					{
						Body.push_back(Value());
					}
					Body.push_back({EField::Word, 0x8000 | Flags});
					for (int32 Operand = 0; Operand < 4; ++Operand)
					{
						Body.push_back(Value());
					}
				}
				else if (Opcode == 0xFEA0)
				{
					FlaggedRun(Body, {0x80, 0x40, 0x20, 0x10, 0x08});
				}
				else
				{
					return AddLeave(Opcode, Body);
				}
				return true;
			}

			/** The operands of the map changes, the battles and the world map the scripts ask the field for; false for any other. */
			bool AddLeave(uint32 Opcode, FInstruction& Body)
			{
				if (Opcode == 0x98)
				{
					Body.push_back(Value());
					Body.push_back(Value());
				}
				else if (Opcode == 0x12)
				{
					// While a movie is asked for, 12 stops inside itself and takes its first words for the transition
					// (the opcode with the map's low byte, the map's high byte with the entrance's low byte): a constant
					// map whose low byte has bit 7 and an entrance a variable from 80h to FFh make both constants (a
					// variable past the 1,024 would be memory the original does not own); the pc then stops on the
					// entrance's high byte, which ends the thread (00).
					Body.push_back({EField::Word, 0x8080 | (G.Next() & 0x7F7F)});
					Body.push_back({EField::Word, 0x80 | G.Below(0x80)});
					Body.push_back(Value());
					Body.push_back(Value());
				}
				else if (Opcode == 0x71)
				{
					Body.push_back(Value());
				}
				else if (Opcode == 0xFE84)
				{
					// The formation, a word the handler skips, and half the time no map after the battle (7FFFh).
					Body.push_back(Value());
					Body.push_back({EField::Word, G.Next() & 0xFFFF});
					if (G.Below(2) == 0)
					{
						Body.push_back({EField::Word, 0xFFFF});
					}
					else
					{
						Body.push_back(Value());
					}
					Body.push_back(Value());
				}
				else if (Opcode == 0x56)
				{
					FlaggedRun(Body, {0x80, 0x40, 0x20, 0x10});
				}
				else
				{
					return false;
				}
				return true;
			}

			/** The music, sound effect, particle, VRAM, camera, idle animation, door, scroll and wave instructions' operands; the moves' otherwise. */
			void AddPresentation(uint32 Opcode, FInstruction& Body)
			{
				if (AddRequest(Opcode, Body))
				{
				}
				else if (Opcode == 0xFEDF)
				{
					// The mode: 0 clears VRAM and widens the display, 1 and 2 show and hide the still, others nothing.
					Body.push_back(SmallValue(4));
				}
				else if (Opcode == 0xFE9A)
				{
					Body.push_back({EField::Byte, G.Below(4)});
					Body.push_back({EField::Byte, Actor(false)});
					for (int32 Operand = 0; Operand < 3; ++Operand)
					{
						Body.push_back(Value());
					}
				}
				else if (IsOneOf(Opcode, {0xFE88, 0xFE89}))
				{
					// A constant light of the three (past them the original's arrays spill into each other), then
					// its colours and radius (FE 88) or its place and the actor it follows (FE 89).
					const uint32 Flags = (G.Next() & 0xFF) | 0x80;
					Body.push_back({EField::Word, G.Below(3)});
					const uint32 Operands = Opcode == 0xFE88 ? 7 : 3;
					for (uint32 Operand = 0, Bit = 0x40; Operand < Operands; ++Operand, Bit >>= 1)
					{
						Body.push_back(Flagged(Flags, Bit));
					}
					Body.push_back({EField::Byte, Flags});
					if (Opcode == 0xFE89)
					{
						Body.push_back({EField::Byte, Actor(false)});
					}
				}
				else if (IsOneOf(Opcode, {0xC7, 0xC8}))
				{
					// The turn's ticks: a variable's may be negative or large.
					Body.push_back(SmallValue(8));
				}
				else if (Opcode == 0xAA)
				{
					// A direction of the table's second eight.
					Body.push_back({EField::Byte, G.Below(8)});
				}
				else if (IsOneOf(Opcode, {0xFE0E, 0xFE10}))
				{
					Body.push_back(Value());
					Body.push_back(Value());
				}
				else if (IsOneOf(Opcode, {0xFE0F, 0xFE11}))
				{
					FlaggedRun(Body, {0x80, 0x40});
				}
				else if (Opcode == 0xFE12)
				{
					Body.push_back(Value());
				}
				else if (Opcode == 0xFEA2)
				{
				}
				else if (IsOneOf(Opcode, {0xA2, 0xFE45}))
				{
					Body.push_back({EField::Byte, G.Next() & 0xFF});
				}
				else if (IsOneOf(Opcode, {0xFEA8, 0xFEA9, 0xAD, 0xAE}))
				{
					for (int32 Operand = 0; Operand < 3; ++Operand)
					{
						Body.push_back(Variable());
					}
				}
				else if (Opcode == 0xFEAA)
				{
					Body.push_back({EField::Byte, Actor(false)});
				}
				else if (IsOneOf(Opcode, {0xC4, 0xC5}))
				{
					Body.push_back({EField::Byte, G.Below(2)});
				}
				else if (IsOneOf(Opcode, {0xE8, 0xE9}))
				{
					// The speed, the steps, then up, down or an angle from the door's yaw.
					Body.push_back(Value());
					Body.push_back(SmallValue(40));
					const uint32 Angle = G.Below(0x1000);
					const std::array<uint32, 3> Directions = {0x1000, 0x1001, Angle};
					Body.push_back({EField::Word, 0x8000 | Directions[G.Below(3)]});
				}
				else if (Opcode == 0xFE8F)
				{
					Body.push_back({EField::Byte, Actor(false)});
					Body.push_back(SmallValue(5));
					Body.push_back(Value());
					Body.push_back(Value());
				}
				else if (Opcode == 0xFEC2)
				{
					Body.push_back(SmallValue(8));
					Body.push_back(SmallValue(5));
					Body.push_back(Value());
					Body.push_back(Value());
				}
				else if (Opcode == 0xFE90)
				{
					// A constant emitter: an index past the eight writes outside the setup.
					Body.push_back({EField::Word, 0x8000 | G.Below(8)});
					for (int32 Operand = 0; Operand < 3; ++Operand)
					{
						Body.push_back(Value());
					}
				}
				else if (IsOneOf(Opcode, {0xFE91, 0xFE92, 0xFE95}))
				{
					FlaggedRun(Body, {0x80, 0x40, 0x20, 0x10, 0x08, 0x04});
				}
				else if (Opcode == 0xFE93)
				{
					for (int32 Operand = 0; Operand < 5; ++Operand)
					{
						Body.push_back(Value());
					}
				}
				else if (Opcode == 0xFE94)
				{
					FlaggedRun(Body, {0x80, 0x40, 0x20, 0x10});
				}
				else if (Opcode == 0xFE97)
				{
					Body.push_back({EField::Byte, G.Below(2)});
				}
				else if (Opcode == 0xFEA5)
				{
					for (int32 Operand = 0; Operand < 3; ++Operand)
					{
						Body.push_back(Value());
					}
				}
				else if (Opcode == 0xFEBD)
				{
					// The shape, then four bytes the instruction skips.
					Body.push_back(SmallValue(3));
					Body.push_back({EField::Word, G.Next() & 0xFFFF});
					Body.push_back({EField::Word, G.Next() & 0xFFFF});
				}
				else if (IsOneOf(Opcode, {0xFEC8, 0xFEC9}))
				{
					FlaggedRun(Body, {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01});
				}
				else if (Opcode == 0x74)
				{
					// An effect, 0 stopping the channel (74 and FE 65).
					Body.push_back(SmallValue(4));
				}
				else if (Opcode == 0xFE65)
				{
					Body.push_back(SmallValue(4));
					Body.push_back(Value());
				}
				else if (Opcode == 0xFE66)
				{
					// The effect, its pan, volume and channel.
					Body.push_back(SmallValue(4));
					for (int32 Operand = 0; Operand < 3; ++Operand)
					{
						Body.push_back(Value());
					}
				}
				else if (Opcode == 0xFE5D)
				{
					// The effect, its pan and volume.
					Body.push_back(SmallValue(4));
					Body.push_back(Value());
					Body.push_back(Value());
				}
				else if (IsOneOf(Opcode, {0xFE62, 0xFE63}))
				{
					// The volume or pan, then a constant channel: past the eighth, the tracks lie outside the effect player.
					Body.push_back(Value());
					Body.push_back({EField::Word, 0x8000 | G.Below(8)});
				}
				else if (Opcode == 0xFE8C)
				{
					Body.push_back(Value());
					Body.push_back({EField::Word, 0x8000 | G.Below(8)});
					Body.push_back(Value());
				}
				else if (Opcode == 0xFE8A)
				{
					Body.push_back(SmallValue(4));
				}
				else if (IsOneOf(Opcode, {0xFE64, 0xFE8D, 0xFE98}))
				{
					Body.push_back(Value());
				}
				else if (Opcode == 0xE1)
				{
					// A source at (0, 0) clears the rectangle the other operands give.
					uint32 Flags = G.Next() & 0xFF;
					if (G.Below(2) == 0)
					{
						Flags |= 0xC0;
						Body.push_back({EField::Word, 0});
						Body.push_back({EField::Word, 0});
					}
					else
					{
						Body.push_back(Flagged(Flags, 0x80));
						Body.push_back(Flagged(Flags, 0x40));
					}
					for (const uint32 Bit : {0x20u, 0x10u, 0x08u, 0x04u})
					{
						Body.push_back(Flagged(Flags, Bit));
					}
					Body.push_back({EField::Byte, Flags});
				}
				else if (Opcode == 0xDA)
				{
					// Constants: a scroller has bands and a width; a few bands, so the ticks' copies stay few.
					const uint32 X = G.Below(0x400);
					const uint32 Y = G.Below(0x200);
					const uint32 Width = 1 + G.Below(64);
					const uint32 Height = G.Below(65);
					const uint32 Bands = 1 + G.Below(8);
					const uint32 ToX = G.Below(0x400);
					const uint32 ToY = G.Below(0x200);
					const uint32 Speed = G.Next() & 0xFFFF;
					for (const uint32 Value : {X, Y, Width, Height, Bands, ToX, ToY, Speed})
					{
						Body.push_back({EField::Word, Value});
					}
				}
				else if (Opcode == 0xFE40)
				{
					// Constant scroller and band: a negative band writes before the speeds (or near address 0).
					Body.push_back({EField::Word, 0x8000 | G.Below(32)});
					Body.push_back({EField::Word, 0x8000 | G.Below(12)});
					Body.push_back({EField::Word, G.Next() & 0xFFFF});
				}
				else if (Opcode == 0xFE26)
				{
					for (int32 Operand = 0; Operand < 7; ++Operand)
					{
						Body.push_back(Value());
					}
				}
				else if (Opcode == 0xFE27)
				{
					// Operations past 3 run the instruction's second byte as the next instruction.
					const uint32 Operation = G.Below(4);
					Body.push_back({EField::Byte, Operation});
					if (Operation == 0)
					{
						Body.push_back(Value());
					}
				}
				else
				{
					AddModelsAndCamera(Opcode, Body);
				}
			}

			/** Three variables' addresses, which an instruction writes. */
			void Variables(FInstruction& Body)
			{
				for (int32 Operand = 0; Operand < 3; ++Operand)
				{
					Body.push_back(Variable());
				}
			}

			/** The models' part values and draw modes, the gears' restore, the camera's turns, locks, registers, shake, tilt and orbits; the moves' otherwise. */
			void AddModelsAndCamera(uint32 Opcode, FInstruction& Body)
			{
				if (Opcode == 0xDB)
				{
					// A constant key: one past the 32 weights writes outside them.
					Body.push_back({EField::Word, 0x8000 | G.Below(32)});
					Body.push_back(Value());
				}
				else if (Opcode == 0xDD)
				{
					FlaggedRun(Body, {0x80, 0x40});
				}
				else if (Opcode == 0xE0)
				{
					Body.push_back({EField::Byte, Actor(false)});
					FlaggedRun(Body, {0x80, 0x40});
				}
				else if (Opcode == 0xFE46)
				{
					Body.push_back({EField::Byte, G.Below(3)});
				}
				else if (IsOneOf(Opcode, {0xFE5B, 0xFEA4}))
				{
					if (Opcode == 0xFE5B)
					{
						Body.push_back(Value());
					}
				}
				else if (Opcode == 0xB5)
				{
					// Directions past 7 read the steps' next rows; the camera looks in direction 0 throughout.
					Body.push_back({EField::Word, 0x8000 | G.Below(16)});
					Body.push_back(Value());
				}
				else if (IsOneOf(Opcode, {0x76, 0x77, 0xB7, 0xB8}))
				{
				}
				else if (IsOneOf(Opcode, {0xFE22, 0xA5}))
				{
					Body.push_back(Variable());
				}
				else if (IsOneOf(Opcode, {0xAF, 0xB0, 0xB1}))
				{
					// Mode 0 reads into the variable the word names; another sets the word itself.
					const uint32 Mode = G.Below(2);
					Body.push_back(Mode == 0 ? Variable() : FField{EField::Word, G.Next() & 0xFFFF});
					Body.push_back({EField::Byte, Mode});
				}
				else if (IsOneOf(Opcode, {0xF2, 0xFE9E}))
				{
					for (int32 Operand = 0; Operand < 4; ++Operand)
					{
						Body.push_back(Value());
					}
				}
				else if (IsOneOf(Opcode, {0xFE48, 0xFE6F}))
				{
					FlaggedRun(Body, {0x80, 0x40, 0x20});
				}
				else if (Opcode == 0x9A)
				{
					// A constant divisor: 0 moves the pc three bytes past the instruction.
					Body.push_back({EField::Word, 0x8000 | (1 + G.Below(16))});
				}
				else if (Opcode == 0xEB)
				{
					FlaggedRun(Body, {0x80, 0x40, 0x20, 0x10, 0x08, 0x04});
					Variables(Body);
				}
				else if (Opcode == 0xEC)
				{
					Body.push_back({EField::Byte, G.Below(4)});
					FlaggedRun(Body, {0x80, 0x40, 0x20});
					Variables(Body);
				}
				else if (Opcode == 0xED)
				{
					Body.push_back({EField::Byte, G.Below(4)});
					Variables(Body);
				}
				else if (Opcode == 0xEE)
				{
					// A destination past the four copies nothing.
					Body.push_back({EField::Byte, G.Below(4)});
					Body.push_back({EField::Byte, G.Below(5)});
				}
				else if (IsOneOf(Opcode, {0xF0, 0xF3}))
				{
					Variables(Body);
				}
				else if (Opcode == 0x17)
				{
					FlaggedRun(Body, {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01});
				}
				else if (IsOneOf(Opcode, {0xFE16, 0xFEB5, 0xFE24}))
				{
				}
				else if (Opcode == 0xFEB6)
				{
					Body.push_back({EField::Byte, Actor(false)});
				}
				else if (Opcode == 0xFE23)
				{
					AddPartyWalk(Body);
				}
				else
				{
					AddMove(Opcode, Body);
				}
			}

			void Add(uint32 Opcode)
			{
				FInstruction Body;
				if (Opcode > 0xFF)
				{
					Body = {{EField::Byte, 0xFE}, {EField::Byte, Opcode & 0xFF}};
				}
				else
				{
					Body = {{EField::Byte, Opcode}};
				}
				if (IsOneOf(Opcode, {0x00, 0x04, 0x0D, 0x33, 0x92, 0xC3, 0xC6, 0xFE00, 0x22, 0x23, 0x2A, 0x2B, 0x46, 0x96, 0xCD, 0xCE, 0xFE01, 0x59, 0x5A, 0x5B, 0x5E, 0xFE49}))
				{
				}
				else if (Opcode == 0x18)
				{
					for (int32 Size = 0; Size < 4; ++Size)
					{
						Body.push_back({EField::Byte, G.Below(64)});
					}
				}
				else if (IsOneOf(Opcode, {0x1F, 0x2C, 0xFE99, 0x5D}))
				{
					Body.push_back({EField::Byte, G.Next() & 0xFF});
				}
				else if (IsOneOf(Opcode, {0x20, 0x6B, 0x6C, 0xD7, 0xD8, 0xD9, 0xFE09, 0xEF}))
				{
					Body.push_back(Value());
				}
				else if (IsOneOf(Opcode, {0x24, 0x25, 0x28, 0x6F, 0xF9}))
				{
					Body.push_back({EField::Byte, Actor(false)});
				}
				else if (Opcode == 0x2D)
				{
					Body.push_back({EField::Byte, Actor(false)});
					Body.push_back(Variable());
					Body.push_back(Variable());
					Body.push_back(Variable());
				}
				else if (IsOneOf(Opcode, {0x2E, 0x2F, 0x30, 0xFE71}))
				{
					Body.push_back(Variable());
				}
				else if (Opcode == 0x5F)
				{
					Body.push_back({EField::Byte, G.Below(8)});
				}
				else if (IsOneOf(Opcode, {0x67, 0x68}))
				{
					Body.push_back({EField::Byte, Actor(false)});
					Body.push_back({EField::Word, 0x8000 | G.Below(16)});
				}
				else if (IsOneOf(Opcode, {0x69, 0x6A}))
				{
					Body.push_back({EField::Word, 0x8000 | G.Below(16)});
				}
				else if (Opcode == 0x70)
				{
					Body.push_back({EField::Byte, G.Below(3)});
				}
				else if (Opcode == 0x89)
				{
					Body.push_back({EField::Byte, Actor(false)});
					Body.push_back(Value());
					Body.push_back(Target());
				}
				else if (Opcode == 0x94)
				{
					Body.push_back(Value());
					Body.push_back(Value());
				}
				else if (Opcode == 0x95)
				{
					Body.push_back({EField::Byte, ClockModes[G.Below(static_cast<uint32>(ClockModes.size()))]});
				}
				else if (Opcode == 0xF8)
				{
					Body.push_back({EField::Byte, G.Below(9)});
					Body.push_back({EField::Word, G.Next() & 0xFFFF});
				}
				else if (Opcode == 0xFE07)
				{
					Body.push_back({EField::Byte, G.Below(3)});
				}
				else if (Opcode == 0xFE0D)
				{
					const bool bParty = G.Below(2) == 0;
					const uint32 Character = bParty ? Characters[G.Below(4)] : G.Below(16);
					Body.push_back({EField::Word, 0x8000 | Character});
				}
				else if (Opcode == 0xFE17)
				{
					Body.push_back({EField::Byte, Actor(false)});
					Body.push_back({EField::Byte, Actor(false)});
				}
				else if (Opcode == 0xFE38)
				{
					Body.push_back(Variable());
					Body.push_back({EField::Byte, Actor(false)});
					Body.push_back({EField::Byte, Actor(false)});
				}
				else if (Opcode == 0xFE75)
				{
					Body.push_back({EField::Byte, Actor(false)});
					Body.push_back(Variable());
				}
				else if (IsOneOf(Opcode, {0x34, 0x8B, 0x8C, 0x8D}))
				{
					const uint32 List = G.Below(5);
					Body.push_back({EField::Word, 0x8000 | List << 8 | G.Below(8)});
					if (Opcode == 0x34)
					{
						Body.push_back(Variable());
					}
					else if (Opcode == 0x8B)
					{
						Body.push_back(Target());
					}
				}
				else if (IsOneOf(Opcode, {0x79, 0x7A}))
				{
				}
				else if (IsOneOf(Opcode, {0x7B, 0x7C, 0x7D, 0x7E}))
				{
					const uint32 Flags = G.Next() & 0xFF;
					Body.push_back(Flagged(Flags, 0x80));
					Body.push_back({EField::Byte, Flags});
				}
				else if (Opcode == 0x8E)
				{
					const uint32 Amount = G.Below(20000000);
					Body.push_back({EField::Word, Amount & 0xFFFF});
					Body.push_back({EField::Word, Amount >> 16});
					Body.push_back(Target());
				}
				else if (IsOneOf(Opcode, {0x8F, 0x90}))
				{
					Body.push_back(Value());
				}
				else if (Opcode == 0x91)
				{
					const bool bEmpty = G.Below(4) == 0;
					Body.push_back({EField::Byte, bEmpty ? 0xFFu : G.Below(12)});
					Body.push_back(Target());
				}
				else if (Opcode == 0xB9)
				{
					Body.push_back({EField::Byte, G.Below(20)});
					Body.push_back(Target());
				}
				else if (IsOneOf(Opcode, {0xBA, 0xBB}))
				{
					Body.push_back({EField::Byte, G.Below(20)});
				}
				else if (Opcode == 0x19)
				{
					const uint32 Flags = G.Next() & 0xFF;
					Body.push_back(Ground(Flags, 0x80));
					Body.push_back(Ground(Flags, 0x40));
					Body.push_back({EField::Byte, Flags});
				}
				else if (Opcode == 0x1A)
				{
					Body.push_back({EField::Byte, G.Below(WalkingLayers)});
				}
				else if (Opcode == 0x1B)
				{
					const uint32 Flags = G.Next() & 0xFF;
					Body.push_back(Ground(Flags, 0x80));
					Body.push_back(Ground(Flags, 0x40));
					Body.push_back({EField::Byte, G.Below(WalkingLayers)});
					Body.push_back({EField::Byte, Flags});
				}
				else if (Opcode == 0x1C)
				{
					const uint32 Flags = G.Next() & 0xFF;
					Body.push_back(Flagged(Flags, 0x80));
					Body.push_back({EField::Byte, Flags});
				}
				else if (Opcode == 0x1D)
				{
					Body.push_back({EField::Word, (G.Below(0xC00) - 0x600) & 0xFFFF});
					Body.push_back({EField::Word, (G.Below(0xC00) - 0x600) & 0xFFFF});
					Body.push_back({EField::Word, G.Next() & 0xFFFF});
				}
				else if (IsOneOf(Opcode, {0x1E, 0x0E, 0x0F, 0x13, 0xD1, 0xE4, 0xFD, 0xFF, 0xFE79, 0xFE7A}))
				{
				}
				else if (IsOneOf(Opcode, {0xFE74, 0xFE8B}))
				{
					Body.push_back(Variable());
				}
				else if (IsOneOf(Opcode, {0xFE3A, 0xFE3B}))
				{
					Body.push_back(Character(16));
				}
				else if (IsOneOf(Opcode, {0xFE41, 0xFE42}))
				{
					Body.push_back({EField::Word, 0x8000 | G.Below(5)});
				}
				else if (Opcode == 0xFE9F)
				{
					Body.push_back({EField::Byte, G.Below(2)});
					Body.push_back(Character(16));
				}
				else if (Opcode == 0xFE69)
				{
					Body.push_back(Variable());
					Body.push_back(Character(11));
				}
				else if (Opcode == 0xFE6B)
				{
					Body.push_back(Value());
					Body.push_back(Character(11));
				}
				else if (Opcode == 0xFED2)
				{
					Body.push_back({EField::Word, G.Next() & 0xFFFF});
				}
				else if (Opcode == 0xFE1C)
				{
					FlaggedRun(Body, {0x80, 0x40, 0x20});
				}
				else if (IsOneOf(Opcode, {0x01, 0x05}))
				{
					Body.push_back(Target());
				}
				else if (Opcode == 0x02)
				{
					const uint32 Kind = IfKinds[G.Below(static_cast<uint32>(IfKinds.size()))];
					const uint32 Flags = Kind | G.Below(12);
					Body.push_back(Flagged(Flags, 0x80));
					Body.push_back(Flagged(Flags, 0x40));
					Body.push_back({EField::Byte, Flags});
					Body.push_back(Target());
				}
				else if (Opcode == 0x06)
				{
					Body.push_back(Target());
					Body.push_back({EField::Word, G.Next() & 0xFFFF});
				}
				else if (IsOneOf(Opcode, {0x07, 0x08, 0x09}))
				{
					Body.push_back({EField::Byte, Actor(false)});
					const uint32 Script = G.Below(32);
					Body.push_back({EField::Byte, Script | G.Below(8) << 5});
				}
				else if (IsOneOf(Opcode, {0x0A, 0xC9, 0xCB, 0xCC}))
				{
					Body.push_back({EField::Byte, G.Below(Zones)});
					Body.push_back(Target());
				}
				else if (Opcode == 0x26)
				{
					Body.push_back(SmallValue(4));
				}
				else if (IsOneOf(Opcode, {0x31, 0x32, 0xE2, 0xE3, 0xFE30, 0xFE31, 0xFE32, 0xFE33}))
				{
					Body.push_back({EField::Word, G.Next() & 0xFFFF});
					Body.push_back(Target());
				}
				else if (IsOneOf(Opcode, {0x35, 0x38, 0x39, 0x3A, 0x3B, 0x3E, 0x3F, 0x40, 0xDE, 0xDF}))
				{
					const uint32 Flags = G.Next() & 0xFF;
					Body.push_back(Variable());
					Body.push_back(Flagged(Flags, 0x40));
					Body.push_back({EField::Byte, Flags});
				}
				else if (IsOneOf(Opcode, {0x36, 0x37, 0x3C, 0x3D, 0x43, 0x88, 0xFE28, 0xFE29, 0xFE2A, 0xFE2B}))
				{
					Body.push_back(Variable());
				}
				else if (IsOneOf(Opcode, {0x41, 0x42, 0xA8}))
				{
					Body.push_back(Variable());
					Body.push_back(Value());
				}
				else if (IsOneOf(Opcode, {0x48, 0x49}))
				{
					Body.push_back(Target());
					Body.push_back(Variable());
					Body.push_back({EField::Word, 0x8000 | G.Below(2)});
					if (Opcode == 0x49)
					{
						Body.push_back({EField::Byte, G.Below(2)});
					}
				}
				else if (IsOneOf(Opcode, {0x6D, 0x6E, 0xCA}))
				{
					FInstruction Operands;
					FlaggedRun(Operands, {0x40, 0x20});
					Body.push_back(Variable());
					Body.insert(Body.end(), Operands.begin(), Operands.end());
				}
				else if (IsOneOf(Opcode, {0x84, 0x85, 0x86}))
				{
					Body.push_back(Value());
					Body.push_back(Target());
				}
				else if (Opcode == 0x87)
				{
					Body.push_back(Value());
				}
				else if (Opcode == 0xA6)
				{
					Body.push_back({EField::Word, 0x8000 | G.Below(3)});
					Instructions.push_back(Body);
					for (int32 Entry = 0; Entry < 3; ++Entry)
					{
						Instructions.push_back({{EField::Byte, 0x01}, Target()});
					}
					return;
				}
				else if (Opcode == 0xDC)
				{
					Body.push_back(Variable());
					Body.push_back(Variable());
				}
				else if (IsOneOf(Opcode, {0xFB, 0xFE0A, 0xFE0B}))
				{
					Body.push_back({EField::Word, G.Next() & 0x7FFF});
					if (Opcode == 0xFB)
					{
						Body.push_back(Target());
					}
				}
				else if (IsOneOf(Opcode, {0xFE05, 0xFE06}))
				{
					Body.push_back({EField::Byte, Actor(false)});
					Body.push_back(SmallValue(4));
					Body.push_back(Target());
				}
				else if (IsOneOf(Opcode, {0xFE2C, 0xFE2D, 0xFE2E, 0xFE2F}))
				{
					const uint32 High = G.Below(8) << 8;
					Body.push_back({EField::Word, High | Actor(false)});
				}
				else if (IsOneOf(Opcode, {0xFE34, 0xFE35, 0xFE36, 0xFE37}))
				{
					Body.push_back({EField::Word, G.Next() & 0xFFFF});
					Body.push_back({EField::Byte, Actor(true)});
					Body.push_back(Target());
				}
				else if (IsOneOf(Opcode, {0xFE72, 0xFE73, 0xFE76}))
				{
					FInstruction Operands;
					if (Opcode == 0xFE72)
					{
						FlaggedRun(Operands, {0x40, 0x20, 0x10});
					}
					else if (Opcode == 0xFE73)
					{
						Coordinates(Operands, {0x40, 0x20, 0x10, 0x08});
					}
					else
					{
						Coordinates(Operands, {0x40, 0x20, 0x20, 0x10, 0x08, 0x08});
					}
					Body.push_back(Variable());
					Body.insert(Body.end(), Operands.begin(), Operands.end());
				}
				else if (Opcode == 0xFED3)
				{
					Body.push_back(Value());
					Body.push_back(Value());
					Body.push_back({EField::Word, 0x8000 | (1 + G.Below(0x7FFF))});
					Body.push_back({EField::Word, 0x8000 | (1 + G.Below(0x7FFF))});
					Body.push_back(Value());
					Body.push_back(Value());
					Body.push_back(Variable());
					Body.push_back(Variable());
				}
				else if (Opcode == 0x9D)
				{
					Body.push_back(Value());
					Body.push_back({EField::Byte, G.Below(4)});
				}
				else if (Opcode == 0xA0)
				{
					Body.push_back(SmallValue(8));
					Body.push_back(Value());
					Body.push_back(Value());
				}
				else if (Opcode == 0xA1)
				{
					Body.push_back(Value());
				}
				else if (Opcode == 0xA4)
				{
					FlaggedRun(Body, {0x80});
				}
				else if (Opcode == 0xB6)
				{
					Body.push_back(Value());
					Body.push_back(SmallValue(4));
				}
				else if (IsOneOf(Opcode, {0x21, 0xFE8E}))
				{
					Body.push_back(Value());
					if (Opcode == 0xFE8E)
					{
						Body.push_back(Value());
					}
				}
				else if (IsOneOf(Opcode, {0x27, 0x29}))
				{
					Body.push_back({EField::Byte, Actor(false)});
				}
				else if (IsOneOf(Opcode, {0x99, 0xFE51, 0xFE52, 0xFE53, 0xFE54, 0xFE4F, 0xFE50}))
				{
				}
				else if (Opcode == 0xE6)
				{
					for (int32 Side = 0; Side < 4; ++Side)
					{
						Body.push_back({EField::Word, G.Next() & 0xFFFF});
					}
				}
				else if (Opcode == 0xE7)
				{
					Body.push_back(Value());
					Body.push_back(Value());
					Body.push_back(Value());
				}
				else if (Opcode == 0xF7)
				{
					// Constants: more encounters than a period has ticks never finish drawing.
					const uint32 Period = G.Below(0x100);
					Body.push_back({EField::Word, 0x8000 | Period});
					Body.push_back({EField::Word, 0x8000 | G.Below(std::min<uint32>(0x22, Period + 2))});
				}
				else if (IsOneOf(Opcode, {0x72, 0x75}))
				{
					Body.push_back(SmallValue(0x100));
				}
				else if (Opcode == 0xFEB8)
				{
					Body.push_back({EField::Byte, G.Below(3)});
					Body.push_back(Value());
				}
				else if (IsOneOf(Opcode, {0xFEB7, 0x9B}))
				{
					Body.push_back(Value());
					if (Opcode == 0x9B)
					{
						Body.push_back(Value());
					}
				}
				else if (Opcode == 0xF1)
				{
					Body.push_back(SmallValue(4));
					Body.push_back(Value());
					Body.push_back(Value());
					Body.push_back(Value());
					Body.push_back(SmallValue(0x40));
				}
				else if (Opcode == 0x80)
				{
					Body.push_back({EField::Byte, G.Below(8)});
					Body.push_back({EField::Byte, G.Below(6)});
					Body.push_back(Value());
				}
				else if (Opcode == 0xFE25)
				{
					Body.push_back({EField::Byte, G.Next() & 0xFF});
				}
				else if (IsOneOf(Opcode, {0xE5, 0xFE82, 0xFE3F, 0xFE6A, 0xFE70}))
				{
					const int32 Count = Opcode == 0xE5 ? 8 : Opcode == 0xFE82 ? 12 : Opcode == 0xFE3F ? 3 : 1;
					for (int32 Operand = 0; Operand < Count; ++Operand)
					{
						Body.push_back(Value());
					}
				}
				else if (IsOneOf(Opcode, {0xFE3D, 0xFE3E}))
				{
					// A constant light whose words stay within the two matrices: lights past the three spill
					// into the next rows, the pads and (colours, from light -16) the light matrix.
					const uint32 Flags = (G.Next() & 0xFF) | 0x80;
					const uint32 Light = Opcode == 0xFE3D ? G.Below(10) : G.Below(26) - 16;
					Body.push_back({EField::Word, Light & 0xFFFF});
					for (const uint32 Bit : {0x40u, 0x20u, 0x10u})
					{
						Body.push_back(Flagged(Flags, Bit));
					}
					Body.push_back({EField::Byte, Flags});
				}
				else if (Opcode == 0xFE80)
				{
					// A width of 0 becomes 1.
					for (int32 Operand = 0; Operand < 2; ++Operand)
					{
						Body.push_back({EField::Word, G.Next() & 0xFFFF});
					}
					Body.push_back({EField::Word, G.Below(4) == 0 ? 0 : G.Next() & 0xFFFF});
					for (int32 Operand = 0; Operand < 4; ++Operand)
					{
						Body.push_back({EField::Word, G.Next() & 0xFFFF});
					}
				}
				else if (Opcode == 0xFE81)
				{
					FlaggedRun(Body, {0x80, 0x40, 0x20});
				}
				else if (Opcode == 0x58)
				{
					// Axis 3 sets no angle.
					Body.push_back(Value());
					Body.push_back({EField::Byte, G.Below(4)});
				}
				else if (IsOneOf(Opcode, {0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xFE03, 0xFE47, 0x7F, 0xFE39}))
				{
					Body.push_back(Value());
				}
				else if (Opcode == 0xFA)
				{
					// Operation 6 turns nothing.
					Body.push_back({EField::Byte, G.Below(7)});
					Body.push_back({EField::Byte, Actor(false)});
					Body.push_back(Value());
				}
				else if (IsOneOf(Opcode, {0xFE08, 0xFE5F}))
				{
					if (Opcode == 0xFE5F)
					{
						Body.push_back({EField::Byte, G.Below(4)});
					}
					for (int32 Operand = 0; Operand < 3; ++Operand)
					{
						Body.push_back(Value());
					}
				}
				else if (Opcode == 0xFEC3)
				{
				}
				else if (Opcode == 0xF6)
				{
					// Mode 3 changes nothing.
					Body.push_back({EField::Byte, G.Below(4)});
				}
				else if (Opcode == 0xFE04)
				{
					Body.push_back(Value());
				}
				else if (Opcode == 0xFE0C)
				{
					for (int32 Operand = 0; Operand < 6; ++Operand)
					{
						Body.push_back({EField::Word, G.Next() & 0xFFFF});
					}
				}
				else if (IsOneOf(Opcode, {0xFE13, 0xFE14}))
				{
					Body.push_back(SmallValue(4));
					Body.push_back(Value());
				}
				else if (IsOneOf(Opcode, {0x60, 0x64, 0xAB}))
				{
				}
				else if (IsOneOf(Opcode, {0x61, 0x63, 0x65, 0xA3}))
				{
					// Constants: AC's paced moves square the path's length, which must stay within 31 bits.
					Coordinates(Body, {0x80, 0x40, 0x20});
				}
				else if (IsOneOf(Opcode, {0x62, 0x66}))
				{
					Body.push_back({EField::Byte, Actor(false)});
				}
				else if (Opcode == 0xAC)
				{
					// Kind 4 moves nothing; bit 7 snaps the camera to the path's start.
					const uint32 Kind = G.Below(5);
					Body.push_back({EField::Byte, Kind | (G.Below(2) == 0 ? 0x80u : 0u)});
					Body.push_back(SmallValue(0x40));
				}
				else if (Opcode == 0xFEC4)
				{
					Body.push_back({EField::Byte, Actor(false)});
				}
				else if (IsOneOf(Opcode, {0x0C, 0xA7, 0x14, 0x15, 0xFE43}))
				{
				}
				else if (Opcode == 0xD5)
				{
					Body.push_back({EField::Word, G.Next() & 0xFFFF});
				}
				else if (Opcode == 0xFEAE)
				{
					for (int32 Operand = 0; Operand < 3; ++Operand)
					{
						Body.push_back(Value());
					}
				}
				else if (Opcode == 0xFED9)
				{
					Body.push_back({EField::Byte, G.Below(2)});
				}
				else
				{
					AddPresentation(Opcode, Body);
				}
				Instructions.push_back(Body);
			}

			/** The bytecode, and each instruction's offset. */
			std::vector<uint8> Layout(std::vector<uint32>& Offsets) const
			{
				uint32 Position = 0;
				for (const FInstruction& Instruction : Instructions)
				{
					Offsets.push_back(Position);
					for (const FField& Field : Instruction)
					{
						Position += Field.Kind == EField::Byte ? 1 : 2;
					}
				}
				std::vector<uint8> Code;
				for (const FInstruction& Instruction : Instructions)
				{
					for (const FField& Field : Instruction)
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
			uint32 Planned;
			EEventScenarioFamily Family;
		};
	}

	std::vector<uint8> GenerateEventProgram(FScenarioRandom& Random, uint32 Actors, uint32 Planned, std::vector<uint32>& Offsets, EEventScenarioFamily Family)
	{
		FProgram Program(Random, Actors, Planned, Family);
		const std::span<const uint32> Drawn = Family == EEventScenarioFamily::Jumps ? std::span<const uint32>(JumpOpcodes)
			: Family == EEventScenarioFamily::Moves ? std::span<const uint32>(MoveOpcodes) : std::span<const uint32>(Opcodes);
		for (uint32 Index = 0; Index < Planned; ++Index)
		{
			Program.Add(Drawn[Random.Below(static_cast<uint32>(Drawn.size()))]);
		}
		Program.Instructions.push_back({{EField::Byte, 0x01}, {EField::Jump, 0}});
		if (Family != EEventScenarioFamily::Machine)
		{
			Program.Instructions.push_back({{EField::Byte, 0x00}});
		}
		return Program.Layout(Offsets);
	}
}
