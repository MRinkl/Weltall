#pragma once

#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	class FPsxRandom;

	/**
	 * The fire behind Battling's menus and versus screen, which a bout then fades in through: rows of heat, lit along the
	 * bottom by rand() and rising as each cell takes the mean of its neighbours below, less a little. Its steps draw from the
	 * game's generator, so the computer's choices follow from how long the fire burned.
	 */
	class FBattlingFire
	{
	public:
		static constexpr int32 Width = 0x70;
		/** The rows the picture keeps; the heat has two more below, the lower never lit. */
		static constexpr int32 PictureRows = 48;
		static constexpr int32 HeatRows = 50;
		/** The rows of the picture the screen shows, stretched over it. */
		static constexpr int32 ShownRows = 43;

		/** The buffers taken and cleared; a fire already burning is cleared too. */
		WELTALLCORE_API void Init();

		/** One step: the bottom rows lit anew (a draw a cell), the heat risen a row. Nothing without its buffers. */
		WELTALLCORE_API void Step(FPsxRandom& Random);

		void Free() { bBurning = false; }

		bool IsBurning() const { return bBurning; }

		/** The picture: a palette index a cell, row-major from the top. */
		const std::array<uint8, Width * PictureRows>& Picture() const { return Texture; }

		const std::array<int16, Width * HeatRows>& Heat() const { return Current; }

	private:
		bool bBurning = false;
		std::array<int16, Width * HeatRows> Current{};
		std::array<int16, Width * HeatRows> Next{};
		std::array<uint8, Width * PictureRows> Texture{};
	};
}
