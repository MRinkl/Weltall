#include "Media/DiscIdentification.h"

#include "Foundation/Sha1.h"
#include "Media/PsxExecutable.h"

#include <array>
#include <string_view>

namespace Weltall
{
	namespace
	{
		struct FKnownDisc
		{
			FDiscIdentity Identity;
			std::string_view BootExecutable;
			std::string_view ExecutableSha1;
		};

		constexpr std::array KnownDiscs = {
			FKnownDisc{{EDiscRelease::UsaDisc1, 1, "Xenogears (USA) Disc 1"},
				"SLUS_006.64", "560bbdbeb9264c935294ecad5a3d4ab230a006a9"},
			FKnownDisc{{EDiscRelease::UsaDisc2, 2, "Xenogears (USA) Disc 2"},
				"SLUS_006.69", "a378608389f04456925c6afaccb644ab90eaff30"},
		};
	}

	TResult<FDiscIdentity> IdentifyDisc(const FIso9660& Disc)
	{
		auto BootPath = FindBootExecutable(Disc);
		if (!BootPath)
		{
			return BootPath.Error();
		}
		bool bKnownDisc = false;
		std::string Hash;
		for (const FKnownDisc& Known : KnownDiscs)
		{
			if (*BootPath != Known.BootExecutable)
			{
				continue;
			}
			if (Hash.empty())
			{
				auto Executable = Disc.ReadFile(*BootPath);
				if (!Executable)
				{
					return Executable.Error();
				}
				Hash = FSha1::ToHex(FSha1::Hash(*Executable));
			}
			bKnownDisc = true;
			if (Hash == Known.ExecutableSha1)
			{
				return Known.Identity;
			}
		}
		if (bKnownDisc)
		{
			return MakeError(*BootPath + " is a supported disc, but this release of it is not (executable " + Hash + ")");
		}
		return MakeError("not a supported disc (boot executable " + *BootPath + ")");
	}
}
