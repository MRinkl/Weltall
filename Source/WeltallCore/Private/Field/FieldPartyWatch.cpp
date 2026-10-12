#include "Field/FieldPartyWatch.h"

#include "Field/EventContext.h"
#include "Field/FieldArchive.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr uint8 NoCharacter = 0xFF;

		const FEventActorState* StateOf(const FEventContext& Context, uint8 Actor)
		{
			return Actor < Context.States.size() ? &Context.States[Actor] : nullptr;
		}
	}

	bool FFieldPartyWatch::Follows(const FEventContext& Context)
	{
		const FEventActorState* Leader = StateOf(Context, Context.PartyActors[0]);
		if (Leader == nullptr || (Leader->Flags0 & EventActorFlags0::PadControlled) == 0 || Context.Controls.bLeaderApart)
		{
			return false;
		}
		for (std::size_t Slot = 1; Slot < Context.PartyCharacters.size(); ++Slot)
		{
			if (Context.PartyCharacters[Slot] == NoCharacter)
			{
				continue;
			}
			const uint8 Actor = Context.PartyActors[Slot];
			const FEventActorState* Member = StateOf(Context, Actor);
			if (Member == nullptr || Actor >= Context.Actors.size() || (Member->Flags0 & EventActorFlags0::PartyFollower) == 0
				|| (Context.Actors[Actor].Flags & FieldActorFlag::Hidden) != 0)
			{
				return false;
			}
		}
		// A member a message is shown at speaks as itself, the leader too: the scene is about who they are.
		for (const FFieldMessageWindow& Window : Context.Messages.Windows)
		{
			if (!Window.bOpen)
			{
				continue;
			}
			for (std::size_t Slot = 0; Slot < Context.PartyCharacters.size(); ++Slot)
			{
				if (Context.PartyCharacters[Slot] != NoCharacter && Window.Speaker == Context.PartyActors[Slot])
				{
					return false;
				}
			}
		}
		return true;
	}

	void FFieldPartyWatch::Watch(const FEventContext& Context)
	{
		const bool bFollows = Follows(Context);
		if (!bFollows)
		{
			FollowedTicks = 0;
			bTogether = false;
		}
		else if (!bKnown || Context.SettleTicks != 0)
		{
			// Nothing of the party has been shown yet: it is drawn as it is.
			FollowedTicks = EndTicks;
			bTogether = true;
		}
		else
		{
			FollowedTicks = std::min(FollowedTicks + 1, EndTicks);
			bTogether = bTogether || FollowedTicks >= EndTicks;
		}
		bKnown = true;
	}
}
