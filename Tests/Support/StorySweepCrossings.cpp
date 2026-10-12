#include "Support/StorySweepRun.h"

#include "Field/EventContext.h"
#include "Game/FieldProgram.h"
#include "Support/StorySweepApproach.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <utility>

namespace Weltall::Testing
{
	namespace
	{
		/**
		 * The release reads its face buttons swapped (the kernel's button map): the field's run is the pad's Circle, its
		 * jump Triangle. Under the player's Always run (the explorer plays with it, as a new player does) the leader runs
		 * unless Circle is held: a crossing's walk is then a run and its running jump a walking one.
		 */
		constexpr uint16 Run = FPadState::Bit(EPadButton::Circle);
		constexpr uint16 Jump = FPadState::Bit(EPadButton::Triangle);
		/** A floor of fewer triangles is a sliver at a wall's foot, not a place to go. */
		constexpr int32 MinFloorTriangles = 4;
		/** How far back from where it jumps a try sets off, for the run-up, and how far past the other floor's nearest point it makes for. */
		constexpr int32 RunUp = 24;
		constexpr int32 AimPast = 24;
		/** The widest gap between the rim of the ground reached and another floor a try is made across. */
		constexpr int32 CrossingReach = 128;
		/** The tries made onto one floor, and so many from each floor reached: from points of its rim at least Spread apart. */
		constexpr std::size_t MostProbes = 3;
		constexpr std::size_t ProbesAFloor = 3;
		constexpr int32 Spread = 64;
		/** The other floor's points a try may make for: each so many-th along both ways, with its rim. */
		constexpr int32 TargetEvery = 2;
		/** How near the point it jumps from the leader presses the jump button. */
		constexpr int32 TakeOffReach = 6;
		/** The frames a walk toward the rim may make no way before the jump is pressed where it stands. */
		constexpr int32 StillFrames = 3;
		/** How near its aim a walk counts as arrived, and the frames on the ground after which a move has come to rest. */
		constexpr int32 AimReach = 6;
		constexpr int32 RestFrames = 8;
		/** A gap's cost: its width, and per unit of height the floor beyond lies above (a jump's climb) or below (a fall that walls may hide). */
		constexpr double ClimbCost = 2.0;
		constexpr double DropCost = 0.25;
		/** The other floors' points are looked up by squares of this side. */
		constexpr int32 Cell = 32;
		/** The eight ways a try onto a floor that hands the walker up may come from, as steps of a unit square. */
		constexpr std::array<std::pair<int32, int32>, 8> Ways = {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}}};

		using FCells = std::map<std::pair<int32, int32>, std::vector<FGroundPoint>>;

		std::pair<int32, int32> CellOf(int32 X, int32 Z)
		{
			return {X >= 0 ? X / Cell : (X - Cell + 1) / Cell, Z >= 0 ? Z / Cell : (Z - Cell + 1) / Cell};
		}

		bool IsAirborne(const FEventContext& Context)
		{
			return (Context.States[Context.LeaderActor].Flags0 & EventActorFlags0::InTheAir) != 0;
		}

		/** A floor not reached: points of the ground it holds. */
		struct FUnreachedFloor
		{
			FWalkedFloor Floor;
			std::vector<FGroundPoint> Points;
		};

		/** A gap between a point at the rim of the ground reached and one of another floor. */
		struct FGap
		{
			double Cost = 0.0;
			double Length = 0.0;
			FGroundPoint Edge;
			FGroundPoint Beyond;
		};
	}

	std::optional<FWalkedFloor> FStorySweepRun::Floor() const
	{
		const FEventContext* Context = FieldContext();
		return Context != nullptr ? LeaderFloor(*Context) : std::nullopt;
	}

	bool FStorySweepRun::ReachesFloor(const FWalkedFloor& Wanted) const
	{
		const FEventContext* Context = FieldContext();
		const std::vector<FReachedFloor> Reached = Context != nullptr ? ReachedFloors(*Context) : std::vector<FReachedFloor>();
		return std::any_of(Reached.begin(), Reached.end(), [&](const FReachedFloor& Each) { return Each.Floor == Wanted; });
	}

	std::vector<FStorySweepRun::FReachedFloor> FStorySweepRun::ReachedFloors(const FEventContext& Context) const
	{
		std::vector<FReachedFloor> Found;
		const std::optional<FWalkedFloor> Own = LeaderFloor(Context);
		if (!Own)
		{
			return Found;
		}
		Found.push_back({*Own, LeaderStanding(Context), {}});
		for (std::size_t At = 0; At < Found.size(); ++At)
		{
			const FReachedFloor From = Found[At];
			for (std::size_t Number = 0; Number < Crossings.size(); ++Number)
			{
				const FFloorCrossing& Crossing = Crossings[Number];
				const bool bKnown = std::any_of(Found.begin(), Found.end(), [&](const FReachedFloor& Each) { return Each.Floor == Crossing.To; });
				if (Crossing.From != From.Floor || bKnown)
				{
					continue;
				}
				std::vector<std::size_t> Route = From.Route;
				Route.push_back(Number);
				Found.push_back({Crossing.To, Crossing.Landed, std::move(Route)});
			}
		}
		return Found;
	}

	const std::vector<FStorySweepRun::FFloorProbe>& FStorySweepRun::FloorProbes(const FEventContext& Context) const
	{
		// The same as last time while the leader is on the same floor and no crossing or touch was found since.
		const std::optional<FWalkedFloor> Own = LeaderFloor(Context);
		const uint16 Progress = Context.Variables.Raw(0);
		if (KeptProbes && KeptProbes->Map == Context.RunningMap && KeptProbes->Story == Progress && KeptProbes->Floor == Own && KeptProbes->Crossings == Crossings.size()
			&& KeptProbes->Met == Crossed.size())
		{
			return KeptProbes->Probes;
		}
		KeptProbes = FKeptProbes{Context.RunningMap, Progress, Own, Crossings.size(), Crossed.size(), {}};
		std::vector<FFloorProbe>& Probes = KeptProbes->Probes;
		const std::vector<FReachedFloor> Reached = ReachedFloors(Context);
		const auto IsReached = [&](const FWalkedFloor& Floor) { return std::any_of(Reached.begin(), Reached.end(), [&](const FReachedFloor& Each) { return Each.Floor == Floor; }); };
		std::vector<FUnreachedFloor> Unreached;
		for (const auto& [Floor, Standing] : WalkedFloors(Context, MinFloorTriangles))
		{
			if (IsReached(Floor))
			{
				continue;
			}
			Unreached.push_back({Floor, FWalkedGround(Context, -1, nullptr, Standing).Points(TargetEvery)});
		}
		if (Unreached.empty())
		{
			return Probes;
		}
		for (const FReachedFloor& From : Reached)
		{
			const FWalkedGround Ground(Context, -1, MetOnWalks(), From.Route.empty() ? std::nullopt : std::optional<FFieldStanding>(From.Standing));
			// Onto a floor that hands the walker up: to the floor nearest above it there, on another layer, walked onto from a step or two away.
			for (const auto& [X, Z, Triangle] : Ground.PassesUp())
			{
				const std::optional<int32> Height = Ground.HeightAt(X, Z);
				std::optional<FFloorUnder> Above;
				for (int32 Layer = 0; Height && Layer < WalkingLayers(Context); ++Layer)
				{
					const std::optional<FFloorUnder> Under = Layer != From.Floor.Layer ? WalkedFloorAt(Context, Layer, X, Z) : std::nullopt;
					if (Under && Under->Height < *Height && (!Above || Under->Height > Above->Height))
					{
						Above = Under;
					}
				}
				if (!Above || IsReached(Above->Floor))
				{
					continue;
				}
				for (const auto& [Across, Along] : Ways)
				{
					const double Length = std::hypot(Across, Along);
					const int32 StartX = X - static_cast<int32>(std::lround(Across * RunUp / Length));
					const int32 StartZ = Z - static_cast<int32>(std::lround(Along * RunUp / Length));
					if (Ground.Reaches(StartX, StartZ))
					{
						const int32 AimX = X + static_cast<int32>(std::lround(Across * AimPast / Length));
						const int32 AimZ = Z + static_cast<int32>(std::lround(Along * AimPast / Length));
						Probes.push_back({From, Above->Floor, {From.Floor.Layer, StartX, StartZ}, AimX, AimZ, X, Z, true, 0});
						break;
					}
				}
			}
			// Across a gap: from a level point of the rim to the nearest point of the other floor that does not lie over this
			// floor's ground (another layer's beneath or above it), the nearest few such gaps from points of the rim apart.
			std::vector<FGroundPoint> Rim = Ground.Points(0);
			std::erase_if(Rim, [](const FGroundPoint& Point) { return Point.bSteep; });
			for (const FUnreachedFloor& Other : Unreached)
			{
				FCells Beyond;
				for (const FGroundPoint& Point : Other.Points)
				{
					if (!Ground.Covers(Point.X, Point.Z))
					{
						Beyond[CellOf(Point.X, Point.Z)].push_back(Point);
					}
				}
				std::vector<FGap> Gaps;
				for (const FGroundPoint& Edge : Rim)
				{
					FGap Nearest{CrossingReach * ClimbCost, CrossingReach + 1.0, Edge, {}};
					const auto [Column, Row] = CellOf(Edge.X, Edge.Z);
					for (int32 Down = Row - CrossingReach / Cell - 1; Down <= Row + CrossingReach / Cell + 1; ++Down)
					{
						for (int32 Right = Column - CrossingReach / Cell - 1; Right <= Column + CrossingReach / Cell + 1; ++Right)
						{
							const auto Points = Beyond.find({Right, Down});
							for (const FGroundPoint& Point : Points != Beyond.end() ? Points->second : std::vector<FGroundPoint>())
							{
								const double Length = std::hypot(Point.X - Edge.X, Point.Z - Edge.Z);
								const int32 Fall = Point.Height - Edge.Height;
								const double Cost = Length + (Fall < 0 ? -Fall * ClimbCost : Fall * DropCost);
								if (Length >= 1.0 && Length <= CrossingReach && Cost < Nearest.Cost)
								{
									Nearest = {Cost, Length, Edge, Point};
								}
							}
						}
					}
					if (Nearest.Length <= CrossingReach)
					{
						Gaps.push_back(Nearest);
					}
				}
				std::sort(Gaps.begin(), Gaps.end(), [](const FGap& A, const FGap& B) { return A.Cost < B.Cost; });
				std::vector<FGap> Picked;
				for (const FGap& Gap : Gaps)
				{
					const bool bApart = std::none_of(Picked.begin(), Picked.end(), [&](const FGap& Taken) { return std::hypot(Taken.Edge.X - Gap.Edge.X, Taken.Edge.Z - Gap.Edge.Z) < Spread; });
					if (Picked.size() < ProbesAFloor && bApart)
					{
						Picked.push_back(Gap);
					}
				}
				for (const FGap& Gap : Picked)
				{
					const double AcrossX = (Gap.Beyond.X - Gap.Edge.X) / Gap.Length;
					const double AcrossZ = (Gap.Beyond.Z - Gap.Edge.Z) / Gap.Length;
					int32 StartX = Gap.Edge.X - static_cast<int32>(std::lround(AcrossX * RunUp));
					int32 StartZ = Gap.Edge.Z - static_cast<int32>(std::lround(AcrossZ * RunUp));
					if (!Ground.Reaches(StartX, StartZ))
					{
						StartX = Gap.Edge.X;
						StartZ = Gap.Edge.Z;
					}
					const int32 AimX = Gap.Beyond.X + static_cast<int32>(std::lround(AcrossX * AimPast));
					const int32 AimZ = Gap.Beyond.Z + static_cast<int32>(std::lround(AcrossZ * AimPast));
					Probes.push_back({From, Other.Floor, {From.Floor.Layer, StartX, StartZ}, AimX, AimZ, Gap.Edge.X, Gap.Edge.Z, false, static_cast<int32>(std::lround(Gap.Cost))});
				}
			}
		}
		std::stable_sort(Probes.begin(), Probes.end(), [](const FFloorProbe& A, const FFloorProbe& B) { return A.Gap < B.Gap; });
		return Probes;
	}

	std::vector<std::size_t> FStorySweepRun::RouteToward(const std::function<bool(const FFieldStanding&)>& Reaches) const
	{
		const FEventContext* Context = FieldContext();
		if (Context == nullptr || Options.bForcesTriggers)
		{
			return {};
		}
		const std::vector<FReachedFloor> Reached = ReachedFloors(*Context);
		for (std::size_t Index = 1; Index < Reached.size(); ++Index)
		{
			if (Reaches(Reached[Index].Standing))
			{
				return Reached[Index].Route;
			}
		}
		return {};
	}

	std::optional<FError> FStorySweepRun::FollowRoute(const std::vector<std::size_t>& Steps, bool& bFollowed)
	{
		bFollowed = false;
		for (const std::size_t Step : Steps)
		{
			const FFloorCrossing Crossing = Crossings[Step];
			const FEventContext* Context = FieldContext();
			if (Context == nullptr || Context->PlayerLock != 0 || LeaderFloor(*Context) != Crossing.From)
			{
				return std::nullopt;
			}
			bool bPlayed = false;
			if (auto Error = PlayCrossing(Crossing.Start, Crossing.AimX, Crossing.AimZ, Crossing.TakeOffX, Crossing.TakeOffZ, Crossing.Move, bPlayed))
			{
				return Error;
			}
			Context = FieldContext();
			if (!bPlayed || Context == nullptr || Context->PlayerLock != 0 || LeaderFloor(*Context) != Crossing.To)
			{
				return std::nullopt;
			}
		}
		bFollowed = true;
		return std::nullopt;
	}

	std::optional<FError> FStorySweepRun::PlayCrossing(const FFieldStanding& Start, int32 AimX, int32 AimZ, int32 TakeOffX, int32 TakeOffZ, ECrossingMove Move, bool& bPlayed)
	{
		bPlayed = false;
		FFieldProgram* Field = Loop->Program() == EGameProgram::Field ? Loop->Field() : nullptr;
		if (Field == nullptr)
		{
			return std::nullopt;
		}
		FEventContext& Context = Field->Runtime().Context();
		const std::optional<FWalkedFloor> From = LeaderFloor(Context);
		if (Context.PlayerLock != 0 || !From || From->Layer != Start.Layer || !StandLeaderAt(Context, Start.X, Start.Z, AimX, AimZ, MetOnWalks()))
		{
			return std::nullopt;
		}
		bPlayed = true;
		bCrossed = true;
		// A frame for the field to settle the party where it was stood, before the pad is taken up.
		if (auto Error = Frame({}))
		{
			return Error;
		}
		bool bJumped = Move == ECrossingMove::Walk;
		int32 Grounded = 0;
		const auto Rested = [&](const FEventContext& Walked)
		{
			Grounded = IsAirborne(Walked) ? 0 : Grounded + 1;
			if (Grounded < RestFrames)
			{
				return false;
			}
			if (Move != ECrossingMove::Walk)
			{
				return bJumped;
			}
			const auto [X, Z] = LeaderPoint(Walked);
			return LeaderFloor(Walked) != From || std::hypot(X - AimX, Z - AimZ) <= AimReach;
		};
		// The jump is pressed as the leader comes to the rim, or stops short of it against a wall; a running jump's run is held from the start.
		std::pair<double, double> Last = LeaderPoint(Context);
		int32 Still = 0;
		const auto Buttons = [&](const FEventContext& Walked) -> uint16
		{
			uint16 Held = Move == ECrossingMove::RunningJump ? Run : 0;
			const std::pair<double, double> Now = LeaderPoint(Walked);
			Still = Now == Last ? Still + 1 : 0;
			Last = Now;
			if (!bJumped && (std::hypot(Now.first - TakeOffX, Now.second - TakeOffZ) <= TakeOffReach || Still >= StillFrames))
			{
				bJumped = true;
				Grounded = 0;
				Held |= Jump;
			}
			return Held;
		};
		bool bRested = false;
		return WalkLeader(AimX, AimZ, Rested, bRested, Buttons);
	}

	std::optional<FError> FStorySweepRun::OpenFloor(const FStorySweepTrigger& Trigger, FStorySweepStretch& Stretch)
	{
		const FWalkedFloor Target{Trigger.Number / FloorLayerSpan, Trigger.Number % FloorLayerSpan};
		const FEventContext* Context = FieldContext();
		Stretch.Opening = EStorySweepOpening::Unanswered;
		if (Context == nullptr || Context->PlayerLock != 0)
		{
			return std::nullopt;
		}
		const int32 StartMap = Context->RunningMap;
		std::vector<FFloorProbe> Probes = FloorProbes(*Context);
		std::erase_if(Probes, [&](const FFloorProbe& Probe) { return Probe.Toward != Target; });
		Probes.resize(std::min(Probes.size(), MostProbes));
		Stretch.Opening = EStorySweepOpening::Unreached;
		for (const FFloorProbe& Probe : Probes)
		{
			const std::vector<ECrossingMove> Moves = Probe.bUpward ? std::vector<ECrossingMove>{ECrossingMove::Walk}
																   : std::vector<ECrossingMove>{ECrossingMove::Walk, ECrossingMove::Jump, ECrossingMove::RunningJump};
			for (const ECrossingMove Move : Moves)
			{
				// Each try sets off from the floor the probe was found on: back there first, where the last left the leader elsewhere.
				Context = FieldContext();
				if (Context == nullptr || Context->PlayerLock != 0 || Context->RunningMap != StartMap)
				{
					Stretch.Opening = EStorySweepOpening::Unanswered;
					return std::nullopt;
				}
				const std::vector<FReachedFloor> Reached = ReachedFloors(*Context);
				const auto Back = std::find_if(Reached.begin(), Reached.end(), [&](const FReachedFloor& Each) { return Each.Floor == Probe.From.Floor; });
				if (Back == Reached.end())
				{
					continue;
				}
				bool bBack = false;
				if (auto Error = FollowRoute(Back->Route, bBack))
				{
					return Error;
				}
				bool bPlayed = false;
				if (!bBack)
				{
					continue;
				}
				if (auto Error = PlayCrossing(Probe.Start, Probe.AimX, Probe.AimZ, Probe.TakeOffX, Probe.TakeOffZ, Move, bPlayed))
				{
					return Error;
				}
				Context = FieldContext();
				if (Context == nullptr || Context->PlayerLock != 0 || Context->RunningMap != StartMap)
				{
					Stretch.Opening = EStorySweepOpening::Unanswered;
					return std::nullopt;
				}
				// Wherever it came to rest off the floor it set off from is a way there, kept for every later walk.
				const std::optional<FWalkedFloor> Landed = LeaderFloor(*Context);
				const bool bKnown = std::any_of(Crossings.begin(), Crossings.end(), [&](const FFloorCrossing& Each) { return Each.From == Probe.From.Floor && Each.To == Landed; });
				if (!bPlayed || !Landed || *Landed == Probe.From.Floor)
				{
					continue;
				}
				if (!bKnown)
				{
					Crossings.push_back({Probe.From.Floor, *Landed, Probe.Start, Probe.AimX, Probe.AimZ, Probe.TakeOffX, Probe.TakeOffZ, Move, LeaderStanding(*Context)});
				}
				if (*Landed == Target)
				{
					Stretch.Opening = EStorySweepOpening::Answered;
					return std::nullopt;
				}
			}
		}
		return std::nullopt;
	}
}
