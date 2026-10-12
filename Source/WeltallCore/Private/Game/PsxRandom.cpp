#include "Game/PsxRandom.h"

#include "Foundation/ProcessorDivide.h"

namespace Weltall
{
	uint8 RandomByteBetween(FPsxRandom& Random, uint8 Low, uint8 High)
	{
		if (Low == 0xFF)
		{
			return 0xFF;
		}
		if (High == 0)
		{
			return 0;
		}
		if (Low == High)
		{
			return Low;
		}
		const int32 Span = static_cast<int32>(High) - static_cast<int32>(Low);
		if (Span < 0xFF)
		{
			return static_cast<uint8>(Low + ProcessorRemainder(Random.Next() & 0xFF, Span + 1));
		}
		return Random.NextByte();
	}
}
