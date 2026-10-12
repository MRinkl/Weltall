#include "Graphics/GteMatrix.h"

namespace Weltall
{
	FGteMatrix ScaleColumns(const FGteMatrix& Matrix, const std::array<int32, 3>& Scale)
	{
		FGteMatrix Result = Matrix;
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				// The product is formed in 32 bits, shifted, then stored as a 16-bit entry.
				const uint32 Product = static_cast<uint32>(int32{Matrix.M[Row][Column]}) * static_cast<uint32>(Scale[Column]);
				Result.M[Row][Column] = static_cast<int16>(static_cast<uint16>(static_cast<uint32>(static_cast<int32>(Product) >> 12)));
			}
		}
		return Result;
	}
}
