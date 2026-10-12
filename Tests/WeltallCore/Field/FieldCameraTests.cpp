#include "Field/FieldCamera.h"
#include "Graphics/GteVectors.h"
#include "Support/OracleDigest.h"

#include <catch_amalgamated.hpp>

#include <cmath>

using namespace Weltall;
using Testing::DrawSigned;
using Testing::DrawSignedBits;
using Testing::FOracleDigest;
using Testing::FXorShift32;

// The oracle digests come from the game's camera following an actor, looking at a point and
// building its view matrix, run under the oracle.

namespace
{
	/** A 16.16 point with a whole part of up to 14 bits, as the oracle script draws it. */
	FFieldPoint DrawPoint(FXorShift32& Random)
	{
		FFieldPoint Point{};
		for (int32& Component : Point)
		{
			const int32 Whole = DrawSignedBits(Random, 1, 14);
			Component = static_cast<int32>(static_cast<uint32>(Whole) << 16 | (Random.Next() & 0xFFFF));
		}
		return Point;
	}

	void AddMatrix(FOracleDigest& Digest, const FGteMatrix& Matrix, bool bTranslation = true)
	{
		for (const auto& Row : Matrix.M)
		{
			Digest.I16(Row[0], Row[1], Row[2]);
		}
		if (bTranslation)
		{
			Digest.I32(Matrix.T[0], Matrix.T[1], Matrix.T[2]);
		}
	}

	constexpr int32 Unit = 0x10000;
}

TEST_CASE("The field-start camera sits 1024 units from its target, 30 degrees above it", "[Field]")
{
	const FFieldCamera Camera;
	const FFieldPoint Target = FollowTarget({100 * Unit, 0, -50 * Unit});
	CHECK(Target == FFieldPoint{100 * Unit, -32 * Unit, -50 * Unit});
	// cos 300 degrees = 0.5 of 2H up, sin = -0.866 of 2H along z; the half turn of the field-start
	// yaw puts the eye on the -z side.
	const FFieldPoint Eye = FollowEye(Camera, Target);
	CHECK(Eye == FFieldPoint{100 * Unit, -544 * Unit, -937 * Unit});

	FFieldCamera Turned = Camera;
	Turned.Rotation[1] = 0;
	CHECK(FollowEye(Turned, Target) == FFieldPoint{100 * Unit, -544 * Unit, 837 * Unit});
}

TEST_CASE("The field's look-at turns the view toward the target and scales it by 3", "[Field]")
{
	FFieldCamera Camera;
	Camera.Target = {0, 0, 0};
	Camera.Eye = {0, 0, -1000 * Unit};
	const FFieldView View = BuildFieldView(Camera);
	// Looking along +z: right is +x, down is +y. The library's table normalizes the forward axis to
	// 4095 and down to 4100, not 4096; the view carries them times 3.
	CHECK(View.View.M[2][2] == 3 * 4095);
	CHECK(View.View.M[1][1] == 3 * 4100);
	CHECK((View.View.M[0][1] == 0 && View.View.M[0][2] == 0 && View.View.M[1][0] == 0 && View.View.M[2][0] == 0));
	CHECK(View.View.T == std::array<int32, 3>{0, 0, 3000});
	CHECK(View.ProjectionDistance == 512);
}

TEST_CASE("The field's view unrounded is its look-at of the points as they are, its rows exact", "[Field]")
{
	const auto Through = [](const FAffine& View, const std::array<float, 3>& Point)
	{
		const std::array<float, 3> Turned = View.Turn(Point);
		return std::array<float, 3>{Turned[0] + View.T[0], Turned[1] + View.T[1], Turned[2] + View.T[2]};
	};
	const auto Apart = [](const std::array<float, 3>& A, const std::array<float, 3>& B)
	{
		return std::sqrt((A[0] - B[0]) * (A[0] - B[0]) + (A[1] - B[1]) * (A[1] - B[1]) + (A[2] - B[2]) * (A[2] - B[2]));
	};
	// Whole points looking along +z: the look-at's own but for its table's rows, 4095 and 4100 of 4096.
	FFieldCamera Camera;
	Camera.Target = {0, 0, 0};
	Camera.Eye = {0, 0, -1000 * Unit};
	const FAffine Unrounded = UnroundedFieldView(Camera, Camera.Eye, Camera.Target);
	CHECK(Unrounded.M[2][2] == Catch::Approx(3.0));
	CHECK(Unrounded.M[1][1] == Catch::Approx(3.0));
	CHECK(Unrounded.T[2] == Catch::Approx(3000.0));

	// Anywhere, under a screen's tilt, a point near the target lands within the look-at's rounding of where the game puts it
	// (its table normalizes the rows to a few parts in a thousand, by their direction), and an eye a sixteenth of a unit
	// further on moves the unrounded view.
	FXorShift32 Random(0x5EED);
	const auto Within = [&Random](int32 Reach) { return static_cast<int32>(Random.Below(static_cast<uint32>(2 * Reach + 1))) - Reach; };
	for (int32 Case = 0; Case < 200; ++Case)
	{
		FFieldCamera Drawn;
		Drawn.ScreenAngles = {static_cast<int16>(Within(64)), 0, static_cast<int16>(Within(64))};
		Drawn.Target = {Within(3000) * Unit, Within(300) * Unit, Within(3000) * Unit};
		Drawn.Eye = {Drawn.Target[0] + Within(700) * Unit, Drawn.Target[1] - 500 * Unit, Drawn.Target[2] + 800 * Unit};
		const std::array<float, 3> Near{static_cast<float>(Drawn.Target[0] / Unit + 40), static_cast<float>(Drawn.Target[1] / Unit),
			static_cast<float>(Drawn.Target[2] / Unit - 30)};
		const FAffine Game = FAffine::From(BuildFieldView(Drawn).View);
		INFO("case " << Case);
		const std::array<float, 3> ByGame = Through(Game, Near);
		CHECK(Apart(ByGame, Through(UnroundedFieldView(Drawn, Drawn.Eye, Drawn.Target), Near)) < ByGame[2] / 100.0f);
		FFieldPoint Moved = Drawn.Eye;
		Moved[0] += Unit / 16;
		CHECK(Apart(Through(UnroundedFieldView(Drawn, Moved, Drawn.Target), Near), Through(UnroundedFieldView(Drawn, Drawn.Eye, Drawn.Target), Near)) > 0.0f);
	}
}

TEST_CASE("The follow camera's eye matches the game's", "[Field][Oracle]")
{
	FXorShift32 Random(0xCA3);
	FOracleDigest Digest;
	for (int32 Case = 0; Case < 256; ++Case)
	{
		const FFieldPoint Position = DrawPoint(Random);
		FFieldCamera Camera;
		Camera.Pitch = static_cast<int16>(static_cast<int32>(Random.Next() % 181) - 90);
		Camera.ProjectionDistance = static_cast<int32>(1 + Random.Next() % 1024);
		Camera.DistanceScale = static_cast<int16>(Random.Next() % 0x4000);
		const int16 X = static_cast<int16>(DrawSigned(Random, 12));
		const int16 Y = static_cast<int16>(DrawSigned(Random, 13));
		Camera.Rotation = {X, Y, static_cast<int16>(DrawSigned(Random, 12))};
		const FFieldPoint Target = FollowTarget(Position);
		const FFieldPoint Eye = FollowEye(Camera, Target);
		Digest.I32(Target[0], Target[1], Target[2], Eye[0], Eye[1], Eye[2]);
	}
	CHECK(Digest.Hex() == "4ad48692ac617f8dcc9305b3ccd105b023eea4cc");
}

TEST_CASE("The field's look-at matches the game's", "[Field][Oracle]")
{
	FXorShift32 Random(0x100C);
	FOracleDigest Digest;
	for (int32 Case = 0; Case < 256; ++Case)
	{
		const FFieldPoint Eye = DrawPoint(Random);
		const FFieldPoint Target = DrawPoint(Random);
		AddMatrix(Digest, FieldLookAt(Eye, Target));
	}
	CHECK(Digest.Hex() == "d02561170a9d4a82b4cc35427a16aa8f8b785d14");
}

TEST_CASE("The field's view matrix matches the game's", "[Field][Oracle]")
{
	FXorShift32 Random(0xB1D);
	FOracleDigest Digest;
	for (int32 Case = 0; Case < 128; ++Case)
	{
		FFieldCamera Camera;
		Camera.Eye = DrawPoint(Random);
		Camera.Target = DrawPoint(Random);
		if (Case % 2 != 0)
		{
			const int16 X = static_cast<int16>(DrawSigned(Random, 12));
			const int16 Y = static_cast<int16>(DrawSigned(Random, 12));
			Camera.ScreenAngles = {X, Y, static_cast<int16>(DrawSigned(Random, 12))};
		}
		const FFieldView View = BuildFieldView(Camera);
		AddMatrix(Digest, View.View);
		AddMatrix(Digest, CompMatrix(View.ScreenRotation, FieldLookAt(Camera.Eye, Camera.Target)));
		AddMatrix(Digest, View.ScreenRotation, false);
	}
	CHECK(Digest.Hex() == "94955736f603df5841afd44db59d7fde3810354e");
}

TEST_CASE("The field's horizontal length matches the game's", "[Field][Oracle]")
{
	FXorShift32 Random(0x1E2);
	FOracleDigest Digest;
	for (int32 Case = 0; Case < 256; ++Case)
	{
		const int32 X = DrawSignedBits(Random, 1, 15);
		Digest.I32(FieldLength2D(X, DrawSignedBits(Random, 1, 15)));
	}
	CHECK(Digest.Hex() == "1c0b79f081e8434728e3051c3a980e531d58751e");
}
