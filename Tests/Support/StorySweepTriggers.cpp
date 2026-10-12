#include "Support/StorySweepRun.h"

#include "Field/EventContext.h"
#include "Field/EventMachine.h"
#include "Field/FieldMap.h"
#include "Game/FieldProgram.h"
#include "Game/WorldMapProgram.h"
#include "Support/StorySweepApproach.h"
#include "Support/StorySweepGround.h"
#include "Support/StorySweepWorldMap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace Weltall::Testing
{
	namespace
	{
		constexpr uint16 Cross = FPadState::Bit(EPadButton::Cross);
		/** The frames a talk's button is held for. */
		constexpr int32 TapFrames = 2;
		/** The frames the confirm button is tapped for on a place of the world map, two of every twelve, before the run gives up on it. */
		constexpr int32 PlaceFrames = 300;
		/** The frames a way into a place that the world map takes by itself (the ship under the sand of the Road to Nisan) may go on for. */
		constexpr int32 PlaceVoyageFrames = 3000;
		constexpr int32 TapCycle = 12;
		/** The frames the field's own check is given to start a trigger for the leader beside its actor. */
		constexpr int32 OpeningFrames = 6;
		/** The frames the view is given to come over to an actor the leader was stood near. */
		constexpr int32 ViewFrames = 150;
		/** The frames the view is given before a walk into a zone begins. */
		constexpr int32 ZoneViewFrames = 12;
		/** The frames a walk by the pad may take, and those without a step after which it is given up as stopped. */
		constexpr int32 WalkFrames = 300;
		constexpr int32 StoppedFrames = 24;
		/** The frames a walk goes on inside a zone before it counts as in it. */
		constexpr int32 StepsIntoZone = 6;
		/** The frames the leader then stands in the zone for its scripts to answer, before it walks on through. */
		constexpr int32 ZoneDwellFrames = 24;
		/**
		 * A zone that answers by showing a window to the party still in the player's hands (a mark on a town's paths
		 * names where it leads) is given the confirm button once its window has had these frames to open, held as a
		 * hand holds it: the mark's script asks whether the button is still down as its window closes.
		 */
		constexpr int32 ZoneWindowFrames = 16;
		constexpr int32 ZonePressFrames = 10;
		/** How near a walk comes before a talk's button is pressed, and a touch's contact is counted on: past touching, in units. */
		constexpr int32 TalkGap = 20;
		constexpr int32 TouchGap = 2;
		/** The sides an actor that does not answer is stood beside in turn. */
		constexpr int32 BesideSides = 8;
		/** How often a trigger a body bars the way to is asked again, and the frames waited before each. */
		constexpr int32 BarredTries = 3;
		constexpr int32 BarredWaitFrames = 60;
		/**
		 * How many more such waits are given while a body that bars the way is still walking: Margie leads the party
		 * down the corridor of Nisan's cathedral to its far door (map 100 at the town's step 16) and then follows the
		 * leader, standing in the way for some seconds.
		 */
		constexpr int32 WalkingBarredWaits = 15;
		/**
		 * How often a trigger a gate bars the way to (a floor the map's scripts open and shut by themselves: a
		 * floodgate's cycle) is asked again, each as the gate's word changes, and the frames waited for that at most.
		 */
		constexpr int32 GateTries = 4;
		constexpr int32 GateWaitFrames = 3000;
		/** How near a gate's middle the walk through it goes, in units. */
		constexpr double GateReach = 16.0;
		/**
		 * The frames windows that opened by themselves while the player keeps the party are read on for, the confirm
		 * button tapped, before a talk is pressed for: the press would turn their page instead (two guards talking
		 * among themselves as Bart comes by, map 133 at 70).
		 */
		constexpr int32 ReadOnFrames = 600;
		/** The pad's eight directions, clockwise from up. */
		constexpr uint16 Up = FPadState::Bit(EPadButton::Up);
		constexpr uint16 Right = FPadState::Bit(EPadButton::Right);
		constexpr uint16 Down = FPadState::Bit(EPadButton::Down);
		constexpr uint16 Left = FPadState::Bit(EPadButton::Left);
		constexpr std::array<uint16, 8> PadDirections = {Up, Up | Right, Right, Right | Down, Down, Down | Left, Left, Left | Up};
		/** In an actor's direction: none asked for. Angles count 4096 to a turn. */
		constexpr uint16 NoDirection = 0x8000;
		constexpr double Turn = 6.283185307179586;
		constexpr double AngleUnits = 4096.0;

		/** An actor's first flag word: talking to it starts nothing. */
		constexpr uint32 TalkBlocked = 0x220000;
		/** In an actor's facing: set by the field as a talk or a touch starts its script. */
		constexpr uint16 TurnSet = 0x8000;

		bool AnswersTalk(const FEventActorState& Actor)
		{
			return (Actor.Flags1 & EventActorFlags1::NoTalkScript) == 0 && (Actor.Flags0 & TalkBlocked) == 0;
		}

		/**
		 * Whether the field would look at the actor for a talk or a touch at all: not the leader nor a member that
		 * follows it, not one whose scripts do not run. A member the scripts stand somewhere as one of the map's
		 * people (Citan and Bart in their room at the inn, the night before the tournament) answers like them.
		 */
		bool IsAsked(const FEventContext& Context, int32 Number, const FEventActorState& Actor)
		{
			const auto& Party = Context.PartyActors;
			const bool bMember = std::find(Party.begin(), Party.end(), static_cast<uint8>(Number)) != Party.end();
			return (Actor.Flags0 & EventActorFlags0::Frozen) == 0 && Number != Context.LeaderActor && (!bMember || (Actor.Flags0 & EventActorFlags0::PartyFollower) == 0);
		}

		/** FieldWalkBarriers, then each gate of the map's scripts shut beside the leader's floor (ShutGatesBeside): "gate 152". */
		std::string WalkBarriers(const FEventContext& Context, int32 Target, const std::set<int32>* Crossed, const std::set<int32>& Records)
		{
			std::string Told = FieldWalkBarriers(Context, Target, Crossed);
			bool bFirst = Told.find(", barred by ") == std::string::npos;
			for (const int32 Record : ShutGatesBeside(Context, Records))
			{
				Told += (bFirst ? ", barred by gate " : ", gate ") + std::to_string(Record);
				bFirst = false;
			}
			return Told;
		}

		/** Where each actor whose body stops the leader's walk short of the trigger's actor (Target, -1 for none) stands. */
		std::vector<std::array<int32, 3>> BarringBodies(const FEventContext& Context, int32 Target, const std::set<int32>* Crossed)
		{
			std::vector<std::array<int32, 3>> Bodies;
			const FWalkedGround Ground(Context, Target, Crossed);
			for (const FWalkedGround::FBarrier& Barrier : Ground.Barriers())
			{
				if (!Barrier.bTouch)
				{
					const FEventActorState& Actor = Context.States[static_cast<std::size_t>(Barrier.Actor)];
					Bodies.push_back({Barrier.Actor, Actor.Position[0], Actor.Position[2]});
				}
			}
			return Bodies;
		}

		/** Whether one of the bodies has moved from where it stood. */
		bool AnyHasMoved(const FEventContext& Context, const std::vector<std::array<int32, 3>>& Bodies)
		{
			return std::any_of(Bodies.begin(), Bodies.end(), [&](const std::array<int32, 3>& Body)
				{
					const FEventActorState& Actor = Context.States[static_cast<std::size_t>(Body[0])];
					return Actor.Position[0] != Body[1] || Actor.Position[2] != Body[2];
				});
		}
	}

	std::optional<FError> FStorySweepRun::WalkLeader(double X, double Z, const std::function<bool(const FEventContext&)>& Arrived, bool& bArrived,
		const std::function<uint16(const FEventContext&)>& Buttons)
	{
		bArrived = false;
		// The way each of the pad's directions walked when last held: the view decides it, so it is learnt, not known.
		std::array<std::optional<double>, PadDirections.size()> Headings;
		int32 Stopped = 0;
		for (int32 Walked = 0; Walked < WalkFrames && Stopped < StoppedFrames; ++Walked)
		{
			const FEventContext* Context = FieldContext();
			if (Context == nullptr || Context->PlayerLock != 0 || !Context->bFieldRunning || Menus->Running() != nullptr)
			{
				return std::nullopt;
			}
			if (Arrived(*Context))
			{
				bArrived = true;
				return std::nullopt;
			}
			const auto [FromX, FromZ] = LeaderPoint(*Context);
			// A direction not held yet is held once; then the one that walks nearest the way wanted.
			std::size_t Held = 0;
			double Best = -2.0;
			const double Length = std::max(std::hypot(X - FromX, Z - FromZ), 1.0);
			for (std::size_t Direction = 0; Direction < PadDirections.size(); ++Direction)
			{
				// A heading H walks along (cos H, -sin H).
				const double Along = Headings[Direction] ? (std::cos(*Headings[Direction]) * (X - FromX) - std::sin(*Headings[Direction]) * (Z - FromZ)) / Length : 2.0;
				if (Along > Best)
				{
					Best = Along;
					Held = Direction;
				}
			}
			if (auto Error = Frame({static_cast<uint16>(PadDirections[Held] | (Buttons ? Buttons(*Context) : 0))}))
			{
				return Error;
			}
			Context = FieldContext();
			if (Context == nullptr)
			{
				return std::nullopt;
			}
			const FEventActorState& Leader = Context->States[Context->LeaderActor];
			if ((Leader.MoveDirection & NoDirection) == 0)
			{
				Headings[Held] = (Leader.MoveDirection & 0xFFF) * Turn / AngleUnits;
			}
			const auto [NowX, NowZ] = LeaderPoint(*Context);
			Stopped = NowX == FromX && NowZ == FromZ ? Stopped + 1 : 0;
		}
		return std::nullopt;
	}

	std::optional<FError> FStorySweepRun::AwaitView(int32 Actor, bool& bShown)
	{
		bShown = false;
		for (int32 Waited = 0; Waited < ViewFrames; ++Waited)
		{
			const FEventContext* Context = FieldContext();
			if (Context == nullptr || Context->PlayerLock != 0)
			{
				return std::nullopt;
			}
			// The field judges who is shown as it draws: not before a frame has passed where the leader now stands.
			if (Waited > 0 && IsFieldActorShown(*Context, Actor))
			{
				bShown = true;
				return std::nullopt;
			}
			if (auto Error = Frame({}))
			{
				return Error;
			}
		}
		return std::nullopt;
	}

	std::string FStorySweepTrigger::Name() const
	{
		if (Kind == EStorySweepTrigger::Place)
		{
			return "place " + std::to_string(Number / PlaceListSpan) + "." + std::to_string(Number % PlaceListSpan);
		}
		if (Kind == EStorySweepTrigger::Floor)
		{
			return "floor " + std::to_string(Number / FloorLayerSpan) + "." + std::to_string(Number % FloorLayerSpan);
		}
		const char* Kinds[] = {"talk ", "touch ", "", "area "};
		return Kinds[static_cast<std::size_t>(Kind)] + std::to_string(Number);
	}

	std::optional<FError> FStorySweepRun::Open(FStorySweepStretch& Stretch)
	{
		const FStorySweepTrigger Trigger = *Asked;
		const int32 Target = Trigger.Kind == EStorySweepTrigger::Area ? -1 : Trigger.Number;
		int32 WalkingWaits = 0;
		for (int32 Try = 0;; ++Try)
		{
			Asked.reset();
			Stretch.Barred.clear();
			if (auto Error = OpenOnce(Trigger, Stretch))
			{
				return Error;
			}
			const FEventContext* Context = FieldContext();
			if (Stretch.Opening != EStorySweepOpening::Unreached || Context == nullptr || Context->PlayerLock != 0)
			{
				return std::nullopt;
			}
			// Each gate is waited by once a map: one that stayed shut through a whole wait opens for something else.
			const int32 Map = Context->RunningMap;
			std::set<int32> Gates = ShutGatesBeside(*Context, MapReferences(Map).WalkRecords);
			std::erase_if(Gates, [&](int32 Record) { return StillGates.contains({Map, Record}); });
			if (Gates.empty() ? Stretch.Barred.find("body") == std::string::npos || Try >= BarredTries : Try >= GateTries)
			{
				return std::nullopt;
			}
			bool bChanged = true;
			const std::vector<std::array<int32, 3>> Bodies = Gates.empty() ? BarringBodies(*Context, Target, MetOnWalks()) : std::vector<std::array<int32, 3>>();
			for (int32 Waited = 0; Gates.empty() && Waited < BarredWaitFrames; ++Waited)
			{
				if (auto Error = Frame({}))
				{
					return Error;
				}
			}
			// Whoever walks on is waited for until they stop: a try of the walk counts only once they stand still.
			const FEventContext* Now = FieldContext();
			if (Gates.empty() && Now != nullptr && Now->RunningMap == Map && WalkingWaits < WalkingBarredWaits && AnyHasMoved(*Now, Bodies))
			{
				++WalkingWaits;
				--Try;
			}
			bool bWaitedOut = false;
			if (auto Error = Gates.empty() ? std::nullopt : AwaitGate(Gates, bChanged, bWaitedOut))
			{
				return Error;
			}
			for (const int32 Record : bChanged || !bWaitedOut ? std::set<int32>{} : Gates)
			{
				StillGates.insert({Map, Record});
			}
			// The walk through the gate taken by what lies past it: the zone asked for, where the leader stands in it.
			const FEventContext* After = FieldContext();
			if (!Gates.empty() && (After == nullptr || After->RunningMap != Map || After->PlayerLock != 0))
			{
				if (Trigger.Kind == EStorySweepTrigger::Area && (After == nullptr || After->RunningMap != Map || IsLeaderInFieldZone(*After, Trigger.Number)))
				{
					Stretch.Opening = EStorySweepOpening::Answered;
					Stretch.Barred.clear();
				}
				return std::nullopt;
			}
			// A gate nothing opened in that time is one the scripts open for something else: a talk, a switch.
			if (!bChanged || !Ask(Trigger))
			{
				return std::nullopt;
			}
		}
	}

	std::optional<FError> FStorySweepRun::ReadOn()
	{
		for (int32 Read = 0; Read < ReadOnFrames; Read += TapCycle)
		{
			const FEventContext* Now = FieldContext();
			if (Now == nullptr || Now->PlayerLock != 0 || Now->Messages.OpenMask == 0)
			{
				return std::nullopt;
			}
			for (int32 Tapped = 0; Tapped < TapCycle; ++Tapped)
			{
				if (auto Error = Frame(Tapped < TapFrames ? FPadState{Cross} : FPadState{}))
				{
					return Error;
				}
			}
		}
		return std::nullopt;
	}

	std::optional<FError> FStorySweepRun::AwaitGate(const std::set<int32>& Gates, bool& bChanged, bool& bWaitedOut)
	{
		const auto Words = [this, &Gates]()
		{
			std::vector<uint32> Read;
			const FEventContext* Now = FieldContext();
			for (const int32 Record : Gates)
			{
				const bool bHas = Now != nullptr && Now->Walkmesh != nullptr && static_cast<std::size_t>(Record) < Now->Walkmesh->Attributes.size();
				Read.push_back(bHas ? Now->Walkmesh->Attributes[static_cast<std::size_t>(Record)] : 0);
			}
			return Read;
		};
		const std::vector<uint32> Shut = Words();
		const auto [GateX, GateZ] = GateMiddle(*FieldContext(), *Gates.begin());
		const auto Opens = [&](const FEventContext&) { return Words() != Shut; };
		const int32 Since = Played;
		while (Played - Since < GateWaitFrames && Words() == Shut)
		{
			const FEventContext* Now = FieldContext();
			if (Now == nullptr || !Now->bFieldRunning)
			{
				break;
			}
			const int32 Before = Played;
			if (Now->Messages.OpenMask != 0 || Now->PlayerLock != 0)
			{
				// Read on, as a player would: a line said by the gate (Bart's, of the floodgate) holds the party until it closes.
				for (int32 Tapped = 0; Tapped < TapCycle; ++Tapped)
				{
					if (auto Error = Frame(Tapped < TapFrames && Now->Messages.OpenMask != 0 ? FPadState{Cross} : FPadState{}))
					{
						return Error;
					}
				}
				continue;
			}
			// Held toward the gate: the floor before one may carry whoever stands idle away from it (a channel's current).
			bool bArrived = false;
			if (auto Error = WalkLeader(GateX, GateZ, Opens, bArrived))
			{
				return Error;
			}
			if (Played == Before)
			{
				if (auto Error = Frame({}))
				{
					return Error;
				}
			}
		}
		bChanged = Words() != Shut;
		bWaitedOut = !bChanged && Played - Since >= GateWaitFrames;
		// And through it while it stands open, as the one who waited there goes on, at a run: the open floodgate's
		// current carries a walker back down the channel. Staged anew from short of the trigger, the way back up
		// would come to the gate's line again and stop there.
		const auto Through = [&](const FEventContext& Walked)
		{
			const auto [X, Z] = LeaderPoint(Walked);
			return Words() == Shut || std::hypot(X - GateX, Z - GateZ) < GateReach;
		};
		bool bThrough = false;
		return bChanged ? WalkLeader(GateX, GateZ, Through, bThrough, FieldRunButtons) : std::nullopt;
	}

	std::optional<FError> FStorySweepRun::OpenOnce(const FStorySweepTrigger& Trigger, FStorySweepStretch& Stretch)
	{
		Stretch.Opening = EStorySweepOpening::Unanswered;
		if (Trigger.Kind == EStorySweepTrigger::Place)
		{
			// The party stands in the place as the world map starts: the confirm button enters it, or the walk onto it did.
			Visit(Stretch, "world map");
			const auto bOnItsWay = [this]()
			{
				const FWorldMapProgram* WorldMap = std::as_const(*Loop).WorldMap();
				return WorldMap != nullptr && IsWorldMapVoyage(WorldMap->Runtime().Context());
			};
			for (int32 Waited = 0; (Waited < PlaceFrames || (Waited < PlaceVoyageFrames && bOnItsWay())) && Loop->Program() == EGameProgram::WorldMap; ++Waited)
			{
				if (auto Error = Frame(Waited % TapCycle < TapFrames ? FPadState{Cross} : FPadState{}))
				{
					return Error;
				}
			}
			if (Loop->Program() != EGameProgram::WorldMap)
			{
				Stretch.Opening = EStorySweepOpening::Answered;
			}
			return std::nullopt;
		}
		if (Trigger.Kind == EStorySweepTrigger::Floor)
		{
			return OpenFloor(Trigger, Stretch);
		}
		const FEventContext* Start = FieldContext();
		const int32 StartMap = Start != nullptr ? Start->RunningMap : -1;
		// The map it begins on, which a way out may have left before the opening is over.
		if (Start != nullptr)
		{
			Visit(Stretch, std::to_string(StartMap));
		}
		if (Trigger.Kind != EStorySweepTrigger::Area)
		{
			return OpenByActor(Trigger, Stretch);
		}
		const int32 Zone = Trigger.Number;
		FFieldProgram* Field = Loop->Program() == EGameProgram::Field ? Loop->Field() : nullptr;
		if (Field == nullptr || Field->Runtime().Context().PlayerLock != 0)
		{
			return std::nullopt;
		}
		// A way out at a map's edge takes the party only as it comes to the screen's edge: a walk into a zone goes on
		// through it the way it came, until it is out of it or can go no further. One that stands in it already (a
		// battle broke into its walk) goes on from there past the zone's middle.
		const auto Through = [Zone](const FEventContext& Walked) { return !IsLeaderInFieldZone(Walked, Zone); };
		const auto WalkOnThrough = [&](const FEventContext& Here)
		{
			const auto [MiddleX, MiddleZ] = FieldZoneMiddle(Here, Zone);
			const auto [FromX, FromZ] = LeaderPoint(Here);
			bool bThrough = false;
			Stretch.Opening = EStorySweepOpening::Answered;
			return WalkLeader(MiddleX + (MiddleX - FromX), MiddleZ + (MiddleZ - FromZ), Through, bThrough);
		};
		if (IsLeaderInFieldZone(Field->Runtime().Context(), Zone))
		{
			return WalkOnThrough(Field->Runtime().Context());
		}
		// A zone no walk reaches from the leader's floor is walked to from one the crossings found lead to, by them.
		const FEventContext& Here = Field->Runtime().Context();
		if (!Crossings.empty() && !Options.bForcesTriggers && !ReachesFieldZone(Here, Zone, MetOnWalks(), LeaderStanding(Here)))
		{
			const std::vector<std::size_t> Steps = RouteToward([&](const FFieldStanding& From) { return ReachesFieldZone(Here, Zone, MetOnWalks(), From); });
			bool bFollowed = false;
			if (auto Error = Steps.empty() ? std::nullopt : FollowRoute(Steps, bFollowed))
			{
				return Error;
			}
			Field = Loop->Program() == EGameProgram::Field ? Loop->Field() : nullptr;
			if (Field == nullptr || Field->Runtime().Context().PlayerLock != 0 || Field->Runtime().Context().RunningMap != StartMap)
			{
				// The route's own walk came into the zone: it led on, or its scripts have the party there.
				const bool bTaken = Field == nullptr || Field->Runtime().Context().RunningMap != StartMap || IsLeaderInFieldZone(Field->Runtime().Context(), Zone);
				Stretch.Opening = !Steps.empty() && bTaken ? EStorySweepOpening::Answered : Stretch.Opening;
				return std::nullopt;
			}
			if (IsLeaderInFieldZone(Field->Runtime().Context(), Zone))
			{
				return WalkOnThrough(Field->Runtime().Context());
			}
		}
		// Stood short of the zone and walked into it: by the straight way to its middle, then by the way the leader's
		// own walk first comes to it, which is the way in where the middle lies off the floor.
		for (const bool bByItsWalk : {false, true})
		{
			const std::optional<std::pair<double, double>> Aim = StageBeforeFieldZone(Field->Runtime().Context(), Zone, MetOnWalks(), bByItsWalk);
			if (!Aim)
			{
				continue;
			}
			for (int32 Waited = 0; Waited < ZoneViewFrames; ++Waited)
			{
				if (auto Error = Frame({}))
				{
					return Error;
				}
			}
			const FEventContext* Context = FieldContext();
			if (Context == nullptr || Context->PlayerLock != 0)
			{
				// Something by the way took the party before the zone did, or the zone itself, whose edge the stand met.
				if (Context != nullptr && IsLeaderInFieldZone(*Context, Zone))
				{
					Stretch.Opening = EStorySweepOpening::Answered;
				}
				return std::nullopt;
			}
			const auto [FromX, FromZ] = LeaderPoint(*Context);
			bool bArrived = false;
			// Some steps into the zone, not onto its edge, where the game's own test may still say outside.
			int32 Inside = 0;
			const auto Arrived = [Zone, &Inside](const FEventContext& Walked)
			{
				Inside = IsLeaderInFieldZone(Walked, Zone) ? Inside + 1 : 0;
				return Inside > StepsIntoZone;
			};
			if (auto Error = WalkLeader(Aim->first, Aim->second, Arrived, bArrived))
			{
				return Error;
			}
			Context = FieldContext();
			// Stood in it a moment: a zone may answer whoever stays (a gate that names where it leads, then leads there),
			// and leaves alone whoever walks on.
			const auto bUnanswered = [&]()
			{
				return bArrived && Context != nullptr && Context->PlayerLock == 0 && Context->RunningMap == StartMap && Context->Messages.OpenMask == 0;
			};
			for (int32 Stood = 0; Stood < ZoneDwellFrames && bUnanswered(); ++Stood)
			{
				if (auto Error = Frame({}))
				{
					return Error;
				}
				Context = FieldContext();
			}
			if (bUnanswered())
			{
				bool bThrough = false;
				if (auto Error = WalkLeader(Aim->first + (Aim->first - FromX), Aim->second + (Aim->second - FromZ), Through, bThrough))
				{
					return Error;
				}
				Context = FieldContext();
			}
			else
			{
				const auto bNamed = [&]()
				{
					return Context != nullptr && Context->PlayerLock == 0 && Context->bPlayerControlled && Context->RunningMap == StartMap && Context->Messages.OpenMask != 0;
				};
				for (int32 Shown = 0; Shown < ZoneWindowFrames && bNamed(); ++Shown)
				{
					if (auto Error = Frame({}))
					{
						return Error;
					}
					Context = FieldContext();
				}
				// Held on as the window closes: that is when the script looks.
				for (int32 Pressed = bNamed() ? 0 : ZonePressFrames; Pressed < ZonePressFrames && Context != nullptr && Context->RunningMap == StartMap; ++Pressed)
				{
					if (auto Error = Frame({Cross}))
					{
						return Error;
					}
					Context = FieldContext();
				}
			}
			if (bArrived || Context == nullptr || Context->PlayerLock != 0 || Context->RunningMap != StartMap)
			{
				Stretch.Opening = EStorySweepOpening::Answered;
				return std::nullopt;
			}
			Field = Loop->Program() == EGameProgram::Field ? Loop->Field() : nullptr;
			if (Field == nullptr)
			{
				return std::nullopt;
			}
		}
		Field = Loop->Program() == EGameProgram::Field ? Loop->Field() : nullptr;
		if (Field != nullptr && Field->Runtime().Context().PlayerLock == 0)
		{
			Stretch.Opening = EnterFieldZone(Field->Runtime().Context(), Zone, MetOnWalks()) ? EStorySweepOpening::Answered : EStorySweepOpening::Unreached;
			if (Stretch.Opening == EStorySweepOpening::Unreached)
			{
				Stretch.Barred = WalkBarriers(Field->Runtime().Context(), -1, MetOnWalks(), MapReferences(Field->Runtime().Context().RunningMap).WalkRecords);
			}
		}
		return std::nullopt;
	}

	std::optional<FError> FStorySweepRun::OpenByActor(const FStorySweepTrigger& Trigger, FStorySweepStretch& Stretch)
	{
		const bool bTalk = Trigger.Kind == EStorySweepTrigger::Talk;
		const int32 Script = bTalk ? FEventMachine::TalkScript : FEventMachine::TouchScript;
		// An actor no walk reaches from the leader's floor is walked up to from one the crossings found lead to, by them.
		if (!AskedRoute.empty())
		{
			bool bFollowed = false;
			if (auto Error = FollowRoute(std::exchange(AskedRoute, {}), bFollowed))
			{
				return Error;
			}
			FFieldProgram* Over = bFollowed && Loop->Program() == EGameProgram::Field ? Loop->Field() : nullptr;
			bStoodNear = Over != nullptr && Over->Runtime().Context().PlayerLock == 0
				&& ApproachFieldActor(Over->Runtime().Context(), Trigger.Number, EFieldApproach::Near, MetOnWalks());
		}
		// Whatever comes of it, the walk has met this actor's touch: a later one may pass it.
		const bool bNear = bStoodNear;
		if (!bTalk)
		{
			Crossed.insert(Trigger.Number);
		}
		// The view comes over before anything begins: the game ends a talk with an actor it does not show. Where
		// the leader could not be stood near, only one the forced play starts outright is waited for.
		bool bShown = false;
		if (bNear || Options.bForcesTriggers)
		{
			if (auto Error = AwaitView(Trigger.Number, bShown))
			{
				return Error;
			}
		}
		// The walk up to the actor, from near it: where it gets there, the game's own check answers as it does a player.
		if (bShown && bNear)
		{
			const int32 Actor = Trigger.Number;
			const FEventContext* Context = FieldContext();
			const auto [X, Z] = Context != nullptr ? FieldActorPoint(*Context, Actor) : std::pair<double, double>{};
			bool bArrived = false;
			const auto Arrived = [Actor, Script, bTalk](const FEventContext& Walked)
			{
				return RunsFieldScript(Walked, Actor, Script) || IsLeaderWithin(Walked, Actor, bTalk ? TalkGap : TouchGap);
			};
			if (auto Error = WalkLeader(X, Z, Arrived, bArrived))
			{
				return Error;
			}
			if (auto Error = bArrived ? ReadOn() : std::nullopt)
			{
				return Error;
			}
			for (int32 Waited = 0; bArrived && Waited < OpeningFrames; ++Waited)
			{
				Context = FieldContext();
				if (Context == nullptr)
				{
					return std::nullopt;
				}
				if (RunsFieldScript(*Context, Actor, Script))
				{
					Stretch.Opening = EStorySweepOpening::Answered;
					return std::nullopt;
				}
				if (auto Error = Frame(bTalk && Waited < TapFrames ? FPadState{Cross} : FPadState{}))
				{
					return Error;
				}
			}
		}
		FFieldProgram* Field = Loop->Program() == EGameProgram::Field ? Loop->Field() : nullptr;
		if (Field == nullptr || Field->Runtime().Context().PlayerLock != 0)
		{
			return std::nullopt;
		}
		// Beside it, from each side in turn until it answers: an actor may answer a leader that faces one way only (a
		// script that reads the leader's facing, a box that asks to be faced), and its script then ends as it starts.
		std::vector<std::pair<int32, int32>> Stands;
		for (int32 Side = 0; Side < (Options.bForcesTriggers ? 1 : BesideSides); ++Side)
		{
			if (auto Error = ReadOn())
			{
				return Error;
			}
			Field = Loop->Program() == EGameProgram::Field ? Loop->Field() : nullptr;
			if (Field == nullptr || Field->Runtime().Context().PlayerLock != 0 || Field->Runtime().Context().Messages.OpenMask != 0
				|| !ApproachFieldActor(Field->Runtime().Context(), Trigger.Number, EFieldApproach::Beside, MetOnWalks(), &Stands))
			{
				break;
			}
			// Stood beside it anew, the view is waited for again: a press before the field has drawn the actor from the
			// leader's new place talks to one it does not show, and the game ends that talk at its first message.
			if (auto Error = AwaitView(Trigger.Number, bShown))
			{
				return Error;
			}
			for (int32 Waited = 0; bShown && Waited <= OpeningFrames; ++Waited)
			{
				const FEventContext* Context = FieldContext();
				if (Context == nullptr)
				{
					return std::nullopt;
				}
				if (RunsFieldScript(*Context, Trigger.Number, Script))
				{
					Stretch.Opening = EStorySweepOpening::Answered;
					return std::nullopt;
				}
				if (Waited == OpeningFrames)
				{
					break;
				}
				if (auto Error = Frame(bTalk && Waited < TapFrames ? FPadState{Cross} : FPadState{}))
				{
					return Error;
				}
			}
		}
		Field = Loop->Program() == EGameProgram::Field ? Loop->Field() : nullptr;
		if (!bNear && Stands.empty())
		{
			Stretch.Opening = EStorySweepOpening::Unreached;
			if (Field != nullptr)
			{
				Stretch.Barred = WalkBarriers(Field->Runtime().Context(), Trigger.Number, MetOnWalks(), MapReferences(Field->Runtime().Context().RunningMap).WalkRecords);
			}
		}
		if (Field == nullptr || !bShown)
		{
			return std::nullopt;
		}
		if (!Options.bForcesTriggers && !RunsFieldScript(Field->Runtime().Context(), Trigger.Number, Script))
		{
			return std::nullopt;
		}
		FEventContext& Context = Field->Runtime().Context();
		if (RunsFieldScript(Context, Trigger.Number, Script))
		{
			Stretch.Opening = EStorySweepOpening::Answered;
			return std::nullopt;
		}
		FEventActorState* Actor = Context.StateOf(Trigger.Number);
		if (Actor == nullptr || Context.PlayerLock != 0 || !IsAsked(Context, Trigger.Number, *Actor)
			|| !(bTalk ? AnswersTalk(*Actor) : AnswersFieldTouch(Context, Trigger.Number))
			|| !FEventMachine::StartScript(Context, Trigger.Number, Script, bTalk ? FEventMachine::TalkPriority : FEventMachine::TouchPriority))
		{
			return std::nullopt;
		}
		// As the field leaves an actor whose script its check starts.
		Actor->Facing |= TurnSet;
		Actor->MoveDirection = Actor->Facing;
		Stretch.Opening = EStorySweepOpening::Forced;
		return std::nullopt;
	}

	std::vector<FStorySweepTrigger> FStorySweepRun::Triggers() const
	{
		std::vector<FStorySweepTrigger> Found;
		if (const FWorldMapProgram* WorldMap = Loop != nullptr ? std::as_const(*Loop).WorldMap() : nullptr)
		{
			for (const FWorldMapWay& Way : WorldMapWays(WorldMap->Runtime().Context()))
			{
				Found.push_back({EStorySweepTrigger::Place, Way.List * PlaceListSpan + Way.Record});
			}
			return Found;
		}
		const FEventContext* Context = FieldContext();
		if (Context == nullptr || Context->Scripts == nullptr || !Context->bFieldRunning || Context->PlayerLock != 0)
		{
			return Found;
		}
		const int32 Actors = std::min(static_cast<int32>(Context->States.size()), Context->Scripts->ActorCount());
		for (int32 Number = 0; Number < Actors; ++Number)
		{
			const FEventActorState& Actor = Context->States[static_cast<std::size_t>(Number)];
			if (!IsAsked(*Context, Number, Actor))
			{
				continue;
			}
			if (AnswersTalk(Actor))
			{
				Found.push_back({EStorySweepTrigger::Talk, Number});
			}
			if (AnswersFieldTouch(*Context, Number))
			{
				Found.push_back({EStorySweepTrigger::Touch, Number});
			}
		}
		// The floors no walk reaches but a try from one that is might, nearest first: none where triggers are started outright.
		const std::vector<FFloorProbe> None;
		for (const FFloorProbe& Probe : Options.bForcesTriggers ? None : FloorProbes(*Context))
		{
			const FStorySweepTrigger Floor{EStorySweepTrigger::Floor, Probe.Toward.Layer * FloorLayerSpan + Probe.Toward.First};
			if (std::none_of(Found.begin(), Found.end(), [&](const FStorySweepTrigger& Each) { return Each.Kind == Floor.Kind && Each.Number == Floor.Number; }))
			{
				Found.push_back(Floor);
			}
		}
		for (int32 Zone = 0; Zone < static_cast<int32>(Context->Zones.size()); ++Zone)
		{
			Found.push_back({EStorySweepTrigger::Area, Zone});
		}
		return Found;
	}

	std::vector<FStorySweepTrigger> FStorySweepRun::ZonesByNearness() const
	{
		std::vector<FStorySweepTrigger> Zones;
		const FEventContext* Context = FieldContext();
		if (Context == nullptr || !Context->bFieldRunning || Context->PlayerLock != 0)
		{
			return Zones;
		}
		const auto [X, Z] = LeaderPoint(*Context);
		const auto Distance = [&](const FStorySweepTrigger& Zone)
		{
			const auto [MiddleX, MiddleZ] = FieldZoneMiddle(*Context, Zone.Number);
			return std::hypot(MiddleX - X, MiddleZ - Z);
		};
		for (int32 Zone = 0; Zone < static_cast<int32>(Context->Zones.size()); ++Zone)
		{
			Zones.push_back({EStorySweepTrigger::Area, Zone});
		}
		std::stable_sort(Zones.begin(), Zones.end(), [&](const FStorySweepTrigger& A, const FStorySweepTrigger& B) { return Distance(A) < Distance(B); });
		return Zones;
	}

	bool FStorySweepRun::Ask(const FStorySweepTrigger& Trigger)
	{
		if (Trigger.Kind == EStorySweepTrigger::Place)
		{
			const FWorldMapProgram* WorldMap = std::as_const(*Loop).WorldMap();
			if (WorldMap == nullptr)
			{
				return false;
			}
			const std::vector<FWorldMapWay> Ways = WorldMapWays(WorldMap->Runtime().Context());
			const auto Way = std::find_if(Ways.begin(), Ways.end(), [&](const FWorldMapWay& Each) { return Each.List * PlaceListSpan + Each.Record == Trigger.Number; });
			if (Way == Ways.end())
			{
				return false;
			}
			// The world map anew, the party in the place: what a save made there and loaded starts with.
			FGameState There = Session->Game();
			StandOnWorldMap(There, *Way);
			if (Resume(There))
			{
				return false;
			}
			Asked = Trigger;
			return true;
		}
		FFieldProgram* Field = Loop->Program() == EGameProgram::Field ? Loop->Field() : nullptr;
		if (Field == nullptr)
		{
			return false;
		}
		FEventContext& Context = Field->Runtime().Context();
		if (Trigger.Kind == EStorySweepTrigger::Floor)
		{
			const FWalkedFloor Wanted{Trigger.Number / FloorLayerSpan, Trigger.Number % FloorLayerSpan};
			const std::vector<FFloorProbe> None;
			const std::vector<FFloorProbe>& Probes = Context.PlayerLock == 0 && !Options.bForcesTriggers ? FloorProbes(Context) : None;
			if (std::none_of(Probes.begin(), Probes.end(), [&](const FFloorProbe& Probe) { return Probe.Toward == Wanted; }))
			{
				return false;
			}
			Asked = Trigger;
			return true;
		}
		if (Trigger.Kind == EStorySweepTrigger::Area)
		{
			if (Context.PlayerLock != 0 || Trigger.Number < 0 || static_cast<std::size_t>(Trigger.Number) >= Context.Zones.size())
			{
				return false;
			}
			Asked = Trigger;
			return true;
		}
		const bool bTalk = Trigger.Kind == EStorySweepTrigger::Talk;
		const FEventActorState* Actor = Context.StateOf(Trigger.Number);
		if (Actor == nullptr || Context.Scripts == nullptr || Context.PlayerLock != 0 || Trigger.Number >= Context.Scripts->ActorCount()
			|| !IsAsked(Context, Trigger.Number, *Actor) || !(bTalk ? AnswersTalk(*Actor) : AnswersFieldTouch(Context, Trigger.Number)))
		{
			return false;
		}
		bStoodNear = ApproachFieldActor(Context, Trigger.Number, EFieldApproach::Near, MetOnWalks());
		AskedRoute.clear();
		if (!bStoodNear)
		{
			AskedRoute = RouteToward([&](const FFieldStanding& From) { return ReachesFieldActor(Context, Trigger.Number, EFieldApproach::Near, MetOnWalks(), From); });
		}
		Asked = Trigger;
		return true;
	}
}
