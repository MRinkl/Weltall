#pragma once

#include "Graphics/Vram.h"
#include "Menu/MenuPrimitive.h"

#include <array>
#include <vector>

namespace Weltall
{
	/**
	 * A text label: one string of the menu's text rendered into VRAM with the message font, two to a
	 * 13-row strip (one per 2-bit plane), and the textured quad that shows it, on the screen or on the
	 * menu's layer.
	 */
	struct FMenuLabel
	{
		/**
		 * The quad as drawn: the copy of the draw buffer the label was last placed in. The menu keeps a
		 * copy per buffer, its texture set in both and its shading changed in the one drawn, so a label
		 * placed anew in the other buffer shows that buffer's copy.
		 */
		FMenuPrimitive Quad;
		FMenuPrimitive Other;
		int32 Buffer = 0;
		/** Its corners on the layer, for the labels drawn in 3D. */
		std::array<FGteVector, 4> Points{};
		/** The strip's place in VRAM. */
		FVramRect Strip;
		uint8 Plane = 0;
		/** The string's width in pixels. */
		uint8 Width = 0;
	};

	/** A quad's corners on the layer over the screen's rectangle (X, Y, Width, Height), as the menu places its 3D pieces. */
	void SetLayerQuad(std::array<FGteVector, 4>& Points, int32 X, int32 Y, int32 Width, int32 Height);

	/** A textured quad's texels: the rectangle at (U, V), Width by Height. */
	void SetTexelRect(FMenuPrimitive& Quad, int32 U, int32 V, int32 Width, int32 Height);

	/** Each primitive's corners on the layer over its place on the screen, from the one at First on (a sprite's parts drawn flat). */
	void SetLayerSprites(std::vector<FMenuPrimitive>& Parts, std::size_t First = 0);

	/**
	 * A window: the corners and edges of its frame, each a textured quad from the sprite sheet, and its
	 * dark fill, all in 3D, drawn through the layer's transform or at the screen's own scale.
	 */
	struct FMenuWindow
	{
		/** Top left, top right, bottom left, bottom right. */
		std::array<FMenuPrimitive, 4> Corners{};
		/** Two halves of each edge: top, bottom, left, right. */
		std::array<FMenuPrimitive, 8> Edges{};
		/** A scroll bar's track down the left edge, for the lists: its top, its bottom, its length between. */
		std::array<FMenuPrimitive, 3> ScrollBar{};
		FMenuPrimitive Fill;
		FMenuPrimitive FillMode;
		bool bOnLayer = false;
		int32 Entry = 0;
		bool bScrollBar = false;
	};

	/** A window opening by growing 32 pixels a frame each way, centred on its place. */
	struct FMenuWindowGrowth
	{
		int16 X = 0;
		int16 Y = 0;
		uint16 Width = 0;
		uint16 Height = 0;
		uint16 GrownWidth = 0;
		uint16 GrownHeight = 0;
		int32 Entry = 0;
		bool bGrown = false;
		bool bOnLayer = false;
		bool bScrollBar = false;
	};

	/**
	 * The blend keys of what moves between frames, which the display draws in between (unique in a
	 * frame): the background, the windows' pieces, the party panels' parts.
	 */
	namespace MenuBlendKey
	{
		constexpr uint32 Background = 1;
		/** + window * 0x10 + piece: corners 0 to 3, edges 4 to 11, fill 12. */
		constexpr uint32 Windows = 0x100;
		/** + member * 0x100 + part * 0x10 + piece. */
		constexpr uint32 Panels = 0x1000;

		/** Keys First, First + 1... on the primitives, in order. */
		inline void Set(std::vector<FMenuPrimitive>& Primitives, uint32 First)
		{
			for (FMenuPrimitive& Primitive : Primitives)
			{
				Primitive.BlendKey = First++;
			}
		}
	}

	/** An arc of orbs and the labels beside them, as the menu prepares them: the orbs', then the labels' quads. */
	struct FMenuArc
	{
		std::vector<FMenuPrimitive> Orbs;
		std::vector<FMenuPrimitive> Labels;
		/** Drawn dimmed (a screen below it runs), and whether the quads were last made so. */
		bool bDimmed = false;
		bool bDimmedApplied = false;
	};

	/** A sprite of the sheet shown by its first part on the layer: the list cursor, the scroll bar's knob. */
	struct FMenuLayerSprite
	{
		FMenuPrimitive Quad;
		/** The list cursor's animation: its cel (4 down to 0) and the frames the cel has shown. */
		int32 Cel = 4;
		uint8 Frames = 0;
	};
}
