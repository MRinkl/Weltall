#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/MdlModel.h"
#include "Graphics/MdlPackets.h"

#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	struct FGearBone;

	/**
	 * Where a load puts a gear's textures and CLUTs in VRAM. A place from row 512 on lies in the port's own rows
	 * (FVram::PortRows): its parts' pages and CLUTs carry the high y bits FTexturePage::FromPortAttribute reads.
	 */
	struct FGearTexturePlace
	{
		int16 X = 0;
		int16 Y = 0;
		int16 ClutX = 0;
		int16 ClutY = 0;
	};

	/**
	 * A bone's part as the model's load built it for that bone (a part two bones draw is built twice):
	 * each polygon's packet, and the texture page and CLUT it samples.
	 */
	struct FGearBonePackets
	{
		uint16 Bone = 0;
		uint16 Part = 0;
		std::vector<FMdlPacket> Packets;
		std::vector<FMdlTextureState> Textures;
	};

	/**
	 * A textured polygon's page and CLUT once its model's load has put the textures at Place: the page's column and row
	 * and the CLUT's row and 256-unit block moved there (null: as the model holds them).
	 */
	WELTALLCORE_API FMdlTextureState PlaceGearTexture(const FMdlPrimitive& Primitive, const FGearTexturePlace* Place);

	/**
	 * Every bone's part built as the load builds it, in bone order: lit through the builder with
	 * Lighting in LightMode (the gears' loads 2; 0 keeps the stored colours), each page and CLUT command
	 * moved to Place unless the textures are absolute (Place null), where an environment-mapped
	 * polygon's page and CLUT go too. Fails where the original's result is not the model's own: a
	 * polygon lit at the build and never again (the Gouraud kinds the drawing does not relight) without
	 * the build's lighting known, a polygon taking its page or CLUT from an earlier build, an
	 * environment-mapped polygon of absolute textures (it takes an earlier build's place), a polygon
	 * relit in light mode 2 whose part keeps no light data, and the colours the builder leaves undefined.
	 */
	WELTALLCORE_API TResult<std::vector<FGearBonePackets>> BuildGearPackets(const FMdlModel& Model, std::span<const FGearBone> Bones, int32 LightMode,
		const FGearTexturePlace* Place, const std::optional<FGte>& Lighting);
}
