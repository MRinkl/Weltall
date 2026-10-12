#pragma once

#include "Gears/GearFiles.h"
#include "Graphics/GteMath.h"
#include "Media/GameFileIndex.h"

#include <optional>
#include <vector>

namespace Weltall
{
	class FIso9660;
}

namespace Weltall::Testing
{
	/** A disc's file index, libgte's arctangents and every gear model's files, read once for a test. */
	struct FGearDisc
	{
		FGameFileIndex Index;
		FGteArctangents Arctangents;
		std::vector<FGearFiles> Files;
	};

	std::optional<FGearDisc> ReadGearDisc(const FIso9660& Disc);
}
