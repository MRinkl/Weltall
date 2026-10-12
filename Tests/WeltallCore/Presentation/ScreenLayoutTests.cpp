#include "Presentation/ScreenLayout.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	constexpr FScreenSize Screen{320, 240};
	constexpr FScreenRect WholeScreen{0, 0, 320, 240};
	constexpr FScreenRect Inset{16, 16, 288, 120};
}

TEST_CASE("The game screen fills a 4:3 display", "[Presentation]")
{
	const FScreenLayout Layout = FScreenLayout::Fit(1600, 1200, Screen);
	CHECK(Layout.Scale() == 5.0);
	CHECK(Layout.ToDisplay(WholeScreen) == FDisplayRect{0, 0, 1600, 1200});
	CHECK(Layout.ToDisplay(Inset) == FDisplayRect{80, 80, 1440, 600});
}

TEST_CASE("A wide display shows the game screen between bars", "[Presentation]")
{
	const FScreenLayout FullHd = FScreenLayout::Fit(1920, 1080, Screen);
	CHECK(FullHd.Scale() == 4.5);
	CHECK(FullHd.ToDisplay(WholeScreen) == FDisplayRect{240, 0, 1440, 1080});
	CHECK(FullHd.ToDisplay(Inset) == FDisplayRect{312, 72, 1296, 540});

	// A handheld's 16:10 screen: a fractional scale, edges on the nearest pixel.
	const FScreenLayout Handheld = FScreenLayout::Fit(1280, 800, Screen);
	CHECK(Handheld.ToDisplay(WholeScreen) == FDisplayRect{107, 0, 1066, 800});
	CHECK(Handheld.ToDisplay(Inset) == FDisplayRect{160, 53, 960, 400});
}

TEST_CASE("A tall display shows the game screen between bars above and below", "[Presentation]")
{
	const FScreenLayout Layout = FScreenLayout::Fit(800, 1280, Screen);
	CHECK(Layout.Scale() == 2.5);
	CHECK(Layout.ToDisplay(WholeScreen) == FDisplayRect{0, 340, 800, 600});
}

TEST_CASE("Neighbouring rectangles share their edge whatever the scale", "[Presentation]")
{
	const FScreenLayout Layout = FScreenLayout::Fit(1111, 777, Screen);
	for (int16 Split = 1; Split < 240; ++Split)
	{
		const FDisplayRect Upper = Layout.ToDisplay({0, 0, 320, Split});
		const FDisplayRect Lower = Layout.ToDisplay({0, Split, 320, static_cast<int16>(240 - Split)});
		REQUIRE(Upper.Y + Upper.Height == Lower.Y);
	}
}

TEST_CASE("An empty display shows nothing", "[Presentation]")
{
	const FScreenLayout Layout = FScreenLayout::Fit(0, 720, Screen);
	CHECK(Layout.Scale() == 0.0);
	CHECK(Layout.ToDisplay(Inset).Width == 0);
}

TEST_CASE("Any screen size fits, such as a taller picture than 240 lines", "[Presentation]")
{
	const FScreenLayout Layout = FScreenLayout::Fit(1920, 1080, {320, 256});
	CHECK(Layout.Scale() == Catch::Approx(1080.0 / 256.0));
	CHECK(Layout.ToDisplay({0, 0, 320, 256}) == FDisplayRect{285, 0, 1350, 1080});
	CHECK(FScreenLayout::Fit(1920, 1080, {}).Scale() == 0.0);
}

TEST_CASE("The picture takes the window's shape, or the largest 16:9 or 4:3 in its middle", "[Presentation]")
{
	CHECK(FitAspect(3840, 1080, EDisplayAspect::Window) == FDisplayRect{0, 0, 3840, 1080});
	CHECK(FitAspect(3840, 1080, EDisplayAspect::Wide) == FDisplayRect{960, 0, 1920, 1080});
	CHECK(FitAspect(3840, 1080, EDisplayAspect::Standard) == FDisplayRect{1200, 0, 1440, 1080});
	CHECK(FitAspect(1920, 1080, EDisplayAspect::Wide) == FDisplayRect{0, 0, 1920, 1080});
	// A window taller than the shape has its bars above and below.
	CHECK(FitAspect(1280, 1024, EDisplayAspect::Wide) == FDisplayRect{0, 152, 1280, 720});
	CHECK(FitAspect(1080, 1920, EDisplayAspect::Standard) == FDisplayRect{0, 555, 1080, 810});
	CHECK(FitAspect(0, 720, EDisplayAspect::Wide) == FDisplayRect{0, 0, 0, 720});
}
