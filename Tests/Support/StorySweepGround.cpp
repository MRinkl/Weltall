#include "Support/StorySweepGround.h"

#include "Field/EventContext.h"
#include "Field/EventMachine.h"
#include "Field/FieldActorBox.h"
#include "Field/FieldWalkmesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace Weltall::Testing
{
	namespace
	{
		/** How far apart the ground's points lie. */
		constexpr int32 Step = 4;
		constexpr int32 NoFloor = std::numeric_limits<int32>::max();
		// An actor's flags as the leader's collisions and the field's triggers read them.
		constexpr uint32 BoxBody = 0x2000;
		constexpr uint32 Platform = 0x80;
		constexpr uint32 PlatformBits = 0x180;
		constexpr uint32 TouchBlocked = 0xA20000;
		/** How far past its box, and past touching it, an actor's touch answers the leader. */
		constexpr int32 BoxTouchMargin = 0x10;
		constexpr int32 TouchReach = 8;
		/** The instruction a script that does nothing begins with. */
		constexpr uint8 EndOpcode = 0x00;
		// A floor's collision classes in its attributes, and an actor's in its first flag word.
		constexpr uint32 FloorClassShift = 5;
		constexpr uint32 WalkerClassShift = 8;
		constexpr uint32 ClassBits = 0x7;
		/** A floor's attribute: it hands whoever walks onto it to the layer above. */
		constexpr uint32 HandsUp = 0x4;

		int32 Whole(int32 Value)
		{
			return static_cast<int16>(Value >> 16);
		}

		bool IsSolid(const FEventActorState& Actor)
		{
			return (Actor.Flags0 & (EventActorFlags0::Frozen | EventActorFlags0::PassesThroughActors)) == 0 && (Actor.Flags1 & Platform) == 0;
		}

		/** Whether the actor's touch would answer the leader standing at (X, Z) with its feet at Feet, as the field's check measures it. */
		bool TouchesLeaderAt(const FEventActorState& Actor, const FEventActorState& Leader, int32 X, int32 Z, int32 Feet)
		{
			const int32 Point = Whole(Actor.Position[1]) + Actor.Word62;
			if (Point < Feet - Leader.Height || Feet < Point - Actor.Height)
			{
				return false;
			}
			if ((Actor.Flags0 & BoxBody) != 0)
			{
				return IsInActorBox(Actor, X, Z, BoxTouchMargin);
			}
			const int64 Across = Whole(Actor.Position[0]) + Actor.InteractX - X;
			const int64 Along = Whole(Actor.Position[2]) + Actor.InteractZ - Z;
			const int64 Reach = Leader.Radius + TouchReach + Actor.Radius;
			return Across * Across + Along * Along < Reach * Reach;
		}

		/** Whether the layer's triangle is one the walker may walk onto: not closed, and of none of its collision classes. */
		bool IsOpenTo(const FFieldWalkmesh& Walkmesh, std::size_t Layer, int32 Triangle, const FEventActorState& Walker)
		{
			const std::vector<FWalkmeshTriangle>& Triangles = Walkmesh.Layers[Layer].Triangles;
			if (Triangle < 0 || static_cast<std::size_t>(Triangle) >= Triangles.size())
			{
				return false;
			}
			const std::size_t Attribute = Triangles[static_cast<std::size_t>(Triangle)].Attribute;
			const uint32 Word = Attribute < Walkmesh.Attributes.size() ? Walkmesh.Attributes[Attribute] : 0;
			const uint32 Classes = Walker.Flags0 >> WalkerClassShift & ClassBits;
			return (Word & FieldFloorAttribute::Closed) == 0 && (Word >> FloorClassShift & ClassBits & Classes) == 0;
		}

		/**
		 * The triangles of a layer the walker's walk reaches from the one it stands on (From): those joined to it edge
		 * to edge through triangles open to it. The triangle it stands on counts whatever it is.
		 */
		std::vector<bool> WalkedFrom(const FFieldWalkmesh& Walkmesh, std::size_t Layer, const FEventActorState& Walker, int32 From)
		{
			const std::vector<FWalkmeshTriangle>& Triangles = Walkmesh.Layers[Layer].Triangles;
			std::vector<bool> Walked(Triangles.size(), false);
			std::vector<int32> Pending;
			if (From >= 0 && static_cast<std::size_t>(From) < Triangles.size())
			{
				Walked[static_cast<std::size_t>(From)] = true;
				Pending.push_back(From);
			}
			while (!Pending.empty())
			{
				const int32 At = Pending.back();
				Pending.pop_back();
				for (const int16 Next : Triangles[static_cast<std::size_t>(At)].Neighbours)
				{
					if (IsOpenTo(Walkmesh, Layer, Next, Walker) && !Walked[static_cast<std::size_t>(Next)])
					{
						Walked[static_cast<std::size_t>(Next)] = true;
						Pending.push_back(Next);
					}
				}
			}
			return Walked;
		}

		/** Each triangle's floor on the layer, as the lowest triangle joined to it through those open to the walker; -1 for one closed to it. */
		std::vector<int32> FloorsOfLayer(const FFieldWalkmesh& Walkmesh, std::size_t Layer, const FEventActorState& Walker)
		{
			const std::vector<FWalkmeshTriangle>& Triangles = Walkmesh.Layers[Layer].Triangles;
			std::vector<int32> Floor(Triangles.size(), -1);
			std::vector<int32> Pending;
			for (int32 First = 0; static_cast<std::size_t>(First) < Triangles.size(); ++First)
			{
				if (Floor[static_cast<std::size_t>(First)] >= 0 || !IsOpenTo(Walkmesh, Layer, First, Walker))
				{
					continue;
				}
				Floor[static_cast<std::size_t>(First)] = First;
				Pending.push_back(First);
				while (!Pending.empty())
				{
					const int32 At = Pending.back();
					Pending.pop_back();
					for (const int16 Next : Triangles[static_cast<std::size_t>(At)].Neighbours)
					{
						if (IsOpenTo(Walkmesh, Layer, Next, Walker) && Floor[static_cast<std::size_t>(Next)] < 0)
						{
							Floor[static_cast<std::size_t>(Next)] = First;
							Pending.push_back(Next);
						}
					}
				}
			}
			return Floor;
		}

		/** Whether the ground point lies in the triangle seen from above, an edge included. */
		bool IsInTriangle(const std::array<FGteVector, 3>& Corners, int32 X, int32 Z)
		{
			bool bLeft = false;
			bool bRight = false;
			for (std::size_t Edge = 0; Edge < Corners.size(); ++Edge)
			{
				const FGteVector& From = Corners[Edge];
				const FGteVector& To = Corners[(Edge + 1) % 3];
				const int64 Side = int64{To[0] - From[0]} * (Z - From[2]) - int64{To[2] - From[2]} * (X - From[0]);
				bLeft = bLeft || Side > 0;
				bRight = bRight || Side < 0;
			}
			return !(bLeft && bRight);
		}

		/** The floor's height at a ground point of the triangle's plane; its first corner's where the triangle stands on edge. */
		int32 PlaneHeight(const std::array<FGteVector, 3>& Corners, int32 X, int32 Z)
		{
			const double Ax = Corners[1][0] - Corners[0][0];
			const double Ay = Corners[1][1] - Corners[0][1];
			const double Az = Corners[1][2] - Corners[0][2];
			const double Bx = Corners[2][0] - Corners[0][0];
			const double By = Corners[2][1] - Corners[0][1];
			const double Bz = Corners[2][2] - Corners[0][2];
			const double NormalX = Ay * Bz - Az * By;
			const double NormalY = Az * Bx - Ax * Bz;
			const double NormalZ = Ax * By - Ay * Bx;
			if (std::abs(NormalY) < 1e-9)
			{
				return Corners[0][1];
			}
			return static_cast<int32>(std::lround(Corners[0][1] - (NormalX * (X - Corners[0][0]) + NormalZ * (Z - Corners[0][2])) / NormalY));
		}

		/** A triangle's corners, none where one of them is past the layer's vertices. */
		std::optional<std::array<FGteVector, 3>> CornersOf(const FWalkmeshLayer& Layer, std::size_t Triangle)
		{
			const std::array<uint16, 3>& Vertices = Layer.Triangles[Triangle].Vertices;
			if (std::any_of(Vertices.begin(), Vertices.end(), [&](uint16 Vertex) { return Vertex >= Layer.Vertices.size(); }))
			{
				return std::nullopt;
			}
			return std::array<FGteVector, 3>{Layer.Vertices[Vertices[0]], Layer.Vertices[Vertices[1]], Layer.Vertices[Vertices[2]]};
		}

		/** The first of the layer's triangles open to the walker that holds the ground point; -1 where none does. */
		int32 OpenTriangleAt(const FFieldWalkmesh& Walkmesh, std::size_t Layer, const FEventActorState& Walker, int32 X, int32 Z)
		{
			const FWalkmeshLayer& Triangles = Walkmesh.Layers[Layer];
			for (std::size_t Number = 0; Number < Triangles.Triangles.size(); ++Number)
			{
				const auto Corners = CornersOf(Triangles, Number);
				if (Corners && IsInTriangle(*Corners, X, Z) && IsOpenTo(Walkmesh, Layer, static_cast<int32>(Number), Walker))
				{
					return static_cast<int32>(Number);
				}
			}
			return -1;
		}

		const FEventActorState* LeaderOf(const FEventContext& Context)
		{
			return Context.LeaderActor < Context.States.size() && Context.Walkmesh != nullptr ? &Context.States[Context.LeaderActor] : nullptr;
		}

		bool IsWalkingLayer(const FEventContext& Context, int32 Layer)
		{
			return Layer >= 0 && static_cast<std::size_t>(Layer) + 1 < Context.Walkmesh->Layers.size();
		}
	}

	bool AnswersFieldTouch(const FEventContext& Context, int32 Actor)
	{
		if (Context.Scripts == nullptr || Actor < 0 || static_cast<std::size_t>(Actor) >= Context.States.size() || Actor >= Context.Scripts->ActorCount())
		{
			return false;
		}
		const uint16 Entry = Context.Scripts->EntryPoint(Actor, FEventMachine::TouchScript);
		return (Context.States[static_cast<std::size_t>(Actor)].Flags0 & TouchBlocked) == 0 && Entry < Context.Scripts->CodeSize()
			&& Context.Scripts->Code()[Entry] != EndOpcode;
	}

	bool StopsLeaderAt(const FEventActorState& Actor, const FEventActorState& Leader, int32 X, int32 Z, int32 Feet)
	{
		if (!IsSolid(Actor))
		{
			return false;
		}
		const int32 Bottom = Whole(Actor.Position[1]);
		if (Bottom < Feet - Leader.Height || Feet < Bottom - Actor.Height)
		{
			return false;
		}
		if ((Actor.Flags0 & BoxBody) != 0)
		{
			return IsInActorBox(Actor, X, Z, 0);
		}
		const int64 Across = Whole(Actor.Position[0]) - X;
		const int64 Along = Whole(Actor.Position[2]) - Z;
		const int64 Reach = Leader.Radius + Actor.Radius;
		return Across * Across + Along * Along < Reach * Reach;
	}

	FFieldStanding LeaderStanding(const FEventContext& Context)
	{
		const FEventActorState* Leader = LeaderOf(Context);
		return Leader != nullptr ? FFieldStanding{static_cast<int16>(Leader->Layer), Whole(Leader->Position[0]), Whole(Leader->Position[2])} : FFieldStanding{};
	}

	std::optional<FFloorUnder> WalkedFloorAt(const FEventContext& Context, int32 Layer, int32 X, int32 Z)
	{
		const FEventActorState* Leader = LeaderOf(Context);
		if (Leader == nullptr || !IsWalkingLayer(Context, Layer))
		{
			return std::nullopt;
		}
		const std::size_t Walking = static_cast<std::size_t>(Layer);
		const int32 Triangle = OpenTriangleAt(*Context.Walkmesh, Walking, *Leader, X, Z);
		const auto Corners = Triangle >= 0 ? CornersOf(Context.Walkmesh->Layers[Walking], static_cast<std::size_t>(Triangle)) : std::nullopt;
		if (!Corners)
		{
			return std::nullopt;
		}
		return FFloorUnder{{Layer, FloorsOfLayer(*Context.Walkmesh, Walking, *Leader)[static_cast<std::size_t>(Triangle)]}, PlaneHeight(*Corners, X, Z)};
	}

	int32 WalkingLayers(const FEventContext& Context)
	{
		return Context.Walkmesh != nullptr && !Context.Walkmesh->Layers.empty() ? static_cast<int32>(Context.Walkmesh->Layers.size()) - 1 : 0;
	}

	std::optional<FWalkedFloor> LeaderFloor(const FEventContext& Context)
	{
		const FEventActorState* Leader = LeaderOf(Context);
		const int32 Layer = Leader != nullptr ? static_cast<int16>(Leader->Layer) : -1;
		if (Leader == nullptr || !IsWalkingLayer(Context, Layer))
		{
			return std::nullopt;
		}
		const std::size_t Walking = static_cast<std::size_t>(Layer);
		const int32 Triangle = Leader->FloorTriangles[Walking];
		const std::vector<int32> Floors = FloorsOfLayer(*Context.Walkmesh, Walking, *Leader);
		if (Triangle < 0 || static_cast<std::size_t>(Triangle) >= Floors.size())
		{
			return std::nullopt;
		}
		// A triangle closed to the leader that it stands on all the same walks onto the floor beside it.
		if (Floors[static_cast<std::size_t>(Triangle)] < 0)
		{
			for (const int16 Next : Context.Walkmesh->Layers[Walking].Triangles[static_cast<std::size_t>(Triangle)].Neighbours)
			{
				if (Next >= 0 && static_cast<std::size_t>(Next) < Floors.size() && Floors[static_cast<std::size_t>(Next)] >= 0)
				{
					return FWalkedFloor{Layer, Floors[static_cast<std::size_t>(Next)]};
				}
			}
			return std::nullopt;
		}
		return FWalkedFloor{Layer, Floors[static_cast<std::size_t>(Triangle)]};
	}

	std::set<int32> ShutGatesBeside(const FEventContext& Context, const std::set<int32>& Records)
	{
		std::set<int32> Shut;
		const std::optional<FWalkedFloor> Floor = Records.empty() ? std::nullopt : LeaderFloor(Context);
		if (!Floor)
		{
			return Shut;
		}
		const std::size_t Walking = static_cast<std::size_t>(Floor->Layer);
		const std::vector<FWalkmeshTriangle>& Triangles = Context.Walkmesh->Layers[Walking].Triangles;
		const std::vector<int32> Floors = FloorsOfLayer(*Context.Walkmesh, Walking, *LeaderOf(Context));
		for (std::size_t Triangle = 0; Triangle < Triangles.size(); ++Triangle)
		{
			if (Floors[Triangle] != Floor->First)
			{
				continue;
			}
			for (const int16 Next : Triangles[Triangle].Neighbours)
			{
				const int32 Record = Next >= 0 ? Triangles[static_cast<std::size_t>(Next)].Attribute : -1;
				if (Record >= 0 && Records.contains(Record) && !IsOpenTo(*Context.Walkmesh, Walking, Next, *LeaderOf(Context)))
				{
					Shut.insert(Record);
				}
			}
		}
		return Shut;
	}

	std::pair<double, double> GateMiddle(const FEventContext& Context, int32 Record)
	{
		const FEventActorState* Leader = LeaderOf(Context);
		const int32 Layer = Leader != nullptr ? static_cast<int16>(Leader->Layer) : -1;
		double X = 0.0;
		double Z = 0.0;
		int32 Corners = 0;
		if (IsWalkingLayer(Context, Layer))
		{
			const FWalkmeshLayer& Walking = Context.Walkmesh->Layers[static_cast<std::size_t>(Layer)];
			for (const FWalkmeshTriangle& Triangle : Walking.Triangles)
			{
				if (Triangle.Attribute != Record)
				{
					continue;
				}
				for (const uint16 Vertex : Triangle.Vertices)
				{
					X += Walking.Vertices[Vertex][0];
					Z += Walking.Vertices[Vertex][2];
					++Corners;
				}
			}
		}
		return Corners > 0 ? std::pair<double, double>{X / Corners, Z / Corners} : std::pair<double, double>{};
	}

	std::vector<std::pair<FWalkedFloor, FFieldStanding>> WalkedFloors(const FEventContext& Context, int32 MinTriangles)
	{
		std::vector<std::pair<FWalkedFloor, FFieldStanding>> Found;
		const FEventActorState* Leader = LeaderOf(Context);
		for (int32 Layer = 0; Leader != nullptr && IsWalkingLayer(Context, Layer); ++Layer)
		{
			const std::size_t Walking = static_cast<std::size_t>(Layer);
			const FWalkmeshLayer& Triangles = Context.Walkmesh->Layers[Walking];
			const std::vector<int32> Floors = FloorsOfLayer(*Context.Walkmesh, Walking, *Leader);
			std::vector<int32> Counts(Floors.size(), 0);
			for (const int32 Floor : Floors)
			{
				if (Floor >= 0)
				{
					++Counts[static_cast<std::size_t>(Floor)];
				}
			}
			for (std::size_t First = 0; First < Floors.size(); ++First)
			{
				const auto Corners = CornersOf(Triangles, First);
				if (Floors[First] != static_cast<int32>(First) || Counts[First] < MinTriangles || !Corners)
				{
					continue;
				}
				// The middle of its first triangle, which no triangle before it holds.
				const int32 X = ((*Corners)[0][0] + (*Corners)[1][0] + (*Corners)[2][0]) / 3;
				const int32 Z = ((*Corners)[0][2] + (*Corners)[1][2] + (*Corners)[2][2]) / 3;
				Found.push_back({FWalkedFloor{Layer, static_cast<int32>(First)}, FFieldStanding{Layer, X, Z}});
			}
		}
		return Found;
	}

	FWalkedGround::FWalkedGround(const FEventContext& Context, int32 Target, const std::set<int32>* Crossed, const std::optional<FFieldStanding>& From)
	{
		const FEventActorState* Leader = LeaderOf(Context);
		const FFieldStanding Start = From.value_or(LeaderStanding(Context));
		if (Leader == nullptr || !IsWalkingLayer(Context, Start.Layer))
		{
			return;
		}
		const std::size_t WalkedLayer = static_cast<std::size_t>(Start.Layer);
		const FWalkmeshLayer& Layer = Context.Walkmesh->Layers[WalkedLayer];
		// Where the leader stands, its own triangle: the field's, which another holding the same point does not replace.
		const int32 StartTriangle = From ? OpenTriangleAt(*Context.Walkmesh, WalkedLayer, *Leader, Start.X, Start.Z) : Leader->FloorTriangles[WalkedLayer];
		const std::vector<bool> Walked = WalkedFrom(*Context.Walkmesh, WalkedLayer, *Leader, StartTriangle);
		std::vector<int32> FloorTriangles;
		int32 MaxX = std::numeric_limits<int32>::min();
		int32 MaxZ = std::numeric_limits<int32>::min();
		MinX = MinZ = std::numeric_limits<int32>::max();
		for (std::size_t Number = 0; Number < Layer.Triangles.size(); ++Number)
		{
			const auto Corners = CornersOf(Layer, Number);
			if (!Walked[Number] || !Corners)
			{
				continue;
			}
			Floors.push_back(*Corners);
			FloorTriangles.push_back(static_cast<int32>(Number));
			for (const FGteVector& Corner : Floors.back())
			{
				MinX = std::min<int32>(MinX, Corner[0]);
				MaxX = std::max<int32>(MaxX, Corner[0]);
				MinZ = std::min<int32>(MinZ, Corner[2]);
				MaxZ = std::max<int32>(MaxZ, Corner[2]);
			}
		}
		if (Floors.empty())
		{
			return;
		}
		Columns = (MaxX - MinX) / Step + 1;
		Rows = (MaxZ - MinZ) / Step + 1;
		const auto Index = [this](int32 Column, int32 Row) { return static_cast<std::size_t>(Row) * static_cast<std::size_t>(Columns) + static_cast<std::size_t>(Column); };

		// The floor's height at each point, and the triangle it lies on: none where the walk has no floor.
		Heights.assign(static_cast<std::size_t>(Columns) * static_cast<std::size_t>(Rows), NoFloor);
		Steep.assign(Heights.size(), 0);
		std::vector<int32> TriangleOf(Heights.size(), -1);
		for (std::size_t Number = 0; Number < Floors.size(); ++Number)
		{
			const std::array<FGteVector, 3>& Corners = Floors[Number];
			const auto [Left, Right] = std::minmax({Corners[0][0], Corners[1][0], Corners[2][0]});
			const auto [Near, Far] = std::minmax({Corners[0][2], Corners[1][2], Corners[2][2]});
			for (int32 Row = (Near - MinZ + Step - 1) / Step; Row <= (Far - MinZ) / Step; ++Row)
			{
				for (int32 Column = (Left - MinX + Step - 1) / Step; Column <= (Right - MinX) / Step; ++Column)
				{
					const int32 X = MinX + Column * Step;
					const int32 Z = MinZ + Row * Step;
					if (Heights[Index(Column, Row)] == NoFloor && IsInTriangle(Corners, X, Z))
					{
						Heights[Index(Column, Row)] = PlaneHeight(Corners, X, Z);
						TriangleOf[Index(Column, Row)] = FloorTriangles[Number];
						const std::size_t Attribute = Layer.Triangles[static_cast<std::size_t>(FloorTriangles[Number])].Attribute;
						Steep[Index(Column, Row)] = Attribute < Context.Walkmesh->Attributes.size() && (Context.Walkmesh->Attributes[Attribute] & FieldFloorAttribute::Steep) != 0;
					}
				}
			}
		}

		// The points an actor's body or touch takes, but for what holds the walk where it sets off already.
		const int32 FromX = Start.X;
		const int32 FromZ = Start.Z;
		const int32 FromFeet = From ? HeightAt(FromX, FromZ).value_or(0) : Whole(Leader->Position[1]);
		const int32 Actors = Crossed != nullptr ? static_cast<int32>(Context.States.size()) : 0;
		// For each point taken, who took it: the actor's number, doubled, plus one for its touch.
		std::vector<int32> TakenBy(Heights.size(), -1);
		for (int32 Number = 0; Number < Actors; ++Number)
		{
			const FEventActorState& Actor = Context.States[static_cast<std::size_t>(Number)];
			const bool bParty = std::find(Context.PartyActors.begin(), Context.PartyActors.end(), static_cast<uint8>(Number)) != Context.PartyActors.end();
			if (Number == Context.LeaderActor || bParty || (Actor.Flags0 & EventActorFlags0::Frozen) != 0 || (Actor.Flags1 & EventActorFlags1::Removed) != 0)
			{
				continue;
			}
			const bool bStops = !StopsLeaderAt(Actor, *Leader, FromX, FromZ, FromFeet);
			const bool bTouches = Number != Target && Crossed->count(Number) == 0 && (Actor.Flags1 & PlatformBits) == 0 && AnswersFieldTouch(Context, Number)
				&& !TouchesLeaderAt(Actor, *Leader, FromX, FromZ, FromFeet);
			// As far from its place as its body and its touch can reach, whichever way its box is turned.
			const int32 Reach = static_cast<int32>(std::ceil(std::hypot(Actor.HalfWidth, Actor.HalfDepth))) + Actor.Radius + Leader->Radius + BoxTouchMargin
				+ std::abs(Actor.InteractX) + std::abs(Actor.InteractZ);
			const int32 CentreX = Whole(Actor.Position[0]);
			const int32 CentreZ = Whole(Actor.Position[2]);
			for (int32 Row = std::max((CentreZ - Reach - MinZ) / Step, 0); Row <= std::min((CentreZ + Reach - MinZ) / Step + 1, Rows - 1); ++Row)
			{
				for (int32 Column = std::max((CentreX - Reach - MinX) / Step, 0); Column <= std::min((CentreX + Reach - MinX) / Step + 1, Columns - 1); ++Column)
				{
					int32& Height = Heights[Index(Column, Row)];
					if (Height == NoFloor)
					{
						continue;
					}
					const int32 X = MinX + Column * Step;
					const int32 Z = MinZ + Row * Step;
					const bool bBody = bStops && StopsLeaderAt(Actor, *Leader, X, Z, Height);
					if (bBody || (bTouches && TouchesLeaderAt(Actor, *Leader, X, Z, Height)))
					{
						Height = NoFloor;
						TakenBy[Index(Column, Row)] = Number * 2 + (bBody ? 0 : 1);
					}
				}
			}
		}

		// From the point nearest the start that has floor, through each point's eight neighbours.
		// The nearest first, each point keeping the one it was come to from: the walk's own way to it.
		Reached.assign(Heights.size(), 0);
		Before.assign(Heights.size(), -1);
		const int32 FromColumn = (FromX - MinX + Step / 2) / Step;
		const int32 FromRow = (FromZ - MinZ + Step / 2) / Step;
		for (int32 Around = 0; Around <= 2 && Order.empty(); ++Around)
		{
			for (int32 Row = FromRow - Around; Row <= FromRow + Around && Order.empty(); ++Row)
			{
				for (int32 Column = FromColumn - Around; Column <= FromColumn + Around && Order.empty(); ++Column)
				{
					if (Column >= 0 && Column < Columns && Row >= 0 && Row < Rows && Heights[Index(Column, Row)] != NoFloor)
					{
						const int32 Point = static_cast<int32>(Index(Column, Row));
						Reached[static_cast<std::size_t>(Point)] = 1;
						Before[static_cast<std::size_t>(Point)] = Point;
						Order.push_back(Point);
					}
				}
			}
		}
		for (std::size_t Next = 0; Next < Order.size(); ++Next)
		{
			const int32 At = Order[Next];
			const int32 AtColumn = At % Columns;
			const int32 AtRow = At / Columns;
			// The four beside it before the four across its corners: a way's length is then nearly the walk's.
			for (const auto& [Across, Along] : {std::pair<int32, int32>{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}})
			{
				const int32 Column = AtColumn + Across;
				const int32 Row = AtRow + Along;
				if (Column < 0 || Column >= Columns || Row < 0 || Row >= Rows)
				{
					continue;
				}
				const std::size_t Point = Index(Column, Row);
				if (Heights[Point] != NoFloor && Reached[Point] == 0)
				{
					Reached[Point] = 1;
					Before[Point] = At;
					Order.push_back(static_cast<int32>(Point));
				}
			}
		}

		// Who stands between the ground the walk came to and floor it did not: whoever took points beside both.
		std::vector<std::pair<int32, uint8>> Sides;
		constexpr uint8 BesideReached = 1;
		constexpr uint8 BesideUnreached = 2;
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Column = 0; Column < Columns; ++Column)
			{
				const int32 Taker = TakenBy[Index(Column, Row)];
				if (Taker < 0)
				{
					continue;
				}
				uint8 Beside = 0;
				for (int32 Along = std::max(Row - 1, 0); Along <= std::min(Row + 1, Rows - 1); ++Along)
				{
					for (int32 Across = std::max(Column - 1, 0); Across <= std::min(Column + 1, Columns - 1); ++Across)
					{
						const std::size_t Point = Index(Across, Along);
						Beside |= Reached[Point] != 0 ? BesideReached : Heights[Point] != NoFloor ? BesideUnreached : uint8{0};
					}
				}
				auto Side = std::find_if(Sides.begin(), Sides.end(), [Taker](const std::pair<int32, uint8>& Each) { return Each.first == Taker; });
				if (Side == Sides.end())
				{
					Side = Sides.insert(Sides.end(), {Taker, uint8{0}});
				}
				Side->second |= Beside;
			}
		}
		for (const auto& [Taker, Beside] : Sides)
		{
			if (Beside == (BesideReached | BesideUnreached))
			{
				Stopped.push_back({Taker / 2, Taker % 2 != 0});
			}
		}
		std::sort(Stopped.begin(), Stopped.end());

		// The triangles that hand the walker up, each by the middle of the points the walk reaches on it.
		std::vector<std::array<int64, 4>> Sums;
		for (std::size_t Point = 0; Point < Reached.size(); ++Point)
		{
			const int32 Triangle = TriangleOf[Point];
			if (Reached[Point] == 0 || Triangle < 0)
			{
				continue;
			}
			const std::size_t Attribute = Layer.Triangles[static_cast<std::size_t>(Triangle)].Attribute;
			if (Attribute >= Context.Walkmesh->Attributes.size() || (Context.Walkmesh->Attributes[Attribute] & HandsUp) == 0)
			{
				continue;
			}
			auto Sum = std::find_if(Sums.begin(), Sums.end(), [&](const std::array<int64, 4>& Each) { return Each[0] == Triangle; });
			if (Sum == Sums.end())
			{
				Sum = Sums.insert(Sums.end(), {Triangle, 0, 0, 0});
			}
			(*Sum)[1] += MinX + static_cast<int32>(Point % static_cast<std::size_t>(Columns)) * Step;
			(*Sum)[2] += MinZ + static_cast<int32>(Point / static_cast<std::size_t>(Columns)) * Step;
			++(*Sum)[3];
		}
		for (const std::array<int64, 4>& Sum : Sums)
		{
			Upward.push_back({static_cast<int32>(Sum[1] / Sum[3]), static_cast<int32>(Sum[2] / Sum[3]), static_cast<int32>(Sum[0])});
		}
	}

	int32 FWalkedGround::PointAt(int32 X, int32 Z) const
	{
		if (X < MinX || Z < MinZ)
		{
			return -1;
		}
		const int32 Column = (X - MinX + Step / 2) / Step;
		const int32 Row = (Z - MinZ + Step / 2) / Step;
		return Column < Columns && Row < Rows ? Row * Columns + Column : -1;
	}

	bool FWalkedGround::Reaches(int32 X, int32 Z) const
	{
		// On the floor itself, and at or beside a point the walk came to.
		return std::any_of(Floors.begin(), Floors.end(), [&](const std::array<FGteVector, 3>& Corners) { return IsInTriangle(Corners, X, Z); }) && Covers(X, Z);
	}

	bool FWalkedGround::Covers(int32 X, int32 Z) const
	{
		for (const int32 Along : {0, -Step, Step})
		{
			for (const int32 Across : {0, -Step, Step})
			{
				const int32 Point = PointAt(X + Across, Z + Along);
				if (Point >= 0 && Reached[static_cast<std::size_t>(Point)] != 0)
				{
					return true;
				}
			}
		}
		return false;
	}

	std::vector<FGroundPoint> FWalkedGround::Points(int32 Every) const
	{
		std::vector<FGroundPoint> Points;
		const auto IsReached = [this](int32 Column, int32 Row)
		{
			return Column >= 0 && Column < Columns && Row >= 0 && Row < Rows && Reached[static_cast<std::size_t>(Row * Columns + Column)] != 0;
		};
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Column = 0; Column < Columns; ++Column)
			{
				bool bRim = false;
				for (int32 Along = -1; Along <= 1 && !bRim; ++Along)
				{
					for (int32 Across = -1; Across <= 1 && !bRim; ++Across)
					{
						bRim = !IsReached(Column + Across, Row + Along);
					}
				}
				const bool bPicked = bRim || (Every > 0 && Column % Every == 0 && Row % Every == 0);
				if (bPicked && IsReached(Column, Row))
				{
					const std::size_t Point = static_cast<std::size_t>(Row * Columns + Column);
					Points.push_back({MinX + Column * Step, MinZ + Row * Step, Heights[Point], bRim, Steep[Point] != 0});
				}
			}
		}
		return Points;
	}

	std::optional<FGroundWay> FWalkedGround::WayTo(const std::function<bool(int32 X, int32 Z)>& Wanted, int32 Back) const
	{
		const auto PlaceOf = [this](int32 Point) { return std::pair<int32, int32>{MinX + Point % Columns * Step, MinZ + Point / Columns * Step}; };
		for (const int32 Point : Order)
		{
			const auto [X, Z] = PlaceOf(Point);
			if (!Wanted(X, Z))
			{
				continue;
			}
			int32 From = Point;
			for (int32 Walked = 0; Walked < Back && Before[static_cast<std::size_t>(From)] != From; Walked += Step)
			{
				From = Before[static_cast<std::size_t>(From)];
			}
			const auto [FromX, FromZ] = PlaceOf(From);
			return FGroundWay{X, Z, FromX, FromZ};
		}
		return std::nullopt;
	}

	std::optional<int32> FWalkedGround::HeightAt(int32 X, int32 Z) const
	{
		const int32 Point = PointAt(X, Z);
		if (Point < 0 || Heights[static_cast<std::size_t>(Point)] == NoFloor)
		{
			return std::nullopt;
		}
		return Heights[static_cast<std::size_t>(Point)];
	}
}
