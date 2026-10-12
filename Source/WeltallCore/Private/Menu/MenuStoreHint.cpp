#include "Menu/MenuStoreHint.h"

#include "Menu/MenuSpriteSheet.h"

namespace Weltall
{
	namespace
	{
		constexpr int32 StoresShown = 2;
		constexpr int32 PreviousHint = 0x164;
		constexpr int32 NextHint = 0x165;
		constexpr int32 Orb = 0x115;
		constexpr int32 LitOrb = 0x122;
		constexpr int32 FullScale = 0x1000;
		/** The orbs at half their size, as the menus draw their pointer. */
		constexpr int32 OrbScale = 0x800;
		constexpr int32 OrbSize = 8;
		constexpr int32 OrbGap = 3;
		constexpr int32 ScreenCentreX = 160;
		/** In the strip over the cards' first row. */
		constexpr int32 HintY = 2;
		/** From a hint's origin: where the previous one's box ends and the next one's begins. */
		constexpr int32 PreviousBoxEnd = 24;
		constexpr int32 NextBoxStart = 16;
		constexpr int32 BoxGap = 5;
	}

	void AddStorePairHint(const FMenuStorePair& Stores, const FMenuSpriteSheet& Sheet, std::vector<FMenuPrimitive>& Primitives)
	{
		const int32 Pairs = Stores.StoreCount / StoresShown;
		if (!Stores.bShown || Pairs < 2)
		{
			return;
		}
		const int32 OrbsWidth = Pairs * OrbSize + (Pairs - 1) * OrbGap;
		const int32 OrbsX = ScreenCentreX - OrbsWidth / 2;
		Sheet.Draw(PreviousHint, OrbsX - BoxGap - PreviousBoxEnd, HintY, FullScale, Primitives);
		for (int32 Pair = 0; Pair < Pairs; ++Pair)
		{
			const bool bShown = Pair == Stores.FirstStore / StoresShown;
			Sheet.Draw(bShown ? LitOrb : Orb, OrbsX + Pair * (OrbSize + OrbGap), HintY, OrbScale, Primitives);
		}
		Sheet.Draw(NextHint, OrbsX + OrbsWidth + BoxGap - NextBoxStart, HintY, FullScale, Primitives);
	}
}
