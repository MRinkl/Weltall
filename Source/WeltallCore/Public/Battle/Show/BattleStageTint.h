#pragma once

#include "Foundation/Types.h"
#include "Graphics/Vram.h"

#include <array>
#include <span>

namespace Weltall
{
	class FBattleObjects;
	struct FBattleStageBackdrop;
	struct FBattleTintLayers;
	struct FGearTextureBlock;

	/**
	 * What the stage's tint layers recolour, kept aside while a tint fade lasts: the stage's palettes, the floor dome's
	 * bands, the first panorama's bands and the clear colour. As a fade begins anew the palettes are read from VRAM into the
	 * objects' tint palettes, which the stage's running CLUT animations feed from then on in place of VRAM, and the colours
	 * are kept; each frame after a layer changed or the palettes were fed, what was kept is recoloured through each layer
	 * switched on, in order; as the fade ends everything is put back. The original keeps each band's colours in both draw
	 * buffers' packets, recoloured alike.
	 */
	class FBattleStageTint
	{
	public:
		/** The images' bounds: the palettes are the rectangle around the CLUTs of the stage's images. */
		WELTALLCORE_API void Measure(std::span<const FGearTextureBlock> Images);
		const FVramRect& Palettes() const { return PaletteRect; }

		/**
		 * The tint's start on the stage, as a tint fade begins anew (the fade switches the layers off): the palettes kept
		 * aside, whose start clears the layers' changed flag, and the colours kept.
		 */
		WELTALLCORE_API void Begin(FBattleTintLayers& Layers, FBattleObjects& Objects, const FBattleStageBackdrop& Backdrop, std::span<const uint8, 3> Clear);
		/**
		 * The tint applied, before the stage is drawn: when a layer changed or the palettes were fed, and the palettes
		 * are kept aside, the kept palettes and colours recoloured through the layers into the palettes' second buffer
		 * (then loaded into VRAM), the backdrop and the clear colour. With no layer on, the palettes go back as kept and the
		 * colours stay as they are.
		 */
		WELTALLCORE_API void Apply(FBattleTintLayers& Layers, FBattleObjects& Objects, FBattleStageBackdrop& Backdrop, std::span<uint8, 3> Clear);
		/** The tint's end, as the fade's task ends: the palettes back in VRAM, the kept colours put back. */
		WELTALLCORE_API void End(FBattleObjects& Objects, FBattleStageBackdrop& Backdrop, std::span<uint8, 3> Clear);

	private:
		/** The colours recoloured: the dome's sky, graded band's top, ground, glow's top and bottom; the panorama's three bands; the clear colour. */
		static constexpr std::size_t ColourCount = 9;
		using FColours = std::array<std::array<uint8, 3>, ColourCount>;

		static FColours Gather(const FBattleStageBackdrop& Backdrop, std::span<const uint8, 3> Clear);
		static void Scatter(const FColours& Colours, FBattleStageBackdrop& Backdrop, std::span<uint8, 3> Clear);

		FVramRect PaletteRect;
		FColours Kept{};
	};
}
