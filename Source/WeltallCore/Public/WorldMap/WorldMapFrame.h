#pragma once

#include "Foundation/Types.h"
#include "Graphics/AffineBlend.h"
#include "Graphics/GteMatrix.h"
#include "Graphics/TexturePictures.h"
#include "Presentation/DrawList.h"
#include "WorldMap/WorldMapPlayerCamera.h"

#include <array>
#include <vector>

namespace Weltall
{
	/** Which rules decide what a frame of the world map draws. */
	enum class EWorldMapCulling : uint8
	{
		/**
		 * The original's, made for its 320 x 216 screen: the terrain's blocks and quadrants about the
		 * camera that its frustum and the camera's quarter leave, and of them the triangles the GTE
		 * projects without fault, with a corner on the screen across and one down, facing the eye and
		 * nearer than the draw distance; the sky's quads the GTE projects without fault.
		 */
		Original,
		/**
		 * Everything a view of any width shows within the original's square and draw distance: every
		 * quadrant of the square about the camera, and of them the triangles facing the eye and nearer
		 * than the draw distance; every quad of the sky, behind everything. The drawing clips the rest.
		 */
		InRange,
		/**
		 * As InRange over twice the original's reach: the terrain of the 9 x 9 blocks about the camera's,
		 * the terrain and the models drawn to twice the depth, and the fog moved out as far.
		 */
		Extended,
	};

	/** What drew a polygon of the frame. */
	enum class EWorldMapLayer : uint8
	{
		Terrain,
		Sky,
		/** The band of distant clouds over the horizon. */
		Horizon,
		/** A part of a party member's or a gear's sprite. */
		Sprite,
		/** A polygon of a place model: a town, a landmark, the Yggdrasil. */
		Model,
		/** A shadow on the ground under a party member, a gear or the Yggdrasil. */
		Shadow,
		/** A billboard of the blocks' lists: a tree. */
		Billboard,
		/** A cloud drifting above the world. */
		Cloud,
		/** A particle of an effect: dust, spray, a wake. */
		Particle,
		/**
		 * A polygon of a gear's model: in the arena's bouts, which the Battling program draws in the world map's frame
		 * form, and of a party gear's model drawn in its sprite's place on the world map.
		 */
		Gear,
		/** An effect of the arena's bouts: its sprites, trails and bands, dust, splashes, lines and hit sparks. */
		Effect,
	};

	/** What the original's GPU draws a frame's polygon as; a renderer draws the quad of its view corners whatever it is. */
	enum class EWorldMapPrimitive : uint8
	{
		Polygon,
		/** A rectangle on the screen (a tile): its first screen corner its top left, its last one past its bottom right pixel. */
		Rectangle,
		/** A line a pixel wide: its first two screen corners its ends; its view corners the pixel-wide quad along it. */
		Line,
	};

	/**
	 * A polygon of a world map frame: a triangle, or a four-point polygon drawn as the triangles
	 * (0, 1, 2) and (1, 2, 3), its corners where the view puts them and where the original's GTE put
	 * them on its screen, with what the GPU draws it with.
	 */
	struct FWorldMapPolygon
	{
		EWorldMapLayer Layer = EWorldMapLayer::Terrain;
		EWorldMapPrimitive Primitive = EWorldMapPrimitive::Polygon;
		uint8 CornerCount = 3;
		/** View space (x right, y down, z ahead, in the world's units), without the GTE's rounding. */
		std::array<std::array<float, 3>, 4> View{};
		/** On the original's 320 x 216 screen, its top left at (0, 0), as the GTE projects them. */
		std::array<int16, 4> ScreenX{};
		std::array<int16, 4> ScreenY{};
		/** One per corner when Gouraud-shaded; otherwise the first covers it. */
		std::array<FDrawColor, 4> Colors{};
		bool bGouraud = false;
		bool bTextured = false;
		/** Texels are drawn as they are rather than multiplied by the colour. */
		bool bRaw = false;
		bool bSemiTransparent = false;
		/**
		 * Drawn only where its corners wind clockwise on the screen as the display places them: a polygon whose facing
		 * the frame leaves to the display, which draws it between ticks and without the GTE's whole units. Otherwise it
		 * is drawn whichever way it faces: the frame has dropped what faces away.
		 */
		bool bOneSided = false;
		/** Of the party's ship (the Yggdrasil's place models in a playable scene), which a renderer may draw in the game's order. */
		bool bVehicle = false;
		/** Its texels lie in VRAM's port rows, below the console's: its page and CLUT are read with their high y bits. */
		bool bPortRows = false;
		/** The texture page (with its blend mode) and CLUT, as the GPU packet holds them. */
		uint16 TexturePage = 0;
		uint16 Clut = 0;
		/** Texel coordinates in the page, as the packet holds them. */
		std::array<uint8, 4> U{};
		std::array<uint8, 4> V{};
		/**
		 * Where a renderer maps the texels: the packet's coordinates, but a sprite's parts map theirs edge
		 * to edge (the far edges one past the last texel), and a polygon in a texture window unwrapped.
		 */
		std::array<std::array<float, 2>, 4> TexCoords{};
		/** The texels it samples, inclusive (u, v); in a texture window, the window's. */
		std::array<uint8, 2> TexelMin{};
		std::array<uint8, 2> TexelMax{};
		/** The texture window its texels repeat in: the width of the texels u wraps within, 0 none. */
		uint8 WindowWidth = 0;
		/** The ordering-table entry it was linked into. */
		uint16 Bucket = 0;
		/** The frame's anchor it moves with (FWorldMapFrame::Anchors), NoAnchor for none. */
		uint16 Anchor = NoAnchor;

		static constexpr uint16 NoAnchor = 0xFFFF;
	};

	/** What a world map anchor holds its polygons to. */
	enum class EWorldMapAnchorKind : uint8
	{
		/** The terrain: the world about the camera's target. */
		World,
		/** The sky's quads and the horizon's band, turned with the camera but for its yaw. */
		Sky,
		Sprite,
		Model,
		Shadow,
		Billboard,
		Particle,
		Cloud,
		/** A part of a party gear's model, or its cloths. */
		Gear,
	};

	/**
	 * Something a frame's polygons move with between ticks: the world, a sprite, a place model, a particle..., and
	 * the matrix that took its own space to view space this tick. A renderer pairs it with the anchor of the same
	 * key in the tick before to draw its polygons in between.
	 */
	struct FWorldMapAnchor
	{
		/** The kind in the top 8 bits, then what tells it from the others of its kind; unique in a frame. */
		uint32 Key = 0;
		FGteMatrix Matrix;
		/** How far past the matrix's whole units the anchor stands in view space: what the GTE dropped of its place. */
		std::array<float, 3> Fraction{};
		/**
		 * The matrix as real numbers, for an anchor placed finer than the GTE's entries hold (a model drawn at a
		 * few hundredths of its files' size): what its polygons' corners went through.
		 */
		bool bFine = false;
		FAffine Fine{};

		static constexpr uint32 MakeKey(EWorldMapAnchorKind Kind, uint32 Id) { return static_cast<uint32>(Kind) << 24 | (Id & 0xFFFFFF); }
		EWorldMapAnchorKind Kind() const { return static_cast<EWorldMapAnchorKind>(Key >> 24); }
	};

	/** The blend keys of the 2D layer's primitives that move between ticks: unique across the minimap, the windows and the fade. */
	namespace WorldMapBlendKeys
	{
		inline constexpr uint32 MinimapPicture = 1;
		/** Plus the place's dot. */
		inline constexpr uint32 MinimapDots = 0x10;
		/** Plus the arrow's triangle. */
		inline constexpr uint32 MinimapArrow = 0x40;
		inline constexpr uint32 Fade = 0x80;
	}

	/**
	 * What the renderer reads of one world map tick, as the tick drew it: every polygon of the
	 * particles, the terrain, the sky, the sprites, the place models, the shadows, the billboards and the clouds, in the order the GPU draws them (the ordering table's entries
	 * from the far end, the last linked first within one), and the screen's 2D layer over them. Built
	 * into the same memory tick after tick.
	 */
	struct FWorldMapFrame
	{
		/** Counts the world map's ticks since its scene was entered. */
		uint32 Tick = 0;
		/** The GTE's projection: its screen's centre (x, y) and projection distance. */
		int32 ScreenCenterX = 160;
		int32 ScreenCenterY = 0;
		int32 ProjectionDistance = 0;
		/** The camera's matrix (world about the target to view), the target (whole units), its yaw, and the world's size it wraps at. */
		FGteMatrix Camera;
		int32 TargetX = 0;
		int32 TargetZ = 0;
		int16 Yaw = 0;
		int32 WorldUnitsX = 0;
		int32 WorldUnitsZ = 0;
		/**
		 * Where the player's view applies (the game's camera on its orbit: not in a cutscene, nor while a script has
		 * it): the game's camera this tick and the player's offsets the frame shows over it, which a display between
		 * ticks moves the view on from.
		 */
		bool bPlayerView = false;
		FWorldMapCameraBasis CameraBasis;
		FWorldMapCameraOffsets ShownOffsets;
		/**
		 * How much nearer than their view depths the ordering-table entries were taken (whole units; the player's zoom
		 * pushing the eye back, WorldMapSortDepth): a sprite whose depth its entry tells stands this much farther.
		 */
		int32 SortBack = 0;
		/**
		 * Its gears stand on beds below the ground drawn (the arena's: a lake's bed 100h under its surface, sand's 40h under
		 * it), and the original draws them over the ground all the same: against them the ground keeps the order.
		 */
		bool bGearsUnderGround = false;
		std::vector<FWorldMapPolygon> Polygons;
		std::vector<FWorldMapAnchor> Anchors;
		/** The texture regions of the gears' models drawn for the party's sprites, with their loads' colours (FGearModel::TextureSeeds). */
		std::vector<FTexturePictureSeed> TextureSeeds;
		/** The minimap over the scene when it is shown, on the 320 x 216 screen, drawn before the 2D layer. */
		FDrawList Minimap;
		/** The place name's and the notice's windows over the minimap, on the 320 x 216 screen. */
		FDrawList Windows;
		/**
		 * A cutscene's screen waver (FWorldMapScreenWaver), drawn after the scene and under the 2D layer: each row's
		 * shift, in pixels of the 320 x 216 screen to the right; empty where the scene has none.
		 */
		std::vector<int16> WaverShifts;
		/** The 2D layer over the scene, on the 320 x 216 screen: the fade. */
		FDrawList Screen;
	};
}
