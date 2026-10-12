#include "Field/FieldBackdrop.h"
#include "Graphics/GteMath.h"

#include <catch_amalgamated.hpp>

#include <cmath>
#include <numbers>

using namespace Weltall;

// The field's panorama (FE 80-FE 82) as a tick draws it, on synthetic panoramas and views: the strip's
// quads across the screen, the colour bands, where the view puts them, and the strip carried on past
// the screen for wider views.

namespace
{
	using EKind = FFieldBackdropQuad::EKind;

	/** A 320 x 224 15-bit picture at VRAM (512, 0), as the title's map makes it. */
	FFieldBackdrop Picture()
	{
		FFieldBackdrop Backdrop;
		Backdrop.bShown = true;
		FFieldPanorama& Panorama = Backdrop.Panorama;
		Panorama.bEnabled = true;
		Panorama.VramX = 0x200;
		Panorama.VramY = 0;
		Panorama.Width = 320;
		Panorama.Height = 224;
		Panorama.ClutY = 0x1E0;
		Panorama.TexturePage = 2;
		Panorama.Colors = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}};
		Backdrop.HorizonY = 224;
		Backdrop.BandY = 300;
		return Backdrop;
	}

	/** A 64-texel 4-bit strip 32 rows high at VRAM (965, 256), its CLUT at (0, 480). */
	FFieldBackdrop NarrowStrip()
	{
		FFieldBackdrop Backdrop = Picture();
		FFieldPanorama& Panorama = Backdrop.Panorama;
		Panorama.VramX = 965;
		Panorama.VramY = 256;
		Panorama.Width = 64;
		Panorama.Height = 32;
		Panorama.TexturePage = 0;
		Backdrop.HorizonY = 100;
		Backdrop.BandY = 120;
		return Backdrop;
	}

	/** libgte's arctangents, computed. */
	FGteArctangents Arctangents()
	{
		FGteArctangents Table;
		for (std::size_t Index = 0; Index < Table.Angles.size(); ++Index)
		{
			Table.Angles[Index] = static_cast<int16>(std::lround(std::atan(static_cast<double>(Index) / 1024.0) * 2048.0 / std::numbers::pi));
		}
		return Table;
	}

	FFieldPoint Point(int32 X, int32 Y, int32 Z)
	{
		return {X << 16, Y << 16, Z << 16};
	}
}

TEST_CASE("The panorama's strip is cut where its texture page's reach and the strip end, and the bands follow the horizon", "[Field]")
{
	FFieldBackdrop Backdrop = Picture();
	Backdrop.ScrollU = 100;
	std::vector<FFieldBackdropQuad> Quads;
	DrawFieldBackdrop(Backdrop, 0, FieldBackdropWidth, Quads);
	REQUIRE(Quads.size() == 3);
	// Linked last, the graded band is drawn first: from the horizon down to the band's end. Nothing lies above the strip.
	CHECK(Quads[0].Kind == EKind::Graded);
	CHECK(Quads[0].Corners[0] == std::array<int16, 2>{0, 224});
	CHECK(Quads[0].Corners[3] == std::array<int16, 2>{320, 300});
	CHECK(Quads[0].Colors[0] == std::array<uint8, 3>{4, 5, 6});
	CHECK(Quads[0].Colors[2] == std::array<uint8, 3>{7, 8, 9});
	// The strip's second quad: from the strip's start, up to the screen's edge.
	const FFieldBackdropQuad& Second = Quads[1];
	CHECK(Second.Kind == EKind::Textured);
	CHECK(Second.Corners[0] == std::array<int16, 2>{220, 0});
	CHECK(Second.Corners[3] == std::array<int16, 2>{320, 224});
	CHECK(Second.Texels[0] == std::array<uint8, 2>{0, 0});
	CHECK(Second.Texels[3] == std::array<uint8, 2>{99, 224});
	CHECK(Second.TexturePage == 0x108);
	// Its first: from column 100 (texel 36 of the page at x 576) to the page's reach.
	const FFieldBackdropQuad& First = Quads[2];
	CHECK(First.Corners[0] == std::array<int16, 2>{0, 0});
	CHECK(First.Corners[3] == std::array<int16, 2>{220, 224});
	CHECK(First.Texels[0] == std::array<uint8, 2>{36, 0});
	CHECK(First.Texels[1] == std::array<uint8, 2>{255, 0});
	CHECK(First.TexturePage == 0x109);
	CHECK(First.Clut == 0x7800);

	// With the horizon higher, the flat band above the strip, the graded band and the flat one below it.
	Backdrop.HorizonY = 230;
	Backdrop.BandY = 235;
	DrawFieldBackdrop(Backdrop, 0, FieldBackdropWidth, Quads);
	REQUIRE(Quads.size() == 5);
	CHECK(Quads[0].Kind == EKind::Flat);
	CHECK(Quads[0].Corners[0] == std::array<int16, 2>{0, 235});
	CHECK(Quads[0].Corners[3] == std::array<int16, 2>{320, 240});
	CHECK(Quads[0].Colors[0] == std::array<uint8, 3>{7, 8, 9});
	CHECK(Quads[1].Kind == EKind::Graded);
	CHECK(Quads[2].Kind == EKind::Flat);
	CHECK(Quads[2].Corners[3] == std::array<int16, 2>{320, 6});
	CHECK(Quads[2].Colors[0] == std::array<uint8, 3>{1, 2, 3});
	CHECK(Quads[3].Kind == EKind::Textured);

	// A band ending above the screen leaves the flat band below to fill it all.
	Backdrop.BandY = -10;
	DrawFieldBackdrop(Backdrop, 0, FieldBackdropWidth, Quads);
	REQUIRE(Quads.size() == 4);
	CHECK(Quads[0].Kind == EKind::Flat);
	CHECK(Quads[0].Corners[0] == std::array<int16, 2>{0, 0});
	CHECK(Quads[1].Kind == EKind::Flat);
}

TEST_CASE("A zoomed panorama's strip is centred and shrunk, eight quads at most, and carried on past the screen when asked", "[Field]")
{
	FFieldBackdrop Backdrop = NarrowStrip();
	Backdrop.Zoom = 256;
	Backdrop.Panorama.Scroll = 1;
	std::vector<FFieldBackdropQuad> Quads;
	DrawFieldBackdrop(Backdrop, 0, FieldBackdropWidth, Quads);
	// Eight quads of half size: the first from column 32 to the strip's end, the rest whole; the eighth ends at x 240.
	std::vector<const FFieldBackdropQuad*> Strip;
	for (const FFieldBackdropQuad& Quad : Quads)
	{
		if (Quad.Kind == EKind::Textured)
		{
			Strip.push_back(&Quad);
		}
	}
	REQUIRE(Strip.size() == 8);
	const FFieldBackdropQuad& First = *Strip.back();
	CHECK(First.Corners[0] == std::array<int16, 2>{0, 84});
	CHECK(First.Corners[3] == std::array<int16, 2>{16, 100});
	// Column 32 of a strip whose texels start 5 VRAM units (20 texels) into their page.
	CHECK(First.Texels[0] == std::array<uint8, 2>{52, 0});
	CHECK(First.Texels[3] == std::array<uint8, 2>{83, 32});
	CHECK(First.TexturePage == 0x1F);
	CHECK(Strip.front()->Corners[3] == std::array<int16, 2>{240, 100});

	// Wider, the strip goes on to either side; the screen's own quads stay as they were.
	std::vector<FFieldBackdropQuad> Wide;
	DrawFieldBackdrop(Backdrop, -64, 384, Wide);
	int32 Covered = 0;
	int32 Screen = 0;
	for (const FFieldBackdropQuad& Quad : Wide)
	{
		if (Quad.Kind == EKind::Textured)
		{
			const int32 From = Quad.Corners[0][0];
			const int32 To = Quad.Corners[1][0];
			Covered += To - From;
			Screen += From >= 0 && To <= FieldBackdropWidth ? 1 : 0;
			CHECK(((From >= -64 && To <= 0) || (From >= 0 && To <= 240) || (From >= 320 && To <= 384)));
		}
		else
		{
			CHECK(Quad.Corners[0][0] == -64);
			CHECK(Quad.Corners[1][0] == 384);
		}
	}
	CHECK(Screen == 8);
	CHECK(Covered == 64 + 240 + 64);

	// A strip that does not turn with the view is a picture of the screen: nothing of it past the screen.
	Backdrop.Panorama.Scroll = 0;
	DrawFieldBackdrop(Backdrop, -64, 384, Wide);
	for (const FFieldBackdropQuad& Quad : Wide)
	{
		if (Quad.Kind == EKind::Textured)
		{
			CHECK(Quad.Corners[0][0] >= 0);
			CHECK(Quad.Corners[1][0] <= 240);
		}
	}
}

TEST_CASE("The view puts the panorama's horizon where its point projects, turns its strip with the heading and zooms it with the distance", "[Field]")
{
	const FGteArctangents Table = Arctangents();
	FFieldPanorama Made;
	Made.bEnabled = true;
	Made.Width = 320;
	Made.Height = 224;
	Made.Scroll = 1;
	Made.Position = {0, 200, 1000};
	Made.BandScale = 0x200;
	FFieldView View;
	View.View = FGteMatrix::Identity();
	View.View.T = {0, 0, 1000};
	View.ProjectionDistance = 500;

	// Looking along z: the horizon's point 1000 ahead of the target, 2000 from the eye, at height 200.
	FFieldBackdrop Backdrop = PlaceFieldBackdrop(Made, false, Point(0, 0, -1000), Point(0, 0, 0), View, Table);
	REQUIRE(Backdrop.bShown);
	CHECK(Backdrop.HorizonY == 112 + 200 * 500 / 2000);
	// The band's end: the point twice as far and high.
	CHECK(Backdrop.BandY == 112 + 400 * 500 / 3000);
	CHECK(Backdrop.ScrollU == 0);
	CHECK(Backdrop.Zoom == 0);

	// A quarter turn scrolls a quarter of the strip; a horizon behind the target scrolls the other way.
	CHECK(PlaceFieldBackdrop(Made, false, Point(-1000, 0, 0), Point(0, 0, 0), View, Table).ScrollU == 80);
	Made.Position[2] = -1000;
	CHECK(PlaceFieldBackdrop(Made, false, Point(-1000, 0, 0), Point(0, 0, 0), View, Table).ScrollU == -80);

	// The zoom: the camera's distance past the offset over the divisor, from 0 to 256.
	Made.DepthDivisor = 10;
	Made.DepthOffset = 495;
	CHECK(PlaceFieldBackdrop(Made, false, Point(-1000, 0, 0), Point(0, 0, 0), View, Table).Zoom == 50);
	Made.DepthOffset = 2000;
	CHECK(PlaceFieldBackdrop(Made, false, Point(-1000, 0, 0), Point(0, 0, 0), View, Table).Zoom == 0);
	Made.DepthDivisor = 1;
	Made.DepthOffset = 0;
	CHECK(PlaceFieldBackdrop(Made, false, Point(-1000, 0, 0), Point(0, 0, 0), View, Table).Zoom == 256);

	// None made, or hidden after a movie: nothing drawn.
	CHECK_FALSE(PlaceFieldBackdrop(Made, true, Point(-1000, 0, 0), Point(0, 0, 0), View, Table).bShown);
	Made.bEnabled = false;
	CHECK_FALSE(PlaceFieldBackdrop(Made, false, Point(-1000, 0, 0), Point(0, 0, 0), View, Table).bShown);
	std::vector<FFieldBackdropQuad> Quads{FFieldBackdropQuad{}};
	DrawFieldBackdrop(FFieldBackdrop{}, 0, FieldBackdropWidth, Quads);
	CHECK(Quads.empty());
}
