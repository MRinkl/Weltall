#include "Field/FieldActorBox.h"

#include "WalkmeshGeometry.h"

namespace Weltall
{
	bool IsInActorBox(const FEventActorState& Actor, int32 X, int32 Z, int32 Margin)
	{
		using namespace WalkmeshGeometry;
		const int32 CentreX = static_cast<int16>(Actor.Position[0] >> 16);
		const int32 CentreZ = static_cast<int16>(Actor.Position[2] >> 16);
		const int32 Left = CentreX - Actor.HalfWidth - Margin;
		const int32 Right = CentreX + Actor.HalfWidth + Margin;
		const int32 Back = CentreZ - Actor.HalfDepth - Margin;
		const int32 Front = CentreZ + Actor.HalfDepth + Margin;
		const uint32 Point = PackPoint(X, Z);
		const uint32 LeftFront = PackPoint(Left, Front);
		const uint32 RightFront = PackPoint(Right, Front);
		const uint32 RightBack = PackPoint(Right, Back);
		const uint32 LeftBack = PackPoint(Left, Back);
		return PackedNclip(LeftFront, RightFront, Point) >= 0 && PackedNclip(RightFront, RightBack, Point) >= 0 && PackedNclip(RightBack, LeftBack, Point) >= 0
			&& PackedNclip(LeftBack, LeftFront, Point) >= 0;
	}
}
