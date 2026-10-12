#include "Support/FieldScrollBytes.h"

#include <array>

namespace Weltall::Testing
{
	namespace
	{
		void AddWord(std::vector<uint8>& Bytes, uint32 Value, int32 Size)
		{
			for (int32 Byte = 0; Byte < Size; ++Byte)
			{
				Bytes.push_back(static_cast<uint8>(Value >> (8 * Byte)));
			}
		}

		void AddHalf(std::vector<uint8>& Bytes, int32 Value)
		{
			AddWord(Bytes, static_cast<uint32>(Value), 2);
		}
	}

	std::vector<uint8> LineScrollBytes(const std::vector<FFieldLineScroll>& Scrolls)
	{
		std::vector<uint8> Bytes;
		AddWord(Bytes, static_cast<uint32>(Scrolls.size()), 4);
		for (const FFieldLineScroll& Scroll : Scrolls)
		{
			const int32 Bands = static_cast<int32>(Scroll.Speeds.size());
			AddHalf(Bytes, Bands);
			for (const int8 Speed : Scroll.Speeds)
			{
				Bytes.push_back(static_cast<uint8>(Speed));
			}
			for (const int32 Value : {int32{Scroll.X}, int32{Scroll.Y}, int32{Scroll.Width}, int32{Scroll.Height}, int32{Scroll.BandHeight}, Bands, int32{Scroll.ToX},
					 int32{Scroll.ToY}})
			{
				AddHalf(Bytes, Value);
			}
			for (const int16 Offset : Scroll.Offsets)
			{
				AddHalf(Bytes, Offset);
			}
		}
		return Bytes;
	}

	std::vector<uint8> ScreenWaveBytes(const FFieldScreenWave& Wave)
	{
		std::vector<uint8> Bytes;
		AddHalf(Bytes, Wave.bOn ? 1 : 0);
		AddHalf(Bytes, Wave.bOffWhenDone ? 1 : 0);
		AddHalf(Bytes, Wave.Ticks);
		AddHalf(Bytes, 0);
		for (const std::array<int32, FFieldScreenWave::ParameterCount>* Words : {&Wave.Parameters, &Wave.Steps})
		{
			for (const int32 Value : *Words)
			{
				AddWord(Bytes, static_cast<uint32>(Value), 4);
			}
		}
		AddHalf(Bytes, Wave.PhaseX);
		AddHalf(Bytes, Wave.PhaseY);
		return Bytes;
	}

	std::vector<uint8> ScreenWaveCellBytes(const FFieldScreenWave& Wave)
	{
		std::vector<uint8> Bytes;
		std::array<FFieldScreenWaveCell, FFieldScreenWave::Columns * FFieldScreenWave::Rows> Cells;
		Wave.BuildCells(Cells);
		for (const FFieldScreenWaveCell& Cell : Cells)
		{
			for (const std::array<int32, 2>& Corner : Cell.Corners)
			{
				AddWord(Bytes, static_cast<uint32>(Corner[0]), 4);
				AddWord(Bytes, static_cast<uint32>(Corner[1]), 4);
			}
		}
		return Bytes;
	}

	std::vector<uint8> VramMoveBytes(const std::vector<FFieldVramOperation>& Moves)
	{
		std::vector<uint8> Bytes;
		for (const FFieldVramOperation& Move : Moves)
		{
			for (const int16 Value : {Move.Rect.X, Move.Rect.Y, Move.Rect.Width, Move.Rect.Height, Move.ToX, Move.ToY})
			{
				AddHalf(Bytes, Value);
			}
		}
		return Bytes;
	}
}
