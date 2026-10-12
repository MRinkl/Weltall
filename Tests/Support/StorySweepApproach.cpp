#include "Support/StorySweepApproach.h"

#include "Field/EventContext.h"
#include "Field/EventPlacement.h"
#include "Field/FieldActorBox.h"
#include "Field/FieldFollowers.h"
#include "Field/FieldMap.h"
#include "Field/FieldWalkmesh.h"
#include "Graphics/Sprite.h"
#include "Input/PadState.h"
#include "Support/StorySweepGround.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Weltall::Testing
{
	namespace
	{
		/** How far past touching the leader is stood beside an actor, nearest first: a touch reaches 8, a talk 32. */
		constexpr std::array<int32, 5> BesideGaps = {4, 12, 20, 28, 31};
		/** The same near it, past both. */
		constexpr std::array<int32, 2> NearGaps = {40, 56};
		/** How far short of a zone's edge the leader is stood to walk into it, nearest first. */
		constexpr std::array<double, 3> StageSteps = {40.0, 24.0, 64.0};
		/** How far past a zone's edge the leader is stood in it, nearest first. */
		constexpr std::array<double, 3> EdgeSteps = {4.0, 12.0, 24.0};
		/** How far back along the leader's own way to a zone a walk into it is begun, the first that goes straight taken. */
		constexpr std::array<int32, 3> WayBacks = {40, 24, 12};
		/** How far around a point of the walked ground a zone's floor is looked for: the ground's points lie four units apart. */
		constexpr int32 ZoneSearch = 2;
		/** The step of the points a zone is searched for floor at its height by, in units. */
		constexpr int32 ZoneFloorStep = 8;
		/** How far past the point of a zone its way comes to a walk into it is aimed. */
		constexpr double PastZonePoint = 64.0;
		/** How far apart the points of a straight walk are looked at for ground under them. */
		constexpr double StraightStep = 4.0;
		/** How far outside a box the leader is stood beside it: a talk and a touch reach 16 past its sides. */
		constexpr std::array<int32, 2> BoxGaps = {8, 14};
		constexpr int32 Directions = 16;
		constexpr double Turn = 6.283185307179586;
		/** Angles count 4096 to a turn. */
		constexpr double AngleUnits = 4096.0;
		constexpr uint16 AngleBits = 0xFFF;
		/** An actor's first flag word: its body is a box, not a circle. */
		constexpr uint32 BoxBody = 0x2000;
		/** An actor's second flag word: a box that talks only to a leader facing it. */
		constexpr uint32 TalksWhenFaced = 0x40000;
		/** How far from every stand tried before the next one tried lies. */
		constexpr double StandsApart = 16.0;
		/** The ticks the view takes its goals at once for, as a map's start gives it. */
		constexpr int32 ViewSnapTicks = 2;

		int32 Whole(int32 Value)
		{
			return static_cast<int16>(Value >> 16);
		}

		/** Whether the ground point lies in the zone: on one side of all four of its edges, seen from above, an edge included. */
		bool IsInZone(const std::array<FGteVector, 4>& Corners, double X, double Z)
		{
			bool bLeft = false;
			bool bRight = false;
			for (std::size_t Edge = 0; Edge < Corners.size(); ++Edge)
			{
				const FGteVector& From = Corners[Edge];
				const FGteVector& To = Corners[(Edge + 1) % Corners.size()];
				const double Side = (To[0] - From[0]) * (Z - From[2]) - (To[2] - From[2]) * (X - From[0]);
				bLeft = bLeft || Side > 0.0;
				bRight = bRight || Side < 0.0;
			}
			return !(bLeft && bRight);
		}

		/** The ground points to try around an actor's interaction point (Centre), nearest the side of the walk's start (From) first. */
		std::vector<std::pair<int32, int32>> PlacesBy(const FEventActorState& Actor, const FFieldStanding& From, double CentreX, double CentreZ, EFieldApproach Approach, int32 LeaderRadius)
		{
			std::vector<std::pair<int32, int32>> Places;
			const bool bNear = Approach == EFieldApproach::Near;
			const double Toward = std::atan2(From.Z - CentreZ, From.X - CentreX);
			if ((Actor.Flags0 & BoxBody) != 0)
			{
				// Beside a box is in it where it is no body (an area to walk into) and it asks for no facing, which
				// nobody standing at its middle has; else just past the middle of a side, then past a corner and part
				// of the way along a side: a box against a wall has floor by some of its rim only.
				if (!bNear && (Actor.Flags1 & TalksWhenFaced) == 0)
				{
					Places.emplace_back(static_cast<int32>(CentreX), static_cast<int32>(CentreZ));
				}
				for (const int32 Gap : bNear ? std::span<const int32>(NearGaps) : std::span<const int32>(BoxGaps))
				{
					const int32 Wide = Actor.HalfWidth + Gap + (bNear ? LeaderRadius : 0);
					const int32 Deep = Actor.HalfDepth + Gap + (bNear ? LeaderRadius : 0);
					for (const auto& [Across, Along] : {std::pair<int32, int32>{Wide, 0}, {-Wide, 0}, {0, Deep}, {0, -Deep}})
					{
						Places.emplace_back(Whole(Actor.Position[0]) + Across, Whole(Actor.Position[2]) + Along);
					}
					for (const auto& [Across, Along] : {std::pair<int32, int32>{Wide, Deep}, {Wide, -Deep}, {-Wide, Deep}, {-Wide, -Deep}, {Wide, Deep / 2}, {Wide, -Deep / 2},
							 {-Wide, Deep / 2}, {-Wide, -Deep / 2}, {Wide / 2, Deep}, {-Wide / 2, Deep}, {Wide / 2, -Deep}, {-Wide / 2, -Deep}})
					{
						Places.emplace_back(Whole(Actor.Position[0]) + Across, Whole(Actor.Position[2]) + Along);
					}
				}
				return Places;
			}
			for (const int32 Gap : bNear ? std::span<const int32>(NearGaps) : std::span<const int32>(BesideGaps))
			{
				const double Reach = Actor.Radius + LeaderRadius + Gap;
				for (int32 Step = 0; Step < Directions; ++Step)
				{
					// The side the leader is on first, then alternately to either side of it.
					const int32 Offset = (Step + 1) / 2 * (Step % 2 == 0 ? 1 : -1);
					const double Angle = Toward + Turn * Offset / Directions;
					const long X = std::lround(CentreX + Reach * std::cos(Angle));
					const long Z = std::lround(CentreZ + Reach * std::sin(Angle));
					Places.emplace_back(static_cast<int32>(X), static_cast<int32>(Z));
				}
			}
			return Places;
		}
	}

	namespace
	{
		/** The actor's sprite put where the actor now stands, at rest. */
		void MoveSpriteTo(FEventContext& Context, int32 Actor, const FEventActorState& State)
		{
			if (FSprite* Sprite = Context.Models != nullptr ? Context.Models->ActorSprite(Actor) : nullptr)
			{
				Sprite->FloorY = State.FloorHeight;
				Sprite->Position = State.Position;
				Sprite->Velocity = {};
			}
		}

		const FEventActorState* StateIn(const FEventContext& Context, int32 Actor)
		{
			return Actor >= 0 && static_cast<std::size_t>(Actor) < Context.States.size() ? &Context.States[static_cast<std::size_t>(Actor)] : nullptr;
		}

		/** The leader as it would stand at a ground point: its state and its record placed there. */
		struct FPlacedLeader
		{
			FEventActorState State;
			FFieldActor Record;
			int32 X = 0;
			int32 Z = 0;
		};

		/**
		 * The leader stood at the first of Places the Ground's walk reaches (on Layer, the ground's) and no actor's
		 * body covers, facing (FaceX, FaceZ), and that lies apart from every stand of Tried. None where no such place is.
		 */
		std::optional<FPlacedLeader> FindStand(const FEventContext& Context, const std::vector<std::pair<int32, int32>>& Places, double FaceX, double FaceZ,
			const FWalkedGround& Ground, int32 Layer, const std::vector<std::pair<int32, int32>>* Tried = nullptr)
		{
			const FEventActorState* Leader = StateIn(Context, Context.LeaderActor);
			if (Leader == nullptr || Context.Walkmesh == nullptr || Context.LeaderActor >= Context.Actors.size() || Layer < 0
				|| static_cast<std::size_t>(Layer) + 1 >= Context.Walkmesh->Layers.size())
			{
				return std::nullopt;
			}
			for (const auto& [X, Z] : Places)
			{
				// On ground the leader's walk reaches: placing alone asks for no floor under the point.
				const std::pair<int32, int32> Place{X, Z};
				const auto IsNear = [&Place](const std::pair<int32, int32>& Stand) { return std::hypot(Stand.first - Place.first, Stand.second - Place.second) < StandsApart; };
				if (!Ground.Reaches(X, Z) || (Tried != nullptr && std::any_of(Tried->begin(), Tried->end(), IsNear)))
				{
					continue;
				}
				FPlacedLeader Placed{*Leader, Context.Actors[Context.LeaderActor], X, Z};
				Placed.State.Layer = static_cast<uint16>(Layer);
				if (!PlaceActor(Placed.State, Placed.Record, *Context.Walkmesh, X, Z))
				{
					continue;
				}
				// Nobody's body in the way: no walk ends inside one.
				bool bStopped = false;
				for (std::size_t Index = 0; Index < Context.States.size() && !bStopped; ++Index)
				{
					bStopped = Index != Context.LeaderActor && StopsLeaderAt(Context.States[Index], *Leader, X, Z, Placed.State.FloorHeight);
				}
				if (bStopped)
				{
					continue;
				}
				// Facing as the field's talk asks: the direction from the leader to the point, turned the screen's way.
				if (FaceX != X || FaceZ != Z)
				{
					const double Toward = std::atan2(FaceZ - Z, FaceX - X);
					Placed.State.Facing = static_cast<uint16>(static_cast<int32>(std::lround(-Toward * AngleUnits / Turn)) & AngleBits);
				}
				return Placed;
			}
			return std::nullopt;
		}

		/** The leader stood where FindStand put it; the party and the view come with it, as they do behind a walk. */
		void Stand(FEventContext& Context, const FPlacedLeader& Placed)
		{
			FEventActorState& Leader = *Context.StateOf(Context.LeaderActor);
			Leader = Placed.State;
			Context.Actors[Context.LeaderActor] = Placed.Record;
			MoveSpriteTo(Context, Context.LeaderActor, Leader);
			// A scene that speaks at a member left across the map would speak at someone the view does not show. The
			// trail the members follow starts afresh here, or they would walk the leader's steps of before first. A
			// member the scripts keep standing elsewhere follows no walk, and stays: stood on the leader, its body
			// would hold the leader for ever.
			for (const uint8 Member : Context.PartyActors)
			{
				FEventActorState* Follower = Member != Context.LeaderActor ? Context.StateOf(Member) : nullptr;
				if (Follower == nullptr || Member >= Context.Actors.size() || (Follower->Flags0 & EventActorFlags0::Frozen) != 0
					|| (Follower->Flags0 & EventActorFlags0::PartyFollower) == 0)
				{
					continue;
				}
				FEventActorState Moved = *Follower;
				FFieldActor MovedRecord = Context.Actors[Member];
				Moved.Layer = Leader.Layer;
				if (PlaceActor(Moved, MovedRecord, *Context.Walkmesh, Placed.X, Placed.Z))
				{
					*Follower = Moved;
					Context.Actors[Member] = MovedRecord;
					MoveSpriteTo(Context, Member, *Follower);
				}
			}
			RestartFollowTrail(Context);
			// And the view, which a walk would have brought along: it arrives at once, as at a map's start.
			Context.Camera.SnapTicks = ViewSnapTicks;
		}

		bool StandLeader(FEventContext& Context, const std::vector<std::pair<int32, int32>>& Places, double FaceX, double FaceZ, const FWalkedGround& Ground)
		{
			const std::optional<FPlacedLeader> Placed = FindStand(Context, Places, FaceX, FaceZ, Ground, LeaderStanding(Context).Layer);
			if (Placed)
			{
				Stand(Context, *Placed);
			}
			return Placed.has_value();
		}

		/**
		 * Where an actor is approached from, as a walk that sets off from From would: none where its interaction point
		 * is not at the height of the leader's body standing there, which is all the field's talk and touch ask of
		 * height. An actor's layer says nothing of it: one that never walks keeps layer 0 wherever it stands (the
		 * doors of the gallery above the hall below Aveh's arena, map 133, 464 units over the hall's own).
		 */
		std::optional<FPlacedLeader> ActorStand(const FEventContext& Context, int32 Actor, EFieldApproach Approach, const std::set<int32>* Crossed, const FFieldStanding& From,
			bool bOwnPlace, const std::vector<std::pair<int32, int32>>* Tried = nullptr)
		{
			const FEventActorState* Leader = StateIn(Context, Context.LeaderActor);
			const FEventActorState* Other = StateIn(Context, Actor);
			if (Leader == nullptr || Other == nullptr)
			{
				return std::nullopt;
			}
			const double CentreX = Whole(Other->Position[0]) + Other->InteractX;
			const double CentreZ = Whole(Other->Position[2]) + Other->InteractZ;
			const FWalkedGround Ground(Context, Actor, Crossed, bOwnPlace ? std::nullopt : std::optional<FFieldStanding>(From));
			std::optional<FPlacedLeader> Placed = FindStand(Context, PlacesBy(*Other, From, CentreX, CentreZ, Approach, Leader->Radius), CentreX, CentreZ, Ground, From.Layer, Tried);
			const int32 Point = Whole(Other->Position[1]) + Other->Word62;
			if (Placed && (Point < Placed->State.FloorHeight - Leader->Height || Placed->State.FloorHeight < Point - Other->Height))
			{
				return std::nullopt;
			}
			return Placed;
		}

		/** The middle of a zone's four corners, on the ground. */
		std::pair<int32, int32> ZoneMiddle(const std::array<FGteVector, 4>& Corners)
		{
			int32 MiddleX = 0;
			int32 MiddleZ = 0;
			for (const FGteVector& Corner : Corners)
			{
				MiddleX += Corner[0];
				MiddleZ += Corner[2];
			}
			return {MiddleX / 4, MiddleZ / 4};
		}

		/**
		 * Where a walk from (FromX, FromZ) to a zone's middle first is in the zone, and a few steps on: a player crosses
		 * a zone's edge and never stands at its middle, where a scene may send its people. Then the middle, and half way
		 * to each corner: inside a zone no wider than a wall's foot, one of them is floor.
		 */
		std::vector<std::pair<int32, int32>> PlacesIn(const std::array<FGteVector, 4>& Corners, double FromX, double FromZ)
		{
			const auto [MiddleX, MiddleZ] = ZoneMiddle(Corners);
			std::vector<std::pair<int32, int32>> Places;
			const double Length = std::hypot(MiddleX - FromX, MiddleZ - FromZ);
			for (double Walked = 0.0; Walked < Length; Walked += 1.0)
			{
				const double X = FromX + (MiddleX - FromX) * Walked / Length;
				const double Z = FromZ + (MiddleZ - FromZ) * Walked / Length;
				if (IsInZone(Corners, X, Z))
				{
					for (const double Step : EdgeSteps)
					{
						const double Along = std::min(Walked + Step, Length) / Length;
						Places.emplace_back(static_cast<int32>(std::lround(FromX + (MiddleX - FromX) * Along)), static_cast<int32>(std::lround(FromZ + (MiddleZ - FromZ) * Along)));
					}
					break;
				}
			}
			Places.emplace_back(MiddleX, MiddleZ);
			for (const FGteVector& Corner : Corners)
			{
				Places.emplace_back((MiddleX + Corner[0]) / 2, (MiddleZ + Corner[2]) / 2);
			}
			return Places;
		}

		/** How far along the straight way from (FromX, FromZ) to a zone's middle the zone begins; none where the way starts in it or never meets it. */
		std::optional<double> ZoneEdgeOnWay(const std::array<FGteVector, 4>& Corners, double FromX, double FromZ, double MiddleX, double MiddleZ)
		{
			const double Length = std::hypot(MiddleX - FromX, MiddleZ - FromZ);
			if (Length < 1.0 || IsInZone(Corners, FromX, FromZ))
			{
				return std::nullopt;
			}
			for (double Walked = 1.0; Walked <= Length; Walked += 1.0)
			{
				if (IsInZone(Corners, FromX + (MiddleX - FromX) * Walked / Length, FromZ + (MiddleZ - FromZ) * Walked / Length))
				{
					return Walked;
				}
			}
			return std::nullopt;
		}

		/**
		 * The way the leader's walk first comes into a zone: the nearest point of the zone that is floor of the walked
		 * ground, at the height the zone's own test asks for where the ground has such a point, and the point Back units
		 * before it on the walk's way there. A doorway's zone may lie off the floor but for a strip at its edge, which
		 * neither its middle nor the straight way to it need meet.
		 */
		/**
		 * Whether the zone is another layer's: the walked ground (of layer Layer) has no point inside it at the height
		 * the zone's own test asks for, and a floor of another layer has. On the stairs of map 157 the way on to map 158
		 * is a zone of the landing above them; walked into on the stairs, below it, the walk met the stairs' own zone
		 * first. A floor of the walk's own layer is walked onto (the ladder up to map 147's way on, which the ground
		 * leaves out).
		 */
		bool IsZoneOfAnotherFloor(const FEventContext& Context, const std::array<FGteVector, 4>& Corners, const FWalkedGround& Ground, int32 Layer)
		{
			const FEventActorState* Leader = StateIn(Context, Context.LeaderActor);
			if (Leader == nullptr)
			{
				return false;
			}
			const int32 ZoneHeight = Corners[0][1];
			const auto AtItsHeight = [&](int32 Feet) { return ZoneHeight < Feet && Feet - Leader->Height < ZoneHeight; };
			const int32 Left = std::min({Corners[0][0], Corners[1][0], Corners[2][0], Corners[3][0]});
			const int32 Right = std::max({Corners[0][0], Corners[1][0], Corners[2][0], Corners[3][0]});
			const int32 Near = std::min({Corners[0][2], Corners[1][2], Corners[2][2], Corners[3][2]});
			const int32 Far = std::max({Corners[0][2], Corners[1][2], Corners[2][2], Corners[3][2]});
			bool bElsewhere = false;
			for (int32 X = Left; X <= Right; X += ZoneFloorStep)
			{
				for (int32 Z = Near; Z <= Far; Z += ZoneFloorStep)
				{
					if (!IsInZone(Corners, X, Z))
					{
						continue;
					}
					if (const std::optional<int32> Feet = Ground.HeightAt(X, Z); Feet && AtItsHeight(*Feet))
					{
						return false;
					}
					for (int32 Other = 0; Other < WalkingLayers(Context) && !bElsewhere; ++Other)
					{
						const std::optional<FFloorUnder> Under = Other != Layer ? WalkedFloorAt(Context, Other, X, Z) : std::nullopt;
						bElsewhere = Under && AtItsHeight(Under->Height);
					}
				}
			}
			return bElsewhere;
		}

		std::optional<FGroundWay> WayIntoZone(const FEventContext& Context, const std::array<FGteVector, 4>& Corners, const FWalkedGround& Ground, int32 Back)
		{
			const FEventActorState* Leader = StateIn(Context, Context.LeaderActor);
			if (Leader == nullptr)
			{
				return std::nullopt;
			}
			const int32 Left = std::min({Corners[0][0], Corners[1][0], Corners[2][0], Corners[3][0]});
			const int32 Right = std::max({Corners[0][0], Corners[1][0], Corners[2][0], Corners[3][0]});
			const int32 Near = std::min({Corners[0][2], Corners[1][2], Corners[2][2], Corners[3][2]});
			const int32 Far = std::max({Corners[0][2], Corners[1][2], Corners[2][2], Corners[3][2]});
			const int32 ZoneHeight = Corners[0][1];
			for (const bool bAtItsHeight : {true, false})
			{
				int32 InX = 0;
				int32 InZ = 0;
				const auto Wanted = [&](int32 X, int32 Z)
				{
					if (X < Left - ZoneSearch || X > Right + ZoneSearch || Z < Near - ZoneSearch || Z > Far + ZoneSearch)
					{
						return false;
					}
					const std::optional<int32> Feet = Ground.HeightAt(X, Z);
					if (bAtItsHeight && (!Feet || !(ZoneHeight < *Feet && *Feet - Leader->Height < ZoneHeight)))
					{
						return false;
					}
					// The point itself before those around it.
					for (int32 Around = 0; Around <= ZoneSearch; ++Around)
					{
						for (int32 Along = -Around; Along <= Around; ++Along)
						{
							for (int32 Across = -Around; Across <= Around; ++Across)
							{
								if (std::max(std::abs(Along), std::abs(Across)) == Around && IsInZone(Corners, X + Across, Z + Along) && Ground.Reaches(X + Across, Z + Along))
								{
									InX = X + Across;
									InZ = Z + Along;
									return true;
								}
							}
						}
					}
					return false;
				};
				if (std::optional<FGroundWay> Way = Ground.WayTo(Wanted, Back))
				{
					Way->X = InX;
					Way->Z = InZ;
					return Way;
				}
			}
			return std::nullopt;
		}

		/** Whether the straight walk from one ground point to another has the walked ground under all of it. */
		bool GoesStraight(const FWalkedGround& Ground, const FGroundWay& Way)
		{
			const double Length = std::hypot(Way.X - Way.FromX, Way.Z - Way.FromZ);
			for (double Walked = 0.0; Walked < Length; Walked += StraightStep)
			{
				if (!Ground.Covers(static_cast<int32>(std::lround(Way.FromX + (Way.X - Way.FromX) * Walked / Length)), static_cast<int32>(std::lround(Way.FromZ + (Way.Z - Way.FromZ) * Walked / Length))))
				{
					return false;
				}
			}
			return true;
		}

		/** Short of a zone, on the straight way from (FromX, FromZ) to its middle: none where the way starts in it or never meets it. */
		std::vector<std::pair<int32, int32>> PlacesBefore(const std::array<FGteVector, 4>& Corners, double FromX, double FromZ)
		{
			const auto [MiddleX, MiddleZ] = ZoneMiddle(Corners);
			const std::optional<double> Edge = ZoneEdgeOnWay(Corners, FromX, FromZ, MiddleX, MiddleZ);
			std::vector<std::pair<int32, int32>> Places;
			if (!Edge)
			{
				return Places;
			}
			const double Length = std::hypot(MiddleX - FromX, MiddleZ - FromZ);
			for (const double Step : StageSteps)
			{
				const double Along = std::max(*Edge - Step, 0.0) / Length;
				Places.emplace_back(static_cast<int32>(std::lround(FromX + (MiddleX - FromX) * Along)), static_cast<int32>(std::lround(FromZ + (MiddleZ - FromZ) * Along)));
			}
			return Places;
		}
	}

	bool ApproachFieldActor(FEventContext& Context, int32 Actor, EFieldApproach Approach, const std::set<int32>* Crossed, std::vector<std::pair<int32, int32>>* Tried)
	{
		const std::optional<FPlacedLeader> Placed = ActorStand(Context, Actor, Approach, Crossed, LeaderStanding(Context), true, Tried);
		if (Placed)
		{
			Stand(Context, *Placed);
			if (Tried != nullptr)
			{
				Tried->emplace_back(Placed->X, Placed->Z);
			}
		}
		return Placed.has_value();
	}

	bool ReachesFieldActor(const FEventContext& Context, int32 Actor, EFieldApproach Approach, const std::set<int32>* Crossed, const FFieldStanding& From)
	{
		return ActorStand(Context, Actor, Approach, Crossed, From, false).has_value();
	}

	bool EnterFieldZone(FEventContext& Context, int32 Zone, const std::set<int32>* Crossed)
	{
		if (Zone < 0 || static_cast<std::size_t>(Zone) >= Context.Zones.size() || Context.StateOf(Context.LeaderActor) == nullptr)
		{
			return false;
		}
		const std::array<FGteVector, 4>& Corners = Context.Zones[static_cast<std::size_t>(Zone)].Corners;
		const auto [FromX, FromZ] = LeaderPoint(Context);
		const auto [MiddleX, MiddleZ] = ZoneMiddle(Corners);
		const FWalkedGround Ground(Context, -1, Crossed);
		std::vector<std::pair<int32, int32>> Places = PlacesIn(Corners, FromX, FromZ);
		if (const std::optional<FGroundWay> Way = WayIntoZone(Context, Corners, Ground, 0))
		{
			Places.emplace_back(Way->X, Way->Z);
		}
		return StandLeader(Context, Places, MiddleX, MiddleZ, Ground);
	}

	std::optional<std::pair<double, double>> StageBeforeFieldZone(FEventContext& Context, int32 Zone, const std::set<int32>* Crossed, bool bByItsWalk)
	{
		if (Zone < 0 || static_cast<std::size_t>(Zone) >= Context.Zones.size() || Context.StateOf(Context.LeaderActor) == nullptr)
		{
			return std::nullopt;
		}
		const std::array<FGteVector, 4>& Corners = Context.Zones[static_cast<std::size_t>(Zone)].Corners;
		const FWalkedGround Ground(Context, -1, Crossed);
		if (!bByItsWalk)
		{
			const auto [FromX, FromZ] = LeaderPoint(Context);
			const auto [MiddleX, MiddleZ] = ZoneMiddle(Corners);
			const std::vector<std::pair<int32, int32>> Places = PlacesBefore(Corners, FromX, FromZ);
			if (Places.empty() || !StandLeader(Context, Places, MiddleX, MiddleZ, Ground))
			{
				return std::nullopt;
			}
			return std::pair<double, double>{MiddleX, MiddleZ};
		}
		// The furthest back the walk goes straight from; the nearest where its way bends before that.
		std::optional<FGroundWay> Taken;
		for (const int32 Back : WayBacks)
		{
			const std::optional<FGroundWay> Way = WayIntoZone(Context, Corners, Ground, Back);
			if (!Way || IsInZone(Corners, Way->FromX, Way->FromZ) || (Way->FromX == Way->X && Way->FromZ == Way->Z))
			{
				continue;
			}
			Taken = Way;
			if (GoesStraight(Ground, *Way))
			{
				break;
			}
		}
		if (!Taken || !StandLeader(Context, {{Taken->FromX, Taken->FromZ}}, Taken->X, Taken->Z, Ground))
		{
			return std::nullopt;
		}
		const double Length = std::hypot(Taken->X - Taken->FromX, Taken->Z - Taken->FromZ);
		return std::pair<double, double>{Taken->X + (Taken->X - Taken->FromX) * PastZonePoint / Length, Taken->Z + (Taken->Z - Taken->FromZ) * PastZonePoint / Length};
	}

	bool ReachesFieldZone(const FEventContext& Context, int32 Zone, const std::set<int32>* Crossed, const FFieldStanding& From)
	{
		if (Zone < 0 || static_cast<std::size_t>(Zone) >= Context.Zones.size())
		{
			return false;
		}
		const std::array<FGteVector, 4>& Corners = Context.Zones[static_cast<std::size_t>(Zone)].Corners;
		const auto [MiddleX, MiddleZ] = ZoneMiddle(Corners);
		const FWalkedGround Ground(Context, -1, Crossed, From);
		if (IsZoneOfAnotherFloor(Context, Corners, Ground, From.Layer))
		{
			return false;
		}
		std::vector<std::pair<int32, int32>> Places = PlacesBefore(Corners, From.X, From.Z);
		const std::vector<std::pair<int32, int32>> Inside = PlacesIn(Corners, From.X, From.Z);
		Places.insert(Places.end(), Inside.begin(), Inside.end());
		return FindStand(Context, Places, MiddleX, MiddleZ, Ground, From.Layer).has_value() || WayIntoZone(Context, Corners, Ground, 0).has_value();
	}

	bool StandLeaderAt(FEventContext& Context, int32 X, int32 Z, double FaceX, double FaceZ, const std::set<int32>* Crossed)
	{
		return StandLeader(Context, {{X, Z}}, FaceX, FaceZ, FWalkedGround(Context, -1, Crossed));
	}

	std::string FieldWalkBarriers(const FEventContext& Context, int32 Target, const std::set<int32>* Crossed)
	{
		const FWalkedGround Ground(Context, Target, Crossed);
		const FFieldStanding From = LeaderStanding(Context);
		std::string Told = "walks " + std::to_string(Ground.Size()) + " points from (" + std::to_string(From.X) + "," + std::to_string(From.Z) + ") on layer " + std::to_string(From.Layer);
		bool bFirst = true;
		for (const FWalkedGround::FBarrier& Barrier : Ground.Barriers())
		{
			Told += (bFirst ? ", barred by " : ", ") + std::string(Barrier.bTouch ? "touch " : "body ") + std::to_string(Barrier.Actor);
			bFirst = false;
		}
		return Told;
	}

	uint16 FieldRunButtons(const FEventContext& Context)
	{
		return Context.bAlwaysRun ? uint16{0} : FPadState::Bit(EPadButton::Circle);
	}

	std::pair<double, double> LeaderPoint(const FEventContext& Context)
	{
		const FEventActorState& Leader = Context.States[Context.LeaderActor];
		return {Whole(Leader.Position[0]), Whole(Leader.Position[2])};
	}

	std::pair<double, double> FieldZoneMiddle(const FEventContext& Context, int32 Zone)
	{
		if (Zone < 0 || static_cast<std::size_t>(Zone) >= Context.Zones.size())
		{
			return LeaderPoint(Context);
		}
		double X = 0.0;
		double Z = 0.0;
		for (const FGteVector& Corner : Context.Zones[static_cast<std::size_t>(Zone)].Corners)
		{
			X += Corner[0] / 4.0;
			Z += Corner[2] / 4.0;
		}
		return {X, Z};
	}

	bool IsLeaderInFieldZone(const FEventContext& Context, int32 Zone)
	{
		const auto [X, Z] = LeaderPoint(Context);
		return Zone >= 0 && static_cast<std::size_t>(Zone) < Context.Zones.size() && IsInZone(Context.Zones[static_cast<std::size_t>(Zone)].Corners, X, Z);
	}

	std::pair<double, double> FieldActorPoint(const FEventContext& Context, int32 Actor)
	{
		const FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
		return {Whole(State.Position[0]) + State.InteractX, Whole(State.Position[2]) + State.InteractZ};
	}

	bool IsLeaderWithin(const FEventContext& Context, int32 Actor, int32 Gap)
	{
		if (Actor < 0 || static_cast<std::size_t>(Actor) >= Context.States.size())
		{
			return false;
		}
		const FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
		const FEventActorState& Leader = Context.States[Context.LeaderActor];
		const auto [X, Z] = LeaderPoint(Context);
		if ((State.Flags0 & BoxBody) != 0)
		{
			return IsInActorBox(State, static_cast<int32>(X), static_cast<int32>(Z), Gap);
		}
		const auto [CentreX, CentreZ] = FieldActorPoint(Context, Actor);
		return std::hypot(CentreX - X, CentreZ - Z) <= State.Radius + Leader.Radius + Gap;
	}

	bool IsFieldActorShown(const FEventContext& Context, int32 Actor)
	{
		return Actor >= 0 && static_cast<std::size_t>(Actor) < Context.States.size()
			&& (Context.States[static_cast<std::size_t>(Actor)].Flags1 & EventActorFlags1::OffScreen) == 0;
	}

	bool RunsFieldScript(const FEventContext& Context, int32 Actor, int32 Script)
	{
		if (Actor < 0 || static_cast<std::size_t>(Actor) >= Context.States.size())
		{
			return false;
		}
		const auto& Threads = Context.States[static_cast<std::size_t>(Actor)].Threads;
		return std::any_of(Threads.begin(), Threads.end(), [Script](const FEventThread& Thread) { return !Thread.IsFree() && Thread.Script == Script; });
	}
}
