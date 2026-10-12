#include "Graphics/GteVectors.h"
#include "Graphics/MdlLights.h"
#include "Support/OracleDigest.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using Testing::DrawSigned;
using Testing::DrawSignedBits;
using Testing::FOracleDigest;
using Testing::FXorShift32;

// Digests of what the game's own copies of libgte's routines, and the kernel's rotation, triangle
// normal and light routines, compute from inputs drawn by the same generator, run under the oracle
// harness.

namespace
{
	FGteLongVector DrawVector(FXorShift32& Random, int32 Bits)
	{
		const int32 X = DrawSigned(Random, Bits);
		const int32 Y = DrawSigned(Random, Bits);
		return {X, Y, DrawSigned(Random, Bits)};
	}

	FGteVector DrawShortVector(FXorShift32& Random, int32 Bits)
	{
		const FGteLongVector Vector = DrawVector(Random, Bits);
		return {static_cast<int16>(Vector[0]), static_cast<int16>(Vector[1]), static_cast<int16>(Vector[2])};
	}

	FGteMatrix DrawMatrix(FXorShift32& Random, int32 Bits = 16)
	{
		FGteMatrix Matrix;
		for (auto& Row : Matrix.M)
		{
			for (int16& Entry : Row)
			{
				Entry = static_cast<int16>(DrawSigned(Random, Bits));
			}
		}
		return Matrix;
	}

	void AddRotation(FOracleDigest& Digest, const FGteMatrix& Matrix)
	{
		for (const auto& Row : Matrix.M)
		{
			Digest.I16(Row[0], Row[1], Row[2]);
		}
	}

	void AddVector(FOracleDigest& Digest, const FGteLongVector& Vector)
	{
		Digest.I32(Vector[0], Vector[1], Vector[2]);
	}

	uint32 PackColor(const FGteColor& Color)
	{
		return Color.R | Color.G << 8 | Color.B << 16 | static_cast<uint32>(Color.Code) << 24;
	}
}

TEST_CASE("VectorNormal and VectorNormalS normalize as the game's libgte does", "[Graphics][Oracle]")
{
	FXorShift32 Random(0x5EC7);
	FOracleDigest Digest;
	for (int32 Case = 0; Case < 512; ++Case)
	{
		const int32 Bits = 1 + static_cast<int32>(Random.Next() % 15);
		const FGteLongVector Vector = DrawVector(Random, Bits);
		AddVector(Digest, VectorNormal(Vector));
		const FGteVector Short = VectorNormalS(Vector);
		Digest.I16(Short[0], Short[1], Short[2]);
	}
	CHECK(Digest.Hex() == "a63f9cf62b8e9a7226689117ba5e4347b0986bc4");
}

TEST_CASE("SquareRoot0 takes the root from the game's libgte table", "[Graphics][Oracle]")
{
	FXorShift32 Random(0x5A1);
	FOracleDigest Digest;
	for (int32 Case = 0; Case < 512; ++Case)
	{
		const uint32 Shift = Random.Next() % 31;
		Digest.I32(SquareRoot0(static_cast<int32>((Random.Next() & 0x7FFFFFFF) >> Shift)));
	}
	CHECK(Digest.Hex() == "c481977f1b8c405260d26d63e75e6de083f87b62");
}

TEST_CASE("OuterProduct12 and OuterProduct0 cross 16-bit components as the game's libgte does", "[Graphics][Oracle]")
{
	FXorShift32 Random(0x0B7);
	FOracleDigest Digest;
	for (int32 Case = 0; Case < 256; ++Case)
	{
		const int32 Bits = 1 + static_cast<int32>(Random.Next() % 16);
		const FGteLongVector A = DrawVector(Random, Bits);
		const FGteLongVector B = DrawVector(Random, Bits);
		AddVector(Digest, OuterProduct12(A, B));
		AddVector(Digest, OuterProduct0(A, B));
	}
	CHECK(Digest.Hex() == "27584e77745934b9f9290fc215385cb7a7e9b88f");
}

TEST_CASE("libgte's matrix routines match the game's copies", "[Graphics][Oracle]")
{
	FXorShift32 Random(0xA11);
	FOracleDigest Digest;
	for (int32 Case = 0; Case < 256; ++Case)
	{
		FGteMatrix First = DrawMatrix(Random);
		First.T = DrawVector(Random, 30);
		FGteMatrix Second = DrawMatrix(Random);
		Second.T = DrawVector(Random, 30);
		const FGteVector Short = DrawShortVector(Random, 16);
		const FGteLongVector Long = {DrawSignedBits(Random, 1, 31), DrawSignedBits(Random, 1, 31), DrawSignedBits(Random, 1, 31)};
		const std::array<int32, 3> Scale = {DrawSignedBits(Random, 1, 20), DrawSignedBits(Random, 1, 20), DrawSignedBits(Random, 1, 20)};
		AddVector(Digest, ApplyMatrix(First, Short));
		AddVector(Digest, ApplyMatrixLV(First, Long));
		AddRotation(Digest, MulMatrix(First, Second));
		const FGteMatrix Composed = CompMatrix(First, Second);
		AddRotation(Digest, Composed);
		AddVector(Digest, Composed.T);
		AddRotation(Digest, ScaleColumns(First, Scale));
		uint32 Flag = 0;
		AddVector(Digest, RotTrans(First, Short, &Flag));
		Digest.I32(Flag);
	}
	CHECK(Digest.Hex() == "3240c0d30b21bdc6fcb4f04ca63fa3dc8e69cf19");
}

TEST_CASE("RotTransPers projects as the game's libgte does", "[Graphics][Oracle]")
{
	FXorShift32 Random(0x9E5);
	FOracleDigest Digest;
	for (int32 Case = 0; Case < 256; ++Case)
	{
		FGte Gte;
		Gte.Transform = DrawMatrix(Random);
		Gte.Transform.T = {DrawSignedBits(Random, 1, 20), DrawSignedBits(Random, 1, 20), DrawSignedBits(Random, 1, 20)};
		Gte.ProjectionDistance = static_cast<uint16>(1 + Random.Next() % 1023);
		Gte.OffsetX = DrawSigned(Random, 16) * 0x10000;
		Gte.OffsetY = DrawSigned(Random, 16) * 0x10000;
		Gte.DepthCueA = static_cast<int16>(DrawSigned(Random, 16));
		Gte.DepthCueB = DrawSigned(Random, 28);
		const FGteProjection Projection = Gte.Rtps(DrawShortVector(Random, 12));
		Digest.I32(static_cast<uint16>(Projection.ScreenX) | static_cast<uint32>(static_cast<uint16>(Projection.ScreenY)) << 16);
		Digest.I32(Projection.DepthCue, Projection.Flag, Projection.ScreenZ >> 2);
	}
	CHECK(Digest.Hex() == "44a4299ef41f2319a06a48f3898a43fac2963e88");
}

TEST_CASE("The normal-color commands light as the game's libgte wrappers do", "[Graphics][Oracle]")
{
	FXorShift32 Random(0x1C0);
	FOracleDigest Digest;
	for (int32 Case = 0; Case < 256; ++Case)
	{
		FGte Gte;
		Gte.LightMatrix = DrawMatrix(Random, 14);
		Gte.ColorMatrix = DrawMatrix(Random, 14);
		Gte.BackColor = {DrawSignedBits(Random, 1, 14), DrawSignedBits(Random, 1, 14), DrawSignedBits(Random, 1, 14)};
		std::array<FGteVector, 3> Normals{};
		for (FGteVector& Normal : Normals)
		{
			Normal = DrawShortVector(Random, 13);
		}
		const uint32 Word = Random.Next();
		const FGteColor Color = {static_cast<uint8>(Word), static_cast<uint8>(Word >> 8), static_cast<uint8>(Word >> 16), static_cast<uint8>(Word >> 24)};
		Digest.I32(PackColor(Gte.NormalColor(Normals[0])));
		Digest.I32(PackColor(Gte.NormalColorColor(Normals[0], Color)));
		for (const FGteVector& Normal : Normals)
		{
			Digest.I32(PackColor(Gte.NormalColor(Normal)));
		}
		for (const FGteVector& Normal : Normals)
		{
			Digest.I32(PackColor(Gte.NormalColorColor(Normal, Color)));
		}
	}
	CHECK(Digest.Hex() == "d16fc5d3f717f7aeaf2b450bb7e416294d67e0e4");
}

TEST_CASE("The kernel's light routines load the GTE as the game does", "[Graphics][Oracle]")
{
	FXorShift32 Random(0x716);
	FOracleDigest Digest;
	FMdlLights Lights;
	for (int32 Case = 0; Case < 64; ++Case)
	{
		for (int32 Light = 0; Light < 3; ++Light)
		{
			const FGteLongVector Direction = {DrawSignedBits(Random, 1, 15), DrawSignedBits(Random, 1, 15), DrawSignedBits(Random, 1, 15)};
			Lights.SetDirectional(Light, Direction, DrawShortVector(Random, 16));
		}
		const FGteMatrix Rotation = RotationXyz(DrawShortVector(Random, 16));
		Lights.SetAmbient(DrawShortVector(Random, 16));
		FGte Gte;
		Lights.Load(Gte, Rotation);
		AddRotation(Digest, Gte.LightMatrix);
		AddRotation(Digest, Gte.ColorMatrix);
		Digest.I32(Gte.BackColor[0], Gte.BackColor[1], Gte.BackColor[2]);
	}
	CHECK(Digest.Hex() == "09b8dc64b9819ae2dd5918bda07dec68eb689801");
}

TEST_CASE("The kernel's rotation matrices and triangle normals match the game's", "[Graphics][Oracle]")
{
	FXorShift32 Rotations(0x407);
	FOracleDigest RotationDigest;
	for (int32 Case = 0; Case < 512; ++Case)
	{
		AddRotation(RotationDigest, RotationXyz(DrawShortVector(Rotations, 16)));
	}
	CHECK(RotationDigest.Hex() == "e2a1feb0d45b9cbf1c04bf97212ae880d3ea7033");

	FXorShift32 Triangles(0x7E1);
	FOracleDigest NormalDigest;
	for (int32 Case = 0; Case < 512; ++Case)
	{
		const int32 Bits = 1 + static_cast<int32>(Triangles.Next() % 12);
		std::array<FGteVector, 3> Points{};
		for (FGteVector& Point : Points)
		{
			Point = DrawShortVector(Triangles, Bits);
		}
		if (Case % 16 == 0)
		{
			Points[2] = Points[0];
		}
		const FGteVector Normal = TriangleNormal(Points[0], Points[1], Points[2]);
		NormalDigest.I16(Normal[0], Normal[1], Normal[2]);
	}
	CHECK(NormalDigest.Hex() == "d4e98e05670e43e2ed416a89752c45411c7b1005");
}

TEST_CASE("RotMatrixZYX and ApplyRotMatrixLV match the game's libgte", "[Graphics][Oracle]")
{
	FXorShift32 Random(0x2B7);
	FOracleDigest Digest;
	for (int32 Case = 0; Case < 512; ++Case)
	{
		const FGteMatrix Rotation = RotationZyx(DrawShortVector(Random, 16));
		AddRotation(Digest, Rotation);
		const int32 X = DrawSigned(Random, 32);
		const int32 Y = DrawSigned(Random, 32);
		const std::array<int32, 3> Result = ApplyMatrixLV(Rotation, {X, Y, DrawSigned(Random, 32)});
		Digest.I32(Result[0], Result[1], Result[2]);
	}
	CHECK(Digest.Hex() == "032666d1059e1b052f06123c4829df20053c5111");
}

TEST_CASE("RotMatrixYXZ, MulMatrix2 and the kernel's sines match the game's", "[Graphics][Oracle]")
{
	FXorShift32 Random(0x3A9);
	FOracleDigest Digest;
	for (int32 Case = 0; Case < 512; ++Case)
	{
		const FGteMatrix Rotation = RotationYxz(DrawShortVector(Random, 16));
		AddRotation(Digest, Rotation);
		FGteMatrix Other = DrawMatrix(Random);
		Other.T = DrawVector(Random, 30);
		// MulMatrix2 writes the product over its second matrix and keeps that matrix's translation.
		FGteMatrix Product = MulMatrix(Rotation, Other);
		Product.T = Other.T;
		AddRotation(Digest, Product);
		AddVector(Digest, Product.T);
	}
	for (int32 Angle = -0x1000; Angle < 0x1000; ++Angle)
	{
		Digest.I32(GteSin(Angle), GteCos(Angle));
	}
	CHECK(Digest.Hex() == "8909b8b51ab05b7d5f807a4045fcd33bdc0e871e");
}

TEST_CASE("A degenerate triangle's normal comes from the R3000's division by zero", "[Graphics]")
{
	// Every component divides by a zero root: -1 each, normalized.
	CHECK(TriangleNormal({5, 5, 5}, {9, 1, 3}, {5, 5, 5}) == FGteVector{-2364, -2364, -2364});
}
