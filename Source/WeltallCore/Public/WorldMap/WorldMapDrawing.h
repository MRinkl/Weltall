#pragma once

#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "WorldMap/WorldMapFrame.h"
#include "WorldMap/WorldMapTerrain.h"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FSprite;
	class FWorldMapGround;
	class FWorldMapParticles;
	struct FWorldMapCloud;
	struct FWorldMapContext;
	struct FWorldMapDrawingTables;

	/** The state a world map frame is drawn from, as the frame task leaves it. */
	struct FWorldMapDrawState
	{
		/** The camera's matrix: its rotation, and the eye's offset from the target turned by it. */
		FGteMatrix Camera;
		/** Where the camera looks (20.12 units): the terrain and everything on it is drawn about it. */
		FWorldMapVector Target;
		int16 Pitch = 0;
		int16 Yaw = 0;
		/** The eye's offset from the target (whole units). */
		std::array<int16, 3> Eye{};
		/**
		 * How much farther from the target the eye stands than the game's camera (whole units; the player's zoom):
		 * every depth the drawing cuts or fogs at moves back with it, so the world ends where the game ends it.
		 */
		int32 EyeBack = 0;
		/**
		 * How much the player's view adds to every depth the GTE gives (the camera's translation along the view axis,
		 * less the game camera's): the ordering table sorts by the depths less it (WorldMapSortDepth).
		 */
		int32 SortBack = 0;
		/** The GTE's screen centre y the frame projects with (the loop sets it as a pass ends), and its projection distance. */
		int32 ScreenCenterY = 0;
		int32 ProjectionDistance = 0;
		/** The waves' phases along a quadrant's columns and down its rows. */
		int32 WavePhase = 0;
		int32 WaveRowPhase = 0;
		/** The depth the fog starts at. */
		int32 FogNear = 0;
		/** Counts the frames drawn since the scene was entered. */
		uint32 Tick = 0;
		/** The draw buffer (0 or 1), which also picks each model's packet buffer. */
		int32 DrawBuffer = 0;
	};

	/**
	 * A frame's polygons as they are linked into the ordering table, put in the order the GPU draws
	 * them when the frame is finished. Kept from frame to frame, it allocates only as frames grow.
	 */
	class FWorldMapFrameBuilder
	{
	public:
		/** Starts a frame: Frame's polygons are replaced when it is finished. */
		WELTALLCORE_API void Begin(FWorldMapFrame& InFrame);

		/**
		 * The anchor the polygons added from now on move with, Matrix taking its space to view space, Fraction of a
		 * unit past it where the anchor's place is known finer than the matrix's whole units.
		 */
		WELTALLCORE_API void Anchor(EWorldMapAnchorKind Kind, uint32 Id, const FGteMatrix& Matrix, const std::array<float, 3>& Fraction = {});

		/** An anchor placed finer than a GTE matrix holds: Fine is what its polygons' corners went through, Matrix the same in the GTE's numbers. */
		WELTALLCORE_API void AnchorFine(EWorldMapAnchorKind Kind, uint32 Id, const FGteMatrix& Matrix, const FAffine& Fine);

		/** A polygon linked into ordering-table entry Bucket after those before it. */
		WELTALLCORE_API FWorldMapPolygon& Add(uint16 Bucket);

		/** How many polygons are linked so far. */
		std::size_t LinkedCount() const { return Linked.size(); }

		/**
		 * Links the polygons added from the one of index First on again, each of them with an anchor of its own, the
		 * nearest first by their first corner's view depth, so an entry that holds several of them draws the farthest
		 * first; those of one depth keep their order.
		 */
		WELTALLCORE_API void LinkNearestFirst(std::size_t First);

		/** The frame's polygons in drawing order: the entries from the far end, within one the last linked first; and their anchors. */
		WELTALLCORE_API void Finish();

	private:
		FWorldMapFrame* Frame = nullptr;
		std::vector<FWorldMapPolygon> Linked;
		std::vector<FWorldMapAnchor> Anchors;
		std::vector<uint32> Starts;
	};

	/**
	 * The ordering table's entry the original links the horizon's band into this frame (the sky's turn about the camera's
	 * pitch and roll), or none where the band is not drawn: what lies farther is drawn before it, so the band covers it.
	 */
	WELTALLCORE_API std::optional<uint16> WorldMapHorizonBucket(const FWorldMapDrawingTables& Tables, const FWorldMapDrawState& State);

	/**
	 * A depth the GTE gave as the ordering table sorts it: the depth the game's own camera gives the point wherever the
	 * player's zoom has pulled the view (exactly, the view's turn being the game's). A depth cut into entries as it is
	 * would move the cuts through the scene as the zoom changes, and trees side by side would change places.
	 */
	inline uint16 WorldMapSortDepth(int32 Depth, const FWorldMapDrawState& State)
	{
		return static_cast<uint16>(std::clamp(Depth - State.SortBack, 0, 0xFFFF));
	}

	/** How many times the original's reach a culling draws the world to: 2 for Extended, else 1. */
	WELTALLCORE_API int32 WorldMapReach(EWorldMapCulling Culling);

	/** The terrain's blocks across the square a culling draws about the camera's: 9 for Extended, else 5. */
	WELTALLCORE_API int32 WorldMapBlocksAcross(EWorldMapCulling Culling);

	/**
	 * The GTE as the world map draws with it: the camera's matrix, the screen's centre, the projection,
	 * and the fog, its depths Reach times the original's.
	 */
	WELTALLCORE_API FGte WorldMapDrawingGte(const FWorldMapDrawState& State, int32 Reach = 1);

	/**
	 * The terrain as the frame task builds it: the 5 x 5
	 * blocks about the camera's, each block and quadrant tested against the frustum and the ones the
	 * camera's quarter of its block leaves out dropped, then each quadrant's 8 x 8 cells as two
	 * textured triangles, fogged by their palette's level. The water's corners move with the phases.
	 * Each block's frustum test (-1 outside, row by row) goes to BlockStates when given.
	 */
	WELTALLCORE_API void DrawWorldMapTerrain(const FWorldMapTerrain& Terrain, const FWorldMapDrawingTables& Tables, const FWorldMapDrawState& State,
		EWorldMapCulling Culling, FWorldMapFrameBuilder& Builder, std::vector<int32>* BlockStates = nullptr);

	/**
	 * The sky as the frame task draws it after the terrain:
	 * the horizon's band of distant clouds, scrolled with the camera's yaw, then the four graded quads
	 * of the backdrop, turned with the camera but for its yaw.
	 */
	WELTALLCORE_API void DrawWorldMapSky(const FWorldMapDrawingTables& Tables, const FWorldMapDrawState& State, EWorldMapCulling Culling,
		FWorldMapFrameBuilder& Builder);

	/**
	 * The particles as the frame task draws them first: each living one a semi-transparent quad of
	 * its shape facing the screen, scaled by its size, turned by its angle where its colour step asks, in its colour,
	 * linked by its last corner's depth while that is nearer than C00h (twice that at twice the reach). The original's
	 * culling also drops one with no corner left of the screen's right edge or none above its bottom.
	 */
	WELTALLCORE_API void DrawWorldMapParticles(const FWorldMapParticles& Particles, const FWorldMapDrawingTables& Tables, const FWorldMapGround& Ground,
		const FWorldMapDrawState& State, EWorldMapCulling Culling, FWorldMapFrameBuilder& Builder);

	/**
	 * The clouds as the frame task draws them after the sky: each from the copy of its four tiles
	 * nearest the camera's target, leaning toward the camera, three layers of an additive quad, cut into 4 or 16 pieces a
	 * layer as the cloud nears. The original's culling draws only those in a wedge about the camera's yaw, on its screen,
	 * and starts none past 240 quads.
	 */
	WELTALLCORE_API void DrawWorldMapClouds(std::span<const FWorldMapCloud> Clouds, const FWorldMapDrawingTables& Tables, const FWorldMapDrawState& State,
		EWorldMapCulling Culling, FWorldMapFrameBuilder& Builder);

	/**
	 * The minimap as the frame task draws it last, when the player has not hidden it: the map's
	 * picture, a tile for each place the story has shown (the Yggdrasil, Shevat and Thames where they are), and the
	 * party's arrow turned with the camera's yaw, in drawing order on the 320 x 216 screen.
	 */
	WELTALLCORE_API void DrawWorldMapMinimap(const FWorldMapContext& Context, const FWorldMapDrawingTables& Tables, const FWorldMapDrawState& State,
		FDrawList& Minimap);

	/** The matrix the kernel builds for a world map sprite (its base never fixed): its angles' rotation, scaled. */
	WELTALLCORE_API FGteMatrix WorldMapSpriteRotation(const FSprite& Sprite);

	/**
	 * A sprite as the frame task draws it (the kernel's sprite drawing with the camera's matrix as the view): each
	 * part a textured quad facing the screen, linked into ordering-table entry OtIndex (or nearer, by
	 * its anchor group). Slot is its task's, which tells it from the others between frames.
	 */
	WELTALLCORE_API void DrawWorldMapSprite(const FSprite& Sprite, int32 Slot, const FWorldMapDrawState& State, int32 OtIndex, FWorldMapFrameBuilder& Builder);

	/**
	 * The fog's palettes: Levels copies of a 256-colour palette, the nth faded n / Levels of the
	 * way toward the fog's colour (each channel of eight times the colour, truncated back to five
	 * bits), keeping each colour's semi-transparency bit and leaving colour 0 transparent.
	 */
	WELTALLCORE_API void MakeWorldMapFogPalettes(std::span<const uint16> Palette, int32 Levels, const std::array<uint8, 3>& Fog, std::span<uint16> Palettes);
}
