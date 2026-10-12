#include "Graphics/GteMath.h"

#include "Foundation/LittleEndian.h"
#include "Media/PsxExecutable.h"

namespace Weltall
{
	namespace
	{
		/** Where the kernel keeps libgte's arctangent table (both discs of the USA release). */
		constexpr uint32 ArctangentTableAddress = 0x80057030;
		/** Arguments with any of these bits are divided down before the table lookup. */
		constexpr uint32 LargeArgumentBits = 0x7FE00000;
	}

	TResult<FGteArctangents> FGteArctangents::Read(const FPsxExecutable& Executable)
	{
		FGteArctangents Table;
		const auto Bytes = Executable.Read(ArctangentTableAddress, static_cast<uint32>(Table.Angles.size() * 2));
		if (Bytes.empty())
		{
			return MakeError("the executable holds no arctangent table");
		}
		for (std::size_t Index = 0; Index < Table.Angles.size(); ++Index)
		{
			Table.Angles[Index] = LittleEndian::ReadS16(Bytes, Index * 2);
		}
		return Table;
	}

	int32 GteRatan2(int32 Y, int32 X, const FGteArctangents& Table)
	{
		// Unsigned arithmetic keeps the library's wrap-around for the most negative arguments.
		uint32 Across = static_cast<uint32>(X);
		uint32 Up = static_cast<uint32>(Y);
		const bool bLeft = X < 0;
		const bool bDown = Y < 0;
		if (bLeft)
		{
			Across = 0u - Across;
		}
		if (bDown)
		{
			Up = 0u - Up;
		}
		if (Across == 0 && Up == 0)
		{
			return 0;
		}
		const int32 SignedAcross = static_cast<int32>(Across);
		const int32 SignedUp = static_cast<int32>(Up);
		int32 Angle = 0;
		if (SignedUp < SignedAcross)
		{
			const int32 Ratio = (Up & LargeArgumentBits) != 0 ? SignedUp / (SignedAcross >> 10) : static_cast<int32>(Up << 10) / SignedAcross;
			Angle = Table.Angles[static_cast<std::size_t>(Ratio)];
		}
		else
		{
			const int32 Ratio = (Across & LargeArgumentBits) != 0 ? SignedAcross / (SignedUp >> 10) : static_cast<int32>(Across << 10) / SignedUp;
			Angle = 0x400 - Table.Angles[static_cast<std::size_t>(Ratio)];
		}
		if (bLeft)
		{
			Angle = 0x800 - Angle;
		}
		return bDown ? -Angle : Angle;
	}
}
