#include "Battle/BattleAction.h"
#include "Battle/BattleRules.h"
#include "Battle/BattleSteps.h"

namespace Weltall
{
	using namespace BattleRules;

	namespace
	{
		/** The place entries of a group: party on foot from 0, enemies on foot from 8, party in gears from 16, enemies in gears from 24. */
		constexpr uint32 EnemySide = 8;
		constexpr uint32 GearSide = 0x10;
		constexpr uint8 PlacesPerGroup = 4;
		constexpr uint8 Billy = 4;

		FBattleOccupancy* Entry(FBattleContext& Context, uint32 Group, uint32 Side)
		{
			const std::size_t Index = Group + Side;
			if (Index >= Context.Occupancy.size())
			{
				Context.Fault = EBattleFault::GroupOutsideStage;
				return nullptr;
			}
			return &Context.Occupancy[Index];
		}
	}

	namespace BattleSteps
	{
		void LeaveGroup(FBattleContext& Context, uint8 Slot)
		{
			uint32 Side = Slot >= FBattleContext::PartySlotCount ? EnemySide : 0;
			if (Context.ApRecords[Slot].InGear != 0)
			{
				Side |= GearSide;
			}
			const FBattleSlot& Place = Context.Slots[Slot];
			if (FBattleOccupancy* Occupancy = Entry(Context, Place.Group, Side))
			{
				Occupancy->Count = static_cast<uint8>(Occupancy->Count - 1);
				Occupancy->Places = static_cast<uint8>(Occupancy->Places & ~SlotBit(Place.Place));
			}
		}

		void MoveToTargetGroup(FBattleContext& Context, uint8 Mover, uint8 Target)
		{
			const uint8 Group = Context.Slots[Target].Group;
			if (Context.Slots[Mover].Group == Group)
			{
				return;
			}
			// The side is the target's: an enemy walking up to another enemy takes a place in the party's entry.
			const uint32 Side = Target < FBattleContext::PartySlotCount && Mover >= FBattleContext::PartySlotCount ? EnemySide : 0;
			FBattleOccupancy* To = Entry(Context, Group, Side);
			if (To == nullptr || To->Count >= PlacesPerGroup)
			{
				return;
			}
			LeaveGroup(Context, Mover);
			To->Count = static_cast<uint8>(To->Count + 1);
			uint8 Place = 0;
			while (Place < PlacesPerGroup && (To->Places & SlotBit(Place)) != 0)
			{
				++Place;
			}
			FBattleSlot& Moved = Context.Slots[Mover];
			Moved.Place = Place;
			Moved.Group = Group;
			To->Places = static_cast<uint8>(To->Places | SlotBit(Place));
			Moved.Position = Mover < FBattleContext::PartySlotCount ? Context.Stage.PartyPlace(Group, Place) : Context.Stage.EnemyPlace(Group, Place);
		}

		void MoveGearToTargetGroup(FBattleContext& Context, uint8 Mover, uint8 Target)
		{
			const uint8 Group = Context.Slots[Target].Group;
			if (Context.Slots[Mover].Group == Group)
			{
				return;
			}
			const uint32 Side = Target < FBattleContext::PartySlotCount && Mover >= FBattleContext::PartySlotCount ? GearSide | EnemySide : GearSide;
			FBattleOccupancy* To = Entry(Context, Group, Side);
			if (To == nullptr || To->Count != 0)
			{
				return;
			}
			LeaveGroup(Context, Mover);
			FBattleSlot& Moved = Context.Slots[Mover];
			Moved.Place = 0;
			Moved.Group = Group;
			To->Count = 1;
			To->Places = 1;
			Moved.Position = Mover < FBattleContext::PartySlotCount ? Context.Stage.PartyGearPlace(Group) : Context.Stage.EnemyGearPlace(Group);
		}

		bool PlanApproachPath(FBattleContext& Context, uint8 Actor, uint8 Target)
		{
			for (FBattleWaypoint& Waypoint : Context.Route)
			{
				Waypoint.X = FBattleWaypoint::None;
				Waypoint.Z = FBattleWaypoint::None;
			}
			const FBattleSlot& From = Context.Slots[Actor];
			Context.Route[0] = {From.Position.X, From.Position.Z, 0};
			const uint8 ToGroup = Context.Slots[Target].Group;
			for (int32 Step = 1; Step < 8; ++Step)
			{
				const uint8 Waypoint = Context.Stage.Route(From.Group, ToGroup, Step);
				if (Waypoint == 0xFF)
				{
					break;
				}
				const FBattleStagePoint Point = Context.Stage.GroupPoint(Waypoint & 7);
				Context.Route[static_cast<std::size_t>(Step)] = {Point.X, Point.Z, static_cast<uint8>(Waypoint & 0x80)};
			}
			return Actor < FBattleContext::PartySlotCount && Context.PartyCharacters[Actor] == Billy;
		}
	}

	bool PlanBattleApproachPath(FBattleContext& Context, uint8 Actor, uint8 Target)
	{
		return BattleSteps::PlanApproachPath(Context, Actor, Target);
	}
}
