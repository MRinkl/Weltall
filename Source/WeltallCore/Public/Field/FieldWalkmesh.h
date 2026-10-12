#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	/** Bits of a walkmesh triangle's attribute word that the field's movement, camera and drawing read. */
	namespace FieldFloorAttribute
	{
		/** Slow ground: a move across it covers half the distance. */
		constexpr uint32 SlowGround = 0x100;
		/** The floor's push moves an actor that is in the air or held at a fixed height, and one standing on it. */
		constexpr uint32 PushesInTheAir = 0x4000;
		constexpr uint32 PushesOnTheGround = 0x8000;
		/** A slope an actor slides down. */
		constexpr uint32 Slope = 0x20000;
		/** A ladder: whoever stands on it turns to face it, and its sprite sorts by a point above or below. */
		constexpr uint32 Ladder = 0x200000;
		/** A steep slope: faster sliding, no jumping, and no stepping onto it from lower ground. */
		constexpr uint32 Steep = 0x400000;
		constexpr uint32 Slopes = Slope | Steep;
		/** On a walking layer: closed to the actors. On the camera layer the same bit marks the camera's area. */
		constexpr uint32 Closed = 0x800000;
		constexpr uint32 InCameraArea = 0x800000;
	}

	struct FWalkmeshTriangle
	{
		std::array<uint16, 3> Vertices{};
		/** The triangle across edges 0-1, 1-2 and 2-0, or -1 for a wall. */
		std::array<int16, 3> Neighbours{};
		/** Index into the walkmesh's attribute words. */
		uint8 Attribute = 0;
		/** A negative value c puts a ceiling 4|c| units above the floor; the field ignores the others. */
		int8 Ceiling = 0;
	};

	struct FWalkmeshLayer
	{
		/** x, y (the floor's height, negative is up), z. */
		std::vector<FGteVector> Vertices;
		std::vector<FWalkmeshTriangle> Triangles;
	};

	/**
	 * Where a field map's actors can walk: up to four layers of triangles, each naming its neighbours
	 * and an attribute word (collision classes, floor pushes, slopes and the like). The last layer is
	 * the camera's: the point the camera looks at stays inside it.
	 */
	struct FFieldWalkmesh
	{
		std::vector<FWalkmeshLayer> Layers;
		/** Up to 256 words; a few maps store fewer, and their triangles use only those. */
		std::vector<uint32> Attributes;

		const FWalkmeshLayer& CameraLayer() const { return Layers.back(); }
	};

	/** Reads a field map's walkmesh section. */
	WELTALLCORE_API TResult<FFieldWalkmesh> ParseFieldWalkmesh(std::span<const uint8> Section);
}
