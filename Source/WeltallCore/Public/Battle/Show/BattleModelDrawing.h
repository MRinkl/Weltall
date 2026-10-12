#pragma once

#include "Battle/Show/BattleShowExtensions.h"
#include "Battle/Show/BattleShowFrame.h"
#include "Battle/Show/BattleTmdModel.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/Sprite.h"

#include <array>
#include <array>
#include <optional>
#include <vector>

namespace Weltall
{
	/** A model a frame has drawn, with the packets it linked: a tint later in the frame recolours them. */
	struct FBattleDrawnModel
	{
		const FSprite* Sprite = nullptr;
		/** The build of the sprite's packets that was linked. */
		uint32 Build = 0;
		std::size_t FirstPolygon = 0;
		std::size_t PolygonCount = 0;
	};

	/**
	 * How the lit packets of one build of a model stand in each of the two draw buffers: the lights its last drawing into
	 * the buffer gave them, and whether a tint has replaced a textured one's since.
	 */
	struct FBattleModelLights
	{
		struct FBuffer
		{
			bool bDrawn = false;
			bool bTinted = false;
			FGteMatrix Light;
			FGteMatrix Colors;
			std::array<int32, 3> Back{};
		};

		uint32 Build = 0;
		std::array<FBuffer, 2> Buffers;
	};

	/** The stage's light, for the models that load none: its direction's matrix, its colour matrix, its ambient colour. */
	struct FBattleStageLight
	{
		FGteMatrix Direction;
		FGteMatrix Colours;
		std::array<int32, 3> Ambient{};
	};

	/**
	 * The kernel's drawings of a model attached to a battle sprite, into a frame: the GTE they project and light with
	 * (its lights stay as a drawing leaves them, for the next), the sprites' view, the frame's lists and links.
	 */
	struct FBattleModelDrawing
	{
		FGte* Gte = nullptr;
		const FGteMatrix* View = nullptr;
		FBattleShowFrame* Frame = nullptr;
		std::vector<FBattleShowLink>* Links = nullptr;
		std::vector<FBattleDrawnModel>* Drawn = nullptr;
		/**
		 * When set, a drawing that loads no light of its sprite's own lights its model by this, turned into the model's
		 * frame, and leaves the GTE's lights as it found them; unset, by the GTE's lights as they stand.
		 */
		std::optional<FBattleStageLight> StageLight;
		/** The frame's draw buffer (0 or 1): a drawing lights that buffer's packets alone. */
		int32 Buffer = 0;
		/** Scratch: the model read from its file. */
		FBattleTmdModel Model;

		/**
		 * Draws the model Sprite holds, for the drawer of blend key Key, with Matrix (the sprite's own, its place as
		 * translation). As the model kind's drawing unless bOwnRotation: through the view unless the sprite is in screen
		 * space, lit by the sprite's own light when it has one, about the screen's middle when it asks; with bOwnRotation
		 * as system command 2C's drawing, the sprite's own rotation at its place in the view. Each polygon that faces the
		 * eye is linked by its average depth and the sprite's bias, or into the far entries.
		 *
		 * False for a model that cannot be read.
		 */
		WELTALLCORE_API bool Draw(const FSprite& Sprite, const FGteMatrix& Matrix, uint32 Key, bool bOwnRotation);

		/**
		 * The sprite's packets were tinted again: the polygons the frame linked from them take the colour, a lit textured
		 * polygon's light replaced by it. False for a model that cannot be read.
		 */
		WELTALLCORE_API bool Retint(const FSprite& Sprite);

		/**
		 * The colour and code a corner of a primitive of the model Sprite holds has in draw buffer InBuffer's packet, as the
		 * build, the tints and the drawings so far left it; Normals are the model's.
		 */
		WELTALLCORE_API uint32 PacketColor(const FSprite& Sprite, const FBattleTmdPrimitive& Primitive, const std::vector<FGteVector>& Normals, std::size_t Corner,
			int32 InBuffer) const;

		/** The packet of primitive Number of From, the model Sprite holds, becomes a shard's: its shape, its texels, its colours in either buffer. */
		WELTALLCORE_API void TakePackets(const FSprite& Sprite, const FBattleTmdModel& From, std::size_t Number, FBattleShard& Shard) const;

		/**
		 * The shards' drawing: each shard's polygon at its place and turn under the set's matrix and the view, with the
		 * colours its packet holds in the frame's buffer. Every one is linked, whatever the GTE made of it, into the
		 * table's nearest entry: in front of the HUD.
		 */
		WELTALLCORE_API void DrawShards(const FBattleShards& Set);

		/** A new battle: no light set, no model drawn. */
		WELTALLCORE_API void Reset();

	private:
		/** The battle's model drawing: the model read through the GTE's matrix, each polygon into entry (depth >> Shift) + Bias. */
		bool Link(const FSprite& Sprite, uint32 Key, int32 Shift, int32 Bias);
		FBattleModelLights* LightsOf(uint32 Build);

		/** The models with lit primitives drawn so far in the battle, by their packets' build. */
		std::vector<FBattleModelLights> Lit;
	};
}
