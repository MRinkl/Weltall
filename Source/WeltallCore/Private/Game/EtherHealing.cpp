#include "Game/EtherHealing.h"

namespace Weltall
{
	namespace
	{
		constexpr uint16 WeakGuard = 0x100;
		constexpr uint16 StrongGuard = 0x200;
	}

	int16 EtherHealing(uint8 Ether, uint8 Power, uint16 HealerEtherBuffs, uint16 TargetGuard)
	{
		int16 Heal = static_cast<int16>(Ether * Power);
		if ((HealerEtherBuffs & EtherMarks::EtherDoubler) != 0)
		{
			Heal = static_cast<int16>(Heal * 2);
		}
		if ((TargetGuard & WeakGuard) != 0)
		{
			Heal = static_cast<int16>(Heal * 7 / 10);
		}
		if ((TargetGuard & StrongGuard) != 0)
		{
			Heal = static_cast<int16>(Heal * 13 / 10);
		}
		return Heal;
	}

	uint8 EtherCost(uint8 Cost, uint16 EtherBuffs, uint16 EquipmentFlags)
	{
		if ((EtherBuffs & EtherMarks::EtherDoubler) != 0)
		{
			Cost = static_cast<uint8>(Cost << 1);
		}
		if ((EquipmentFlags & EtherMarks::Economether) != 0)
		{
			Cost = static_cast<uint8>((Cost + 1) >> 1);
		}
		return Cost;
	}
}
