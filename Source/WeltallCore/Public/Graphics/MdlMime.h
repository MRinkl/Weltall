#pragma once

#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/MdlModel.h"

#include <span>

namespace Weltall
{
	/**
	 * One update of a part's MIMe animation, as the kernel runs it on the vertices the part is drawn
	 * with: each vertex of the reset list back to the part's own, then each key whose weight is not 0
	 * adds its deltas times the weight >> 12 (4096 is the whole delta), 16 bits a component. Weights
	 * holds the keys' weights in order; the keys past them weigh 0. The part's normals are not moved: a part whose MIMe moves them is not
	 * handled.
	 */
	WELTALLCORE_API void AnimateMdlMime(const FMdlPart& Part, std::span<const int32> Weights, std::span<FGteVector> Vertices);
}
