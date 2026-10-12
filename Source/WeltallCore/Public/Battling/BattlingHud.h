#pragma once

#include "Battling/BattlingArenaData.h"
#include "Menu/MenuPrimitive.h"

#include <array>
#include <vector>

namespace Weltall
{
	class FBattlingFight;
	struct FBattlingFighter;

	/** The blend keys of the HUD's pieces that move between passes: the gauges' pieces, by side. */
	namespace BattlingHudBlendKeys
	{
		constexpr uint32 Hp(uint32 Side, uint32 Piece) { return 0x100 | Side << 2 | Piece; }
		constexpr uint32 Damage(uint32 Side, uint32 Piece) { return 0x110 | Side << 2 | Piece; }
	}

	/**
	 * Draws a bout's HUD as the program does: on each side the HP gauge in three pieces, over the HP the last hits took (red,
	 * fading) and the gauge's grey, the gauge's outline, a black box under the gear's name, the heat in its tube (blinking
	 * when near overheating), the gear's icon and name, the Ether lamp when there is no heat left for Ether, and the rounds
	 * won. Player 1's side is the top left, player 2's turned about the screen. All of it opaque, in the GPU's order.
	 */
	class FBattlingHud
	{
	public:
		/** Both must outlive the HUD. */
		WELTALLCORE_API FBattlingHud(const FBattlingArenaTables& InTables, const FBattlingArenaTextures& InTextures);

		/** Appends the HUD of the pass the bout just ran to Out. */
		WELTALLCORE_API void Draw(const FBattlingFight& Fight, std::vector<FMenuPrimitive>& Out) const;

		/** Whether a pass draws the HUD: not in the replay, the result screen or practice. */
		WELTALLCORE_API static bool IsDrawn(const FBattlingFight& Fight);

	private:
		using FPieces = std::array<FMenuPrimitive, 3>;

		struct FSide
		{
			FPieces Grey;
			/** The gauge's pieces whole, which a gauge shorter than the whole shortens. */
			FPieces Gauge;
			/** The outline, as two lines through four points each. */
			std::array<FMenuPrimitive, 2> Outline;
			FMenuPrimitive NameBox;
			FMenuPrimitive Heat;
			FMenuPrimitive Name;
			FMenuPrimitive Icon;
			FMenuPrimitive Tube;
			FMenuPrimitive Lamp;
			std::array<FMenuPrimitive, 3> WinMarks;
		};

		std::array<FSide, 2> Sides;
		/** The heat picture's first row in its page: the heat's top shows the rows below it. */
		uint8 HeatV = 0;

		/** Appends a gauge of Length (0 to 7Fh) as the program draws it, its pieces in the GPU's order, and their colour and keys. */
		void AddGauge(int32 Side, int32 Length, const FDrawColor& Colour, bool bDamage, std::vector<FMenuPrimitive>& Out) const;
		void AddHeat(int32 Side, const FBattlingFighter& Fighter, uint32 VBlank, std::vector<FMenuPrimitive>& Out) const;
	};
}
