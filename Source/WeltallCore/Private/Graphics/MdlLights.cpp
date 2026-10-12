#include "Graphics/MdlLights.h"

namespace Weltall
{
	void FMdlLights::SetDirectional(int32 Index, const FGteLongVector& Direction, const FGteVector& Color)
	{
		const std::size_t Light = static_cast<std::size_t>(Index);
		const FGteVector Normal = VectorNormalS({-Direction[0], -Direction[1], -Direction[2]});
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Directions.M[Light][Axis] = Normal[Axis];
			Colors.M[Axis][Light] = Color[Axis];
		}
	}

	void FMdlLights::SetAmbient(const FGteVector& Color)
	{
		for (std::size_t Channel = 0; Channel < 3; ++Channel)
		{
			// SetBackColor takes the color times 16 again.
			Ambient[Channel] = (static_cast<uint16>(Color[Channel]) >> 4) << 4;
		}
	}

	void FMdlLights::Load(FGte& Gte, const FGteMatrix& Rotation) const
	{
		Gte.LightMatrix = MulMatrix(Directions, Rotation);
		Gte.ColorMatrix = Colors;
		Gte.BackColor = Ambient;
	}
}
