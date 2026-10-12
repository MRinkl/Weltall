#include "Menu/MenuPlay.h"
#include "Gears/GearShopModel.h"
#include "Media/PsxExecutable.h"
#include "Menu/MenuGearView.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

// The menu opened on its own from a disc, as the renderer's screen and the shell open it.
namespace
{
	FPadState Holding(EPadButton Button)
	{
		return {FPadState::Bit(Button)};
	}

	/** Whether the menu drew its frame and goes on; a failure fails the test. */
	bool Drawn(const TResult<bool>& Frame)
	{
		if (!Frame)
		{
			FAIL(Frame.Error().Message);
		}
		return *Frame;
	}
}

TEST_CASE("The title opened from a disc starts a new game on New Game, and the menu ends", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	auto Play = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, {EMenuMode::LoadGame, 0, false}, nullptr);
	REQUIRE(Play);
	FMenuPlay& Menu = **Play;
	// The layer flies in and the arc reveals itself; then Up, and Cross (the game's Confirm on the US discs).
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		REQUIRE(Drawn(Menu.Frame({})));
	}
	CHECK_FALSE(Menu.Menu().Snapshot().Primitives.empty());
	// The title's arc from the bottom: Sound, Continue (where it starts), New Game.
	REQUIRE(Drawn(Menu.Frame(Holding(EPadButton::Up))));
	REQUIRE(Drawn(Menu.Frame({})));
	REQUIRE(Drawn(Menu.Frame(Holding(EPadButton::Cross))));
	bool bEnded = false;
	for (int32 Frame = 0; Frame < 200 && !bEnded; ++Frame)
	{
		bEnded = !Drawn(Menu.Frame({}));
	}
	REQUIRE(bEnded);
	CHECK(Menu.Menu().HasEnded());
	CHECK_FALSE(Menu.Menu().EndedBySoftReset());
	CHECK(Menu.Game().ProgramResult == 0);
	CHECK(Menu.Game().MenuLastCommand == 6);
}

TEST_CASE("With a way to close the game the title's arc has Exit at its foot, which closes it and leaves the title as it stood", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	int32 Quits = 0;
	auto Play = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, {EMenuMode::LoadGame, 0, false}, nullptr, nullptr, nullptr, [&Quits] { ++Quits; });
	REQUIRE(Play);
	FMenuPlay& Menu = **Play;
	const auto Press = [&Menu](EPadButton Button)
	{
		REQUIRE(Drawn(Menu.Frame(Holding(Button))));
		for (int32 Frame = 0; Frame < 8; ++Frame)
		{
			REQUIRE(Drawn(Menu.Frame({})));
		}
	};
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		REQUIRE(Drawn(Menu.Frame({})));
	}
	// From the bottom: Exit, Sound, Continue (where it starts), New Game. Down twice from Continue, and Cross.
	Press(EPadButton::Down);
	Press(EPadButton::Down);
	CHECK(Quits == 0);
	Press(EPadButton::Cross);
	CHECK(Quits == 1);
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		REQUIRE(Drawn(Menu.Frame({})));
	}
	CHECK(Quits == 1);
	// Down from the foot goes round to New Game at the top, which starts a new game.
	Press(EPadButton::Down);
	REQUIRE(Drawn(Menu.Frame(Holding(EPadButton::Cross))));
	bool bEnded = false;
	for (int32 Frame = 0; Frame < 200 && !bEnded; ++Frame)
	{
		bEnded = !Drawn(Menu.Frame({}));
	}
	REQUIRE(bEnded);
	CHECK(Menu.Game().ProgramResult == 0);
	CHECK(Quits == 1);
}

TEST_CASE("The gear shop's gear is drawn as the field's gears are, under the shop's camera", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	auto Play = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, {EMenuMode::GearShop, 9, true}, nullptr);
	REQUIRE(Play);
	FMenuPlay& Menu = **Play;
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		REQUIRE(Drawn(Menu.Frame({})));
	}
	const FGearShopModel* Model = Menu.Menu().GearModel();
	REQUIRE(Model != nullptr);
	const FMenuGearView& Gear = Menu.Menu().Snapshot().Gear;
	CHECK(Gear.bShown);
	// Resting: turned a quarter about y, 400h away.
	CHECK(Gear.Camera.M[0][2] == 0x1000);
	CHECK(Gear.Camera.T == std::array<int32, 3>{0, 0, 0x400});
	FFieldFrame Frame;
	REQUIRE_FALSE(EvaluateGearShopFrame(*Model, Gear, 30, EFieldCulling::InRange, Frame));
	CHECK(Frame.bSceneDrawn);
	// The renderer projects the frame with the shop's own camera and distance (H 200h).
	CHECK(Frame.View.ProjectionDistance == 0x200);
	CHECK(Frame.View.View.T == Gear.Camera.T);
	CHECK(Frame.View.Scene.M == Gear.Camera.M);
	REQUIRE_FALSE(Frame.Order.empty());
	for (std::size_t Item = 1; Item < Frame.Order.size(); ++Item)
	{
		CHECK(Frame.Order[Item].Kind == EFieldDrawKind::Gear);
		const auto BucketOf = [&Frame](const FFieldDrawItem& Drawn)
		{
			return Drawn.Gear == EFieldGearDraw::Cloth ? Frame.Gears.Cloths[Drawn.Index].Bucket : Frame.Gears.Polygons[Drawn.Index].Bucket;
		};
		CHECK(BucketOf(Frame.Order[Item - 1]) >= BucketOf(Frame.Order[Item]));
	}
	FMenuGearView Hidden = Gear;
	Hidden.bShown = false;
	REQUIRE_FALSE(EvaluateGearShopFrame(*Model, Hidden, 31, EFieldCulling::InRange, Frame));
	CHECK(Frame.Order.empty());
}
