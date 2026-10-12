#include "Field/EventAssembly.h"
#include "Field/EventInstructionSet.h"

#include <catch_amalgamated.hpp>

#include <set>
#include <string>

using namespace Weltall;

namespace
{
	/** The bytecode Text assembles to from offset 0, or the error's words in a failed check. */
	std::vector<uint8> Code(std::string_view Text)
	{
		const auto Assembly = AssembleEventText(Text);
		if (!Assembly)
		{
			FAIL(Assembly.Error().Message);
		}
		return Assembly->Code;
	}

	std::string Refused(std::string_view Text, const FEventAssemblyContext& Context = {})
	{
		const auto Assembly = AssembleEventText(Text, Context);
		return Assembly ? std::string("assembled") : Assembly.Error().Message;
	}

	using FBytes = std::vector<uint8>;
}

TEST_CASE("The event instruction set names every instruction once", "[Field]")
{
	std::set<std::string_view> Names;
	int32 Count = 0;
	for (uint32 Opcode = 0; Opcode <= 0xFEFF; ++Opcode)
	{
		const FEventInstructionShape* Shape = FindEventInstruction(static_cast<uint16>(Opcode));
		if (Shape == nullptr)
		{
			continue;
		}
		++Count;
		CHECK(Shape->Opcode == Opcode);
		CHECK(Names.insert(Shape->Name).second);
		CHECK(FindEventInstruction(Shape->Name) == Shape);
	}
	// 255 of one byte (FE begins the others) and 227 of two.
	CHECK(Count == 482);
	CHECK(FindEventInstruction(uint16{0xFE}) == nullptr);
	CHECK(FindEventInstruction(uint16{0xFEE3}) == nullptr);
	CHECK(FindEventInstruction(uint16{0x1234}) == nullptr);
	CHECK(FindEventInstruction("NoSuchInstruction") == nullptr);
	CHECK(FindEventInstruction(EventOpcode::If)->Operands.Bytes == 7);
	CHECK(FindEventInstruction(EventOpcode::If)->Operands.FlagBits == 0xC0);
	CHECK(FindEventInstruction(EventOpcode::Jump)->bEndsThread);
	CHECK_FALSE(FindEventInstruction(EventOpcode::If)->bEndsThread);
}

TEST_CASE("An instruction whose first operand selects the rest reads both ways", "[Field]")
{
	// A move's set-up takes its three coordinates and their flags; its run a byte alone.
	const FBytes Moves = {0x10, 0x00, 0x0A, 0x00, 0x14, 0x00, 0x1E, 0x00, 0xE0, 0x10, 0x01, 0x00};
	const auto Setup = ReadEventInstruction(Moves, 0);
	REQUIRE(Setup);
	CHECK(Setup->Length == 9);
	CHECK(Setup->Flags() == 0xE0);
	const auto Run = ReadEventInstruction(Moves, 9);
	REQUIRE(Run);
	CHECK(Run->Length == 2);
	CHECK(Run->Operands->Count == 1);
	// No instruction where the opcode is none, the prefix ends the code or the operands run past it.
	CHECK_FALSE(ReadEventInstruction(FBytes{0xFE, 0xE3}, 0));
	CHECK_FALSE(ReadEventInstruction(FBytes{0xFE}, 0));
	CHECK_FALSE(ReadEventInstruction(FBytes{0x01, 0x00}, 0));
	CHECK_FALSE(ReadEventInstruction(FBytes{0x00}, 1));
}

TEST_CASE("Event instructions assemble to their opcodes and operands", "[Field]")
{
	CHECK(Code("End") == FBytes{0x00});
	CHECK(Code("Wait #30") == FBytes{0x26, 0x1E, 0x80});
	CHECK(Code("Wait var[01E]") == FBytes{0x26, 0x1E, 0x00});
	CHECK(Code("Message msg4 0") == FBytes{0xD2, 0x04, 0x00, 0x00});
	CHECK(Code("StartScript actor3 script2@3") == FBytes{0x07, 0x03, 0x62});
	CHECK(Code("FaceActor party0\nFaceActor party2\nFaceActor self\nFaceActor actor12") == FBytes{0x6F, 0xFF, 0x6F, 0xFD, 0x6F, 0xFB, 0x6F, 0x0C});
	CHECK(Code("PartyAdd 4") == FBytes{0xFE, 0x18, 0x04});
	CHECK(Code("SetPosition3D -1 2 -3") == FBytes{0x1D, 0xFF, 0xFF, 0x02, 0x00, 0xFD, 0xFF});
	CHECK(Code("SetPadMask 0x8040") == FBytes{0xD5, 0x40, 0x80});
	CHECK(Code("IfMoney 100000 @0x0010") == FBytes{0x8E, 0xA0, 0x86, 0x01, 0x00, 0x10, 0x00});
	CHECK(Code("  data 01 9B 05   ; bytes as they are") == FBytes{0x01, 0x9B, 0x05});
}

TEST_CASE("Operands by a flag set their bits of the flags byte", "[Field]")
{
	CHECK(Code("SetVar var[408] var[014]") == FBytes{0x35, 0x08, 0x04, 0x14, 0x00, 0x00});
	CHECK(Code("SetVar var[408] #-2") == FBytes{0x35, 0x08, 0x04, 0xFE, 0xFF, 0x40});
	// The flags byte lies before the last operand it answers for.
	CHECK(Code("GlideTo #1 var[010] #-3") == FBytes{0x4C, 0x01, 0x00, 0x10, 0x00, 0xA0, 0xFD, 0xFF});
	// The bits no operand answers to are given as they are.
	CHECK(Code("SetVar var[000] #1 flags=0x01") == FBytes{0x35, 0x00, 0x00, 0x01, 0x00, 0x41});
	// Two heights of a distance answer to one bit, and two depths to another.
	CHECK(Code("Distance3D var[400] #1 #2 #3 var[010] var[012] var[014]")
		== FBytes{0xFE, 0x76, 0x00, 0x04, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x10, 0x00, 0x12, 0x00, 0x14, 0x00, 0x60});
	CHECK(Refused("Distance3D var[400] #1 #2 var[010] var[010] var[012] var[014]") == "line 1: these two operands are constants or variables together");
	CHECK(Refused("SetVar var[000] #1 flags=0x40") == "line 1: flags= gives the flags byte's bits the operands do not");
	CHECK(Refused("Wait #1 flags=0x01") == "line 1: the instruction has no flags byte");
}

TEST_CASE("If assembles as the sentence it reads as", "[Field]")
{
	CHECK(Code("If var[014] == #1 else @0x0329") == FBytes{0x02, 0x14, 0x00, 0x01, 0x00, 0x40, 0x29, 0x03});
	CHECK(Code("If var[2CE] & #-32768 else @0x00C4") == FBytes{0x02, 0xCE, 0x02, 0x00, 0x80, 0x46, 0xC4, 0x00});
	CHECK(Code("If #5 >= var[010] else @0") == FBytes{0x02, 0x05, 0x00, 0x10, 0x00, 0x84, 0x00, 0x00});
	CHECK(Code("If var[000] ~& var[002] else @0 flags=0x10") == FBytes{0x02, 0x00, 0x00, 0x02, 0x00, 0x1A, 0x00, 0x00});
	for (int32 Test = 0; Test < 16; ++Test)
	{
		CHECK(EventTestOfName(EventTestName(Test)) == Test);
	}
	CHECK(Refused("If var[014] = #1 else @0") == "line 1: If reads 'If a test b else label'");
	CHECK(Refused("If var[014] == #1 @0") == "line 1: If reads 'If a test b else label'");
}

TEST_CASE("An instruction's first operand selects the rest in text too", "[Field]")
{
	CHECK(Code("MoveTo 0 #10 #20 #30\nMoveTo 1") == FBytes{0x10, 0x00, 0x0A, 0x00, 0x14, 0x00, 0x1E, 0x00, 0xE0, 0x10, 0x01});
	CHECK(Code("ParticlePreset 1 #1 #2 #3\nParticlePreset 2") == FBytes{0x73, 0x01, 0x01, 0x80, 0x02, 0x80, 0x03, 0x80, 0x73, 0x02});
	CHECK(Code("LoadSoundBank 1\nLoadSoundBank 0 #3 #4") == FBytes{0xFE, 0xB0, 0x01, 0xFE, 0xB0, 0x00, 0x03, 0x80, 0x04, 0x80});
	CHECK(Refused("MoveTo #10") == "line 1: the instruction's first operand is a byte that selects the rest");
	CHECK(Refused("ParticlePreset 2 #1") == "line 1: the instruction takes fewer operands");
}

TEST_CASE("Labels and definitions name places and operands", "[Field]")
{
	const std::string_view Text =
		"define Progress var[000]\n"
		"define Fei actor4\n"
		"Again:\n"
		"    If Progress >= #16 else Later\n"
		"    FaceActor Fei\n"
		"    Jump Again\n"
		"Later:\n"
		"    End\n";
	CHECK(Code(Text) == FBytes{0x02, 0x00, 0x00, 0x10, 0x00, 0x44, 0x0D, 0x00, 0x6F, 0x04, 0x01, 0x00, 0x00, 0x00});

	// Code that lies after other bytecode: its labels count from where it lies, and it may be given names.
	const std::vector<std::pair<std::string, std::string>> Given = {{"greeting", "msg52"}, {"Back", "@0x00BC"}};
	FEventAssemblyContext Context;
	Context.Origin = 0x0700;
	Context.Definitions = Given;
	const auto Added = AssembleEventText("Start:\n Message greeting 0\n Jump Next\nNext:\n Jump Back\n", Context);
	REQUIRE(Added);
	CHECK(Added->Code == FBytes{0xD2, 0x34, 0x00, 0x00, 0x01, 0x07, 0x07, 0x01, 0xBC, 0x00});
	CHECK(Added->Label("Start") == 0x0700);
	CHECK(Added->Label("Next") == 0x0707);
	CHECK_FALSE(Added->Label("Back"));
}

TEST_CASE("A text of event scripts assembles to a map's script section", "[Field]")
{
	const std::string_view Text =
		"actors 2\n"
		"unsigned 014 0F0-0F4 7FE\n"
		"entry 1 31 @0x0003\n"
		"entrances\n"
		"entrance x 12 z -30 layer 1 camera 2 facing keep\n"
		"actor 0 script 0:\n"
		"actor 1 script 0:\n"
		"    Jump Loop\n"
		"actor 0 script 1:\n"
		"Loop:\n"
		"    Yield\n"
		"    Jump Loop\n"
		"actor 1 script 4:\n";
	const auto Section = AssembleEventScripts(Text);
	REQUIRE(Section);
	const auto Scripts = FEventScripts::Parse(*Section);
	REQUIRE(Scripts);
	CHECK(Scripts->ActorCount() == 2);
	CHECK(Scripts->EntryPoint(0, 0) == 8);
	CHECK(Scripts->EntryPoint(1, 0) == 8);
	CHECK(Scripts->EntryPoint(0, 1) == 11);
	CHECK(Scripts->EntryPoint(1, 31) == 3);
	CHECK(Scripts->EntryPoint(1, 4) == 15);
	CHECK(Scripts->EntryPoint(1, 1) == 0);
	for (const uint32 Address : {0x014u, 0x0F0u, 0x0F2u, 0x0F4u, 0x7FEu})
	{
		CHECK(Scripts->IsVariableUnsigned(Address));
	}
	CHECK_FALSE(Scripts->IsVariableUnsigned(0x016));
	CHECK_FALSE(Scripts->IsVariableUnsigned(0x0F6));
	const auto Entrance = Scripts->Entrance(0);
	REQUIRE(Entrance);
	CHECK(Entrance->X == 12);
	CHECK(Entrance->Z == -30);
	CHECK(Entrance->Layer == 1);
	CHECK(Entrance->CameraDirection == 2);
	CHECK(Entrance->Facing == FEventEntrance::KeepDirection);
	CHECK(FBytes(Scripts->Code() + 8, Scripts->Code() + Scripts->CodeSize()) == FBytes{0x01, 0x0B, 0x00, 0xC3, 0x01, 0x0B, 0x00});
	CHECK(Refused("actors 1\nactor 1 script 0:\n End\n") == "assembled");
	CHECK_FALSE(AssembleEventScripts("actors 1\nactor 1 script 0:\n End\n"));
	CHECK_FALSE(AssembleEventScripts("End\n"));
}

TEST_CASE("Scripts written out as text assemble back to the same section", "[Field]")
{
	// Three actors' scripts over an entrance table: a thread that jumps over bytes no thread reaches (an instruction,
	// then two bytes that are none), a jump into an instruction's operands, a jump table, a jump to the bytecode's end,
	// an entry point inside an instruction and one past the end.
	const std::string_view Text =
		"actors 3\n"
		"unsigned 000 400-40E\n"
		"entry 2 5 @0x0011\n"
		"entry 2 6 @0x1000\n"
		"entrances\n"
		"entrance x -200 z 300 layer 0 camera keep facing 6\n"
		"entrance x 0 z 0 layer 3 camera 0 facing 0\n"
		"actor 0 script 0:\n"
		"    Jump Past\n"
		"    Nop13\n"
		"    data FE F0\n"
		"Past:\n"
		"    If var[400] != #7 else @0x0010 flags=0x20\n"
		"    JumpTable var[402]\n"
		"    Jump First\n"
		"    Jump Second\n"
		"First:\n"
		"    MoveTo 0 #1 var[010] #-1\n"
		"    MoveTo 3\n"
		"    Return\n"
		"Second:\n"
		"actor 1 script 2:\n"
		"actor 2 script 3:\n"
		"    MessageAtActor party1 msg300 5\n"
		"    StartScriptWaitEnd self script31@7\n"
		"    Jump Finish\n"
		"Finish:\n";
	const auto Section = AssembleEventScripts(Text);
	REQUIRE(Section);
	const auto Scripts = FEventScripts::Parse(*Section);
	REQUIRE(Scripts);
	const FEventThreads Threads = FollowEventThreads(*Scripts);
	// What the threads reach: the first jump's two operand bytes as instructions of their own (If's way out and an
	// entry point land there), not the dead instruction and bytes after it.
	CHECK(Threads.Reached == std::vector<uint16>{15, 16, 17, 21, 29, 32, 35, 38, 47, 49, 50, 55, 58});
	CHECK(Threads.Problems == std::vector<std::string>{
		"0010: reached inside the instruction at 000F",
		"0011: reached inside the instruction at 000F",
		"003D: a thread leaves the bytecode",
		"1000: a thread leaves the bytecode",
	});

	const std::string Listing = DisassembleEventScripts(*Scripts);
	INFO(Listing);
	const auto Again = AssembleEventScripts(Listing);
	REQUIRE(Again);
	CHECK(*Again == *Section);
	CHECK(Listing.find("Nop13") != std::string::npos);
	CHECK(Listing.find("unreached") != std::string::npos);
	CHECK(Listing.find("data FE F0") != std::string::npos);
	CHECK(Listing.find("actor 1 script 2:\nactor 2 script 3:\n") != std::string::npos);
	CHECK(Listing.find("entry 2 6 @0x1000") != std::string::npos);
}

TEST_CASE("Event script text that cannot be assembled is refused with its line", "[Field]")
{
	CHECK(Refused("End\nFly #1\n") == "line 2: no instruction is named 'Fly'");
	CHECK(Refused("Wait\n") == "line 1: the instruction takes more operands");
	CHECK(Refused("Wait #1 #2\n") == "line 1: the instruction takes fewer operands");
	CHECK(Refused("Wait #40000\n") == "line 1: not what the instruction takes there: '#40000'");
	CHECK(Refused("Wait var[8000]\n") == "line 1: not what the instruction takes there: 'var[8000]'");
	CHECK(Refused("FaceActor actor256\n") == "line 1: not what the instruction takes there: 'actor256'");
	CHECK(Refused("StartScript self script32@0\n") == "line 1: not what the instruction takes there: 'script32@0'");
	CHECK(Refused("Jump Nowhere\n") == "line 1: no label is named 'Nowhere'");
	CHECK(Refused("Here:\nHere:\n") == "line 2: a name defined twice: 'Here'");
	CHECK(Refused("define Here #1\nHere:\n") == "line 2: a name defined twice: 'Here'");
	CHECK(Refused("define 9Lives #9\n") == "line 1: a definition reads 'define Name operand'");
	CHECK(Refused("End\nentrances\n") == "line 2: 'entrances' stands alone, where the bytecode begins");
	CHECK(Refused("entrance x 1 z 2 layer 3 camera 4\n") == "line 1: an entrance reads 'entrance x N z N layer N camera N facing N'");
	CHECK(Refused("actors 2\nactors 2\n") == "line 2: 'actors' takes the actor count, once");
	CHECK(Refused("unsigned 015\n") == "line 1: not a variable's address or a range of them: '015'");
	CHECK(Refused("unsigned 800\n") == "line 1: not a variable's address or a range of them: '800'");
	CHECK(Refused("data 1\n") == "line 1: data is bytes of two hexadecimal digits, not '1'");
	CHECK(Refused("actor 0 script 0:\nactor 0 script 0:\n") == "line 2: a script given twice");
	CHECK(Refused("actor 0 script 32:\n") == "line 1: a script's line reads 'actor N script N:'");
	CHECK(Refused("entry 0 0 Nowhere\n") == "line 1: no label is named 'Nowhere'");
	FEventAssemblyContext Late;
	Late.Origin = 0xFFFE;
	CHECK(Refused("Jump @0\n", Late) == "line 1: the bytecode outgrows what a 16-bit offset reaches");
	CHECK(Refused("End\nEnd\n", Late) == "assembled");
}
