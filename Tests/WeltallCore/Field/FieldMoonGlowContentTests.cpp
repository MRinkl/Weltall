#include "Field/FieldMap.h"
#include "Field/FieldMoonGlow.h"
#include "Graphics/Vram.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <cmath>

using namespace Weltall;

namespace
{
	/** The map's textures as its load leaves them. */
	FVram Loaded(const FIso9660& Disc, int32 Number)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		const auto Map = LoadFieldMap(Disc.Image(), *Index, Number);
		REQUIRE(Map);
		FVram Vram;
		UploadFieldTextures(*Map, Vram, false);
		return Vram;
	}
}

TEST_CASE("The moon's glow over the Yggdrasil's deck fades out within its picture", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// The deck's moon (map 283's): a picture of 8-bit texels at (320, 256) with its CLUT at (0, 505).
	constexpr int32 Deck = 283;
	constexpr int32 PictureX = 320;
	constexpr int32 PictureY = 256;
	constexpr int32 ClutX = 0;
	constexpr int32 ClutY = 505;
	constexpr int32 Side = 128;
	constexpr double Middle = (Side - 1) / 2.0;
	const FVram Painted = Loaded(*Disc, Deck);
	FVram Faded = Painted;
	FadeMoonGlow(Deck, Faded);

	const auto Texel = [](const FVram& Vram, int32 U, int32 V) { return static_cast<uint8>(Vram.At(PictureX + U / 2, PictureY + V) >> (U % 2 * 8)); };
	const auto Colour = [&](const FVram& Vram, int32 U, int32 V) { return Vram.At(ClutX + Texel(Vram, U, V), ClutY); };
	int32 GlowAtTheEdge = 0;
	int32 Changed = 0;
	for (int32 V = 0; V < Side; ++V)
	{
		for (int32 U = 0; U < Side; ++U)
		{
			const double Distance = std::hypot(U - Middle, V - Middle);
			// The moon and the glow's inner part as painted.
			if (Distance < 58.0)
			{
				CHECK(Texel(Faded, U, V) == Texel(Painted, U, V));
			}
			// Nothing drawn where the picture's edge would show it against the sky.
			if (Distance >= Side / 2.0)
			{
				CHECK(Colour(Faded, U, V) == 0);
				GlowAtTheEdge += Colour(Painted, U, V) != 0 ? 1 : 0;
			}
			Changed += Texel(Faded, U, V) != Texel(Painted, U, V) ? 1 : 0;
		}
	}
	// The painted glow does reach the picture's edges, in navy.
	CHECK(GlowAtTheEdge > 1000);
	CHECK(Changed > 0);

	// Another map, and the same map's moon where it is not: nothing changes.
	FVram Elsewhere = Painted;
	FadeMoonGlow(282, Elsewhere);
	CHECK(Elsewhere.At(PictureX, PictureY + 1) == Painted.At(PictureX, PictureY + 1));
	CHECK(Elsewhere.Revision() == Painted.Revision());
	FVram Blank;
	FadeMoonGlow(Deck, Blank);
	CHECK(Blank.Revision() == FVram().Revision());
}
