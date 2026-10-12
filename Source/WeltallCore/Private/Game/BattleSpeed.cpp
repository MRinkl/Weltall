#include "Game/BattleSpeed.h"

#include <algorithm>
#include <array>

namespace Weltall
{
	namespace
	{
		struct FSpeed
		{
			int32 Rate;
			std::string_view Name;
		};

		constexpr std::array<FSpeed, BattleSpeedCount> Speeds{{{1, ""}, {2, "2x"}, {4, "4x"}}};
	}

	int32 BattleSpeedRate(EBattleSpeed Speed)
	{
		return Speeds[static_cast<std::size_t>(Speed)].Rate;
	}

	std::string_view BattleSpeedName(EBattleSpeed Speed)
	{
		return Speeds[static_cast<std::size_t>(Speed)].Name;
	}

	EBattleSpeed StepBattleSpeed(EBattleSpeed Speed, FPadState Pressed)
	{
		const int32 Faster = (Pressed.Buttons & FPadState::Bit(EPadButton::R1)) != 0 ? 1 : 0;
		const int32 Slower = (Pressed.Buttons & FPadState::Bit(EPadButton::L1)) != 0 ? 1 : 0;
		return static_cast<EBattleSpeed>(std::clamp(static_cast<int32>(Speed) + Faster - Slower, 0, BattleSpeedCount - 1));
	}
}
