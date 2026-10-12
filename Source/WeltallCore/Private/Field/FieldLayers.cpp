#include "Field/FieldLayers.h"

#include "Field/FieldWalkmesh.h"
#include "MovementRules.h"
#include "WalkLayerFloors.h"

#include "Field/EventContext.h"
#include "Field/EventPlacement.h"
#include "Field/FieldActorMotion.h"
#include "Field/FieldFollowers.h"
#include "Graphics/GteVectors.h"
#include "Graphics/Sprite.h"

namespace Weltall
{
	namespace
	{
		using namespace WalkLayerFloors;
		using MovementRules::Add;
		using MovementRules::WholePart;

		/** A floor that hands the actor on to the layer above. */
		constexpr uint32 PassesUp = 0x4;
		constexpr uint32 ClassShift = 5;
		constexpr uint16 AllButtons = 0xFFFF;
		constexpr uint16 DpadShutOut = 0xFFF;
		/** A platform more than this far below the actor's last floor lets go of what the actor stood on. */
		constexpr int32 PlatformDrop = 10;

		/** What the step changes, to undo it. */
		struct FBefore
		{
			std::array<int32, 3> Position{};
			uint16 Layer = 0;
			std::array<int16, 4> Triangles{};
		};

		void UndoStep(FEventActorState& State, const FBefore& Before)
		{
			State.Position[0] = Before.Position[0];
			State.Layer = Before.Layer;
			State.WordF0 = 0;
			State.Position[2] = Before.Position[2];
			State.FloorTriangles = Before.Triangles;
		}

		void Land(FEventActorState& State, FSprite& Sprite)
		{
			if (Sprite.Velocity[1] > 0)
			{
				Sprite.Velocity[1] = 0;
			}
			State.Flags0 &= ~(EventActorFlags0::FollowsFloor | EventActorFlags0::Falling);
			State.Position[1] = static_cast<int32>(static_cast<uint32>(Sprite.FloorY) << 16);
		}

		/** The step onto a floor closed to the actor is undone; it only falls, or lands. */
		void RefuseFloor(FEventContext& Context, int32 Actor, FEventActorState& State, FSprite& Sprite, const FBefore& Before)
		{
			if (Actor == Context.LeaderActor)
			{
				Context.Controls.WalkMask = DpadShutOut;
			}
			State.Position[1] = Add(State.Position[1], Sprite.Velocity[1]);
			UndoStep(State, Before);
			if (WholePart(State.Position[1]) < Sprite.FloorY)
			{
				Sprite.Velocity[1] = Add(Sprite.Velocity[1], Sprite.Gravity);
			}
			else
			{
				Land(State, Sprite);
			}
			Sprite.Position = State.Position;
			Context.Actors[static_cast<std::size_t>(Actor)].Translation[1] = WholePart(State.Position[1]);
		}

		/** Whether the step onto the new floor leaves the actor's body clear of the other layers' floors and its head below the ceiling. */
		std::optional<bool> HasRoom(const FEventContext& Context, const FEventActorState& State, const FSprite& Sprite, const FLayerFloors& Floors, int32 WalkingLayers)
		{
			const int32 Feet = WholePart(State.Position[1]);
			const int32 Head = Feet - State.Height;
			for (std::size_t Rank = 0; Rank < static_cast<std::size_t>(WalkingLayers); ++Rank)
			{
				if (Floors.Floor[Rank] < Feet && Head < Floors.Underside[Rank] && Floors.Floor[Rank] != Floors.Underside[Rank])
				{
					return false;
				}
			}
			const FWalkmeshLayer& Mesh = Context.Walkmesh->Layers[State.Layer];
			const int32 Triangle = State.FloorTriangles[State.Layer];
			if (Triangle < 0 || static_cast<std::size_t>(Triangle) >= Mesh.Triangles.size())
			{
				return std::nullopt;
			}
			const int32 Ceiling = Mesh.Triangles[static_cast<std::size_t>(Triangle)].Ceiling * 4;
			return Ceiling >= 0 || Head >= Ceiling + Sprite.FloorY;
		}
	}

	bool StepWalkLayer(FEventContext& Context, int32 Actor, std::optional<int32> PlatformHeight)
	{
		FEventActorState* Found = Context.StateOf(Actor);
		FSprite* Sprite = Context.Models != nullptr ? Context.Models->ActorSprite(Actor) : nullptr;
		if (Found == nullptr || Sprite == nullptr || Context.Walkmesh == nullptr || static_cast<std::size_t>(Actor) >= Context.Actors.size())
		{
			return false;
		}
		FEventActorState& State = *Found;
		FFieldControls& Controls = Context.Controls;
		const bool bLeader = Actor == Context.LeaderActor;
		if (bLeader)
		{
			Controls.WalkMask = AllButtons;
		}
		if ((State.Flags0 & (EventActorFlags0::PartyFollower | EventActorFlags0::MovedByScript)) != 0 || (State.Flags1 & EventActorFlags1::ExplicitPosition) != 0)
		{
			return true;
		}
		if (!(bLeader && Controls.bFollowClose) && Sprite->Velocity[1] == 0 && IsActorSettled(State, Context.Movement) && Sprite->FloorY == WholePart(State.Position[1]))
		{
			return true;
		}
		// An actor on the camera's layer (a placement put it there) takes the step as on a walking layer
		// with no floor of its own: it moves to the first walking layer whose floor lies at or below its
		// feet, if any; while it stays, the step reads no floor normal there (the original's is stack it
		// never wrote).
		const int32 WalkingLayers = static_cast<int32>(Context.Walkmesh->Layers.size()) - 1;
		if (WalkingLayers > SortedLayers || static_cast<int16>(State.Layer) < 0 || static_cast<int16>(State.Layer) > WalkingLayers)
		{
			return false;
		}
		const FBefore Before{State.Position, State.Layer, State.FloorTriangles};
		FLayerFloors Floors;
		int32 Searched = 0;
		for (; Searched < WalkingLayers; ++Searched)
		{
			const ESearch Search = FloorOnLayer(Context, State, Searched, Floors);
			if (Search == ESearch::OutsideWalkmesh)
			{
				return false;
			}
			if (Search == ESearch::None)
			{
				break;
			}
		}
		for (std::size_t Layer = 0; Layer < SortedLayers; ++Layer)
		{
			if ((State.Flags1 >> Layer & 1) != 0)
			{
				Floors.Floor[Layer] = NoFloor;
				Floors.Underside[Layer] = NoFloor;
			}
		}
		const int32 OwnFloor = Floors.Floor[State.Layer];
		SortFloors(Floors);

		if (Searched == WalkingLayers)
		{
			for (std::size_t Layer = 0; Layer < static_cast<std::size_t>(WalkingLayers); ++Layer)
			{
				State.FloorTriangles[Layer] = Floors.Triangles[Layer];
			}
			const int32 Feet = WholePart(State.Position[1]);
			int32 Rank = 0;
			if (Feet < OwnFloor || (State.Flags0 & (EventActorFlags0::Airborne | EventActorFlags0::Falling)) != 0)
			{
				while (Rank < WalkingLayers && Floors.Floor[static_cast<std::size_t>(Rank)] < Feet)
				{
					++Rank;
				}
				if (Rank < WalkingLayers)
				{
					State.Layer = static_cast<uint16>(Floors.Layer[static_cast<std::size_t>(Rank)]);
				}
			}
			else
			{
				while (Rank < WalkingLayers && static_cast<int16>(State.Layer) != Floors.Layer[static_cast<std::size_t>(Rank)])
				{
					++Rank;
				}
			}
			std::optional<uint32> Attributes = ActorFloorAttributes(State, *Context.Walkmesh);
			if (!Attributes)
			{
				return false;
			}
			if ((*Attributes & PassesUp) != 0 && Rank != 0 && static_cast<int16>(State.Layer) <= WalkingLayers)
			{
				State.Layer = static_cast<uint16>(Floors.Layer[static_cast<std::size_t>(Rank - 1)]);
				Attributes = ActorFloorAttributes(State, *Context.Walkmesh);
				if (!Attributes)
				{
					return false;
				}
			}
			const bool bClassClosed = ((State.Flags0 >> 8) & 7 & (*Attributes >> ClassShift)) != 0;
			if (bClassClosed || (*Attributes & FieldFloorAttribute::Closed) != 0)
			{
				RefuseFloor(Context, Actor, State, *Sprite, Before);
				return RecordLeaderStep(Context, Actor);
			}
			State.Position[0] = Add(State.Position[0], State.Step[0]);
			State.Position[2] = Add(State.Position[2], State.Step[2]);
			for (std::size_t Sorted = 0; Sorted < static_cast<std::size_t>(WalkingLayers); ++Sorted)
			{
				if (static_cast<int16>(State.Layer) == Floors.Layer[Sorted])
				{
					Sprite->FloorY = static_cast<int16>(Floors.Floor[Sorted]);
					break;
				}
			}
			State.FloorNormal = VectorNormal(Floors.Normals[State.Layer]);
		}
		else
		{
			State.WordF0 = 0;
		}

		if (PlatformHeight)
		{
			if (Sprite->FloorY < *PlatformHeight + PlatformDrop)
			{
				State.LinkedActor = FEventContext::NoActor;
			}
			Sprite->FloorY = static_cast<int16>(*PlatformHeight);
			State.Position[1] = static_cast<int32>(static_cast<uint32>(*PlatformHeight) << 16);
		}
		if ((State.Flags0 & EventActorFlags0::FixedHeight) != 0)
		{
			State.Position[1] = static_cast<int32>(static_cast<uint32>(State.TargetHeight) << 16);
			Sprite->Velocity[1] = 0;
		}
		State.Position[1] = Add(State.Position[1], Sprite->Velocity[1]);
		const std::optional<uint32> Attributes = ActorFloorAttributes(State, *Context.Walkmesh);
		if (!Attributes)
		{
			return false;
		}
		if (static_cast<int16>(State.Layer) != static_cast<int16>(Before.Layer))
		{
			State.Flags0 &= ~EventActorFlags0::MovedAlongSlope;
		}
		if ((State.Flags0 & EventActorFlags0::MovedAlongSlope) == 0 && WholePart(State.Position[1]) < Sprite->FloorY)
		{
			Sprite->Velocity[1] = Add(Sprite->Velocity[1], Sprite->Gravity);
			State.Flags0 |= EventActorFlags0::Falling;
			State.WordF0 = Sprite->Velocity[1];
		}
		else
		{
			if ((*Attributes & FieldFloorAttribute::Slopes) == 0)
			{
				State.WordF0 = 0;
			}
			Land(State, *Sprite);
		}
		State.Flags0 &= ~EventActorFlags0::MovedAlongSlope;

		const std::optional<bool> bRoom = HasRoom(Context, State, *Sprite, Floors, WalkingLayers);
		if (!bRoom)
		{
			return false;
		}
		FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Actor)];
		if (*bRoom)
		{
			Sprite->Position = State.Position;
			Record.Translation = {WholePart(State.Position[0]), WholePart(State.Position[1]), WholePart(State.Position[2])};
			State.FloorAttributes = *Attributes;
		}
		else
		{
			UndoStep(State, Before);
			if (Sprite->FloorY != WholePart(State.Position[1]))
			{
				Sprite->Velocity[1] = Add(Sprite->Velocity[1], Sprite->Gravity);
			}
			if (Sprite->Velocity[1] < 0)
			{
				Sprite->Velocity[1] = 0;
				State.Position[1] = Before.Position[1];
			}
			Sprite->Position = State.Position;
			Record.Translation[1] = WholePart(State.Position[1]);
		}
		return RecordLeaderStep(Context, Actor);
	}
}
