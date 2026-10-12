#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/MdlModel.h"

#include <array>
#include <vector>

namespace Weltall
{
	/** How the draw mode that relights a model lights one of its polygons again, every frame. */
	enum class EMdlRelight : uint8
	{
		None,
		/** Flat untextured: the face normal's light times the stored color. */
		FaceTimesColor,
		/** Flat textured: the face normal's light, which then modulates the texture. */
		Face,
		/** Gouraud untextured: each corner normal's light times the stored color. */
		CornersTimesColor,
		/** Gouraud textured: each corner normal's light. */
		Corners,
	};

	/** What the kernel's packet builder leaves for a polygon besides its position: its code and colors. */
	struct FMdlPacket
	{
		/** The GPU command byte: bit 1 semi-transparent, bit 0 raw texture. */
		uint8 Code = 0;
		/** One per corner for the Gouraud kinds; the flat kinds use the first for every corner. */
		std::array<FMdlColor, 4> Colors{};
		EMdlRelight Relight = EMdlRelight::None;
		/** The face normal the builder computed, for the flat kinds it lights. */
		FGteVector FaceNormal{};
	};

	/**
	 * A part's packets as the kernel builds them for a light mode (0 keeps the stored colors, 1-3
	 * light the lightable kinds), with the GTE's lighting registers as Lighting holds them. The
	 * Gouraud kinds are lit whatever the light mode; the environment-mapped kind is raw, its texels
	 * left to its drawing. Fails for what the builder leaves undefined: a lightable textured polygon
	 * built without light and drawn modulated, and a static textured triangle drawn modulated.
	 */
	WELTALLCORE_API TResult<std::vector<FMdlPacket>> BuildMdlPackets(const FMdlPart& Part, int32 LightMode, const FGte& Lighting);

	/** Lights a packet again with the GTE's current lighting, as the draw mode that relights does before drawing it. */
	WELTALLCORE_API void RelightMdlPacket(FMdlPacket& Packet, const FMdlPrimitive& Primitive, const FMdlPart& Part, const FGte& Lighting);

	/** Whether the draw mode that relights reads anything for this polygon from the part's light buffer. */
	WELTALLCORE_API bool RelightReadsLightBuffer(EMdlRelight Relight);

	/** The kernel's environment mapping as a program sets it: each texel's shift and offset. */
	struct FMdlEnvironmentMapping
	{
		uint32 ShiftU = 6;
		uint32 ShiftV = 6;
		uint8 OffsetU = 0x40;
		uint8 OffsetV = 0x40;
	};

	/**
	 * An environment-mapped triangle's texels: each corner's normal turned by the part's matrix, its x and y shifted (as
	 * unsigned words) and offset, the low byte kept. A part without normals of its own points them at its vertices.
	 */
	WELTALLCORE_API std::array<FMdlUv, 3> MdlEnvironmentTexels(const FMdlPart& Part, const FMdlPrimitive& Primitive, const FGteMatrix& Matrix,
		const FMdlEnvironmentMapping& Mapping);
}
