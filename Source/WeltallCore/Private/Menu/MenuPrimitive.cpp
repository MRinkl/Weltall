#include "Menu/MenuPrimitive.h"

#include "Graphics/GteVectors.h"

namespace Weltall
{
	namespace
	{
		constexpr int32 ScreenCentreX = 160;
		constexpr int32 ScreenCentreY = 112;
		constexpr uint16 ProjectionDistance = 0x200;
		/** libgpu's texture window for a whole page: no masking, no offset. */
		constexpr uint32 WholePageWindow = 0xE2000000u;
	}

	FMenuPrimitive FMenuPrimitive::MakeDrawMode(uint16 Page)
	{
		FMenuPrimitive Primitive;
		Primitive.Code = MenuGpu::DrawModeCommand;
		Primitive.Words = {MenuGpu::DrawMode(Page), WholePageWindow};
		return Primitive;
	}

	FGte MenuProjection(const FMenuLayerTransform& Transform)
	{
		FGte Gte;
		Gte.Transform = RotationXyz(Transform.Angles);
		Gte.Transform.T = Transform.Translation;
		Gte.OffsetX = ScreenCentreX << 16;
		Gte.OffsetY = ScreenCentreY << 16;
		Gte.ProjectionDistance = ProjectionDistance;
		return Gte;
	}

	void FMenuOrderingTable::TakeInto(std::vector<FMenuPrimitive>& Drawn)
	{
		for (auto Entry = Linked.rbegin(); Entry != Linked.rend(); ++Entry)
		{
			Drawn.insert(Drawn.end(), Entry->rbegin(), Entry->rend());
			Entry->clear();
		}
	}
}
