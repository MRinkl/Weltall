#pragma once

#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/GteVectors.h"

#include <array>

namespace Weltall
{
	/** The three directional lights and the ambient light as the kernel's light routines keep them. */
	struct FMdlLights
	{
		/** Row i: the direction light i shines from, normalized (the kernel reverses the direction it is given). */
		FGteMatrix Directions;
		/** Column i: light i's color (4096 is full). */
		FGteMatrix Colors;
		/** The GTE's back color: the ambient light in 4096ths. */
		std::array<int32, 3> Ambient{};

		/** Light Index (0 to 2) shining along Direction, with Color as the color matrix takes it. */
		WELTALLCORE_API void SetDirectional(int32 Index, const FGteLongVector& Direction, const FGteVector& Color);

		/** The ambient light from a color given in 16ths (the kernel keeps its low 16 bits, then takes a 16th). */
		WELTALLCORE_API void SetAmbient(const FGteVector& Color);

		/**
		 * Loads the GTE's lighting as the kernel's light matrix routine does: the light matrix is the
		 * directions turned by Rotation (so the lights follow it), with the colors and ambient light.
		 */
		WELTALLCORE_API void Load(FGte& Gte, const FGteMatrix& Rotation) const;
	};
}
