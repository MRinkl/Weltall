#pragma once

#include "Graphics/Texture.h"
#include "Graphics/Vram.h"

#include <array>
#include <string>
#include <vector>

namespace Weltall::Testing
{
	/** What a piece of a frame is, for telling the sorting audit's findings apart. */
	enum class ESortingPiece : uint8
	{
		/** A polygon of the field's scenery: a placed model's (the background table's are left out: drawn behind all). */
		Scenery,
		/** A quad of a character's sprite, standing up to the screen; on the world map, a part of a party member's or a gear's. */
		Sprite,
		/** A character's shadow, lying on its floor; on the world map, a party member's, a gear's or the vehicle's. */
		Shadow,
		Particle,
		/**
		 * A particle linked at the front of the field's ordering table (its emitter's depth mode 0): drawn over everything
		 * by design (dust, weather), so nothing against it counts.
		 */
		Overlay,
		/** A polygon of a gear's body, or a triangle of its cloths. */
		GearPart,
		/** A gear's floor shadow. */
		GearShadow,
		/** A polygon of a sprite's spawned child's model. */
		SpriteModel,
		/** A triangle of the world map's terrain. */
		Ground,
		/** A polygon of a world map's place model: a town, a landmark, the Yggdrasil. */
		PlaceModel,
		/** A billboard of the world map's blocks: a tree. */
		Tree,
		/** A cloud drifting above the world map. */
		Cloud,
		/** A polygon of the party's ship on the world map, which keeps the game's order against the scenery. */
		Vehicle,
		/** Behind everything by design (the world map's sky and its band of clouds): drawn into the pictures, never in a conflict. */
		Background,
		/** Something the audit does not draw: the field's panorama, the background table's scenery. */
		Skipped,
	};

	/** A piece of a frame as the audit draws it: one entry of the frame's painter's order. */
	struct FSortingPiece
	{
		ESortingPiece Kind = ESortingPiece::Skipped;
		/** What it belongs to: the scenery's placed model, the character, the gear's slot, the child's model, the anchor. */
		uint32 Owner = 0;
		/** The ordering-table entry it was sorted into. */
		int32 Entry = 0;
	};

	/**
	 * Where the frame's drawing order and the depth disagree about a pair of pieces: Over is drawn over Under at
	 * Pixels pixels though Under stands nearer there by more than the margin, by Gap view units at most. A
	 * semi-transparent piece drawn over a nearer one counts (a shadow over a gear's leg), as does one the order
	 * hides behind a farther one (a particle behind a wall it stands before).
	 */
	struct FSortingConflict
	{
		uint32 Over = 0;
		uint32 Under = 0;
		uint32 Pixels = 0;
		/** Of those, the pixels where the frame in the painter's order and by depth differ enough to be seen (SortingSeenDifference). */
		uint32 Seen = 0;
		float Gap = 0.0f;
	};

	/** The largest difference of a colour's channels (0 to 255) between the two pictures from which a pixel counts as seen. */
	inline constexpr uint32 SortingSeenDifference = 16;

	struct FSortingFrameAudit
	{
		/** One per entry of the frame's order, in that order. */
		std::vector<FSortingPiece> Pieces;
		/** The pairs that disagree, the most pixels seen first. */
		std::vector<FSortingConflict> Conflicts;
		/**
		 * The frame in the painter's order and the same by depth (the nearest opaque texel, what is nearer blended over
		 * it), 0xRRGGBB by rows, and at each pixel the conflict it counts in plus one (0: none).
		 */
		int32 PictureWidth = 0;
		int32 PictureHeight = 0;
		std::vector<uint32> Picture;
		std::vector<uint32> DepthPicture;
		std::vector<uint32> Marks;
	};

	/** A corner of a piece as the audit draws it: in view space (x right, y down, z ahead), with its texel coordinate and colour. */
	struct FSortingCorner
	{
		std::array<float, 3> View{};
		float U = 0.0f;
		float V = 0.0f;
		std::array<float, 3> Color{};
	};

	/** A piece as the audit draws it: its corners, its texels and how its pixels are drawn. */
	struct FSortingShape
	{
		/** A triangle, or a quad drawn as the triangles 0-1-2 and 1-3-2; none (0) is not drawn. */
		std::array<FSortingCorner, 4> Corners{};
		int32 Count = 0;
		bool bTextured = false;
		bool bSemiTransparent = false;
		bool bRaw = false;
		/** Drawn only where its triangles' corners wind clockwise on the screen: the side facing the eye. */
		bool bOneSided = false;
		FTexturePage Page;
		FClutPosition Clut;
		EBlendMode Blend = EBlendMode::Average;
		/** Added to every texel coordinate, wrapping in the page (the scripts' scroll of a model's texels). */
		uint8 ScrollU = 0;
		uint8 ScrollV = 0;
		/** The width of a texture window the texels repeat in across u, from the page's first column; 0 none. */
		uint8 WindowWidth = 0;
		/** The texels it may read: its corners' extremes, the far ones less a hair when its texels run edge to edge. */
		float LowU = 0.0f;
		float HighU = 0.0f;
		float LowV = 0.0f;
		float HighV = 0.0f;
		/** A shadow's floor: a point of it and its normal toward the eye's side, in view space. */
		std::array<float, 3> FloorPoint{};
		std::array<float, 3> FloorUp{};
	};

	/** The screen a frame was projected on, and the picture of it the audit draws. */
	struct FSortingScreen
	{
		/** The projection's centre on the original's screen, its lines and the projection's distance. */
		float CenterX = 160.0f;
		float CenterY = 112.0f;
		int32 Lines = 224;
		float Projection = 256.0f;
		/** The view's width in the original's screen pixels about its centre (320: the original's screen; wider: a wide view). */
		int32 Width = 398;
		/** How many audit pixels across and down each screen pixel is drawn at. */
		int32 Scale = 2;
	};

	/** What a sorting puts right itself of what the painter's order and the depth disagree on: it is not counted. */
	class ISortingJudge
	{
	public:
		virtual ~ISortingJudge() = default;

		/** Whether the sorting puts the pair right (Sortable false: a blend drawn before what covers it, which no depth test does). */
		virtual bool IsDecided(const FSortingPiece& Over, const FSortingPiece& Under, bool bSortable) const = 0;

		/**
		 * How far a blend drawn after the opaque piece shown under it stands behind it, by Gap view units of depth at
		 * PointUnder (view space): it counts beyond the margin. The depth's gap, unless the sorting draws such a blend by
		 * another rule.
		 */
		virtual float BlendGap(const FSortingPiece&, const FSortingShape&, const FSortingPiece&, const std::array<float, 3>&, float Gap) const
		{
			return Gap;
		}
	};

	/**
	 * Draws a frame's pieces in software twice, once in the order given (the painter's) and once by view depth, their
	 * texels read from VRAM (a transparent texel draws nothing, a semi-transparent one blends), and finds the pixels where
	 * something farther is drawn over something more than Margin view units nearer, but for what the judge's sorting puts
	 * right. One shape per piece; Background is the colour (0xRRGGBB) the screen is cleared to.
	 */
	FSortingFrameAudit AuditSorting(std::vector<FSortingPiece> Pieces, const std::vector<FSortingShape>& Shapes, const FVram& Vram,
		const FSortingScreen& Screen, uint32 Background, float Margin, const ISortingJudge& Judge);

	/** Sets a shape's texture from a GPU code (bit 0 raw texture, bit 1 semi-transparent) and its page and CLUT attributes. */
	void SetSortingTexture(FSortingShape& Shape, uint32 Code, uint16 Page, uint16 Clut, bool bTextured);

	/** Sets the texels a shape may read from its corners' coordinates (edge to edge: its far edges one past its last texels). */
	void BoundSortingTexels(FSortingShape& Shape, bool bEdgeToEdge);

	/** Sets a shadow's floor from its first three corners, its normal turned toward the eye. */
	void SetSortingFloor(FSortingShape& Shape);

	/** How high above its floor (SetSortingFloor) a shadow has a point in view space. */
	float HeightAboveFloor(const FSortingShape& Shadow, const std::array<float, 3>& Point);

	/** What a kind of piece is called in a report: "scenery", "gear shadow", "ground", ... */
	const char* SortingPieceName(ESortingPiece Kind);

	/** The class a conflict falls in by what its two pieces are: "shadow over gear", "scenery over scenery", ... */
	std::string SortingClass(ESortingPiece Over, ESortingPiece Under);
}
