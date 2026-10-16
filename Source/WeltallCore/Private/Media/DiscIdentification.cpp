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
			FKnownDisc{{EDiscRelease::UndubDisc1, 1, "Xenogears (USA undub) Disc 1"},
				"SLUS_006.64", "1e21636bc067bd3873b78be3d72978c8038daca0"},
			FKnownDisc{{EDiscRelease::UndubDisc2, 2, "Xenogears (USA undub) Disc 2"},
				"SLUS_006.69", "2d9a51f9d6fc6855118fc217002edaef30dadec6"},
			FKnownDisc{{EDiscRelease::JapanDisc1, 1, "Xenogears (Japan) Disc 1"},
				"SLPS_011.60", "3f9633f80213427f55894cda9fc8c3346ecf31b4"},
			FKnownDisc{{EDiscRelease::JapanDisc2, 2, "Xenogears (Japan) Disc 2"},
				"SLPS_011.61", "15dae4421e0db5d864b5cada59dfcc786f323c9b"},
		};
	}

	std::string_view DiscReleaseId(EDiscRelease Release)
	{
		switch (Release)
		{
		case EDiscRelease::UsaDisc1: return "usa.disc1";
		case EDiscRelease::UsaDisc2: return "usa.disc2";
		case EDiscRelease::UndubDisc1: return "usa.undub.disc1";
		case EDiscRelease::UndubDisc2: return "usa.undub.disc2";
		case EDiscRelease::JapanDisc1: return "jpn.disc1";
		case EDiscRelease::JapanDisc2: return "jpn.disc2";
		}
		return {};
	}

	uint8 DiscNumberOfRelease(EDiscRelease Release)
	{
		return Release == EDiscRelease::UsaDisc2 || Release == EDiscRelease::UndubDisc2 || Release == EDiscRelease::JapanDisc2 ? 2 : 1;
	}

	bool IsPlayableRelease(EDiscRelease Release)
	{
		return Release == EDiscRelease::UsaDisc1 || Release == EDiscRelease::UsaDisc2 ||
			Release == EDiscRelease::UndubDisc1 || Release == EDiscRelease::UndubDisc2;
	}

	EDiscRelease GameplayRelease(EDiscRelease Release)
	{
		return IsPlayableRelease(Release) ? (DiscNumberOfRelease(Release) == 2 ? EDiscRelease::UsaDisc2 : EDiscRelease::UsaDisc1) : Release;
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
