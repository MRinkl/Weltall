#pragma once

#include "Graphics/Mdec.h"

namespace Weltall
{
	/**
	 * Takes out of a decoded movie picture what its compression added, no more than its quantization can
	 * explain: the steps at the 8x8 blocks' edges where both sides are smooth, and the ripples beside a sharp
	 * edge within a block. Post-filters in the manner of MPEG-4's (ISO/IEC 14496-2 Annex F), run by the
	 * picture's quantization scale, which is the MPEG quantizer: at the movies' usual scales of 1 and 2 they
	 * change little, at 6 and 7 they smooth what the coarser steps left. Each plane by its own blocks.
	 */
	WELTALLCORE_API void CleanMoviePicture(FMdecPicture& Picture);
}
