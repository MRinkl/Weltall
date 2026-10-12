#include "Game/BootNotice.h"
#include "Graphics/Texture.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	/** The notice's brightness after a number of vertical blanks. */
	uint8 BrightnessAfter(FBootNotice& Notice, int32& Done, int32 Vblanks)
	{
		for (; Done < Vblanks; ++Done)
		{
			REQUIRE(Notice.Tick());
		}
		const FDrawList List = Notice.DrawList();
		REQUIRE(List.Primitives.size() == 1);
		return List.Primitives[0].Corners[0].Color.R;
	}
}

TEST_CASE("The boot notice fades in to 120, holds, then fades out from 128 in 143 blanks", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	auto Notice = FBootNotice::Load(*Executable);
	REQUIRE(Notice);
	CHECK(Notice->DrawList().Primitives.empty());
	int32 Done = 0;
	CHECK(BrightnessAfter(*Notice, Done, 1) == 0);
	CHECK(BrightnessAfter(*Notice, Done, 16) == 120);
	CHECK(BrightnessAfter(*Notice, Done, 126) == 120);
	CHECK(BrightnessAfter(*Notice, Done, 127) == 128);
	CHECK(BrightnessAfter(*Notice, Done, 143) == 0);
	CHECK_FALSE(Notice->Tick());

	// The sprite is the 256 x 48 picture at (32, 88), its texels from the kernel's page and CLUT.
	const FDrawPrimitive Sprite = Notice->DrawList().Primitives[0];
	CHECK(Sprite.Corners[0].X == 32.0f);
	CHECK(Sprite.Corners[0].Y == 88.0f);
	CHECK(Sprite.Corners[3].X == 288.0f);
	CHECK(Sprite.Corners[3].Y == 136.0f);
	int32 Drawn = 0;
	for (int32 V = 0; V < 48; ++V)
	{
		for (int32 U = 0; U < 256; ++U)
		{
			Drawn += FetchTexel(Notice->Vram(), Sprite.Texture->Page, Sprite.Texture->Clut, static_cast<uint8>(U), static_cast<uint8>(V)) != 0;
		}
	}
	CHECK(Drawn > 1000);
	CHECK(Drawn < 256 * 48);
}
