#include "Support/BattleAiScripts.h"

#include "Support/BattleAiWords.h"

#include <array>
#include <map>

namespace Weltall::Testing
{
	namespace
	{
		using namespace BattleAiWords;

		constexpr std::array<uint8, 5> BadTypes{0x11, 0x20, 0x5A, 0x80, 0xFF};
		constexpr std::array<uint8, 6> RawOps{0x00, 0x6E, 0x6F, 0x75, 0x7A, 0x7F};
		constexpr uint8 EndWord = 0xFD;
		constexpr uint8 StopWord = 0xFF;
		constexpr int32 MaxCommits = 3;
		constexpr int32 MaxCopies = 2;
		constexpr std::size_t EntrySize = 8;

		uint32 EnemyFirstMask(FXorShift32& G)
		{
			const uint32 Bits = (G.Next() << 3) & 0x7F8;
			return Bits | 1u << (3 + G.Next() % 8);
		}

		/** A queue entry of a random type as entry_fields draws it; Words (when given) the commands that write it. */
		std::map<uint32, uint8> EntryFields(FXorShift32& G, uint32 E, std::vector<FWord>* Words)
		{
			const uint32 Kind = G.Next() % 20;
			const uint32 Type = Kind < 16 ? Kind + 1 : BadTypes[G.Next() % BadTypes.size()];
			std::map<uint32, uint8> Fields;
			auto Put = [&](uint32 Field, uint32 Value)
			{
				Fields[Field] = static_cast<uint8>(Value);
				if (Words != nullptr)
				{
					Words->push_back(Word(0x01, Field, Value));
				}
			};
			auto Put16 = [&](uint32 Field, uint32 Value)
			{
				Fields[Field] = static_cast<uint8>(Value);
				Fields[Field + 1] = static_cast<uint8>(Value >> 8);
				if (Words != nullptr)
				{
					Words->push_back(Word(0x3D, Field, Value, Value >> 8));
				}
			};
			auto Targets = [&](bool bSingle)
			{
				if (bSingle)
				{
					Put16(6, 1u << (G.Next() % 11));
				}
				else if (G.Next() % 3 != 0 || Words == nullptr)
				{
					Put16(6, G.Next() & 0x7FF);
				}
				else
				{
					Words->push_back(Word(0x52, 6, Wv(G, E)));
				}
			};
			auto Value = [&]()
			{
				const uint32 ValueKind = G.Next() % 3;
				if (ValueKind == 0 || (ValueKind == 2 && Words == nullptr))
				{
					Put16(4, G.Next() & 0xFFFF);
				}
				else if (ValueKind == 1)
				{
					Put(4, G.Next());
				}
				else
				{
					Words->push_back(Word(0x02, 4, Bv(G, E)));
				}
			};
			if (Type == 1)
			{
				const uint32 Command = G.Next() % EnemyCommands;
				Put(1, Command);
				Targets(Command < 3 || (Command >= 12 && Command <= 14));
				if (G.Next() % 2 != 0)
				{
					Put(2, G.Next());
				}
				if (G.Next() % 3 == 0)
				{
					Put(3, G.Next());
				}
				if (G.Next() % 2 != 0)
				{
					Value();
				}
			}
			else if (Type == 7)
			{
				Put16(6, EnemyFirstMask(G));
			}
			else if (Type == 8 || Type == 9)
			{
				Put(1, G.Next() % FieldCount);
				Value();
			}
			else if (Type == 10 || Type == 11)
			{
				Put(1, WritableWordField(G));
				Value();
			}
			else if (Type == 12 || Type == 13)
			{
				Put(3, G.Next() % 4 != 0 ? G.Next() : 0);
				if (Type == 13 && G.Next() % 2 != 0)
				{
					Value();
				}
			}
			else
			{
				if ((Type == 2 || Type == 5 || Type == 15) && G.Next() % 2 != 0)
				{
					Targets(false);
				}
				if (G.Next() % 2 != 0)
				{
					Value();
				}
			}
			if (Words != nullptr)
			{
				Words->push_back(Word(0x01, 0, Type));
			}
			Fields[0] = static_cast<uint8>(Type);
			return Fields;
		}

		/** One script's words, block by block, within the queue's and the copies' budgets. */
		struct FScriptWriter
		{
			FXorShift32& G;
			uint32 E;
			bool bEventWork;
			std::vector<FWord> Words;
			int32 Commits = 0;
			int32 Copies = 0;

			void Action(bool bGuardable, std::vector<uint32>& Guards)
			{
				const uint32 Kind = G.Next() % 16;
				if (Kind < 2 && bGuardable)
				{
					const auto [Command, Mask] = GuardedCommand(G, E);
					const bool bPut = Command[0] == 0x2B || Command[0] == 0x2D || Command[0] == 0x2F;
					if (bPut && Commits >= MaxCommits)
					{
						Words.push_back(PlainCommand(G, E, bEventWork));
						return;
					}
					if (bPut)
					{
						++Commits;
					}
					Guards.push_back(Mask);
					Words.push_back(Command);
				}
				else if (Kind < 5 && Commits < MaxCommits)
				{
					++Commits;
					EntryFields(G, E, &Words);
				}
				else if (Kind == 5 && Commits < MaxCommits)
				{
					++Commits;
					const uint32 Op = RawOps[G.Next() % RawOps.size()];
					const uint32 A = G.Next();
					const uint32 B = G.Next();
					Words.push_back(Word(Op, A, B, G.Next()));
				}
				else if (Kind == 6)
				{
					const uint32 Field = 2 + G.Next() % 4;
					if (G.Next() % 2 != 0)
					{
						Words.push_back(Word(0x01, Field, G.Next()));
					}
					else
					{
						Words.push_back(Word(0x02, Field, Bv(G, E)));
					}
				}
				else if (Kind == 7 && Copies < MaxCopies)
				{
					++Copies;
					const uint32 From = G.Next() % 32;
					Words.push_back(Word(0x03, From, 28 + G.Next() % 4));
				}
				else
				{
					Words.push_back(PlainCommand(G, E, bEventWork));
				}
			}

			void Block(bool bLast)
			{
				const std::size_t Start = Words.size();
				if (bLast)
				{
					Words.push_back(Word(G.Next() % 4 != 0 ? 0x80 : 0x9A));
				}
				else if (G.Next() % 6 == 0)
				{
					Words.push_back(Word(0x99));
					for (uint32 Count = 2 + G.Next() % 2; Count > 0; --Count)
					{
						Words.push_back(Condition(G, E));
					}
				}
				else
				{
					for (uint32 Count = 1 + G.Next() % 2; Count > 0; --Count)
					{
						Words.push_back(Condition(G, E));
					}
				}
				std::vector<uint32> Guards;
				const uint32 Actions = 1 + G.Next() % 3;
				for (uint32 Index = 0; Index < Actions; ++Index)
				{
					Action(Index == 0 && !bLast, Guards);
				}
				if (!Guards.empty())
				{
					Words.insert(Words.begin() + static_cast<std::ptrdiff_t>(Start), Word(0x8C, Guards[0], 0xFF, 0x07));
				}
				if (bLast)
				{
					Words.push_back(Word(EndWord, 0xFF, 0xFF, 0xFF));
					Words.push_back(Word(StopWord, 0xFF, 0xFF, 0xFF));
				}
				else if (G.Next() % 8 != 0)
				{
					Words.push_back(Word(EndWord, 0xFF, 0xFF, 0xFF));
				}
			}
		};

	}

	void DrawAiScript(FXorShift32& G, uint32 Enemy, bool bEventWork, std::vector<uint8>& Scene)
	{
		FScriptWriter Writer{G, Enemy, bEventWork, {}};
		const uint32 Blocks = 1 + G.Next() % 4;
		for (uint32 Index = 0; Index < Blocks; ++Index)
		{
			Writer.Block(Index == Blocks - 1);
		}
		for (const FWord& Each : Writer.Words)
		{
			Scene.insert(Scene.end(), Each.begin(), Each.end());
		}
	}

	void DrawAiQueueEntry(FXorShift32& G, std::span<uint8> Queue, uint32 Entry)
	{
		for (const auto& [Field, Value] : EntryFields(G, 0, nullptr))
		{
			Queue[Entry * EntrySize + Field] = Value;
		}
	}
}
