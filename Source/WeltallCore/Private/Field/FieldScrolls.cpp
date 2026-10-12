#include "Field/FieldRenderState.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** A copy with no width or height moves nothing: the library refuses it before the GPU sees it. */
		void AddMove(std::vector<FFieldVramOperation>& Moves, const FVramRect& Rect, int16 ToX, int16 ToY)
		{
			if (Rect.Width != 0 && Rect.Height != 0)
			{
				FFieldVramOperation Move;
				Move.Rect = Rect;
				Move.ToX = ToX;
				Move.ToY = ToY;
				Moves.push_back(Move);
			}
		}
	}

	FFieldLineScroll FFieldLineScroll::Make(int16 X, int16 Y, uint16 Width, int16 Height, uint16 Bands, int16 ToX, int16 ToY, int8 Speed)
	{
		FFieldLineScroll Scroll;
		Scroll.X = X;
		Scroll.Y = Y;
		Scroll.Width = Width;
		Scroll.Height = Height;
		Scroll.BandHeight = static_cast<int16>(Height / static_cast<int16>(Bands));
		Scroll.ToX = ToX;
		Scroll.ToY = ToY;
		Scroll.Speeds.assign(Bands, Speed);
		Scroll.Offsets.assign(Bands, 0);
		return Scroll;
	}

	uint16 FFieldLineScroll::BandShift(std::size_t Band) const
	{
		const uint16 Texels = static_cast<uint16>(Offsets[Band] >> 4);
		return static_cast<uint16>(Texels % Width);
	}

	void FFieldRenderState::TickLineScrolls(bool bWithinWidth)
	{
		LineScrollMoves.clear();
		for (FFieldLineScroll& Scroll : LineScrolls)
		{
			int16 Y = Scroll.Y;
			int16 ToY = Scroll.ToY;
			// The width in 16ths, when an offset can hold it.
			const int32 Period = Scroll.Width * 16;
			for (std::size_t Band = 0; Band < Scroll.Offsets.size(); ++Band)
			{
				if (bWithinWidth && Period <= 0x8000)
				{
					const int32 Moved = (Scroll.Offsets[Band] + Scroll.Speeds[Band]) % Period;
					Scroll.Offsets[Band] = static_cast<int16>(Moved < 0 ? Moved + Period : Moved);
				}
				else
				{
					Scroll.Offsets[Band] = static_cast<int16>(Scroll.Offsets[Band] + Scroll.Speeds[Band]);
				}
				const uint16 Shift = Scroll.BandShift(Band);
				const uint16 Rest = static_cast<uint16>(Scroll.Width - Shift);
				// The band's first Shift texels go to its end, the rest to its start.
				AddMove(LineScrollMoves, {Scroll.X, Y, static_cast<int16>(Shift), Scroll.BandHeight}, static_cast<int16>(Scroll.ToX + Rest), ToY);
				AddMove(LineScrollMoves, {static_cast<int16>(Scroll.X + Shift), Y, static_cast<int16>(Rest), Scroll.BandHeight}, Scroll.ToX, ToY);
				Y = static_cast<int16>(Y + Scroll.BandHeight);
				ToY = static_cast<int16>(ToY + Scroll.BandHeight);
			}
		}
	}

	void FFieldRenderState::ScrollModelUv(uint16 Actor, FMdlUv By)
	{
		auto Found = std::find_if(ModelUvScrolls.begin(), ModelUvScrolls.end(), [Actor](const FFieldModelUvScroll& Scroll) { return Scroll.Actor == Actor; });
		if (Found == ModelUvScrolls.end())
		{
			Found = ModelUvScrolls.insert(ModelUvScrolls.end(), FFieldModelUvScroll{Actor, {}});
		}
		Found->Offset.U = static_cast<uint8>(Found->Offset.U + By.U);
		Found->Offset.V = static_cast<uint8>(Found->Offset.V + By.V);
	}

	FMdlUv FFieldRenderState::ModelUvScroll(uint16 Actor) const
	{
		const auto Found = std::find_if(ModelUvScrolls.begin(), ModelUvScrolls.end(), [Actor](const FFieldModelUvScroll& Scroll) { return Scroll.Actor == Actor; });
		return Found == ModelUvScrolls.end() ? FMdlUv{} : Found->Offset;
	}
}
