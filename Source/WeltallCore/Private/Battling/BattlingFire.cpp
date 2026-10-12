#include "Battling/BattlingFire.h"

#include "Game/PsxRandom.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int16 FireLit = 0x180;
		constexpr int32 FireSeedRow = 47;
		/** What a cell loses as it rises, once it is warm enough to lose it. */
		constexpr int32 FireCooling = 3;
	}

	void FBattlingFire::Init()
	{
		bBurning = true;
		Current.fill(0);
		Next.fill(0);
		Texture.fill(0);
	}

	void FBattlingFire::Step(FPsxRandom& Random)
	{
		if (!bBurning)
		{
			return;
		}
		// A cell in four is lit, one in four put out, the others take the cell's to their left.
		int16 Seed = 0;
		for (int32 Cell = 0; Cell < Width; ++Cell)
		{
			const int32 Drawn = Random.Next() & 3;
			if (Drawn == 0)
			{
				Seed = FireLit;
			}
			else if (Drawn == 1)
			{
				Seed = 0;
			}
			Next[static_cast<std::size_t>((FireSeedRow + 1) * Width + Cell)] = Seed;
			Next[static_cast<std::size_t>(FireSeedRow * Width + Cell)] = Seed;
		}
		// The cells are read as one run of rows, so a row's last cell takes the next row's first as its right neighbour.
		for (int32 Row = FireSeedRow; Row > 1; --Row)
		{
			for (int32 Cell = 1; Cell < Width; ++Cell)
			{
				const std::size_t At = static_cast<std::size_t>(Row * Width + Cell);
				int32 Mean = (Current[At - Width] + Current[At + 1] + Current[At - 1] + Current[At + Width + 1] + Current[At + Width - 1]) / 5;
				if (Mean > FireCooling)
				{
					Mean -= FireCooling;
				}
				Next[At - Width] = static_cast<int16>(Mean);
			}
		}
		std::copy_n(Next.begin(), Texture.size(), Current.begin());
		for (std::size_t Cell = 0; Cell < Texture.size(); ++Cell)
		{
			Texture[Cell] = static_cast<uint8>(Next[Cell]);
		}
	}
}
