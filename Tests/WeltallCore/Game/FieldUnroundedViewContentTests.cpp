#include "Field/FieldPlayerCamera.h"
#include "Field/FieldRuntime.h"
#include "Game/FieldProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace Weltall;

// The field's view without the whole steps of the game's look-at, as a display draws it.

namespace
{
	using FPoint = std::array<float, 3>;

	FPoint Through(const FAffine& View, const FPoint& Point)
	{
		const FPoint Turned = View.Turn(Point);
		return {Turned[0] + View.T[0], Turned[1] + View.T[1], Turned[2] + View.T[2]};
	}

	/** Where a view point lands on the original's screen about its centre. */
	std::array<float, 2> OnScreen(const FPoint& Point, float Distance)
	{
		return {Point[0] * Distance / Point[2], Point[1] * Distance / Point[2]};
	}

	/** Where a view's eye stands in the world: what it takes to its origin. */
	FPoint EyeOf(const FAffine& View)
	{
		FAffine Back;
		REQUIRE(View.Invert(Back));
		return Back.T;
	}

	/** The steps along a track that turn back against the step before. */
	int32 TurnsBack(const std::vector<std::array<float, 2>>& Track)
	{
		int32 Turns = 0;
		for (std::size_t At = 2; At < Track.size(); ++At)
		{
			for (std::size_t Axis = 0; Axis < 2; ++Axis)
			{
				const float First = Track[At - 1][Axis] - Track[At - 2][Axis];
				const float Second = Track[At][Axis] - Track[At - 1][Axis];
				Turns += First * Second < 0.0f && std::abs(First) > 0.01f && std::abs(Second) > 0.01f ? 1 : 0;
			}
		}
		return Turns;
	}
}

TEST_CASE("As map 3's camera pans to Dan the view unrounded and averaged moves on, where the game's look-at shook back and forth", "[Game][Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->EnterField(3);
	// The scene's messages go on with Cross held each other quarter of a second; the camera pans over the villagers to Dan
	// from 63 seconds in.
	const FPadState Cross{FPadState::Bit(EPadButton::Cross)};
	const uint32 Start = Game->Session->Game().PlayTime();
	constexpr uint32 PanStart = 63 * 60;
	constexpr uint32 PanEnd = 68 * 60 + 30;
	std::vector<std::array<float, 2>> Drawn;
	std::vector<std::array<float, 2>> Unrounded;
	std::vector<FPoint> GameEyes;
	FPoint Watched{};
	uint32 Ticked = 0xFFFFFFFF;
	float FarthestApart = 0.0f;
	for (uint32 Vblanks = 0; Vblanks < PanEnd; Vblanks = Game->Session->Game().PlayTime() - Start)
	{
		REQUIRE_FALSE(Game->Frame(Vblanks / 15 % 2 == 0 ? Cross : FPadState{}));
		const FFieldProgram* Field = Game->Loop->Field();
		const FFieldFrame* Latest = Field != nullptr ? Field->LatestFrame() : nullptr;
		if (Vblanks < PanStart || Latest == nullptr || Latest->Tick == Ticked)
		{
			continue;
		}
		Ticked = Latest->Tick;
		REQUIRE(Latest->bUnrounded);
		if (Drawn.empty())
		{
			// A point on the ground the camera pans toward.
			Watched = {static_cast<float>(Latest->Target[0] + 40), static_cast<float>(Latest->Target[1] + 32), static_cast<float>(Latest->Target[2] + 30)};
		}
		const FAffine GameView = FAffine::From(Latest->View.View);
		const FPoint ByGame = Through(GameView, Watched);
		const FPoint ByUnrounded = Through(Latest->UnroundedView, Watched);
		INFO("vblank " << Vblanks);
		// The eye of a script's shot is the game's averaged over its last few ticks: among where the game's look-at stood
		// then, at its whole parts.
		GameEyes.push_back(EyeOf(GameView));
		const FPoint UnroundedEye = EyeOf(Latest->UnroundedView);
		const std::size_t Kept = std::min<std::size_t>(GameEyes.size(), FFieldPlayerCamera::SmoothedTicks);
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			float Low = GameEyes.back()[Axis];
			float High = Low;
			for (std::size_t Back = 1; Back <= Kept; ++Back)
			{
				Low = std::min(Low, GameEyes[GameEyes.size() - Back][Axis]);
				High = std::max(High, GameEyes[GameEyes.size() - Back][Axis]);
			}
			REQUIRE(UnroundedEye[Axis] - Low > -0.5f);
			REQUIRE(UnroundedEye[Axis] - High < 1.5f);
		}
		const float Apart = std::sqrt((ByGame[0] - ByUnrounded[0]) * (ByGame[0] - ByUnrounded[0]) + (ByGame[1] - ByUnrounded[1]) * (ByGame[1] - ByUnrounded[1])
			+ (ByGame[2] - ByUnrounded[2]) * (ByGame[2] - ByUnrounded[2]));
		FarthestApart = std::max(FarthestApart, Apart / ByGame[2]);
		const float Distance = Latest->View.ProjectionDistance;
		Drawn.push_back(OnScreen(ByGame, Distance));
		Unrounded.push_back(OnScreen(ByUnrounded, Distance));
	}
	REQUIRE(Drawn.size() > 100);
	// The same view but for the look-at's rounding and its table's rows, which it normalizes to about a part in a hundred.
	CHECK(FarthestApart < 0.02f);
	// How far the point stands from where the game drew it in the ticks the view averages, beyond them.
	float OnScreenApart = 0.0f;
	for (std::size_t At = 0; At < Drawn.size(); ++At)
	{
		const std::size_t First = At + 1 >= FFieldPlayerCamera::SmoothedTicks ? At + 1 - FFieldPlayerCamera::SmoothedTicks : 0;
		for (std::size_t Axis = 0; Axis < 2; ++Axis)
		{
			float Low = Drawn[At][Axis];
			float High = Low;
			for (std::size_t Back = First; Back <= At; ++Back)
			{
				Low = std::min(Low, Drawn[Back][Axis]);
				High = std::max(High, Drawn[Back][Axis]);
			}
			OnScreenApart = std::max({OnScreenApart, Low - Unrounded[At][Axis], Unrounded[At][Axis] - High});
		}
	}
	INFO("as drawn " << TurnsBack(Drawn) << ", unrounded " << TurnsBack(Unrounded) << ", apart " << FarthestApart << ", on screen " << OnScreenApart);
	CHECK(TurnsBack(Unrounded) * 3 < TurnsBack(Drawn));
	// Nothing stands a pixel of the original's screen beyond where the game drew it in those ticks.
	CHECK(OnScreenApart < 1.0f);
}

TEST_CASE("As the camera circles the Kaiser's organ the view shown moves on evenly, where the game's camera steps a unit every other tick", "[Game][Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	// After the continue prompt at the Aveh border: the camera circles the organ from 2 to 22 seconds in, easing toward its
	// path by twelfths and so standing still while it lies less than twelve units behind.
	constexpr uint16 OrganRoom = 237;
	constexpr uint16 KaiserAtTheOrgan = 96;
	Game->Session->Game().SetSavedEventVariable(0, KaiserAtTheOrgan);
	Game->EnterField(OrganRoom);
	constexpr int32 PanStart = 2 * 60;
	constexpr int32 PanEnd = 22 * 60;
	const FPoint Console = {-272.0f, -66.0f, 0.0f};
	std::vector<std::array<float, 2>> Shown;
	std::vector<std::array<float, 2>> Stepped;
	uint32 Ticked = 0xFFFFFFFF;
	while (Game->Vblanks < PanEnd)
	{
		REQUIRE_FALSE(Game->Frame());
		const FFieldProgram* Field = Game->Loop->Field();
		const FFieldFrame* Latest = Field != nullptr ? Field->LatestFrame() : nullptr;
		if (Game->Vblanks < PanStart || Latest == nullptr || Latest->Tick == Ticked)
		{
			continue;
		}
		Ticked = Latest->Tick;
		const FFieldCamera& Camera = Field->Runtime().Context().Camera;
		const float Distance = Latest->View.ProjectionDistance;
		Shown.push_back(OnScreen(Through(Latest->UnroundedView, Console), Distance));
		Stepped.push_back(OnScreen(Through(UnroundedFieldView(Camera, Camera.Eye, Camera.Target), Console), Distance));
	}
	REQUIRE(Shown.size() > 400);
	INFO("game's camera " << TurnsBack(Stepped) << ", shown " << TurnsBack(Shown));
	CHECK(TurnsBack(Stepped) > 20);
	CHECK(TurnsBack(Shown) * 4 < TurnsBack(Stepped));
}
