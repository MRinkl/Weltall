#pragma once

#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	struct FEventActorState;
	struct FEventContext;

	/** The field's run button, in its logical words: the leader runs while it is held. */
	inline constexpr uint16 FieldRunButton = 0x40;

	/**
	 * The controller as the kernel hands it to the field: its logical button words (the US release's
	 * face buttons already swapped; the d-pad in bits 12-15) of every display frame since the field's
	 * last tick, ORed together.
	 */
	struct FFieldPadWords
	{
		uint16 Held = 0;
		uint16 Pressed = 0;
		/** Pressed, or held long enough to repeat. */
		uint16 Repeated = 0;
	};

	/** One step of the leader, kept for the party's other members to replay. */
	struct FFollowStep
	{
		uint32 Flags0 = 0;
		uint32 Flags1 = 0;
		std::array<int16, 3> Position{};
		/** The height its sprite lands on. */
		int16 LandingHeight = 0;
		int16 Animation = 0;
		/** 4096 a turn. */
		uint16 Facing = 0;
		std::array<int16, 4> FloorTriangles{};
		/** Its sprite's velocity, 16.16. */
		std::array<int32, 3> Velocity{};
		std::array<int32, 3> FloorNormal{};
		uint32 FloorAttributes = 0;
		uint8 Layer = 0;
	};

	/**
	 * The field's controls: the buttons of the tick, with the masks the scripts and the walkmesh put on
	 * them; the leader's jumps (the action mode's are the movement's, FFieldMovement); and the trail of
	 * the leader's steps that the party's other members walk along, the second member 10 steps behind
	 * and the third 20.
	 */
	struct FFieldControls
	{
		static constexpr int32 TrailLength = 32;

		/** The buttons newly pressed and repeating this tick; the held ones are the context's PadHeld. */
		uint16 Pressed = 0;
		uint16 Repeated = 0;
		/** The buttons the scripts let through. */
		uint16 ScriptMask = 0xFFFF;
		/** The buttons the walkmesh lets through: the d-pad is kept out for a tick after the leader's step is undone. */
		uint16 WalkMask = 0xFFFF;
		/** The tick's masks and the map's first ticks let the run button through: a held one would be read. */
		bool bRunLetThrough = false;
		/** Ticks the leader has stood still on steep ground; past 32 (where it stays), holding the jump button jumps. */
		int16 StuckTicks = 0;
		/** The d-pad walks the opposite way. */
		bool bReversed = false;
		/** The scripts have walked the party into place: the other members do not follow the leader. */
		bool bPartyPlaced = false;
		/**
		 * A touched platform flagged to do so has stopped the leader's fall: no other does until a platform
		 * the leader is not on is checked.
		 */
		bool bTouchLatched = false;
		/**
		 * Doors walked into (FEventContext::bWalkIntoDoors): where the leader stood as the tick began (16.16 x and
		 * z), the door it walks at while held up (-1 none) and for how many ticks, and the door whose script the
		 * push has already started (-1 none) with the direction that push holds: a door answers a push once.
		 */
		std::array<int32, 2> LeaderFrom{};
		int16 PushedDoor = -1;
		int16 PushedTicks = 0;
		int16 AnsweredDoor = -1;
		uint16 AnsweredDirection = 0;

		std::array<FFollowStep, TrailLength> Trail{};
		/** Where the next step is written (each step moves it back by one), then where each follower replays from. */
		std::array<uint32, 3> TrailIndices{};
		/** A step went into the trail this tick. */
		bool bTrailStepped = false;
		/** The followers replay the newest step instead of trailing behind. */
		bool bFollowClose = false;
		/** The leader is not the party's first member: the others only stand idle. */
		bool bLeaderApart = false;

		/** What a map's start resets; the trail's indices and steps are set when the party is placed. */
		WELTALLCORE_API void ResetForMap();
	};

	/**
	 * The tick's buttons as the field reads them before its scripts: the scripts' and the walkmesh's
	 * masks, nothing in the map's first ticks (the context's SettleTicks), and the jump button's press held back while a
	 * battle is on its way.
	 */
	WELTALLCORE_API void ReadFieldInput(FEventContext& Context, const FFieldPadWords& Words);

	/**
	 * The held buttons as the scripts' bit tests (IfButtonHeld) read them: the pad's, but while the player has the party under the
	 * player's Always run (the leader runs unless the run button is held) the run button reads as the leader's run,
	 * held while it runs. Scripts that match the run (Bart's swim in Aveh's waterway, a follower's pace) read it while
	 * the leader moves; those that take it as a button (the card game, a console's cancel) read it while they hold the
	 * party, where it stays the pad's.
	 */
	WELTALLCORE_API uint16 ScriptHeldButtons(const FEventContext& Context);

	/**
	 * One tick of the pad's control over an actor, as the scripts' instructions 0C and A7 run it. The
	 * leader (the actor the pad controls) walks where the d-pad points, measured from the camera, and
	 * jumps; while it walks it rolls toward encounters. Any other actor running it is marked to follow
	 * the leader this tick. Nothing moves while the scripts hold the player or a message offers a
	 * choice. False when an encounter's schedule cannot be drawn (RollFieldEncounter).
	 */
	WELTALLCORE_API bool ControlByPad(FEventContext& Context, FEventActorState& Actor);

	/**
	 * One step of the leader toward a random encounter, as its control step takes one each tick the
	 * d-pad is held: the period's countdown, the encounters it holds, and when one falls due, a
	 * formation drawn by the map's weights and the battle requested. False when a new period's
	 * encounters cannot all fall on different ticks, where the original never finishes drawing them.
	 */
	WELTALLCORE_API bool RollFieldEncounter(FEventContext& Context);
}
