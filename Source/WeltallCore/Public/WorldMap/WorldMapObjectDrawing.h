#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/MdlModel.h"
#include "Graphics/MdlPackets.h"
#include "WorldMap/WorldMapDrawing.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FWorldMapGround;
	struct FWorldMapDrawingTables;
	struct FWorldMapObject;
	struct FWorldMapScene;

	/** A place model's polygon as the scene's start builds it: its packet's code and colours, the texture page and CLUT it samples, and its corners' texels. */
	struct FWorldMapModelPolygon
	{
		FMdlPacket Packet;
		FMdlTextureState Texture;
		std::array<FMdlUv, 4> Uvs{};
	};

	/** Each placed model's polygons as the scene's start builds them, in the placements' order. */
	struct FWorldMapModelPackets
	{
		std::vector<std::vector<FWorldMapModelPolygon>> Objects;
	};

	/**
	 * The objects' builds: each placement's model part built in light mode 1 under the
	 * program's lights (from above, over a half-grey ambient), the builder's texture page and CLUT
	 * carried from one build to the next from a start of zero. Fails for what no world map model
	 * holds: an environment-mapped polygon, or one the builder cannot build in that mode.
	 */
	WELTALLCORE_API TResult<FWorldMapModelPackets> BuildWorldMapModelPackets(const FWorldMapScene& Scene, const FWorldMapDrawingTables& Tables);

	/**
	 * What the start's hide of the fifth model does in a scene of fewer: its halfword (1) lands PastModels bytes past
	 * the models' block, which the kernel's heap follows, a block header on, with the first model's packets. There it
	 * is a textured quad's last texel, which both packet buffers then hold (the scene's task copies the first buffer
	 * into the second as it sets the model's look). Fails where it would land on anything else.
	 */
	WELTALLCORE_API std::optional<FError> ApplyStrayHiddenWrite(const FWorldMapScene& Scene, int32 PastModels, FWorldMapModelPackets& Packets);

	/** A playable scene's first place models: the Yggdrasil's hull, its deck and the two parts spinning on it. */
	inline constexpr std::size_t VehicleObjectCount = 4;

	/**
	 * The place models as the frame task draws them (with the kernel's part drawing):
	 * each shown object placed about the camera's target through the objects it hangs from, at half
	 * scale, drawn when its origin projects without fault nearer than 3456; its part's box tested on the
	 * screen, then each polygon under the draw mode its flags pick (the fogged modes fade textured
	 * polygons toward the fog's colour by their depth). The first VehicleObjects are the party's ship's.
	 */
	WELTALLCORE_API void DrawWorldMapObjects(std::span<const FWorldMapObject> Objects, const FWorldMapScene& Scene, const FWorldMapModelPackets& Packets,
		const FWorldMapDrawingTables& Tables, const FWorldMapGround& Ground, const FWorldMapDrawState& State, EWorldMapCulling Culling,
		std::size_t VehicleObjects, FWorldMapFrameBuilder& Builder);
}
