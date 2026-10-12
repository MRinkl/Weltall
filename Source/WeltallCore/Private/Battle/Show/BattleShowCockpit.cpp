#include "Battle/Show/BattleShowCockpit.h"

#include "Game/PsxRandom.h"

#include <cstdlib>

namespace Weltall
{
	namespace
	{
		constexpr int32 Whole = 0x100;
		/** A roll above this (of 0-99) sends the reticle on: 8 of the 256 bytes the roll maps. */
		constexpr uint8 Restless = 0x5F;

		/** 256ths to whole pixels, toward zero. */
		int32 Pixels(int32 Moved)
		{
			return (Moved < 0 ? Moved + 0xFF : Moved) >> 8;
		}
	}

	void FBattleCockpitReticle::Aim(int32 FromX, int32 FromY, int32 ToX, int32 ToY, FPsxRandom& Random)
	{
		StartX = FromX;
		StartY = FromY;
		TargetX = ToX;
		TargetY = ToY;
		if (ToX == FromX || ToY == FromY)
		{
			return;
		}
		const int32 AcrossX = std::abs(ToX - FromX);
		const int32 AcrossY = std::abs(ToY - FromY);
		bLeft = FromX > ToX;
		bUp = FromY > ToY;
		if (AcrossX < AcrossY)
		{
			StepX = (AcrossX << 8) / AcrossY;
			StepY = Whole;
		}
		else
		{
			StepY = (AcrossY << 8) / AcrossX;
			StepX = Whole;
		}
		MovedX = 0;
		MovedY = 0;
		Speed = RandomByteBetween(Random, 1, 8);
		bArrived = false;
	}

	void FBattleCockpitReticle::Move()
	{
		for (int32 Step = 0; Step < Speed; ++Step)
		{
			MovedX += bLeft ? -StepX : StepX;
			MovedY += bUp ? -StepY : StepY;
		}
		// Arrived once past the target along the major axis.
		if (StepX == Whole)
		{
			bArrived = bArrived || (bLeft ? Pixels(MovedX) + StartX < TargetX : TargetX < Pixels(MovedX) + StartX);
		}
		else
		{
			bArrived = bArrived || (bUp ? Pixels(MovedY) + StartY < TargetY : TargetY < Pixels(MovedY) + StartY);
		}
	}

	bool FBattleCockpitReticle::Frame(FPsxRandom& Random)
	{
		if (bArrived)
		{
			if (RandomByteBetween(Random, 0, 99) > Restless)
			{
				const uint8 Column = RandomByteBetween(Random, 0, 4);
				const uint8 Row = RandomByteBetween(Random, 0, 4);
				Aim(X, Y, GridX[Column], GridY[Row], Random);
			}
			if (bArrived)
			{
				return false;
			}
		}
		Move();
		X = static_cast<int16>(static_cast<uint16>(StartX) + static_cast<int16>(Pixels(MovedX)));
		Y = static_cast<int16>(static_cast<uint16>(StartY) + static_cast<int16>(Pixels(MovedY)));
		if (bArrived)
		{
			return false;
		}
		for (uint8& Digit : Digits)
		{
			Digit = RandomByteBetween(Random, 0, 9);
		}
		return true;
	}
}
