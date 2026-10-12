#pragma once

#include "Foundation/Types.h"
#include "Graphics/MdlModel.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * A VRAM rectangle the scripts scroll band by band (DA): every tick each band's offset moves by its
	 * speed, and the band is copied to the destination rotated left by the offset's whole texels,
	 * wrapping within the rectangle's width (flowing water, conveyors).
	 */
	struct FFieldLineScroll
	{
		int16 X = 0;
		int16 Y = 0;
		/** Never 0: every tick's copy takes the offset modulo the width. */
		uint16 Width = 0;
		int16 Height = 0;
		/** The rows of each band: the height over the bands, truncated. */
		int16 BandHeight = 0;
		int16 ToX = 0;
		int16 ToY = 0;
		/** Per band (1 to 7FFFh of them): how far it moves each tick, in 16ths of a texel. */
		std::vector<int8> Speeds;
		/** Per band: how far it has moved, in 16ths of a texel, wrapping at 16 bits. */
		std::vector<int16> Offsets;

		/** A scroller as DA makes it: every band at the same speed and offset 0. */
		WELTALLCORE_API static FFieldLineScroll Make(int16 X, int16 Y, uint16 Width, int16 Height, uint16 Bands, int16 ToX, int16 ToY, int8 Speed);

		/** The band's offset in whole texels, as the copy takes it: the 16ths' sign kept into 16 bits, modulo the width. */
		WELTALLCORE_API uint16 BandShift(std::size_t Band) const;
	};

	/** How far the scripts have scrolled an actor's model's texture coordinates (FE 1B), each corner's u and v modulo 256. */
	struct FFieldModelUvScroll
	{
		uint16 Actor = 0;
		FMdlUv Offset;
	};

	/** A cell of the screen as the screen wave draws it: the original screen's 16 x 16 pixels at its row and column, stretched over four corners. */
	struct FFieldScreenWaveCell
	{
		/** Top left, top right, bottom left, bottom right: x and y on the 320 x 224 screen. */
		std::array<std::array<int32, 2>, 4> Corners{};
	};

	/**
	 * The screen wave (FE 26, FE 27): the scene drawn so far is drawn again through a grid of 16-pixel
	 * cells whose vertical edges sway across by a cosine of their column and whose horizontal edges
	 * sway down by a cosine of their row, the phases moving each tick. Its six parameters move toward
	 * the scripts' values over a number of ticks, and a fade to nothing can switch it off at its end.
	 */
	struct FFieldScreenWave
	{
		static constexpr int32 Columns = 20;
		static constexpr int32 Rows = 14;
		static constexpr int32 CellSize = 16;
		static constexpr int32 ScreenWidth = Columns * CellSize;
		static constexpr int32 ScreenHeight = Rows * CellSize;
		/** The columns a wide view's grid has each side of the screen's: past the widest view the port shows (32:9), so its outer edges sway out of sight. */
		static constexpr int32 WideMargin = 18;
		static constexpr int32 WideColumns = Columns + 2 * WideMargin;

		/** The parameters, in their order: amplitudes, the phase steps from cell to cell, the phases' speeds per tick. */
		enum EParameter : int32
		{
			AmplitudeX,
			AmplitudeY,
			ColumnStep,
			RowStep,
			SpeedX,
			SpeedY,
			ParameterCount,
		};

		bool bOn = false;
		/** The fade under way ends in the wave switched off (FE 27's fade out). */
		bool bOffWhenDone = false;
		/** Ticks the parameters still move for. */
		int16 Ticks = 0;
		/** 16.16 each; the wave uses their whole parts. */
		std::array<int32, ParameterCount> Parameters{};
		std::array<int32, ParameterCount> Steps{};
		/** The phases (4096 a turn) of the columns' and the rows' cosines. */
		int16 PhaseX = 0;
		int16 PhaseY = 0;
		/** The last tick drew the screen through the wave (so does the tick that switches it off at a fade's end). */
		bool bDrawn = false;

		/**
		 * Sets the parameters moving toward Targets (whole values) over Ticks ticks, dividing as the
		 * processor does; no ticks takes one.
		 */
		WELTALLCORE_API void MoveTo(const std::array<int32, ParameterCount>& Targets, int32 InTicks);
		/** One tick, as the field runs it among the drawing: the fade's step, then the phases' (only while it is on). */
		WELTALLCORE_API void Tick();

		int16 WholePart(EParameter Parameter) const { return static_cast<int16>(Parameters[Parameter] >> 16); }
		/** How far the tick's drawing moves the edge left of Column (1 to 19) across; Column 19's also moves the bottom rows' right edge. */
		WELTALLCORE_API int16 ColumnOffset(int32 Column) const;
		/** How far the tick's drawing moves the edge above Row (1 to 13) down. */
		WELTALLCORE_API int16 RowOffset(int32 Row) const;
		/**
		 * The cells the tick draws, row after row: the edges swayed, the screen's left, top and
		 * bottom edges fixed, and its right edge fixed except in the last three rows.
		 */
		WELTALLCORE_API void BuildCells(std::span<FFieldScreenWaveCell, Columns * Rows> Cells) const;
		/**
		 * The cells of a view wider than the screen, row after row from WideMargin columns left of it:
		 * the same cosines carried on across the view, every vertical edge swayed by its own column's,
		 * the screen's own left and right edges among them, so no line shows where the screen ends.
		 */
		WELTALLCORE_API void BuildWideCells(std::span<FFieldScreenWaveCell, WideColumns * Rows> Cells) const;
	};
}
