#pragma once

#include "Foundation/Types.h"
#include "Support/StorySweepGround.h"

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Weltall
{
	struct FEventContext;
}

namespace Weltall::Testing
{
	/** How close the leader is stood to an actor. */
	enum class EFieldApproach : uint8
	{
		/** In sight of it and out of its reach: nothing begins while the view comes over. */
		Near,
		/** Where a talk and a touch reach it. */
		Beside,
	};

	/**
	 * Stands the leader by an actor as a player who walked up to it stands: on ground its walk reaches from where it
	 * is with nothing met on its way (FWalkedGround: no body to walk round, no other actor's touch but those of
	 * Crossed, which the walk has met before) and no actor's body covers, facing the actor. False, the leader left
	 * where it was, where no such ground lies around the actor: one on another layer, behind a wall or a body,
	 * across a gap only a jump crosses, or past a touch not yet met. Without Crossed the walk may meet anything:
	 * the floor's joins alone count. With Tried, the stands taken before, the leader is stood apart from each of
	 * them and the stand taken joins them: an actor that answers from one side only is asked from each in turn.
	 */
	bool ApproachFieldActor(FEventContext& Context, int32 Actor, EFieldApproach Approach, const std::set<int32>* Crossed, std::vector<std::pair<int32, int32>>* Tried = nullptr);

	/** Whether ApproachFieldActor would stand the leader by the actor had its walk set off from From, on another floor. */
	bool ReachesFieldActor(const FEventContext& Context, int32 Actor, EFieldApproach Approach, const std::set<int32>* Crossed, const FFieldStanding& From);

	/**
	 * Stands the leader in one of the map's zones, as a player who walked into it stands: a few steps past where the
	 * straight way from the leader's place to the zone's middle crosses its edge (else at the middle, or part of the
	 * way to a corner), on ground the leader's walk reaches as above and no actor's body covers. False, the leader left
	 * where it was, where the zone has no such ground.
	 */
	bool EnterFieldZone(FEventContext& Context, int32 Zone, const std::set<int32>* Crossed);

	/**
	 * Stands the leader short of one of the map's zones, outside it, a walk of a second or two from where it is come
	 * into, and gives the ground point a walk from there into it is aimed at. On the straight way from the leader's
	 * place to the zone's middle, aimed at the middle; or (bByItsWalk) on the way the leader's own walk first comes to
	 * floor of the zone, aimed past that point: the way into a zone whose middle lies off the floor, as a doorway's
	 * does that only the doorstep's edge reaches into. None, the leader left where it was, where the way has no such
	 * ground or the leader is in the zone already.
	 */
	std::optional<std::pair<double, double>> StageBeforeFieldZone(FEventContext& Context, int32 Zone, const std::set<int32>* Crossed, bool bByItsWalk);

	/** Whether StageBeforeFieldZone or EnterFieldZone would stand the leader by the zone had its walk set off from From, on another floor. */
	bool ReachesFieldZone(const FEventContext& Context, int32 Zone, const std::set<int32>* Crossed, const FFieldStanding& From);

	/**
	 * Stands the leader at a ground point its walk reaches from where it is (as above) and no actor's body covers,
	 * facing (FaceX, FaceZ); the party and the view come with it. False, the leader left where it was, where the
	 * walk does not get there.
	 */
	bool StandLeaderAt(FEventContext& Context, int32 X, int32 Z, double FaceX, double FaceZ, const std::set<int32>* Crossed);

	/**
	 * How far the leader's walk gets and what it would have to pass to go further, as a line's detail tells it:
	 * "walks 5120 points from (12,-340) on layer 0, barred by body 12, touch 57": the points of the ground it reaches
	 * (four units apart), then each actor whose body, or whose touch not met before, took floor beside that ground;
	 * none named where nothing but the floor's own ends stops it. Target is the actor walked up to, if any.
	 */
	std::string FieldWalkBarriers(const FEventContext& Context, int32 Target, const std::set<int32>* Crossed);

	/** The middle of a zone, on the ground; the leader's own place where the map has no such zone. */
	std::pair<double, double> FieldZoneMiddle(const FEventContext& Context, int32 Zone);

	bool IsLeaderInFieldZone(const FEventContext& Context, int32 Zone);

	/** An actor's interaction point, on the ground: where a talk or a touch is measured to. */
	std::pair<double, double> FieldActorPoint(const FEventContext& Context, int32 Actor);

	/** Whether the leader stands where the actor's talk (within Gap of touching it, or of its box) would reach it. */
	bool IsLeaderWithin(const FEventContext& Context, int32 Actor, int32 Gap);

	/** The leader's place on the ground. */
	std::pair<double, double> LeaderPoint(const FEventContext& Context);

	/** The pad's buttons held for the leader to run: the release's run button is the pad's Circle, which under the player's Always run walks. */
	uint16 FieldRunButtons(const FEventContext& Context);

	/** Whether the view shows the actor, as the field last judged it. */
	bool IsFieldActorShown(const FEventContext& Context, int32 Actor);

	/** Whether one of the actor's threads runs Script. */
	bool RunsFieldScript(const FEventContext& Context, int32 Actor, int32 Script);
}
