#include "Field/FieldCutscenes.h"

#include "Field/EventMachine.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The camera's mode while it moves along the scripts' paths. */
		constexpr int16 ScriptMode = 1;
		/** A colour layer that subtracts: at full in every channel it leaves the screen black. */
		constexpr uint16 Subtractive = 2;
		constexpr uint8 FullChannel = 0xFF;

		bool TalkUnderWay(const FEventContext& Context)
		{
			return std::any_of(Context.States.begin(), Context.States.end(), [](const FEventActorState& State)
				{
					return std::any_of(State.Threads.begin(), State.Threads.end(),
						[](const FEventThread& Thread) { return !Thread.IsFree() && Thread.Script == FEventMachine::TalkScript; });
				});
		}

		/** Whether the map has an actor the pad walks: the party's leader stands on it. */
		bool HasLeader(const FEventContext& Context)
		{
			return std::any_of(Context.States.begin(), Context.States.end(),
				[](const FEventActorState& State) { return (State.Flags0 & EventActorFlags0::PadControlled) != 0; });
		}

		bool ScreenHidden(const FEventContext& Context)
		{
			if (Context.SettleTicks != 0)
			{
				return true;
			}
			return std::any_of(Context.Render.Fades.begin(), Context.Render.Fades.end(), [](const FFieldFade& Fade)
				{
					const FMdlColor Color = Fade.DrawnColor();
					return Fade.bActive && Fade.BlendMode == Subtractive && Color.R == FullChannel && Color.G == FullChannel && Color.B == FullChannel;
				});
		}
	}

	void FFieldCutscenes::Watch(const FEventContext& Context)
	{
		// A map the leader is not on is nobody's to walk, and its scripts need hold no one: a memory, a view of elsewhere.
		if (!HasLeader(Context))
		{
			FreeTicks = 0;
			bInCutscene = true;
			return;
		}
		const bool bHidden = ScreenHidden(Context);
		const bool bCameraOnPaths = Context.Camera.Mode == ScriptMode;
		if (Context.PlayerLock == 0)
		{
			bCameraFollowed = !bCameraOnPaths;
			FreeTicks = std::min(FreeTicks + 1, EndTicks);
			if (FreeTicks >= EndTicks || bHidden)
			{
				bInCutscene = false;
			}
			return;
		}
		FreeTicks = 0;
		if (!bInCutscene && !TalkUnderWay(Context))
		{
			bInCutscene = bHidden || (bCameraFollowed && bCameraOnPaths);
		}
	}
}
