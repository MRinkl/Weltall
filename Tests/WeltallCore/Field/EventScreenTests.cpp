#include "Field/EventMachine.h"
#include "Game/PsxRandom.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

#include <map>
#include <memory>

using namespace Weltall;
using Weltall::Testing::FByteWriter;

namespace
{
	/** Files of directory 4 by number; another directory holds none. */
	class FTestFiles final : public IFieldFiles
	{
	public:
		bool ReadFile(int32 Directory, int32 File, std::vector<uint8>& Bytes) override
		{
			const auto Found = Files.find(File);
			if (Directory != 4 || Found == Files.end())
			{
				return false;
			}
			Bytes = Found->second;
			return true;
		}

		std::map<int32, std::vector<uint8>> Files;
	};

	/** A 4-bit TIM: a 16-colour CLUT for (16, 480), then 2 x 2 units of pixels for (100, 50). */
	std::vector<uint8> SmallTim()
	{
		FByteWriter Writer;
		Writer.U32(0x10);
		Writer.U32(0x08);
		Writer.U32(12 + 32);
		for (const int32 Value : {16, 480, 16, 1})
		{
			Writer.U16(static_cast<uint16>(Value));
		}
		for (uint16 Color = 0; Color < 16; ++Color)
		{
			Writer.U16(static_cast<uint16>(0x100 + Color));
		}
		Writer.U32(12 + 8);
		for (const int32 Value : {100, 50, 2, 2})
		{
			Writer.U16(static_cast<uint16>(Value));
		}
		for (const int32 Unit : {0x1111, 0x2222, 0x3333, 0x4444})
		{
			Writer.U16(static_cast<uint16>(Unit));
		}
		return Writer.Bytes;
	}

	/** One script actor running Code from its start as its idle script, tick by tick. */
	class FScreenMap
	{
	public:
		explicit FScreenMap(std::vector<uint8> Code)
		{
			FByteWriter Writer;
			for (int32 Word = 0; Word < 32; ++Word)
			{
				Writer.U32(0);
			}
			Writer.U32(1);
			for (int32 Script = 0; Script < 32; ++Script)
			{
				Writer.U16(0);
			}
			Writer.Bytes.insert(Writer.Bytes.end(), Code.begin(), Code.end());
			auto Parsed = FEventScripts::Parse(Writer.Bytes);
			REQUIRE(Parsed);
			Scripts = std::make_unique<FEventScripts>(std::move(*Parsed));
			Context.Scripts = Scripts.get();
			Context.Random = &Random;
			Context.Files = &Files;
			FFieldActor Record;
			Record.Flags = 0x200;
			Context.Actors.push_back(Record);
			Context.States.emplace_back().Reset(Random.Next());
			Context.bInitDone = true;
		}

		/** A tick's scripts, the render state's lists of the last tick gone first. */
		std::optional<FError> Tick()
		{
			Context.Render.BeginTick();
			return FEventMachine::Update(Context);
		}

		uint16 Pc() { return Context.States[0].Threads[0].Pc; }

		FPsxRandom Random{1};
		FTestFiles Files;
		std::unique_ptr<FEventScripts> Scripts;
		FEventContext Context;
	};

	const FFieldVramOperation& Only(const FFieldRenderState& Render, std::size_t Index)
	{
		REQUIRE(Render.VramOperations.size() > Index);
		return Render.VramOperations[Index];
	}
}

TEST_CASE("FE 77 reads the image its upload names, uploads it at the scripts' place and gives it up, a tick each", "[Field]")
{
	// FE 77 00, then FE 77 01 naming file 5 (7FBh + 5) placed at (320, 256), CLUT operand 2, all constants.
	FScreenMap Map({0xFE, 0x77, 0x00, 0xFE, 0x77, 0x01, 0x05, 0x00, 0x40, 0x01, 0x00, 0x01, 0x02, 0x00, 0xF0, 0xFE, 0x77, 0x02, 0x00});
	Map.Files.Files[0x800] = SmallTim();
	REQUIRE_FALSE(Map.Tick());
	CHECK(Map.Pc() == 3);
	CHECK(Map.Context.ScriptImage.has_value());
	CHECK(Map.Context.Render.VramOperations.empty());

	REQUIRE_FALSE(Map.Tick());
	CHECK(Map.Pc() == 15);
	const FFieldRenderState& Render = Map.Context.Render;
	REQUIRE(Render.VramOperations.size() == 2);
	const FFieldVramOperation& Clut = Only(Render, 0);
	CHECK(Clut.Kind == EFieldVramChange::Upload);
	CHECK((Clut.Rect.X == 0 && Clut.Rect.Y == 0xEA && Clut.Rect.Width == 16 && Clut.Rect.Height == 1));
	CHECK(Render.VramUnits[Clut.UnitsAt + 15] == 0x10F);
	const FFieldVramOperation& Pixels = Only(Render, 1);
	CHECK((Pixels.Rect.X == 320 && Pixels.Rect.Y == 256 && Pixels.Rect.Width == 2 && Pixels.Rect.Height == 2));
	CHECK(Render.VramUnits[Pixels.UnitsAt + 3] == 0x4444);
	CHECK_FALSE(Render.bEffectTexturesSaved);

	REQUIRE_FALSE(Map.Tick());
	CHECK(Map.Pc() == 18);
	CHECK_FALSE(Map.Context.ScriptImage.has_value());
}

TEST_CASE("FE 77 keeps the CLUT where the image puts it for FFh, and keeps the effects' textures aside for a large image", "[Field]")
{
	FScreenMap Map({0xFE, 0x77, 0x00, 0xFE, 0x77, 0x01, 0x05, 0x00, 0xC0, 0x02, 0x00, 0x01, 0xFF, 0x00, 0xF0, 0x00});
	Map.Files.Files[0x800] = SmallTim();
	REQUIRE_FALSE(Map.Tick());
	REQUIRE_FALSE(Map.Tick());
	const FFieldRenderState& Render = Map.Context.Render;
	CHECK(Render.bEffectTexturesSaved);
	REQUIRE(Render.VramOperations.size() == 3);
	CHECK(Only(Render, 0).Kind == EFieldVramChange::SaveArea);
	CHECK((Only(Render, 1).Rect.X == 16 && Only(Render, 1).Rect.Y == 480));
	CHECK((Only(Render, 2).Rect.X == 0x2C0 && Only(Render, 2).Rect.Y == 0x100));
}

TEST_CASE("FE 77's upload without an image read, or an image that cannot be read, stops the scripts", "[Field]")
{
	FScreenMap Upload({0xFE, 0x77, 0x01, 0x05, 0x00, 0x40, 0x01, 0x00, 0x01, 0x00, 0x00, 0xF0});
	CHECK(Upload.Tick());
	FScreenMap Missing({0xFE, 0x77, 0x00, 0xFE, 0x77, 0x01, 0x05, 0x00, 0x40, 0x01, 0x00, 0x01, 0x00, 0x00, 0xF0});
	CHECK(Missing.Tick());
}

TEST_CASE("FE DF clears VRAM and widens the screen, then shows and hides its still", "[Field]")
{
	// Mode 0, then 1, 2 and an unknown 3, each a constant.
	FScreenMap Map({0xFE, 0xDF, 0x00, 0x80, 0xFE, 0xDF, 0x01, 0x80, 0x26, 0x00, 0x80, 0xFE, 0xDF, 0x02, 0x80, 0xFE, 0xDF, 0x03, 0x80, 0x00});
	REQUIRE_FALSE(Map.Tick());
	const FFieldRenderState& Render = Map.Context.Render;
	// The library's clear of 1280 x 512 as the scripts ask for it: the drawing holds it within VRAM.
	REQUIRE(Render.VramOperations.size() == 1);
	CHECK(Only(Render, 0).Kind == EFieldVramChange::Clear);
	CHECK((Only(Render, 0).Rect.X == 0 && Only(Render, 0).Rect.Y == 0 && Only(Render, 0).Rect.Width == 0x500 && Only(Render, 0).Rect.Height == 0x200));
	CHECK(Render.bHiResDisplay);
	CHECK((Render.DrawClip.X == 0 && Render.DrawClip.Width == 0x280 && Render.DrawClip.Height == 0xE0));
	CHECK(Render.bHiResStill);
	REQUIRE_FALSE(Map.Tick());
	CHECK_FALSE(Render.bHiResStill);
	CHECK(Render.bHiResDisplay);
	CHECK(Map.Pc() == 19);
}

TEST_CASE("FE A3 keeps the effects' textures aside and puts them back, once each", "[Field]")
{
	FScreenMap Map({0xFE, 0xA3, 0x00, 0xFE, 0xA3, 0x00, 0xFE, 0xA3, 0x01, 0xFE, 0xA3, 0x01, 0x00});
	REQUIRE_FALSE(Map.Tick());
	const FFieldRenderState& Render = Map.Context.Render;
	CHECK_FALSE(Render.bEffectTexturesSaved);
	REQUIRE(Render.VramOperations.size() == 2);
	CHECK(Only(Render, 0).Kind == EFieldVramChange::SaveArea);
	CHECK(Only(Render, 1).Kind == EFieldVramChange::RestoreArea);
	CHECK((Only(Render, 1).Rect.X == 0x3C0 && Only(Render, 1).Rect.Y == 0x100 && Only(Render, 1).Rect.Width == 0x40 && Only(Render, 1).Rect.Height == 0x100));
}

TEST_CASE("FE DD keeps CLUT rows aside, fades them without ending the turn and loads them back with the frame", "[Field]")
{
	// Rows 1E0h and 1E1h kept aside; next tick row 1 faded by 12 and row 0 by 40; then given up.
	FScreenMap Map({0xFE, 0xDD, 0x00, 0xE0, 0x81, 0x02, 0x80, 0xFE, 0xDD, 0x01, 0x01, 0x80, 0x0C, 0x80, 0xFE, 0xDD, 0x01, 0x00, 0x80, 0x28, 0x80,
		0xFE, 0xDD, 0x02, 0xFE, 0xDD, 0x03, 0x00});
	REQUIRE_FALSE(Map.Tick());
	FFieldRenderState& Render = Map.Context.Render;
	CHECK(Map.Pc() == 7);
	CHECK(Render.ClutFade.bKept);
	CHECK((Render.ClutFade.Rect.Y == 0x1E0 && Render.ClutFade.Rect.Width == 0x100 && Render.ClutFade.Rect.Height == 2));
	CHECK(Only(Render, 0).Kind == EFieldVramChange::SaveClutRows);

	REQUIRE_FALSE(Map.Tick());
	CHECK(Map.Pc() == 24);
	REQUIRE(Render.VramOperations.size() == 2);
	CHECK((Only(Render, 0).Kind == EFieldVramChange::FadeClutRow && Only(Render, 0).Row == 1 && Only(Render, 0).Factor == 12));
	CHECK((Only(Render, 1).Row == 0 && Only(Render, 1).Factor == 40));
	CHECK(Render.ClutFade.bLoadPending);
	CHECK_FALSE(Render.ClutFade.bKept);

	REQUIRE_FALSE(Map.Tick());
	CHECK(Map.Pc() == 27);
}

TEST_CASE("FE DD fades no row before the rows are kept aside or past them", "[Field]")
{
	FScreenMap Early({0xFE, 0xDD, 0x01, 0x00, 0x80, 0x0C, 0x80});
	CHECK(Early.Tick());
	FScreenMap Past({0xFE, 0xDD, 0x00, 0xE0, 0x81, 0x01, 0x80, 0xFE, 0xDD, 0x01, 0x01, 0x80, 0x0C, 0x80});
	REQUIRE_FALSE(Past.Tick());
	CHECK(Past.Tick());
}

TEST_CASE("FE D4 makes the markers, colours them and places them for the tick's frame by their kind", "[Field]")
{
	// Make; colour marker 3 (20, 30, 40); place marker 3 at (100, 60) kind 0, marker 0 at (10, 10) kind 1, marker 5 at (7, 8) kind 2.
	FScreenMap Map({0xFE, 0xD4, 0x00, 0xFE, 0xD4, 0x03, 0x03, 0x80, 0x14, 0x80, 0x1E, 0x80, 0x28, 0x80, 0xFE, 0xD4, 0x01, 0x03, 0x80, 0x64, 0x80,
		0x3C, 0x80, 0x00, 0x80, 0xFE, 0xD4, 0x01, 0x00, 0x80, 0x0A, 0x80, 0x0A, 0x80, 0x01, 0x80, 0xFE, 0xD4, 0x01, 0x05, 0x80, 0x07, 0x80,
		0x08, 0x80, 0x02, 0x80, 0xFE, 0xD4, 0x02, 0x00});
	REQUIRE_FALSE(Map.Tick());
	const FFieldScreenMarkers& Markers = Map.Context.Render.Markers;
	CHECK_FALSE(Markers.bMade);
	CHECK(Markers.Colors[3] == std::array<uint8, 3>{20, 30, 40});
	CHECK(Markers.Colors[0] == std::array<uint8, 3>{0x80, 0x80, 0x80});
	REQUIRE(Markers.Placed.size() == 3);
	CHECK((Markers.Placed[0].Marker == 3 && Markers.Placed[0].X == 96 && Markers.Placed[0].Y == 48));
	CHECK((Markers.Placed[1].Marker == 0 && Markers.Placed[1].X == 6 && Markers.Placed[1].Y == 6));
	CHECK((Markers.Placed[2].Marker == 5 && Markers.Placed[2].X == 7 && Markers.Placed[2].Y == 8));
}

TEST_CASE("FE D4 stops the scripts for a marker placed twice in a tick, past the 33, or before the markers are made", "[Field]")
{
	FScreenMap Twice({0xFE, 0xD4, 0x00, 0xFE, 0xD4, 0x01, 0x01, 0x80, 0x00, 0x80, 0x00, 0x80, 0x02, 0x80, 0xFE, 0xD4, 0x01, 0x01, 0x80, 0x00, 0x80,
		0x00, 0x80, 0x02, 0x80});
	CHECK(Twice.Tick());
	FScreenMap Past({0xFE, 0xD4, 0x00, 0xFE, 0xD4, 0x03, 0x21, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80});
	CHECK(Past.Tick());
	FScreenMap Early({0xFE, 0xD4, 0x01, 0x01, 0x80, 0x00, 0x80, 0x00, 0x80, 0x02, 0x80});
	CHECK(Early.Tick());
}

TEST_CASE("A marker placed in one tick can be placed again in the next", "[Field]")
{
	FScreenMap Map({0xFE, 0xD4, 0x00, 0xFE, 0xD4, 0x01, 0x01, 0x80, 0x00, 0x80, 0x00, 0x80, 0x02, 0x80, 0xC3, 0x01, 0x03, 0x00});
	REQUIRE_FALSE(Map.Tick());
	REQUIRE_FALSE(Map.Tick());
	CHECK(Map.Context.Render.Markers.Placed.size() == 1);
}

TEST_CASE("The emitters 8 to 10 a script sets go on and set nothing; one past them stops the scripts", "[Field]")
{
	// FE 8F on actor 0 at gear joint 7, then emitter 7 as map 442 sets it.
	const std::vector<uint8> Setup{0xFE, 0x8F, 0x00, 0x01, 0x80, 0x00, 0x80, 0x07, 0x80, 0xFE, 0x90, 0x07, 0x80, 0x12, 0x80, 0x00, 0x80, 0xFF, 0xFF};
	// Emitter 8 set as map 442 sets it, then its facing offsets.
	const std::vector<uint8> Stray{0xFE, 0x90, 0x08, 0x80, 0x07, 0x80, 0x00, 0x80, 0xFF, 0xFF, 0xFE, 0x91, 0x50, 0xFB, 0xDC, 0x05, 0x00, 0x00, 0x50,
		0xFB, 0xD0, 0x07, 0x00, 0x00, 0xFC, 0xFE, 0x92, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x0B, 0x00, 0xFC, 0xFE, 0x93, 0x01,
		0x80, 0x04, 0x80, 0x03, 0x80, 0x01, 0x80, 0x02, 0x80, 0xFE, 0x94, 0x7C, 0x03, 0x90, 0x03, 0xDF, 0xFF, 0xDF, 0xFF, 0xF0, 0xFE, 0x95, 0x61, 0x00,
		0x5D, 0x00, 0x5A, 0x00, 0xFD, 0xFF, 0xFD, 0xFF, 0xFD, 0xFF, 0xFC, 0xFE, 0xA5, 0x04, 0x80, 0x00, 0x80, 0x00, 0x80, 0xFE, 0xBD, 0x00, 0x80, 0x00,
		0x80, 0x00, 0x80, 0xFE, 0xC8, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00, 0xFF};
	auto Joined = [&](std::initializer_list<const std::vector<uint8>*> Parts)
	{
		std::vector<uint8> Code;
		for (const std::vector<uint8>* Part : Parts)
		{
			Code.insert(Code.end(), Part->begin(), Part->end());
		}
		Code.push_back(0x00);
		return Code;
	};
	FScreenMap Alone(Joined({&Setup}));
	REQUIRE_FALSE(Alone.Tick());
	std::vector<uint8> Code = Joined({&Setup, &Stray});
	FScreenMap Strays(Code);
	REQUIRE_FALSE(Strays.Tick());
	CHECK(Strays.Pc() == Code.size() - 1);
	CHECK(Strays.Context.Particles.Current == 8);
	for (std::size_t Index = 0; Index < FFieldParticles::EmitterCount; ++Index)
	{
		const FFieldParticleEmitter& Expected = Alone.Context.Particles.Setup[Index];
		const FFieldParticleEmitter& Emitter = Strays.Context.Particles.Setup[Index];
		CHECK(Emitter.Count == Expected.Count);
		CHECK(Emitter.EmitTicks == Expected.EmitTicks);
		CHECK(Emitter.Speed == Expected.Speed);
		CHECK(Emitter.Origin == Expected.Origin);
		CHECK(Emitter.Target == Expected.Target);
		CHECK(Emitter.SpawnRadius == Expected.SpawnRadius);
		CHECK(Emitter.SpeedScale == Expected.SpeedScale);
		CHECK(Emitter.Flags == Expected.Flags);
		CHECK(Emitter.FacingOffsets == Expected.FacingOffsets);
		CHECK(Emitter.Lifetime == Expected.Lifetime);
		CHECK(Emitter.Size == Expected.Size);
		CHECK(Emitter.Color == Expected.Color);
		CHECK(Emitter.Roll == Expected.Roll);
	}
	CHECK(Alone.Context.Particles.Setup[7].Count == 0x12);

	const std::vector<uint8> Past{0xFE, 0x90, 0x0B, 0x80, 0x07, 0x80, 0x00, 0x80, 0xFF, 0xFF};
	FScreenMap Beyond(Joined({&Setup, &Past}));
	CHECK(Beyond.Tick());
}
