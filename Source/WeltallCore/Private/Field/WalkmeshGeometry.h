#pragma once

#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/GteVectors.h"

// How the field measures points against the walkmesh's triangles.
namespace Weltall::WalkmeshGeometry
{
	/**
	 * A point as the field packs it for NCLIP: x * 65536 + z in one register, which the GTE reads as
	 * screen x = the low half (z) and screen y = the high half (x, one less when z is negative).
	 */
	inline uint32 PackPoint(int32 X, int32 Z)
	{
		return static_cast<uint32>(X) * 0x10000u + static_cast<uint32>(Z);
	}

	inline uint32 PackVertex(const FGteVector& Vertex)
	{
		return PackPoint(Vertex[0], Vertex[2]);
	}

	/** NCLIP of three packed points: negative when the third lies outside the edge from the first to the second. */
	inline int32 PackedNclip(uint32 From, uint32 To, uint32 Point)
	{
		const auto Low = [](uint32 Packed) { return static_cast<int16>(Packed & 0xFFFF); };
		const auto High = [](uint32 Packed) { return static_cast<int16>(Packed >> 16); };
		return Nclip(Low(From), High(From), Low(To), High(To), Low(Point), High(Point));
	}

	/**
	 * The height of a triangle's plane under (X, Z): v0.y + (-n.z (z - v0.z) - n.x (x - v0.x)) / n.y
	 * with n the plane's normal (4.12, left in Normal), 0 when n.y is 0.
	 */
	int16 PlaneHeight(const FGteVector& V0, const FGteVector& V1, const FGteVector& V2, int16 X, int16 Z, FGteLongVector& Normal);
}
