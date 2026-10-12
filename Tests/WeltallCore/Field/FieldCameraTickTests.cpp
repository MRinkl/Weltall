#include "Field/EventContext.h"
#include "Field/FieldActorFacing.h"
#include "Field/FieldCameraFloor.h"
#include "Field/FieldCameraUpdate.h"
#include "Field/FieldWalkmesh.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteMath.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	constexpr int32 Unit = 0x10000;

	/**
	 * A camera layer over the square (0, 0)-(100, 100): the triangle where x >= z open to the camera,
	 * the other closed, sharing the diagonal.
	 */
	FFieldWalkmesh HalfOpenSquare()
	{
		FFieldWalkmesh Walkmesh;
		FWalkmeshLayer Layer;
		Layer.Vertices = {{0, 0, 0}, {100, 0, 100}, {100, 0, 0}, {0, 0, 100}};
		FWalkmeshTriangle Open;
		Open.Vertices = {0, 1, 2};
		Open.Neighbours = {1, -1, -1};
		Open.Attribute = 0;
		FWalkmeshTriangle Closed;
		Closed.Vertices = {0, 3, 1};
		Closed.Neighbours = {-1, -1, 0};
		Closed.Attribute = 1;
		Layer.Triangles = {Open, Closed};
		Walkmesh.Layers = {Layer};
		Walkmesh.Attributes = {0x800000, 0};
		return Walkmesh;
	}

	/** Bounds that start the way at one point. */
	std::array<int16, 4> StartAt(int16 X, int16 Z)
	{
		return {X, Z, 0, 0};
	}
}

TEST_CASE("The camera eases a divisor's share toward its goal, but not within the divisor's whole units", "[Field]")
{
	FFieldCamera Camera;
	Camera.SnapTicks = 0;
	FPsxRandom Random;
	Camera.Eye = {0, 0, 0};
	Camera.EyeGoal = {100 * Unit, 8 * Unit, 7 * Unit};
	Camera.Target = {0, 0, 0};
	Camera.TargetGoal = {Unit / 2, -40 * Unit, 0};
	SmoothFieldCamera(Camera, Random);
	// 100 / 8 and 8 / 8 of the way; 7 whole units are within the divisor 8, half a unit within the same whole unit.
	CHECK(Camera.Eye == FFieldPoint{100 * Unit / 8, Unit, 0});
	CHECK(Camera.Target == FFieldPoint{0, -5 * Unit, 0});

	// A snap sets both divisors to 1 for its ticks: the camera lands on its goals.
	Camera.SnapTicks = 1;
	SmoothFieldCamera(Camera, Random);
	CHECK(Camera.Eye == Camera.EyeGoal);
	CHECK(Camera.TargetDivisor == 1);
	CHECK(Camera.SnapTicks == 0);
}

TEST_CASE("The camera turns out of a forbidden direction toward the side with fewer forbidden steps", "[Field]")
{
	FFieldCamera Camera;
	// Directions 4 and 5 are forbidden above the camera's 4, only 4 below it.
	Camera.ForbiddenDirections = FieldCameraDirectionBit(4) | FieldCameraDirectionBit(5);
	CHECK(CountFieldCameraMaskedSteps(Camera.ForbiddenDirections, 4, 1) == 2);
	CHECK(CountFieldCameraMaskedSteps(Camera.ForbiddenDirections, 4, -1) == 1);
	CHECK(CountFieldCameraMaskedSteps(0xFF, 4, 1) == 0);
	UpdateFieldCameraYaw(Camera, 0);
	CHECK(Camera.YawGoal == 0x600);
	CHECK(Camera.Rotation[1] == 0x800 - 0x40);
	for (int32 Tick = 1; Tick < 8; ++Tick)
	{
		UpdateFieldCameraYaw(Camera, 0);
	}
	CHECK(Camera.Rotation[1] == 0x600);
	CHECK(Camera.TurnTicks == 0);
}

TEST_CASE("L1 and R1 turn the camera 45 degrees over 8 ticks, never into a forbidden direction", "[Field]")
{
	FFieldCamera Camera;
	for (int32 Tick = 0; Tick < 8; ++Tick)
	{
		// Held throughout: a new turn waits for the one under way.
		UpdateFieldCameraYaw(Camera, FieldCameraButtons::TurnRight);
		CHECK(Camera.Rotation[1] == (Tick < 7 ? 0x800 + 0x40 * (Tick + 1) : 0xA00));
	}
	CHECK(Camera.YawGoal == 0xA00);

	FFieldCamera Blocked;
	Blocked.ForbiddenDirections = FieldCameraDirectionBit(3);
	UpdateFieldCameraYaw(Blocked, FieldCameraButtons::TurnLeft);
	CHECK(Blocked.TurnTicks == 0);
	CHECK(Blocked.Rotation[1] == 0x800);

	FFieldCamera Locked;
	Locked.Flags = FieldCameraFlags::NoButtonTurn;
	UpdateFieldCameraYaw(Locked, FieldCameraButtons::TurnLeft);
	CHECK(Locked.TurnTicks == 0);

	// All eight directions left or forbidden stop every turn, the camera's own as well.
	FFieldCamera Still;
	Still.LeftDirections = 0xFF;
	UpdateFieldCameraYaw(Still, FieldCameraButtons::TurnLeft | FieldCameraButtons::TurnRight);
	CHECK(Still.TurnTicks == 0);
	CHECK(Still.Rotation[1] == 0x800);
}

TEST_CASE("Angles turn the shorter way by their speed and land on their target", "[Field]")
{
	CHECK(StepFieldAngle(0, 0x100, 0x40) == 0x40);
	CHECK(StepFieldAngle(0xFF0, 0x100, 0x40) == 0x30);
	CHECK(StepFieldAngle(0x100, 0xFF0, 0x40) == 0xC0);
	CHECK(StepFieldAngle(0xF0, 0x100, 0x40) == 0x100);
	// Half a turn away, an angle turns upward; the target's upper bits do not count.
	CHECK(StepFieldAngle(0, 0x800, 0x40) == 0x40);
	CHECK(StepFieldAngle(0x7F0, 0x8800, 0x40) == 0x800);
}

TEST_CASE("The camera's way into its area stops at the edge of a triangle closed to it", "[Field]")
{
	const FFieldWalkmesh Walkmesh = HalfOpenSquare();
	CHECK(FindFieldCameraAreaPoint(Walkmesh, StartAt(60, 20), 80, 10).bInside);

	// From (60, 20) to (20, 60), the way crosses the diagonal into the closed triangle: the target
	// goes back along it to about (40, 40), as the library's normalized directions put it.
	const FFieldCameraAreaPoint Clamped = FindFieldCameraAreaPoint(Walkmesh, StartAt(60, 20), 20, 60);
	CHECK_FALSE(Clamped.bInside);
	CHECK((Clamped.X == 39 && Clamped.Z == 40));

	// Starting in the closed triangle itself, the original's way ends without an edge; the port keeps the point.
	CHECK(FindFieldCameraAreaPoint(Walkmesh, StartAt(20, 60), 30, 70).bInside);
}

TEST_CASE("Each frame is drawn with the view of the tick before, once the map has settled", "[Field]")
{
	FFieldWalkmesh Walkmesh = HalfOpenSquare();
	FGteArctangents Arctangents;
	FPsxRandom Random;
	FEventContext Context;
	Context.Walkmesh = &Walkmesh;
	Context.Arctangents = &Arctangents;
	Context.Random = &Random;
	FFieldCamera& Camera = Context.Camera;
	// A mode that neither follows nor moves along a path: only the easing runs.
	Camera.Mode = 3;
	Camera.Eye = {0, -100 * Unit, -300 * Unit};
	Camera.EyeGoal = Camera.Eye;
	Camera.Target = {0, 0, 0};
	Camera.TargetGoal = {0, 0, 50 * Unit};
	Camera.SnapTicks = 0;
	Camera.TargetDivisor = 1;

	Context.SettleTicks = 1;
	REQUIRE(UpdateFieldCamera(Context));
	const FGteMatrix First = FieldLookAt(Camera.Eye, Camera.Target);
	CHECK(Camera.LookAt.M == First.M);
	CHECK(Camera.View.View.M == BuildFieldView(Camera, First).View.M);

	Camera.TargetGoal = {0, 0, 80 * Unit};
	Context.SettleTicks = 0;
	REQUIRE(UpdateFieldCamera(Context));
	CHECK(Camera.LookAt.M == FieldLookAt(Camera.Eye, Camera.Target).M);
	CHECK(Camera.View.View.M == BuildFieldView(Camera, First).View.M);
}

TEST_CASE("Actors turn toward their facing at their speed, at once while the map settles", "[Field]")
{
	FEventContext Context;
	Context.Actors.resize(3);
	Context.States.resize(3);
	for (std::size_t Actor = 0; Actor < 3; ++Actor)
	{
		Context.Actors[Actor].Flags = FieldActorFlag::NoModel;
		Context.States[Actor].Facing = 0x400;
		Context.States[Actor].TurnSpeed = 0x100;
	}
	Context.States[1].Flags0 = EventActorFlags0::FacingLocked;
	Context.States[2].Flags1 = EventActorFlags1::Removed;
	TurnFieldActors(Context);
	CHECK(Context.States[0].Yaw == 0x100);
	CHECK(Context.States[1].Yaw == 0);
	CHECK(Context.States[2].Yaw == 0);

	Context.SettleTicks = 1;
	TurnFieldActors(Context);
	CHECK(Context.States[0].Yaw == 0x400);
}
