#include "Support/BattleProgramHud.h"

#include "Battle/Show/BattleShowFrame.h"
#include "Foundation/Sha1.h"
#include "Support/MenuFrameDigest.h"

namespace Weltall::Testing
{
	namespace
	{
		/** A draw's record, as the oracle's harness makes it: this code, then the SHA-1 of its packets. */
		constexpr uint8 HudPackets = 0x20;
		/** The kernel's window colour and its blend in battle, as a new game has them. */
		constexpr FBattleHudSettings NewGameSettings{{0x88, 0x76, 0x54}, 2};
	}

	FBattleProgramHud::FBattleProgramHud(const FBattleHudResources& Resources, FMesContext& Text, const FBattleEventFiles* EventFiles)
		: Hud(Resources, Text, NewGameSettings, EventFiles)
	{
	}

	void FBattleProgramHud::Draw(FBattleHudFrame& Frame, bool bHidden)
	{
		Hud.Draw(Frame, bHidden);
		FMenuFrame Packets;
		Packets.Primitives = Frame.Primitives;
		const FSha1::FDigest Sum = FSha1::Hash(CanonicalMenuPackets(Packets));
		Draws.push_back(HudPackets);
		Draws.insert(Draws.end(), Sum.begin(), Sum.end());
		++Count;
	}

	void FBattleProgramHud::Digest(FOracleDigest& Digest)
	{
		Digest.I16(Count);
		Digest.Raw(Draws);
		Draws.clear();
		Count = 0;
	}
}
