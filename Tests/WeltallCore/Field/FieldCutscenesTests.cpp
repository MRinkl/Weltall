#include "Field/EventMachine.h"
#include "Field/FieldCutscenes.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	constexpr uint16 PlayerLocked = 0xFFFF;
	constexpr int16 CameraOnScriptsPaths = 1;

	/** A map of two actors, the first the party's leader, drawn, the player with the party. */
	struct FScene
	{
		FScene()
		{
			Context.States.resize(2);
			for (FEventActorState& State : Context.States)
			{
				State.Reset(0);
			}
			Context.States[0].Flags0 |= EventActorFlags0::PadControlled;
		}

		/** A tick the field leaves as it is: how the watcher takes it. */
		bool Tick()
		{
			Cutscenes.Watch(Context);
			return Cutscenes.InCutscene();
		}

		/** The actor's talk script runs in its first slot, as the talk check starts it; or ends. */
		void Talk(int32 Actor, bool bTalking)
		{
			FEventThread& Thread = Context.States[static_cast<std::size_t>(Actor)].Threads[0];
			Thread.Script = bTalking ? static_cast<uint8>(FEventMachine::TalkScript) : FEventThread::NoScript;
			Thread.SetPriority(bTalking ? FEventMachine::TalkPriority : FEventThread::Free);
		}

		FEventContext Context;
		FFieldCutscenes Cutscenes;
	};
}

TEST_CASE("The scripts holding the party make a cutscene once their camera takes its paths, and it lasts until the player has had the party a while", "[Field]")
{
	FScene Scene;
	CHECK_FALSE(Scene.Tick());
	// Held for a lift's ride, or for a message over the field: nothing shows a staged scene.
	Scene.Context.PlayerLock = PlayerLocked;
	for (int32 Tick = 0; Tick < 30; ++Tick)
	{
		Scene.Context.Messages.OpenMask = Tick < 20 ? 1 : 0;
		CHECK_FALSE(Scene.Tick());
	}
	Scene.Context.Camera.Mode = CameraOnScriptsPaths;
	CHECK(Scene.Tick());
	// It goes on while the scripts hold the party, wherever their camera goes, and through their giving it back for a moment.
	Scene.Context.Camera.Mode = 0;
	CHECK(Scene.Tick());
	Scene.Context.PlayerLock = 0;
	for (int32 Tick = 0; Tick < FFieldCutscenes::EndTicks - 1; ++Tick)
	{
		CHECK(Scene.Tick());
	}
	Scene.Context.PlayerLock = PlayerLocked;
	CHECK(Scene.Tick());
	Scene.Context.PlayerLock = 0;
	for (int32 Tick = 0; Tick < FFieldCutscenes::EndTicks - 1; ++Tick)
	{
		CHECK(Scene.Tick());
	}
	CHECK_FALSE(Scene.Tick());
	// The next hold is a lift's again until it shows otherwise.
	Scene.Context.PlayerLock = PlayerLocked;
	CHECK_FALSE(Scene.Tick());
}

TEST_CASE("A talk the player began is no cutscene, though its scripts hold the party and move the camera", "[Field]")
{
	FScene Scene;
	CHECK_FALSE(Scene.Tick());
	Scene.Talk(1, true);
	Scene.Context.PlayerLock = PlayerLocked;
	Scene.Context.Messages.OpenMask = 1;
	Scene.Context.Camera.Mode = CameraOnScriptsPaths;
	for (int32 Tick = 0; Tick < 30; ++Tick)
	{
		CHECK_FALSE(Scene.Tick());
	}
	// A talk that hands on to a scene: once it is over the scene shows itself as any other.
	Scene.Talk(1, false);
	CHECK(Scene.Tick());
}

TEST_CASE("A map the party's leader is not on is a cutscene from its first tick to its last, with nobody held", "[Field]")
{
	// A memory played with the party's second member alone: no actor is the pad's, and the scripts lock nothing.
	FScene Memory;
	Memory.Context.States[0].Flags0 &= ~EventActorFlags0::PadControlled;
	for (int32 Tick = 0; Tick < 3 * FFieldCutscenes::EndTicks; ++Tick)
	{
		CHECK(Memory.Tick());
	}
	// The next map has the leader again: the watch starts anew with its load.
	Memory.Cutscenes.Reset();
	Memory.Context.States[0].Flags0 |= EventActorFlags0::PadControlled;
	CHECK_FALSE(Memory.Tick());
}

TEST_CASE("A camera the map keeps on the scripts' paths shows no cutscene; a screen black or not yet drawn does", "[Field]")
{
	FScene OnPaths;
	OnPaths.Context.Camera.Mode = CameraOnScriptsPaths;
	CHECK_FALSE(OnPaths.Tick());
	OnPaths.Context.PlayerLock = PlayerLocked;
	CHECK_FALSE(OnPaths.Tick());

	// A map that opens on a scene: the scripts hold the party before its first picture.
	FScene AtTheStart;
	AtTheStart.Context.SettleTicks = FEventContext::MapStartSettleTicks;
	AtTheStart.Context.PlayerLock = PlayerLocked;
	CHECK(AtTheStart.Tick());

	// A scene set up behind a fade to black; it ends at once when the party is given back behind one.
	FScene BehindAFade;
	BehindAFade.Context.PlayerLock = PlayerLocked;
	BehindAFade.Context.Render.bScreenFadedOut = false;
	CHECK_FALSE(BehindAFade.Tick());
	BehindAFade.Context.Render.StartFadeOut(1);
	BehindAFade.Context.Render.TickFades(true);
	CHECK(BehindAFade.Tick());
	BehindAFade.Context.PlayerLock = 0;
	CHECK_FALSE(BehindAFade.Tick());
}
