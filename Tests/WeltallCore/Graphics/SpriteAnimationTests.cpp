#include "Graphics/Sprite.h"
#include "Graphics/SpriteFile.h"
#include "Graphics/SpriteSystem.h"

#include <catch_amalgamated.hpp>

#include <vector>

using namespace Weltall;

namespace
{
	/**
	 * A file with one animation: its table at 14h (count 1, the header 4 bytes on), the header at 18h
	 * (one direction, the script at 1Eh: step 50h, the list at 20h), no frames and no palettes.
	 */
	std::vector<uint8> OneAnimationFile()
	{
		return {
			0x03, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x2C, 0x00, 0x00, 0x00, // sections
			0x30, 0x00, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x04, 0x00, 0x50, 0x00, // end, table, header, script
			0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // list, frames, palettes
		};
	}

	/** OneAnimationFile with the animation's script at 30h, Script's bytes, and the header's flags Flags. */
	std::vector<uint8> ScriptFile(std::vector<uint8> Script, uint16 Flags)
	{
		std::vector<uint8> File = OneAnimationFile();
		constexpr std::size_t ScriptAt = 0x30;
		File[0x10] = static_cast<uint8>(ScriptAt + Script.size());
		File[0x18] = static_cast<uint8>(Flags);
		File[0x19] = static_cast<uint8>(Flags >> 8);
		// The script's offset counts from its own place.
		File[0x1A] = static_cast<uint8>(ScriptAt - 0x1A);
		File.insert(File.end(), Script.begin(), Script.end());
		return File;
	}

	/** The wait step 50h leaves at the given time scale, in a system whose ticking loop holds StrayWait. */
	uint16 StrayStepWait(std::optional<int32> StrayWait, int32 TimeScale, FSpriteSystem& System)
	{
		static const auto File = FSpriteFile::Parse(OneAnimationFile());
		System.TickExtraFrames = 0;
		System.StrayWait = StrayWait;
		FSprite Sprite;
		System.Create(Sprite, *File, 0, 0, 0, 0);
		Sprite.SetTimeScale(TimeScale);
		System.TickAnimation(Sprite);
		return Sprite.Wait;
	}
}

TEST_CASE("A sprite's animation past its file's table reads the bytes after the offsets as the kernel does", "[Graphics]")
{
	const auto File = FSpriteFile::Parse(OneAnimationFile());
	REQUIRE(File);
	FSpriteSystem System;
	FSprite Sprite;
	System.Create(Sprite, *File, 0, 0, 0, 0);

	// Animation 1's offset is the first header's flags, 0: the table itself is read as the header, its
	// count as the flags (four directions) and the first offset as the script's.
	System.SetAnimation(Sprite, 1);
	CHECK(Sprite.Header == 0x14);
	CHECK(Sprite.Pc == 0x1A);
	CHECK(Sprite.DirectionMode() == 1);
	CHECK(Sprite.Animation() == 1);
	CHECK_FALSE(System.FaultError());

	// Animation 2's offset is the first header's script offset, 4: the first header again.
	System.SetAnimation(Sprite, 2);
	CHECK(Sprite.Header == 0x18);
	CHECK(Sprite.Pc == 0x1E);
	CHECK_FALSE(System.FaultError());

	// An offset read past the file's end is memory the sprite does not own.
	System.SetAnimation(Sprite, 30);
	CHECK(Sprite.Pc == FSprite::Stopped);
	REQUIRE(System.FaultError());
	CHECK(System.Fault() == ESpriteFault::PcOutsideFile);
}

TEST_CASE("A sprite's steps 40h to 7Fh wait by what the loop ticking the sprites holds", "[Graphics]")
{
	// The field's main loop holds FFh: 255 ticks at the time scale 100h, 95 at 60h.
	FSpriteSystem MainLoop;
	CHECK(StrayStepWait(0xFF, 0x100, MainLoop) == 0xFF);
	CHECK(StrayStepWait(0xFF, 0x60, MainLoop) == 0x5F);
	CHECK_FALSE(MainLoop.FaultError());

	// The entry's loop holds -4 << 16: a wait of 8000h ticks at 60h, and at 100h one that is 0 in 16 bits
	// though not 0 in 32, so the step adds nothing and the sprite never steps again.
	FSpriteSystem Entry;
	CHECK(StrayStepWait(-0x40000, 0x60, Entry) == 0x8000);
	CHECK(StrayStepWait(-0x40000, 0x100, Entry) == 0);
	CHECK_FALSE(Entry.FaultError());

	FSpriteSystem Unknown;
	StrayStepWait(std::nullopt, 0x100, Unknown);
	CHECK(Unknown.Fault() == ESpriteFault::UnsupportedInstruction);
}

namespace
{
	/** OneAnimationFile with the script 31h 80h: a step that waits two ticks, then the end. */
	std::vector<uint8> OneStepFile()
	{
		std::vector<uint8> Bytes = OneAnimationFile();
		Bytes[0x1E] = 0x31;
		Bytes[0x1F] = 0x80;
		return Bytes;
	}
}

TEST_CASE("A model's texture page base set by the script lasts for the one attach that builds its packets", "[Graphics]")
{
	// A sprite of the transform kind: 8D (the base from its texture's place), F5, then a step; and with a second F5.
	constexpr uint16 TransformKind = 0x200;
	const auto Once = FSpriteFile::Parse(ScriptFile({0x8D, 0xF5, 0x00, 0x00, 0x00, 0x30, 0x00}, TransformKind));
	const auto Twice = FSpriteFile::Parse(ScriptFile({0x8D, 0xF5, 0x00, 0x00, 0x00, 0xF5, 0x00, 0x00, 0x00, 0x30, 0x00}, TransformKind));
	REQUIRE(Once);
	REQUIRE(Twice);
	FSpriteSystem System;
	FSprite Sprite;
	System.Create(Sprite, *Once, 0, 0, 0x2C0, 0x100);
	System.TickAnimation(Sprite);
	CHECK_FALSE(System.FaultError());
	REQUIRE(Sprite.ModelFile == &*Once);
	CHECK(Sprite.bModelTextureOverride);
	CHECK((Sprite.ModelTextureX == 0x2C0 && Sprite.ModelTextureY == 0x100));
	CHECK_FALSE(System.bModelTextureOverride);

	FSprite Again;
	System.Create(Again, *Twice, 0, 0, 0x2C0, 0x100);
	System.TickAnimation(Again);
	CHECK_FALSE(System.FaultError());
	CHECK_FALSE(Again.bModelTextureOverride);
}

TEST_CASE("A sprite put back as it was saved runs its animation up to its steps and stands where it stood", "[Graphics]")
{
	const auto File = FSpriteFile::Parse(OneStepFile());
	REQUIRE(File);
	FSpriteSystem System;
	FSprite Sprite;
	System.Create(Sprite, *File, 0, 0, 0, 0);
	System.TickAnimation(Sprite);
	REQUIRE(Sprite.StepsRun() == 1);
	Sprite.Position = {0x10000, -0x20000, 0x30000};
	Sprite.Facing = 0x400;
	Sprite.Weight = 0x2000;
	Sprite.Scale = 0xC00;
	Sprite.TransformScale = {0x1000, 0x800, 0x1000};
	Sprite.Extra.PresetVerticalSpeed = 5;
	Sprite.Extra.PresetGravity = 7;
	const FSpriteSavedState Saved = FSpriteSystem::SaveState(Sprite);
	CHECK(Saved.StepsRun == 1);
	CHECK(Saved.Animation == 0);

	FSprite Again;
	System.Create(Again, *File, 0, 0, 0, 0);
	// The kernel moves the sprite by its velocity while the animation runs up, then puts it back where it stood.
	Again.Velocity = {0x100, 0x200, 0x300};
	System.RestoreState(Again, Saved);
	CHECK_FALSE(System.FaultError());
	CHECK(Again.StepsRun() == 1);
	CHECK(Again.Position == Saved.Position);
	CHECK(Again.Facing == 0x400);
	CHECK(Again.Weight == 0x2000);
	CHECK(Again.Scale == 0xC00);
	CHECK(Again.TransformScale == Saved.TransformScale);
	CHECK(Again.Extra.PresetVerticalSpeed == 5);
	CHECK(Again.Extra.PresetGravity == 7);
	CHECK(System.TickExtraFrames == 1);

	// An animation that ends before the steps saved never gets there: the kernel would wait for ever.
	FSpriteSavedState Unreachable = Saved;
	Unreachable.StepsRun = 5;
	FSprite Stuck;
	System.Create(Stuck, *File, 0, 0, 0, 0);
	System.RestoreState(Stuck, Unreachable);
	CHECK(System.Fault() == ESpriteFault::SavedStepsNeverReached);
}

TEST_CASE("A sprite counts the animations started on it from outside its script, not its script's own restarts", "[Graphics]")
{
	struct FRestarts final : ISpriteListener
	{
		int32 Heard = 0;
		void OnAnimationEvent(FSprite&) override { ++Heard; }
	};
	// 31h 82h: a step that waits two ticks, then the animation again.
	const auto File = FSpriteFile::Parse(ScriptFile({0x31, 0x82}, 0));
	REQUIRE(File);
	FSpriteSystem System;
	FSprite Sprite;
	System.Create(Sprite, *File, 0, 0, 0, 0);
	FRestarts Restarts;
	Sprite.Listener = &Restarts;
	const uint32 Made = Sprite.Starts;
	CHECK(Made == 1);
	for (int32 Tick = 0; Tick < 12; ++Tick)
	{
		System.TickAnimation(Sprite);
	}
	CHECK_FALSE(System.FaultError());
	CHECK(Restarts.Heard > 1);
	CHECK(Sprite.Starts == Made);
	System.SetAnimation(Sprite, 0);
	CHECK(Sprite.Starts == Made + 1);
}

TEST_CASE("A sprite's point at an anchor of a parent that has ended is the point it already has", "[Graphics]")
{
	// BCh CFh: the goal at the parent's anchor 5; BCh 8Fh: the place at it; then a step that waits two ticks.
	const auto File = FSpriteFile::Parse(ScriptFile({0xBC, 0xCF, 0xBC, 0x8F, 0x31, 0x82}, 0));
	REQUIRE(File);
	FSpriteSystem System;
	FSprite Sprite;
	System.Create(Sprite, *File, 0, 0, 0, 0);
	Sprite.SavedPoint = {120, -45, 830};
	Sprite.Position = {0x12345, 0x6789A, -0x23456};
	System.TickAnimation(Sprite);
	CHECK_FALSE(System.FaultError());
	CHECK(Sprite.SavedPoint == std::array<int16, 3>{120, -45, 830});
	CHECK(Sprite.Position == std::array<int32, 3>{0x10000, 0x60000, -0x30000});

	// With its parent there (a sprite without parts) both are the parent's place.
	FSprite Parent;
	Parent.Position = {0x50000, -0x60000, 0x70000};
	FSprite Child;
	System.Create(Child, *File, 0, 0, 0, 0);
	Child.Parent = &Parent;
	System.TickAnimation(Child);
	CHECK_FALSE(System.FaultError());
	CHECK(Child.SavedPoint == std::array<int16, 3>{5, -6, 7});
	CHECK(Child.Position == Parent.Position);
}
