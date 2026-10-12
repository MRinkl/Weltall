#include "Support/GearDisc.h"

#include "Media/Iso9660.h"
#include "Media/PsxExecutable.h"

namespace Weltall::Testing
{
	std::optional<FGearDisc> ReadGearDisc(const FIso9660& Disc)
	{
		const auto Executable = ReadBootExecutable(Disc);
		if (!Executable)
		{
			return std::nullopt;
		}
		auto Index = FGameFileIndex::Read(*Executable);
		auto Arctangents = FGteArctangents::Read(*Executable);
		if (!Index || !Arctangents)
		{
			return std::nullopt;
		}
		FGearDisc Gears{std::move(*Index), std::move(*Arctangents), {}};
		for (int32 Gear = 0; Gear < FGearFileNumbers::ModelCount; ++Gear)
		{
			auto Files = ReadGearFiles(Disc.Image(), Gears.Index, Gear);
			if (!Files)
			{
				return std::nullopt;
			}
			Gears.Files.push_back(std::move(*Files));
		}
		return Gears;
	}
}
