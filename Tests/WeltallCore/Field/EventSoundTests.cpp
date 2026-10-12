#include "Field/EventMachine.h"
#include "Game/PsxRandom.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace Weltall;
using Weltall::Testing::FByteWriter;

namespace
{
	constexpr uint16 Counter = 0x400;

	/** What the scripts asked of the sound driver and of the banks, in order. */
	class FRecordingSound final : public IFieldSoundEffects, public IFieldSoundBanks
	{
	public:
		void StopEffectOn(int32 Channel) override { Calls.push_back("stop " + std::to_string(Channel)); }
		void PlayEffectOn(uint32 Effect, int32 Channel, int32 Volume, int32 Pan) override
		{
			Calls.push_back("play " + std::to_string(Effect) + " " + std::to_string(Channel) + " " + std::to_string(Volume) + " " + std::to_string(Pan));
		}
		void SetEffectVolumeOn(int32 Channel, int32 Volume) override { Calls.push_back("volume " + std::to_string(Channel) + " " + std::to_string(Volume)); }
		void SetEffectPanOn(int32 Channel, int32 Pan) override { Calls.push_back("pan " + std::to_string(Channel) + " " + std::to_string(Pan)); }
		void FadeEffectVolumeOn(int32 Channel, int32 Volume, int32 Ticks) override
		{
			Calls.push_back("fade " + std::to_string(Channel) + " " + std::to_string(Volume) + " " + std::to_string(Ticks));
		}
		uint32 PlayingEffects() override { return Playing; }

		bool BeginBankLoad(int32 Directory, int32 File) override
		{
			Calls.push_back("read " + std::to_string(Directory) + "/" + std::to_string(File));
			return true;
		}
		bool FinishBankLoad() override
		{
			Calls.push_back("upload");
			return true;
		}

		std::vector<std::string> Calls;
		uint32 Playing = 0;
	};

	/** One script actor whose default script (script 1) starts at the bytecode's start. */
	class FSoundMap
	{
	public:
		explicit FSoundMap(std::initializer_list<uint8> Code)
		{
			FByteWriter Writer;
			for (int32 Word = 0; Word < 32; ++Word)
			{
				Writer.U32(0);
			}
			Writer.U32(1);
			for (int32 Entry = 0; Entry < 32; ++Entry)
			{
				Writer.U16(0);
			}
			Writer.Bytes.insert(Writer.Bytes.end(), Code);
			auto Parsed = FEventScripts::Parse(Writer.Bytes);
			REQUIRE(Parsed);
			Scripts = std::make_unique<FEventScripts>(std::move(*Parsed));
			Context.Scripts = Scripts.get();
			Context.MapNumber = 7;
			Context.RunningMap = 7;
			Context.Random = &Random;
			Context.SoundEffects = &Sound;
			Context.SoundBanks = &Sound;
			FFieldActor Record;
			Record.Flags = 0x200;
			Context.Actors.push_back(Record);
			Context.States.emplace_back().Reset(Random.Next());
			Context.bInitDone = true;
		}

		int32 Var(uint16 Address) { return Context.Variables.Get(Address, *Scripts); }

		void Tick()
		{
			if (const auto Error = FEventMachine::Update(Context))
			{
				FAIL(Error->Message);
			}
		}

		std::string Fault()
		{
			const auto Error = FEventMachine::Update(Context);
			return Error ? Error->Message : std::string{};
		}

		std::unique_ptr<FEventScripts> Scripts;
		FPsxRandom Random;
		FRecordingSound Sound;
		FEventContext Context;
	};

	using FCalls = std::vector<std::string>;
}

TEST_CASE("FE B0 reads a bank as it starts and uploads it as it finishes, which ends the turn", "[Field]")
{
	// A bank of the music's directory, then one of the battles' voices (bit 7); Counter += 1 and wait after each pair.
	FSoundMap Map({0xFE, 0xB0, 0x00, 0x03, 0x80, 0x06, 0x80, 0xFE, 0xB0, 0x01, 0x3C, 0x00, 0x04, 0xC3,
		0xFE, 0xB0, 0x00, 0x03, 0x80, 0x81, 0x80, 0xFE, 0xB0, 0x01, 0x3C, 0x00, 0x04, 0xC3, 0x01, 0x1B, 0x00});
	Map.Tick();
	CHECK(Map.Sound.Calls == FCalls{"read 28/8", "upload"});
	CHECK(Map.Var(Counter) == 0);
	Map.Tick();
	CHECK(Map.Var(Counter) == 1);
	Map.Tick();
	CHECK(Map.Sound.Calls == FCalls{"read 28/8", "upload", "read 45/32", "upload"});
	CHECK(Map.Var(Counter) == 1);
	Map.Tick();
	CHECK(Map.Var(Counter) == 2);

	// Without the game's sound the instructions take the same turns.
	FSoundMap Silent({0xFE, 0xB0, 0x00, 0x03, 0x80, 0x06, 0x80, 0xFE, 0xB0, 0x01, 0x3C, 0x00, 0x04, 0xC3, 0x01, 0x0D, 0x00});
	Silent.Context.SoundBanks = nullptr;
	Silent.Tick();
	CHECK(Silent.Var(Counter) == 0);
	Silent.Tick();
	CHECK(Silent.Var(Counter) == 1);
}

TEST_CASE("FE B0 swaps only the gear battles' bank", "[Field]")
{
	FSoundMap Map({0xFE, 0xB0, 0x00, 0x01, 0x80, 0x03, 0x80});
	CHECK(Map.Fault()
		== "field map 7, actor 0, pc 0000, opcode FE B0: sound bank slot 1 is not the gear battles' bank's, where the original unloads no bank or one behind the music's back");
	CHECK(Map.Sound.Calls.empty());

	// Carried past, it reads no bank and the script goes on.
	FSoundMap Recovered({0xFE, 0xB0, 0x00, 0x01, 0x80, 0x03, 0x80, 0x3C, 0x00, 0x04, 0xC3, 0x01, 0x07, 0x00});
	FUnreachedCases Cases;
	Recovered.Context.Unreached = &Cases;
	CHECK(Recovered.Fault().empty());
	CHECK(Recovered.Var(Counter) == 1);
	CHECK(Recovered.Sound.Calls.empty());
	CHECK(Cases.TakeNoted() == std::vector<std::string>{"a sound bank outside the gear battles' slot"});
}

TEST_CASE("FE 64 waits while a selected channel's tracks play, and ends the turn either way", "[Field]")
{
	// Wait for the scripts' channel 3 (tracks 14 and 15), Counter += 1, then wait on nothing.
	FSoundMap Map({0xFE, 0x64, 0xC0, 0x80, 0x3C, 0x00, 0x04, 0xC3, 0x01, 0x07, 0x00});
	Map.Sound.Playing = 0x8000;
	Map.Tick();
	Map.Tick();
	CHECK(Map.Var(Counter) == 0);
	// Tracks the mask does not select (the scripts' channel 7's) do not hold it.
	Map.Sound.Playing = 0x00C0;
	Map.Tick();
	CHECK(Map.Var(Counter) == 0);
	Map.Tick();
	CHECK(Map.Var(Counter) == 1);

	FSoundMap Silent({0xFE, 0x64, 0xFF, 0x80, 0x3C, 0x00, 0x04, 0xC3, 0x01, 0x07, 0x00});
	Silent.Context.SoundEffects = nullptr;
	Silent.Tick();
	Silent.Tick();
	CHECK(Silent.Var(Counter) == 1);
}

TEST_CASE("FE 5D, FE 62, FE 63 and FE 8C play and change a channel's effect; past the eighth channel they stop", "[Field]")
{
	FSoundMap Map({0xFE, 0x5D, 0x27, 0x80, 0x10, 0x80, 0x5A, 0x80, 0xFE, 0x62, 0x30, 0x80, 0x03, 0x80, 0xFE, 0x63,
		0x70, 0x80, 0x01, 0x80, 0xFE, 0x8C, 0x00, 0x80, 0x02, 0x80, 0xF0, 0x80, 0xFE, 0x62, 0x7F, 0x80, 0x08, 0x80});
	CHECK(Map.Fault() == "field map 7, actor 0, pc 001C, opcode FE 62: index 8 is past the table the instruction reads");
	CHECK(Map.Sound.Calls == FCalls{"stop 6", "play 39 6 90 16", "volume 6 48", "pan 2 112", "fade 4 0 240"});
}

TEST_CASE("FE 62, FE 63 and FE 8C on the channels past the eighth that the game names change nothing", "[Field]")
{
	// Map 272's volume 0 on channel 30, a pan on 158 (30 as the driver masks it), map 670's fade on channel 12 over 360
	// ticks; then Counter += 1.
	FSoundMap Map({0xFE, 0x62, 0x00, 0x80, 0x1E, 0x80, 0xFE, 0x63, 0x40, 0x80, 0x9E, 0x80, 0xFE, 0x8C, 0x00, 0x80, 0x0C,
		0x80, 0x68, 0x81, 0x3C, 0x00, 0x04, 0xC3, 0x01, 0x14, 0x00});
	Map.Tick();
	CHECK(Map.Var(Counter) == 1);
	CHECK(Map.Sound.Calls.empty());

	FSoundMap Other({0xFE, 0x8C, 0x00, 0x80, 0x0D, 0x80, 0x68, 0x81});
	CHECK(Other.Fault() == "field map 7, actor 0, pc 0000, opcode FE 8C: index 13 is past the table the instruction reads");
}

TEST_CASE("FE 13 frees the channel the actor's sound holds; FE 8A, FE 8D and FE 98 set the positional sounds up", "[Field]")
{
	FSoundMap Map({0xFE, 0x13, 0x09, 0x80, 0x7F, 0x80, 0xFE, 0x14, 0x00, 0x80, 0x20, 0x80, 0xFE, 0x8A, 0x02, 0x80,
		0xFE, 0x8D, 0x03, 0x80, 0xFE, 0x98, 0x00, 0x90, 0xC3, 0x01, 0x18, 0x00});
	FFieldActorSounds& Sounds = Map.Context.ActorSounds;
	Sounds.Channels[1] = {0, 5};
	Sounds.Channels[2] = {1, 7};
	Map.Tick();
	CHECK(Map.Sound.Calls == FCalls{"stop 2"});
	CHECK(Sounds.Channels[1].Owner == FFieldActorSounds::Free);
	CHECK(Sounds.Channels[1].Sound == FFieldActorSounds::Free);
	CHECK(Sounds.Channels[2].Owner == 1);
	const FEventActorState& State = Map.Context.States[0];
	CHECK(State.Sound == 0);
	CHECK(State.SoundVolume == 0x20);
	CHECK(State.SoundMode == FFieldActorSounds::Silent);
	CHECK(Sounds.Listener == 2);
	CHECK(Sounds.KeepMask == 3);
	CHECK(Sounds.Range == 0x1000);
}
