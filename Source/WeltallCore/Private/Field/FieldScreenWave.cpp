#include "Field/FieldScrolls.h"

#include "Foundation/ProcessorDivide.h"
#include "Graphics/Gte.h"

namespace Weltall
{
	namespace
	{
		// The original draws the rows above FirstCopiedRow DrawnLower pixels lower, reading the picture
		// above what it draws, then moves the picture up: their edges wrap as 16 bits down there. It
		// draws the last rows in place from a copy of them taken first, with their own right edge.
		constexpr int32 FirstCopiedRow = 11;
		constexpr int32 DrawnLower = 32;

		int32 Swayed(int32 Edge, int16 Offset)
		{
			return static_cast<int16>(Edge + Offset);
		}
	}

	void FFieldScreenWave::MoveTo(const std::array<int32, ParameterCount>& Targets, int32 InTicks)
	{
		const int32 Divisor = InTicks == 0 ? 1 : InTicks;
		Ticks = static_cast<int16>(Divisor);
		for (std::size_t Parameter = 0; Parameter < Targets.size(); ++Parameter)
		{
			const uint32 Distance = (static_cast<uint32>(Targets[Parameter]) << 16) - static_cast<uint32>(Parameters[Parameter]);
			Steps[Parameter] = ProcessorDivide(static_cast<int32>(Distance), Divisor);
		}
	}

	void FFieldScreenWave::Tick()
	{
		bDrawn = false;
		if (!bOn)
		{
			return;
		}
		if (bOffWhenDone && Ticks < 1)
		{
			Parameters.fill(0);
			bOn = false;
		}
		else if (Ticks >= 1)
		{
			for (std::size_t Parameter = 0; Parameter < Parameters.size(); ++Parameter)
			{
				Parameters[Parameter] = static_cast<int32>(static_cast<uint32>(Parameters[Parameter]) + static_cast<uint32>(Steps[Parameter]));
			}
			--Ticks;
		}
		PhaseY = static_cast<int16>(PhaseY + WholePart(SpeedY));
		PhaseX = static_cast<int16>(PhaseX + WholePart(SpeedX));
		bDrawn = true;
	}

	int16 FFieldScreenWave::ColumnOffset(int32 Column) const
	{
		return static_cast<int16>((GteCos(PhaseX + Column * WholePart(ColumnStep)) * WholePart(AmplitudeX)) >> 12);
	}

	int16 FFieldScreenWave::RowOffset(int32 Row) const
	{
		return static_cast<int16>((GteCos(PhaseY + Row * WholePart(RowStep)) * WholePart(AmplitudeY)) >> 12);
	}

	void FFieldScreenWave::BuildCells(std::span<FFieldScreenWaveCell, Columns * Rows> Cells) const
	{
		std::array<int32, Columns + 1> Xs{};
		for (int32 Column = 1; Column < Columns; ++Column)
		{
			Xs[static_cast<std::size_t>(Column)] = Swayed(Column * CellSize, ColumnOffset(Column));
		}
		Xs[Columns] = ScreenWidth;
		const int32 CopiedRowsRight = Swayed(ScreenWidth, ColumnOffset(Columns - 1));
		const auto Edge = [this](int32 Row, bool bCopied) -> int32
		{
			if (Row == 0 || Row == Rows)
			{
				return Row == 0 ? 0 : ScreenHeight;
			}
			return bCopied ? Swayed(Row * CellSize, RowOffset(Row)) : Swayed(Row * CellSize + DrawnLower, RowOffset(Row)) - DrawnLower;
		};
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			const bool bCopied = Row >= FirstCopiedRow;
			const int32 Top = Edge(Row, bCopied);
			const int32 Bottom = Edge(Row + 1, bCopied);
			for (int32 Column = 0; Column < Columns; ++Column)
			{
				const int32 Left = Xs[static_cast<std::size_t>(Column)];
				const int32 Right = Column + 1 == Columns && bCopied ? CopiedRowsRight : Xs[static_cast<std::size_t>(Column + 1)];
				Cells[static_cast<std::size_t>(Row * Columns + Column)].Corners = {{{Left, Top}, {Right, Top}, {Left, Bottom}, {Right, Bottom}}};
			}
		}
	}

	void FFieldScreenWave::BuildWideCells(std::span<FFieldScreenWaveCell, WideColumns * Rows> Cells) const
	{
		std::array<int32, WideColumns + 1> Xs{};
		for (int32 Edge = 0; Edge <= WideColumns; ++Edge)
		{
			const int32 Column = Edge - WideMargin;
			Xs[static_cast<std::size_t>(Edge)] = Column * CellSize + ColumnOffset(Column);
		}
		std::array<int32, Rows + 1> Ys{};
		for (int32 Row = 1; Row < Rows; ++Row)
		{
			Ys[static_cast<std::size_t>(Row)] = Row * CellSize + RowOffset(Row);
		}
		Ys[Rows] = ScreenHeight;
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			const int32 Top = Ys[static_cast<std::size_t>(Row)];
			const int32 Bottom = Ys[static_cast<std::size_t>(Row + 1)];
			for (int32 Edge = 0; Edge < WideColumns; ++Edge)
			{
				const int32 Left = Xs[static_cast<std::size_t>(Edge)];
				const int32 Right = Xs[static_cast<std::size_t>(Edge + 1)];
				Cells[static_cast<std::size_t>(Row * WideColumns + Edge)].Corners = {{{Left, Top}, {Right, Top}, {Left, Bottom}, {Right, Bottom}}};
			}
		}
	}
}
