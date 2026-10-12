#include "Field/EventMachine.h"
#include "Field/FieldWalkmesh.h"
#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace Weltall;
using Weltall::Testing::FByteWriter;

namespace
{
	constexpr uint16 Counter = 0x400;
	constexpr uint16 Other = 0x402;

	/** Records the actors given sprites, each standing for a sprite whose first frame is 0x30 texels tall. */
	class FNoModels final : public IFieldActorModels
	{
	public:
		std::optional<FFieldSpriteResult> LoadSprite(const FFieldSpriteRequest& Request) override
		{
			Given.push_back(Request.Actor);
			return FFieldSpriteResult{0x30, false};
		}

		FSprite* ActorSprite(int32) override { return nullptr; }
		FSpriteSystem* SpriteSystem() override { return nullptr; }
		void SwapSprites(int32, int32) override {}
		bool LoadExtraAnimations(int32, int32) override { return true; }
		void UseExtraAnimations(int32) override {}

		std::vector<int32> Given;
	};

	/** A map with one bytecode and a few script actors, each with its scripts' entry points. */
	class FTestMap
	{
	public:
		FTestMap(std::vector<std::array<uint16, 32>> Entries, std::vector<uint8> Code)
		{
			FByteWriter Writer;
			for (int32 Word = 0; Word < 32; ++Word)
			{
				Writer.U32(0);
			}
			Writer.U32(static_cast<uint32>(Entries.size()));
			for (const auto& Actor : Entries)
			{
				for (const uint16 Entry : Actor)
				{
					Writer.U16(Entry);
				}
			}
			Writer.Bytes.insert(Writer.Bytes.end(), Code.begin(), Code.end());
			auto Parsed = FEventScripts::Parse(Writer.Bytes);
			REQUIRE(Parsed);
			Scripts = std::make_unique<FEventScripts>(std::move(*Parsed));
			Context.Scripts = Scripts.get();
			Context.MapNumber = 7;
			Context.RunningMap = 7;
			Context.Random = &Random;
			Context.Models = &Models;
			for (std::size_t Actor = 0; Actor < Entries.size(); ++Actor)
			{
				FFieldActor Record;
				Record.Flags = 0x200;
				Context.Actors.push_back(Record);
				FEventActorState& State = Context.States.emplace_back();
				State.Reset(Random.Next());
			}
			Context.bInitDone = true;
		}

		FEventActorState& State(int32 Actor) { return Context.States[static_cast<std::size_t>(Actor)]; }
		int32 Var(uint16 Address) { return Context.Variables.Get(Address, *Scripts); }

		void Tick()
		{
			const auto Error = FEventMachine::Update(Context);
			if (Error)
			{
				FAIL(Error->Message);
			}
		}

		void StartThread(int32 Actor, int32 Slot, uint16 Pc, uint32 Priority, uint8 Script = 0xFF)
		{
			FEventThread& Thread = State(Actor).Threads[static_cast<std::size_t>(Slot)];
			Thread.Pc = Pc;
			Thread.Script = Script;
			Thread.SetPriority(Priority);
		}

		std::unique_ptr<FEventScripts> Scripts;
		FPsxRandom Random;
		FNoModels Models;
		FEventContext Context;
	};

	std::array<uint16, 32> Entries(std::initializer_list<std::pair<int32, uint16>> Scripts)
	{
		std::array<uint16, 32> Result{};
		for (const auto& [Script, Entry] : Scripts)
		{
			Result[static_cast<std::size_t>(Script)] = Entry;
		}
		return Result;
	}

	/** Instructions: 3C IncVar, C3 Yield, 01 Jump, 00 End... */
	std::vector<uint8> Code(std::initializer_list<uint8> Bytes) { return Bytes; }
}

TEST_CASE("Each tick an actor runs its most urgent thread, the last slot among equals", "[Field]")
{
	// 0: Counter += 1, yield, loop. 4: Other += 1, yield, loop.
	FTestMap Map({Entries({})}, Code({0x3C, 0x00, 0x04, 0xC3, 0x01, 0x00, 0x00, 0x3C, 0x02, 0x04, 0xC3, 0x01, 0x07, 0x00}));
	Map.StartThread(0, 2, 0, 5);
	Map.StartThread(0, 5, 7, 5);
	Map.Tick();
	CHECK(Map.Var(Other) == 1);
	CHECK(Map.Var(Counter) == 0);
	CHECK(Map.State(0).Slot == 5);
	Map.StartThread(0, 1, 0, 4);
	Map.Tick();
	Map.Tick();
	CHECK(Map.Var(Counter) == 2);
	CHECK(Map.Var(Other) == 1);
	// The waiting thread kept its pc.
	CHECK(Map.State(0).Threads[5].Pc == 0x0B);
}

TEST_CASE("An actor with no thread restarts its default script at priority 7", "[Field]")
{
	// Script 1 at 3: Counter += 1, yield, end.
	FTestMap Map({Entries({{1, 3}})}, Code({0x00, 0x00, 0x00, 0x3C, 0x00, 0x04, 0xC3, 0x00}));
	Map.Tick();
	const FEventThread& Thread = Map.State(0).Threads[0];
	CHECK(Thread.Priority() == FEventMachine::DefaultPriority);
	// The restart does not record the script it runs.
	CHECK(Thread.Script == FEventThread::NoScript);
	Map.Tick();
	CHECK(Thread.IsFree());
	Map.Tick();
	CHECK(Map.Var(Counter) == 2);
}

TEST_CASE("A thread runs at most 8 instructions a tick unless instructions grant more", "[Field]")
{
	// A loop of Counter += 1 without a yield.
	FTestMap Map({Entries({{1, 0}})}, Code({0x3C, 0x00, 0x04, 0x01, 0x00, 0x00}));
	Map.Tick();
	CHECK(Map.Var(Counter) == 4);
	Map.Tick();
	CHECK(Map.Var(Counter) == 8);

	// C6 grants 32 more: 40 instructions, the grant and 39 of the loop.
	FTestMap Granted({Entries({{1, 0}})}, Code({0xC6, 0x3C, 0x00, 0x04, 0x01, 0x01, 0x00}));
	Granted.Tick();
	CHECK(Granted.Var(Counter) == 20);
}

TEST_CASE("A run that never yields stops after 0x401 instructions", "[Field]")
{
	// Each pass grants 32 instructions more than it costs.
	FTestMap Map({Entries({{1, 0}})}, Code({0xC6, 0xC6, 0x3C, 0x00, 0x04, 0x01, 0x00, 0x00}));
	Map.Tick();
	CHECK(Map.Var(Counter) == 0x100);
}

TEST_CASE("Waits and yields end an actor's turn", "[Field]")
{
	// Wait 2, Counter += 1, yield, Counter += 1, end: the wait runs 3 times, loading 2 and counting down.
	FTestMap Map({Entries({{1, 0}})}, Code({0x26, 0x02, 0x80, 0x3C, 0x00, 0x04, 0xC3, 0x3C, 0x00, 0x04, 0x00}));
	Map.Tick();
	Map.Tick();
	Map.Tick();
	CHECK(Map.Var(Counter) == 0);
	Map.Tick();
	CHECK(Map.Var(Counter) == 1);
	Map.Tick();
	CHECK(Map.Var(Counter) == 2);
}

TEST_CASE("The map's init runs every script 0 to its end, ignoring yields and budgets", "[Field]")
{
	// Actor 0's init: 12 increments with a yield in the middle, end. Actor 1's: end.
	std::vector<uint8> Bytes;
	for (int32 Step = 0; Step < 12; ++Step)
	{
		Bytes.insert(Bytes.end(), {0x3C, 0x00, 0x04});
		if (Step == 5)
		{
			Bytes.push_back(0xC3);
		}
	}
	Bytes.push_back(0x00);
	const uint16 Last = static_cast<uint16>(Bytes.size() - 1);
	FTestMap Map({Entries({{0, 0}, {2, Last}}), Entries({{0, Last}, {2, 0}})}, Bytes);
	Map.Context.PartyCharacters = {3, 0xFF, 5};
	const auto Error = FEventMachine::InitMap(Map.Context);
	REQUIRE_FALSE(Error);
	CHECK(Map.Var(Counter) == 12);
	CHECK(Map.Context.bInitDone);
	CHECK(Map.Var(0x3E) == 3);
	CHECK(Map.Var(0x40) == 0xFF);
	CHECK(Map.Var(0x42) == 5);
	// Both got the default sprite, which sets their height; the actor whose talk script is an End cannot be talked to.
	CHECK(Map.Models.Given == std::vector<int32>{0, 1});
	CHECK(Map.State(0).Height == 0x60);
	CHECK(Map.State(0).ModelId == 0x80);
	CHECK((Map.State(0).Flags1 & EventActorFlags1::NoTalkScript) != 0);
	CHECK((Map.State(1).Flags1 & EventActorFlags1::NoTalkScript) == 0);
}

TEST_CASE("The pass after the menu runs the first actor's script to its end at once and gives the actor back its state", "[Field]")
{
	// Script 3: Counter += 1, yield, Counter += 1, end; a thread of script 1 waits at 8.
	FTestMap Map({Entries({{1, 8}, {3, 0}})}, Code({0x3C, 0x00, 0x04, 0xC3, 0x3C, 0x00, 0x04, 0x00, 0xC3, 0x00}));
	Map.StartThread(0, 2, 8, 3, 1);
	Map.State(0).Slot = 2;
	Map.State(0).Pc = 8;
	const FEventActorState Before = Map.State(0);
	// A map change asked for does not cut it short.
	Map.Context.Requests.bMapChange = true;
	const auto Error = FEventMachine::RunScriptAside(Map.Context, 3);
	REQUIRE_FALSE(Error);
	CHECK(Map.Var(Counter) == 2);
	CHECK(Map.Context.bInitDone);
	CHECK(Map.State(0).Pc == Before.Pc);
	CHECK(Map.State(0).Slot == 2);
	CHECK(Map.State(0).Threads[2].Pc == 8);
	CHECK(Map.State(0).Threads[2].Priority() == 3);
	CHECK(Map.State(0).Threads[0].IsFree());
}

TEST_CASE("Calls share the actor's stack: a fifth call waits, an ended thread leaves its depth", "[Field]")
{
	// 0: call 4; 3: end. 4: Counter += 1, call 4 (recursing until the stack is full).
	FTestMap Map({Entries({{1, 0}})}, Code({0x05, 0x04, 0x00, 0x00, 0x3C, 0x00, 0x04, 0x05, 0x04, 0x00}));
	Map.Tick();
	CHECK(Map.State(0).CallDepth() == 4);
	CHECK(Map.Var(Counter) == 4);
	// The fifth call yields and waits for ever.
	CHECK(Map.State(0).Threads[0].Pc == 7);
	Map.Tick();
	CHECK(Map.Var(Counter) == 4);

	// 0D at depth 0 ends the thread; a thread ending at depth 2 leaves 2.
	FTestMap Ends({Entries({{1, 0}})}, Code({0x05, 0x04, 0x00, 0x00, 0x05, 0x08, 0x00, 0x00, 0x00}));
	Ends.Tick();
	CHECK(Ends.State(0).CallDepth() == 2);
	CHECK(Ends.State(0).Threads[0].IsFree());
}

TEST_CASE("Frozen actors keep their threads; removed actors and actors that take no part are skipped", "[Field]")
{
	FTestMap Map({Entries({{1, 0}}), Entries({{1, 0}}), Entries({{1, 0}})}, Code({0x3C, 0x00, 0x04, 0x00}));
	Map.State(0).Flags0 |= EventActorFlags0::Frozen;
	Map.State(1).Flags1 |= EventActorFlags1::Removed;
	Map.Context.Actors[2].Flags = 0x40;
	Map.Tick();
	CHECK(Map.Var(Counter) == 0);
	// The frozen actor's default thread was set up all the same.
	CHECK(Map.State(0).Threads[0].Priority() == FEventMachine::DefaultPriority);
	CHECK(Map.State(1).Threads[0].IsFree());
}

TEST_CASE("A requested battle, world map or map change stops the tick's scripts", "[Field]")
{
	FTestMap Map({Entries({{1, 0}})}, Code({0x3C, 0x00, 0x04, 0x00}));
	Map.Context.Requests.bMapChange = true;
	Map.Tick();
	CHECK(Map.Var(Counter) == 0);
	Map.Context.Requests.bMapChange = false;
	Map.Tick();
	CHECK(Map.Var(Counter) == 1);
}

TEST_CASE("FE 00 leaves its 00 to run as End", "[Field]")
{
	// FE 00 then (as 00) the end: two instructions of the budget.
	FTestMap Map({Entries({{1, 0}})}, Code({0xFE, 0x00, 0x3C, 0x00, 0x04}));
	Map.Tick();
	CHECK(Map.Var(Counter) == 0);
	CHECK(Map.State(0).Threads[0].IsFree());
	CHECK(Map.State(0).Pc == 1);
}

TEST_CASE("A script started on another actor reserves the slot of the starter's previous request", "[Field]")
{
	// Actor 0: 09 on actor 1, script 4 at priority 2; Counter += 1... Actor 1's script 4: 2 yields, end.
	FTestMap Map({Entries({{1, 0}}), Entries({{1, 8}, {4, 6}})}, Code({0x09, 0x01, 0x44, 0x3C, 0x00, 0x04, 0xC3, 0xC3, 0x00}));
	Map.State(0).RequestSlot = 3;
	Map.Tick();
	// The script took slot 0 of actor 1, the reservation went on slot 3.
	CHECK(Map.State(1).Threads[0].Script == 4);
	CHECK(Map.State(1).Threads[3].IsReserved());
	CHECK_FALSE(Map.State(1).Threads[0].IsReserved());
	CHECK(Map.State(0).RequestSlot == 0);
	for (int32 Tick = 0; Tick < 5; ++Tick)
	{
		Map.Tick();
	}
	CHECK(Map.Var(Counter) == 1);
	// The stale reservation stays, and keeps slot 3 from new threads.
	CHECK(Map.State(1).Threads[3].IsReserved());
	CHECK(FEventMachine::StartScript(Map.Context, 1, 5, 3));
	CHECK(FEventMachine::StartScript(Map.Context, 1, 6, 3));
	CHECK(FEventMachine::StartScript(Map.Context, 1, 7, 3));
	CHECK(FEventMachine::StartScript(Map.Context, 1, 8, 3));
	CHECK(Map.State(1).Threads[4].Script == 8);
	CHECK(Map.State(1).Threads[4].Priority() == 3);
	CHECK(Map.State(1).Threads[3].IsFree());
	CHECK_FALSE(FEventMachine::StartScript(Map.Context, 1, 8, 3));
}

TEST_CASE("An instruction the port does not run yet stops the scripts with its place", "[Field]")
{
	FTestMap Map({Entries({{1, 2}})}, Code({0x00, 0x00, 0x3C, 0x00, 0x04, 0xFE, 0xE2, 0x00}));
	// The map word a map change asked for (262 in gears) is not the map being run.
	Map.Context.MapNumber = 0xC106;
	const auto Error = FEventMachine::Update(Map.Context);
	REQUIRE(Error);
	CHECK(Error->Message == "field map 7, actor 0, pc 0005, opcode FE E2: opcode FE E2 is not implemented");
	CHECK(Map.Var(Counter) == 1);

	FTestMap Outside({Entries({{1, 0}})}, Code({0x01, 0x40, 0x00}));
	const auto Lost = FEventMachine::Update(Outside.Context);
	REQUIRE(Lost);
	CHECK(Lost->Message == "field map 7, actor 0, pc 0040: the pc 0040 is outside the 3-byte bytecode");
}

TEST_CASE("The field clock steps every 31 ticks and counts a 61-second minute up", "[Field]")
{
	FTestMap Map({Entries({})}, Code({0x00}));
	FFieldClock& Clock = Map.Context.Clock;
	CHECK(Clock.Mode == 0xFF);
	Map.Context.Variables.Set(FFieldClock::Address, 0x003B);
	Clock.Mode = 0;
	for (int32 Tick = 0; Tick < 30; ++Tick)
	{
		Clock.Tick(Map.Context.Variables, *Map.Scripts);
	}
	CHECK(Map.Var(FFieldClock::Address) == 0x003B);
	Clock.Tick(Map.Context.Variables, *Map.Scripts);
	CHECK(Map.Var(FFieldClock::Address) == 0x003C);
	for (int32 Tick = 0; Tick < 31; ++Tick)
	{
		Clock.Tick(Map.Context.Variables, *Map.Scripts);
	}
	CHECK(Map.Var(FFieldClock::Address) == 0x0100);

	// Counting down stops at 0:00; a stopped clock keeps its ticks' count but does not step.
	Map.Context.Variables.Set(FFieldClock::Address, 0x0100);
	Clock.Mode = FFieldClock::CountsDown;
	Clock.Ticks = 30;
	Clock.Tick(Map.Context.Variables, *Map.Scripts);
	CHECK(Map.Var(FFieldClock::Address) == 0x003B);
	Map.Context.Variables.Set(FFieldClock::Address, 0);
	Clock.Ticks = 30;
	Clock.Tick(Map.Context.Variables, *Map.Scripts);
	CHECK(Map.Var(FFieldClock::Address) == 0);
	Clock.Mode = FFieldClock::Stopped;
	Map.Context.Variables.Set(FFieldClock::Address, 5);
	Clock.Ticks = 30;
	Clock.Tick(Map.Context.Variables, *Map.Scripts);
	CHECK(Map.Var(FFieldClock::Address) == 5);
	CHECK(Clock.Ticks == 0);
}

TEST_CASE("Restoring the party's HP restores HP, or EP as in the original", "[Field]")
{
	// 7C: 40 (constant) to party slot 1 (target 2); then end.
	FTestMap Map({Entries({{1, 0}})}, Code({0x7C, 0x28, 0x00, 0x82, 0x00}));
	FGameState Game;
	using ECharacterPoints = FGameState::ECharacterPoints;
	Game.SetCharacterPoints(4, ECharacterPoints::Hp, 100);
	Game.SetCharacterPoints(4, ECharacterPoints::MaxHp, 120);
	Game.SetCharacterPoints(4, ECharacterPoints::Ep, 5);
	Game.SetCharacterPoints(4, ECharacterPoints::MaxEp, 30);
	Map.Context.GameState = &Game;
	Map.Context.PartyCharacters = {0, 4, 0xFF};
	Map.Tick();
	CHECK(Game.CharacterPoints(4, ECharacterPoints::Hp) == 120);
	CHECK(Game.CharacterPoints(4, ECharacterPoints::Ep) == 5);

	FTestMap Original({Entries({{1, 0}})}, Code({0x7C, 0x28, 0x00, 0x82, 0x00}));
	Original.Context.GameState = &Game;
	Original.Context.PartyCharacters = {0, 4, 0xFF};
	Original.Context.Options = FEventOptions::Original();
	Original.Tick();
	CHECK(Game.CharacterPoints(4, ECharacterPoints::Hp) == 120);
	CHECK(Game.CharacterPoints(4, ECharacterPoints::Ep) == 30);
}

TEST_CASE("FE AD and FE B4 read the HP and EP of the character in a party slot, and nothing from an empty one", "[Field]")
{
	// FE AD into 0x40 from slot 1, FE B4 into 0x42 from slot 1, FE AD into 0x44 from slot 2 (empty); then end.
	FTestMap Map({Entries({{1, 0}})}, Code({0xFE, 0xAD, 0x40, 0x00, 0x01, 0xFE, 0xB4, 0x42, 0x00, 0x01, 0xFE, 0xAD, 0x44, 0x00, 0x02, 0x00}));
	FGameState Game;
	using ECharacterPoints = FGameState::ECharacterPoints;
	Game.SetCharacterPoints(4, ECharacterPoints::Hp, 321);
	Game.SetCharacterPoints(4, ECharacterPoints::Ep, 45);
	Map.Context.GameState = &Game;
	Map.Context.PartyCharacters = {0, 4, 0xFF};
	Map.Context.Variables.Set(0x44, 7);
	Map.Tick();
	CHECK(Map.Var(0x40) == 321);
	CHECK(Map.Var(0x42) == 45);
	CHECK(Map.Var(0x44) == 7);

	// A slot past the kernel's three reads memory the port does not have.
	FTestMap Past({Entries({{1, 0}})}, Code({0xFE, 0xAD, 0x40, 0x00, 0x03, 0x00}));
	Past.Context.GameState = &Game;
	Past.Context.PartyCharacters = {0, 4, 0xFF};
	CHECK(FEventMachine::Update(Past.Context).has_value());

	// Carried past, it reads as an empty slot: nothing.
	FTestMap Recovered({Entries({{1, 0}})}, Code({0xFE, 0xAD, 0x40, 0x00, 0x03, 0x00}));
	FUnreachedCases Cases;
	Recovered.Context.Unreached = &Cases;
	Recovered.Context.GameState = &Game;
	Recovered.Context.PartyCharacters = {0, 4, 0xFF};
	Recovered.Context.Variables.Set(0x40, 7);
	Recovered.Tick();
	CHECK(Recovered.Var(0x40) == 7);
	CHECK(Cases.TakeNoted() == std::vector<std::string>{"a party slot past the three read"});
}

TEST_CASE("FE C7 reads a character's gear, and stores nothing for a destination past the variables", "[Field]")
{
	// FE C7 for character 3 into 0x40, then into 8004h as map 73's script names it; then end.
	FTestMap Map({Entries({{1, 0}})}, Code({0xFE, 0xC7, 0x03, 0x80, 0x40, 0x00, 0xFE, 0xC7, 0x03, 0x80, 0x04, 0x80, 0x00}));
	FGameState Game;
	Game.SetCharacterGear(3, 4);
	Map.Context.GameState = &Game;
	Map.Tick();
	CHECK(Map.Var(0x40) == 4);

	// A character read from a variable past them still stops the scripts.
	FTestMap Past({Entries({{1, 0}})}, Code({0xFE, 0xC7, 0x04, 0x08, 0x40, 0x00, 0x00}));
	Past.Context.GameState = &Game;
	CHECK(FEventMachine::Update(Past.Context).has_value());
}

TEST_CASE("FE D5 reads where the world map keeps its sea object", "[Field]")
{
	FTestMap Map({Entries({{1, 0}})}, Code({0xFE, 0xD5, 0x40, 0x00, 0x42, 0x00, 0x00}));
	FGameState Game;
	Game.Block()[0x1844] = 0x80;
	Game.Block()[0x1845] = 0x0D;
	Game.Block()[0x1846] = 0x28;
	Game.Block()[0x1847] = 0x07;
	Map.Context.GameState = &Game;
	Map.Tick();
	CHECK(Map.Var(0x40) == 0x0D80);
	CHECK(Map.Var(0x42) == 0x0728);
	CHECK(Game.SeaObjectPlace() == std::array<uint16, 2>{0x0D80, 0x0728});
}

TEST_CASE("FE D6 reads where the world map keeps its air object, and FE C0 the arena's last result", "[Field]")
{
	// FE D6 into 0x40 and 0x42, FE C0 into 0x44, then a count and the end.
	FTestMap Map({Entries({{1, 0}})}, Code({0xFE, 0xD6, 0x40, 0x00, 0x42, 0x00, 0xFE, 0xC0, 0x44, 0x00, 0x3C, 0x00, 0x04, 0x00}));
	FGameState Game;
	// The whole units of its x and z, each after its fraction's word.
	Game.Block()[0x184C] = 0xEE;
	Game.Block()[0x184E] = 0x34;
	Game.Block()[0x184F] = 0x12;
	Game.Block()[0x1850] = 0xEE;
	Game.Block()[0x1852] = 0x78;
	Game.Block()[0x1853] = 0x06;
	Map.Context.GameState = &Game;
	Map.Context.BattlingResult = 0x83;
	Map.Tick();
	CHECK(Map.Var(0x40) == 0x1234);
	CHECK(Map.Var(0x42) == 0x0678);
	CHECK(Map.Var(0x44) == 0x83);
	CHECK(Map.Var(Counter) == 1);
	CHECK(Game.AirObjectPlace() == std::array<uint16, 2>{0x1234, 0x0678});
}

TEST_CASE("FE AC takes HP from the party's gears down to 1, and FE AB gives it back up to the gear's most", "[Field]")
{
	// FE AC: 300 (a constant) from every slot's gear; FE AB: 50 to slot 1's (selector 2); then a count and the end.
	FTestMap Map({Entries({{1, 0}})}, Code({0xFE, 0xAC, 0x2C, 0x01, 0x80, 0xFE, 0xAB, 0x32, 0x00, 0x82, 0x3C, 0x00, 0x04, 0x00}));
	FGameState Game;
	// Fei in gear 0, the character of slot 1 in gear 5, the third member with no gear.
	Game.SetCharacterGear(0, 0);
	Game.SetCharacterGear(4, 5);
	Game.SetCharacterGear(2, FGameState::EmptySlot);
	for (const int32 Gear : {0, 5, 7})
	{
		LittleEndian::WriteU32(Game.Block(), 0x978 + static_cast<std::size_t>(Gear) * 0xA4 + 0x64, 1000);
	}
	Game.SetGearHp(0, 200);
	Game.SetGearHp(5, 980);
	Game.SetGearHp(7, 555);
	Map.Context.GameState = &Game;
	Map.Context.PartyCharacters = {0, 4, 2};
	Map.Tick();
	CHECK(Game.GearHp(0) == 1u);
	CHECK(Game.GearHp(5) == 730u);
	CHECK(Game.GearHp(7) == 555u);
	CHECK(Game.GearMaxHp(5) == 1000u);
	CHECK(Map.Var(Counter) == 1);

	// The amount from a variable; added past the most it stops there, and an amount below minus the HP wraps and fills the gear.
	FTestMap Filled({Entries({{1, 0}})}, Code({0xFE, 0xAB, 0x40, 0x00, 0x02, 0xFE, 0xAB, 0xFE, 0xFF, 0x81, 0x00}));
	Filled.Context.GameState = &Game;
	Filled.Context.PartyCharacters = {0, 4, 0xFF};
	Filled.Context.Variables.Set(0x40, 400);
	Filled.Tick();
	CHECK(Game.GearHp(5) == 1000u);
	CHECK(Game.GearHp(0) == 1000u);

	// A member whose gear the state keeps no record of: the original changes memory after the gears.
	FTestMap Past({Entries({{1, 0}})}, Code({0xFE, 0xAC, 0x01, 0x00, 0x80, 0x00}));
	Game.SetCharacterGear(0, 20);
	Past.Context.GameState = &Game;
	Past.Context.PartyCharacters = {0, 0xFF, 0xFF};
	CHECK(FEventMachine::Update(Past.Context).has_value());
}

TEST_CASE("FE D0 copies a character's record and skills over another's and FE E1 a gear's, the stand-ins' flags set", "[Field]")
{
	// FE D0: character 2 over 9, then 6 over 10; FE E1: gear 1 over 12; then a count and the end.
	FTestMap Map({Entries({{1, 0}})}, Code({0xFE, 0xD0, 0x02, 0x80, 0x09, 0x80, 0xFE, 0xD0, 0x06, 0x80, 0x0A, 0x80,
										   0xFE, 0xE1, 0x01, 0x80, 0x0C, 0x80, 0x3C, 0x00, 0x04, 0x00}));
	FGameState Game;
	using ECharacterPoints = FGameState::ECharacterPoints;
	Game.SetCharacterPoints(2, ECharacterPoints::MaxHp, 777);
	Game.SetCharacterGear(2, 3);
	Game.SetCharacterSpecialSkills(2, 0x8001);
	Game.SetCharacterPoints(6, ECharacterPoints::Ep, 66);
	Game.SetCharacterPoints(9, ECharacterPoints::MaxHp, 5);
	Game.SetGearHp(1, 4321);
	Game.SetGearHp(12, 9);
	Game.SetStoryFlags(0x8000);
	Map.Context.GameState = &Game;
	Map.Tick();
	CHECK(Game.CharacterPoints(9, ECharacterPoints::MaxHp) == 777);
	CHECK(Game.CharacterGear(9) == std::optional<uint8>(3));
	CHECK(Game.CharacterSpecialSkills(9) == std::optional<uint16>(0x8001));
	CHECK(Game.CharacterPoints(10, ECharacterPoints::Ep) == 66);
	CHECK(Game.CharacterPoints(2, ECharacterPoints::MaxHp) == 777);
	CHECK(Game.GearHp(12) == 4321u);
	CHECK(Game.GearHp(1) == 4321u);
	CHECK(Game.StoryFlags() == 0xB000);
	CHECK(Map.Var(Counter) == 1);

	// Another character's copy sets no flag.
	FTestMap Plain({Entries({{1, 0}})}, Code({0xFE, 0xD0, 0x02, 0x80, 0x03, 0x80, 0x00}));
	Game.SetStoryFlags(0);
	Plain.Context.GameState = &Game;
	Plain.Tick();
	CHECK(Game.CharacterPoints(3, ECharacterPoints::MaxHp) == 777);
	CHECK(Game.StoryFlags() == 0);

	// A character or a gear past the state's records: the original copies whatever lies there.
	FTestMap PastCharacter({Entries({{1, 0}})}, Code({0xFE, 0xD0, 0x02, 0x80, 0x0B, 0x80, 0x00}));
	PastCharacter.Context.GameState = &Game;
	CHECK(FEventMachine::Update(PastCharacter.Context).has_value());
	FTestMap PastGear({Entries({{1, 0}})}, Code({0xFE, 0xE1, 0x14, 0x80, 0x00, 0x80, 0x00}));
	PastGear.Context.GameState = &Game;
	CHECK(FEventMachine::Update(PastGear.Context).has_value());
}

TEST_CASE("FE DB restores one character, FE DE gives a character special skills and FE D1 opens the seventh AP tier", "[Field]")
{
	// FE DB: character 4; FE DE: character 4 the skills 0x1000, then those of variable 0x40; FE D1; a count and the end.
	FTestMap Map({Entries({{1, 0}})}, Code({0xFE, 0xDB, 0x04, 0x80, 0xFE, 0xDE, 0x04, 0x80, 0x00, 0x90, 0xFE, 0xDE, 0x04, 0x80, 0x40, 0x00,
										   0xFE, 0xD1, 0x3C, 0x00, 0x04, 0x00}));
	FGameState Game;
	using ECharacterPoints = FGameState::ECharacterPoints;
	Game.SetCharacterPoints(4, ECharacterPoints::Hp, 1);
	Game.SetCharacterPoints(4, ECharacterPoints::MaxHp, 120);
	Game.SetCharacterPoints(4, ECharacterPoints::Ep, 2);
	Game.SetCharacterPoints(4, ECharacterPoints::MaxEp, 30);
	Game.SetCharacterPoints(5, ECharacterPoints::Hp, 3);
	Game.SetCharacterPoints(5, ECharacterPoints::MaxHp, 99);
	Game.SetCharacterSpecialSkills(4, 0x0001);
	Game.SetStoryFlags(0x0002);
	Map.Context.GameState = &Game;
	Map.Context.Variables.Set(0x40, 0x0204);
	Map.Tick();
	CHECK(Game.CharacterPoints(4, ECharacterPoints::Hp) == 120);
	CHECK(Game.CharacterPoints(4, ECharacterPoints::Ep) == 30);
	CHECK(Game.CharacterPoints(5, ECharacterPoints::Hp) == 3);
	CHECK(Game.CharacterSpecialSkills(4) == std::optional<uint16>(0x1205));
	CHECK(Game.CharacterSpecialSkills(5) == std::optional<uint16>(0));
	CHECK(Game.StoryFlags() == 0x4002);
	CHECK(Map.Var(Counter) == 1);

	// A character without a record.
	FTestMap Past({Entries({{1, 0}})}, Code({0xFE, 0xDE, 0x0B, 0x80, 0x01, 0x80, 0x00}));
	Past.Context.GameState = &Game;
	CHECK(FEventMachine::Update(Past.Context).has_value());
}

TEST_CASE("A fade of no ticks snaps to its colour, or in the original divides by zero as the processor does", "[Field]")
{
	// F1: mode 1 toward (0x10, 0, 0xFF) over 4 ticks, then toward black over none; then end.
	FTestMap Map({Entries({{1, 0}})}, Code({0xF1, 0x01, 0x80, 0x10, 0x80, 0x00, 0x80, 0xFF, 0x80, 0x04, 0x80, 0xC3,
										   0xF1, 0x02, 0x80, 0x10, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00}));
	Map.Context.Options = FEventOptions::Original();
	FFieldFade& Fade = Map.Context.Render.Fades[1];
	Fade.Color = {0x2000, 0, 0xFF00};
	Map.Tick();
	CHECK(Fade.Step == std::array<int32, 3>{-0x400, 0, 0});
	CHECK(Fade.Ticks == 4);
	CHECK(Fade.bActive);
	CHECK(Fade.BlendMode == 1);

	Fade.Color = {0x2000, 0x100, 0};
	Map.Tick();
	CHECK(Fade.Step == std::array<int32, 3>{1, 1, -1});
	CHECK(Fade.Ticks == 0);
	CHECK(Fade.BlendMode == 2);

	// The fix: the same 0-tick fade reaches its colour on the next tick's step.
	FTestMap Snapped({Entries({{1, 0}})}, Code({0xF1, 0x02, 0x80, 0x10, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00}));
	FFieldFade& Snap = Snapped.Context.Render.Fades[1];
	Snap.Color = {0x2000, 0x100, 0};
	Snapped.Tick();
	CHECK(Snap.Step == std::array<int32, 3>{-0x1000, -0x100, 0});
	CHECK(Snap.Ticks == 1);
}

TEST_CASE("The screen fades out and back in over its ticks and over one for none unless the original is asked for", "[Field]")
{
	// B4 over 4 ticks; B3 over none; B4 over none; then end, a tick each.
	const std::vector<uint8> Fades = {0xB4, 0x04, 0x80, 0xC3, 0xB3, 0x00, 0x80, 0xC3, 0xB4, 0x00, 0x80, 0x00};
	FTestMap Map({Entries({{1, 0}})}, Fades);
	Map.Context.Options = FEventOptions::Original();
	FFieldRenderState& Render = Map.Context.Render;
	Render.bScreenFadedOut = false;
	Map.Tick();
	CHECK(Render.bScreenFadedOut);
	CHECK(Render.Fades[0].Color == std::array<int32, 3>{0, 0, 0});
	CHECK(Render.Fades[0].Step == std::array<int32, 3>{0x3FC0, 0x3FC0, 0x3FC0});
	CHECK(Render.Fades[0].Ticks == 4);
	CHECK(Render.Fades[0].bActive);

	// With no ticks the original divides by zero as the processor does, and the layer never steps.
	Map.Tick();
	CHECK_FALSE(Render.bScreenFadedOut);
	CHECK(Render.Fades[0].Color == std::array<int32, 3>{0xFF00, 0xFF00, 0xFF00});
	CHECK(Render.Fades[0].Step == std::array<int32, 3>{1, 1, 1});
	CHECK(Render.Fades[0].Ticks == 0);
	Map.Tick();
	CHECK(Render.bScreenFadedOut);
	CHECK(Render.Fades[0].Color == std::array<int32, 3>{0, 0, 0});
	CHECK(Render.Fades[0].Step == std::array<int32, 3>{-1, -1, -1});
	CHECK(Render.Fades[0].Ticks == 0);

	// The fix: a fade of no ticks takes one, so the screen reaches its colour on the next tick's step.
	FTestMap Fixed({Entries({{1, 0}})}, Fades);
	FFieldRenderState& FixedRender = Fixed.Context.Render;
	FixedRender.bScreenFadedOut = false;
	Fixed.Tick();
	Fixed.Tick();
	CHECK(FixedRender.Fades[0].Step == std::array<int32, 3>{-0x10000, -0x10000, -0x10000});
	CHECK(FixedRender.Fades[0].Ticks == 1);
	Fixed.Tick();
	CHECK(FixedRender.Fades[0].Step == std::array<int32, 3>{0xFF00, 0xFF00, 0xFF00});
	CHECK(FixedRender.Fades[0].Ticks == 1);
}

TEST_CASE("80 writes one byte lane of a walk attribute, spilling a wider value into the lanes above", "[Field]")
{
	// 80: word 1, lane 1, 0x1FF; word 1, lane 7 (nothing), 5; word 2 (past the table), lane 0, 1.
	FTestMap Map({Entries({{1, 0}})}, Code({0x80, 0x01, 0x01, 0xFF, 0x81, 0x80, 0x01, 0x07, 0x05, 0x80, 0x80, 0x02, 0x00, 0x01, 0x80, 0x00}));
	FFieldWalkmesh Walkmesh;
	Walkmesh.Attributes = {0, 0x11223344};
	Map.Context.Walkmesh = &Walkmesh;
	const auto Error = FEventMachine::Update(Map.Context);
	CHECK(Walkmesh.Attributes[1] == 0x1123FF44);
	REQUIRE(Error);
	CHECK(Error->Message == "field map 7, actor 0, pc 000A, opcode 80: index 2 is past the table the instruction reads");
}

TEST_CASE("A gear light past the three lands in the next rows, as map 335's does, and past the matrices stops", "[Field]")
{
	// FE 3E: light 3's colour (1, 2, 3); FE 3D: light 10's direction (past the two matrices); constants.
	FTestMap Map({Entries({{1, 0}})}, Code({0xFE, 0x3E, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0xF0,
										   0xFE, 0x3D, 0x0A, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0xF0, 0x00}));
	const auto Error = FEventMachine::Update(Map.Context);
	const FFieldGearLights& Lights = Map.Context.Render.GearLights;
	// Light 0's green and blue, then the colour matrix's pad.
	CHECK(Lights.Words[FFieldGearLights::ColorWord(0, 1)] == 1);
	CHECK(Lights.Words[FFieldGearLights::ColorWord(0, 2)] == 2);
	CHECK(Lights.Words[FFieldGearLights::MatrixWords + 9] == 3);
	CHECK(Lights.Words[FFieldGearLights::ColorWord(0, 0)] == 0x800);
	REQUIRE(Error);
	CHECK(Error->Message == "field map 7, actor 0, pc 000B, opcode FE 3D: index 10 is past the table the instruction reads");
}

TEST_CASE("12 waits whole while a movie is asked for, or as in the original takes its transition from its own first bytes", "[Field]")
{
	// 12 map 0x85, entrance 0x181, transition 3, 16 ticks, all constants.
	const std::vector<uint8> Bytes = {0x12, 0x85, 0x80, 0x81, 0x81, 0x03, 0x80, 0x10, 0x80, 0x00};
	FTestMap Fixed({Entries({{1, 0}})}, Bytes);
	Fixed.Context.Requests.bMovie = true;
	Fixed.Tick();
	CHECK(Fixed.State(0).Pc == 0);
	CHECK(Fixed.Context.Requests.MapTransition == 2);
	CHECK_FALSE(Fixed.Context.Requests.bMapChange);

	FTestMap Original({Entries({{1, 0}})}, Bytes);
	Original.Context.Options = FEventOptions::Original();
	Original.Context.Requests.bMovie = true;
	Original.Tick();
	// The words at the pc and 2 past it: 12 with the map's low byte, the map's high byte with the entrance's low byte.
	CHECK(Original.State(0).Pc == 4);
	CHECK(Original.Context.Requests.MapTransition == 0x0512);
	CHECK(Original.Context.Requests.MapTransitionTicks == 0x0180);
	CHECK_FALSE(Original.Context.Requests.bMapChange);

	// Without a movie both run as 12 does.
	FTestMap Plain({Entries({{1, 0}})}, Bytes);
	Plain.Tick();
	CHECK(Plain.State(0).Pc == 9);
	CHECK(Plain.Context.Requests.bMapChange);
	CHECK(Plain.Context.MapNumber == 0x85);
	CHECK(Plain.Context.Requests.MapTransition == 3);
	CHECK(Plain.Context.Requests.MapTransitionTicks == 0x10);
}

TEST_CASE("FE BA sets where the world map keeps the party and FE B9 reads it back", "[Field]")
{
	// FE BA x 1234h, y from variable 400h, z 567h, direction 3 (flags B0h: all but y constants), then FE B9
	// into variables 402h-408h.
	FTestMap Map({Entries({{1, 0}})}, Code({0xFE, 0xBA, 0x34, 0x12, 0x00, 0x04, 0x67, 0x05, 0x03, 0x00, 0xB0, 0xFE, 0xB9, 0x02, 0x04, 0x04, 0x04,
											 0x06, 0x04, 0x08, 0x04, 0x00}));
	FGameState Game;
	Map.Context.GameState = &Game;
	Map.Context.Variables.Set(0x400, 0x77);
	Map.Tick();
	CHECK(Game.WorldMapPosition() == std::array<uint16, 4>{0x1234, 0x77, 0x567, 3});
	CHECK(Map.Var(0x402) == 0x1234);
	CHECK(Map.Var(0x404) == 0x77);
	CHECK(Map.Var(0x406) == 0x567);
	CHECK(Map.Var(0x408) == 3);
}

TEST_CASE("FE AF places its point by the identity where no gear's joint is there", "[Field]")
{
	// FE AF slot 0, joint 6, point (100, -200, 300), all constants (flags F8h), into variables 400h-404h, on a
	// map that loaded no gears: the original's matrix is the origin with a stale rotation.
	FTestMap Map({Entries({{1, 0}})}, Code({0xFE, 0xAF, 0x00, 0x00, 0x06, 0x00, 0x64, 0x00, 0x38, 0xFF, 0x2C, 0x01, 0xF8, 0x00, 0x04, 0x02, 0x04,
											 0x04, 0x04, 0x00}));
	Map.Tick();
	CHECK(Map.Var(0x400) == 100);
	CHECK(Map.Var(0x402) == -200);
	CHECK(Map.Var(0x404) == 300);
}

TEST_CASE("FE A0 running past the bytecode's end ends its script and asks for no movie", "[Field]")
{
	// Map 489's actor 1: FE A0 laid out for an older, shorter form, its flags past the code.
	FTestMap Map({Entries({{1, 0}})}, Code({0xFE, 0xA0, 0x00, 0xFE, 0x61, 0xFE, 0xA0, 0x01, 0x5B, 0x00, 0x00}));
	Map.Tick();
	CHECK_FALSE(Map.Context.Requests.bMovie);
	CHECK(Map.State(0).Running().Script == FEventThread::NoScript);
}

TEST_CASE("B5 in a map's init turns the camera whole, or in the original leaves its turning yaw behind", "[Field]")
{
	// The init turns the camera from direction 0 to direction 2 at once.
	const auto InitTurn = [](const FEventOptions& Options)
	{
		FTestMap Map({Entries({{0, 0}})}, Code({0xB5, 0x02, 0x80, 0x00, 0x80, 0x00}));
		Map.Context.bInitDone = false;
		Map.Context.Options = Options;
		Map.Context.Camera.Rotation[1] = 0x800;
		Map.Context.Camera.TurningYaw = 0x8000000;
		REQUIRE_FALSE(FEventMachine::InitMap(Map.Context));
		CHECK(Map.Context.Camera.Rotation[1] == 0xC00);
		CHECK(Map.Context.Camera.YawGoal == 0xC00);
		return Map.Context.Camera.TurningYaw;
	};
	CHECK(InitTurn(FEventOptions()) == 0xC000000);
	CHECK(InitTurn(FEventOptions::Original()) == 0x8000000);
}

TEST_CASE("F2 ramps the shake's depth from its own amplitude, or in the original from the height's", "[Field]")
{
	// A shake of height 40 and depth 2 stopped over 4 ticks.
	const auto DepthGrowth = [](const FEventOptions& Options)
	{
		FTestMap Map({Entries({{1, 0}})}, Code({0xF2, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x04, 0x80, 0xC3}));
		Map.Context.Options = Options;
		Map.Context.Camera.ShakeAmplitude = {0, 40 << 16, 2 << 16};
		Map.Tick();
		CHECK(Map.Context.Camera.bShakeStops);
		CHECK(Map.Context.Camera.ShakeGrowth[1] == -(10 << 16));
		return Map.Context.Camera.ShakeGrowth[2];
	};
	CHECK(DepthGrowth(FEventOptions()) == -0x8000);
	CHECK(DepthGrowth(FEventOptions::Original()) == -(10 << 16));
}

TEST_CASE("A script fix changes the bytecode only where its bytes are the original's", "[Field]")
{
	FTestMap Map({Entries({{1, 0}})}, Code({0x02, 0x40, 0x00, 0xFF, 0x00, 0x40, 0x10, 0x00}));
	const std::array<uint8, 1> Was{0x40};
	const std::array<uint8, 1> Now{0x42};
	CHECK_FALSE(Map.Scripts->Replace(2, Was, Now));
	CHECK(Map.Scripts->Code()[2] == 0x00);
	CHECK_FALSE(Map.Scripts->Replace(Map.Scripts->CodeSize(), Was, Now));
	CHECK(Map.Scripts->Replace(1, Was, Now));
	CHECK(Map.Scripts->Code()[1] == 0x42);
}

TEST_CASE("Under Always run the scripts read the run button as the leader's run while the player has the party", "[Field]")
{
	// Script 1: if the run button (40h) is held, Counter += 1; end. It runs once a tick.
	const auto HeldTicks = [](bool bAlwaysRun, bool bPlayerHasParty, uint16 Held)
	{
		FTestMap Map({Entries({{1, 0}})}, Code({0x31, 0x40, 0x00, 0x08, 0x00, 0x3C, 0x00, 0x04, 0x00}));
		Map.Context.bAlwaysRun = bAlwaysRun;
		Map.Context.PadHeld = Held;
		Map.Context.Controls.bRunLetThrough = true;
		for (int32 Tick = 0; Tick < 4; ++Tick)
		{
			// The leader's control step of the tick before, as the scripts after it in a tick see it.
			Map.Context.bPlayerControlled = bPlayerHasParty;
			Map.Tick();
		}
		return Map.Var(Counter);
	};
	constexpr uint16 Run = 0x40;
	// The pad's own button as the original reads it.
	CHECK(HeldTicks(false, true, 0) == 0);
	CHECK(HeldTicks(false, true, Run) == 4);
	// Always run while the player has the party: running unless the button is held (Bart's fast swim in Aveh's waterway).
	CHECK(HeldTicks(true, true, 0) == 4);
	CHECK(HeldTicks(true, true, Run) == 0);
	// While the scripts hold the party the button is the pad's (a console's cancel, the card game's).
	CHECK(HeldTicks(true, false, 0) == 0);
	CHECK(HeldTicks(true, false, Run) == 4);
}

TEST_CASE("Under Always run a run button the field's masks keep out reads as not held", "[Field]")
{
	// The map's first ticks and the scripts' masks let no button through: nor the run Always run stands for.
	FTestMap Map({Entries({{1, 0}})}, Code({0x31, 0x40, 0x00, 0x08, 0x00, 0x3C, 0x00, 0x04, 0x00}));
	Map.Context.bAlwaysRun = true;
	Map.Context.SettleTicks = 1;
	ReadFieldInput(Map.Context, {});
	Map.Context.bPlayerControlled = true;
	Map.Tick();
	CHECK(Map.Var(Counter) == 0);
	Map.Context.SettleTicks = 0;
	ReadFieldInput(Map.Context, {});
	Map.Context.bPlayerControlled = true;
	Map.Tick();
	CHECK(Map.Var(Counter) == 1);
}
