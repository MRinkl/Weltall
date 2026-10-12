#include "EventOps.h"
#include "EventSprites.h"

#include "Field/FieldGearBoarding.h"
#include "Field/FieldPartySprites.h"
#include "Field/FieldWalkmesh.h"
#include "Game/GameState.h"
#include "Graphics/Sprite.h"

#include <algorithm>

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint32 EntranceVariable = 0x002;
		constexpr uint32 PreviousFacingVariable = 0x006;
		constexpr uint32 PreviousCameraVariable = 0x008;
		/** Where each party slot's gear was parked: the map (with its walkmesh layer in bits 14-15), x and z, six bytes a slot. */
		constexpr uint32 ParkedGearVariables = 0x02A;
		constexpr uint32 ParkedGearStride = 6;
		constexpr int32 LastPartySlot = 2;
		constexpr int32 MapNumberMask = 0xFFF;
		constexpr int32 ParkedMapMask = 0x3FFF;
		constexpr int32 ParkedLayerShift = 14;
		/** Chu-Chu's gear is never shown parked. */
		constexpr uint8 ChuChu = 7;
		constexpr uint8 NoGear = 0xFF;

		FFieldActor& Record(FEventContext& Context)
		{
			return Context.Actors[static_cast<std::size_t>(Context.Run.Actor)];
		}

		/**
		 * An actor for a character who is not in the party: it gets slot 0's sprite, is frozen and taken
		 * out of the map, and its turn ends.
		 */
		void DropAbsentMember(FEventContext& Context)
		{
			if (!LoadActorSprite(Context, Context.Run.Actor, EFieldSpriteKind::Party, 0, 0, 0, true))
			{
				return;
			}
			FEventActorState& State = Self(Context);
			State.Flags0 |= EventActorFlags0::Frozen;
			State.Flags1 |= EventActorFlags1::Removed;
			Context.Run.bUnlimited = false;
			Context.Run.bYield = true;
		}

		/**
		 * Puts the running actor at the entrance the party arrived by, facing and turning the camera as
		 * the record says: a record's 0xFF keeps the direction the previous map left.
		 */
		bool PlaceAtEntrance(FEventContext& Context, int32 Index)
		{
			if (Context.Scripts->CodeSize() == 0 || Context.Scripts->Code()[0] != 0xFF)
			{
				return true;
			}
			const std::optional<FEventEntrance> Entrance = Context.Scripts->Entrance(Index);
			if (!Entrance)
			{
				Fail(Context, EEventFault::ReadOutsideCode, static_cast<uint32>(Index));
				return false;
			}
			FEventActorState& State = Self(Context);
			State.Layer = Entrance->Layer;
			if (!PlaceOnFloor(Context, Entrance->X, Entrance->Z))
			{
				return false;
			}
			int32 Direction = Entrance->CameraDirection;
			if (Direction == FEventEntrance::KeepDirection)
			{
				Direction = GetVar(Context, PreviousCameraVariable);
			}
			const uint32 Step = static_cast<uint32>(Direction + 4) & 7;
			Context.Camera.TurningYaw = static_cast<int32>(Step << 25);
			Context.Camera.YawGoal = static_cast<int32>(Step << 9);
			Context.Camera.Rotation[1] = static_cast<int16>(Step << 9);
			int32 Facing = Entrance->Facing;
			if (Facing == FEventEntrance::KeepDirection)
			{
				Facing = GetVar(Context, PreviousFacingVariable);
			}
			const uint16 Angle = FEventEntrance::FacingAngle(Facing);
			State.MoveDirection = Angle;
			State.Facing = Angle;
			State.Yaw = Angle;
			return true;
		}

		/**
		 * A party member's sprite while the party is in gears: the map's sprite of the character's
		 * pilot; when the member rides its gear, the pilot and the slot's gear hand each other their
		 * sprites.
		 */
		bool GivePilotSprite(FEventContext& Context, int32 Character, int32 Slot)
		{
			const std::optional<int32> File = PilotSpriteFile(Character, Context.PilotSpriteBase);
			if (!File)
			{
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Character));
				return false;
			}
			if (!LoadActorSprite(Context, Context.Run.Actor, EFieldSpriteKind::Map, *File, 0, static_cast<uint8>(*File | 0x80), true))
			{
				return false;
			}
			FEventActorState& State = Self(Context);
			State.Flags0 = (State.Flags0 & ~(EventActorFlags0::MapSprite | EventActorFlags0::GearPilot)) | EventActorFlags0::PartyMember;
			FGameState* Game = GameStateOf(Context);
			if (Game == nullptr)
			{
				return false;
			}
			if (Game->PartySlotInGear(Slot) != 0)
			{
				const uint8 Gear = Context.GearActors[static_cast<std::size_t>(Slot)];
				if (StateOf(Context, Gear) == nullptr)
				{
					return false;
				}
				Context.Models->SwapSprites(Context.Run.Actor, Gear);
				State.Flags0 = (State.Flags0 & ~(EventActorFlags0::MapSprite | EventActorFlags0::PartyMember)) | EventActorFlags0::GearPilot;
			}
			return true;
		}

		/**
		 * 16: the actor stands for a character of the party: its slot's actor, the leader for slot 0,
		 * with the party's sprite (the pilot's while the party is in gears), placed at the entrance the
		 * party arrived by. A character not in the party takes the actor out of the map.
		 */
		void SetPartyCharacter(FEventContext& Context)
		{
			const int32 Character = ResolveCharacter(Context, ReadValue(Context, 1));
			const int32 Slot = PartySlotOf(Context, Character);
			FEventActorState& State = Self(Context);
			State.Character = static_cast<uint16>(Character);
			MarkSpriteActor(Context);
			if (Slot < 0)
			{
				DropAbsentMember(Context);
			}
			else
			{
				const uint8 Actor = static_cast<uint8>(Context.Run.Actor);
				if (Slot == 0)
				{
					Context.LeaderActor = Actor;
					Context.Camera.FollowedActor = Actor;
					State.Flags0 = (State.Flags0 & ~EventActorFlags0::PassesThroughActors) | EventActorFlags0::PadControlled | EventActorFlags0::PartyMember;
				}
				Context.PartyActors[static_cast<std::size_t>(Slot)] = Actor;
				if (Context.PilotSpriteBase == 0)
				{
					if (!LoadActorSprite(Context, Context.Run.Actor, EFieldSpriteKind::Party, Slot, 0, static_cast<uint8>(Slot), true))
					{
						return;
					}
					State.Flags0 = (State.Flags0 & ~(EventActorFlags0::MapSprite | EventActorFlags0::GearPilot)) | EventActorFlags0::PartyMember;
				}
				else if (!GivePilotSprite(Context, Character, Slot))
				{
					return;
				}
				Record(Context).Flags &= static_cast<uint16>(~FieldActorFlag::Hidden);
				if (!PlaceAtEntrance(Context, GetVar(Context, EntranceVariable)))
				{
					return;
				}
				SyncRecordPosition(Context);
				State.Flags1 &= ~EventActorFlags1::NotFloorAligned;
			}
			State.Flags0 |= EventActorFlags0::NoTalk;
			State.Flags1 |= EventActorFlags1::KeepsRunningOffScreen;
			Advance(Context, 3);
		}

		/**
		 * FE 21: the actor looks like a party member (a second copy of the member's sprite, the gear's
		 * while the party's files are its gears') without standing for the slot. A character not in the
		 * party takes the actor out of the map.
		 */
		void BindPartySprite(FEventContext& Context)
		{
			const int32 Slot = PartySlotOf(Context, ResolveCharacter(Context, ReadValue(Context, 1)));
			MarkSpriteActor(Context);
			if (Slot < 0)
			{
				DropAbsentMember(Context);
			}
			else
			{
				if (!LoadActorSprite(Context, Context.Run.Actor, EFieldSpriteKind::PartyCopy, Slot, 0, static_cast<uint8>(Slot), true))
				{
					return;
				}
				Record(Context).Flags &= static_cast<uint16>(~FieldActorFlag::Hidden);
				SyncRecordPosition(Context);
				FEventActorState& State = Self(Context);
				State.Flags0 = (State.Flags0 & ~EventActorFlags0::PassesThroughActors) | EventActorFlags0::MapSprite;
			}
			Advance(Context, 3);
		}

		/**
		 * 5C: the actor is a party slot's parked gear, with the gear's sprite from the slot's file: on
		 * the map where the gear was left, where it was left; elsewhere, hidden at the map's origin. A
		 * gear ridden by its member, and Chu-Chu's, is hidden too. Without a member or a gear the actor
		 * gets the map's first sprite, hidden.
		 */
		void ParkedGear(FEventContext& Context)
		{
			int32 Slot = ReadValue(Context, 1);
			if (Slot > LastPartySlot)
			{
				Slot = LastPartySlot;
			}
			if (Slot < 0)
			{
				// The original writes before the table of the slots' gears.
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Slot));
				return;
			}
			FGameState* Game = GameStateOf(Context);
			if (Game == nullptr)
			{
				return;
			}
			const std::size_t Index = static_cast<std::size_t>(Slot);
			Context.GearActors[Index] = static_cast<uint8>(Context.Run.Actor);
			const uint8 Character = Context.PartyCharacters[Index];
			FEventActorState& State = Self(Context);
			if (Character != FEventContext::NoActor)
			{
				const std::optional<uint8> Gear = Game->CharacterGear(Character);
				if (!Gear)
				{
					Fail(Context, EEventFault::TableIndexOutOfRange, Character);
					return;
				}
				if (*Gear != NoGear)
				{
					const uint32 Parked = ParkedGearVariables + ParkedGearStride * static_cast<uint32>(Slot);
					const int32 Map = GetVar(Context, Parked);
					int32 X = GetVar(Context, Parked + 2);
					int32 Z = GetVar(Context, Parked + 4);
					const bool bHere = (Context.MapNumber & MapNumberMask) == (Map & ParkedMapMask);
					State.Layer = static_cast<uint16>(Map >> ParkedLayerShift & 3);
					if (!bHere)
					{
						X = 0;
						Z = 0;
						State.Layer = 0;
					}
					const bool bShown = bHere && Game->PartySlotInGear(Slot) == 0 && Character != ChuChu;
					MarkSpriteActor(Context);
					if (!LoadActorSprite(Context, Context.Run.Actor, EFieldSpriteKind::Party, Slot, 0, static_cast<uint8>(Slot), true))
					{
						return;
					}
					Record(Context).Flags &= static_cast<uint16>(~FieldActorFlag::Hidden);
					// A gear ridden onto the map is recorded with the layer its rider walked on the map before: the
					// frames of the map change write this map's number beside that layer and place. Where the map
					// has no such layer the gear stands without a floor until the check below moves it to the first.
					if (!PlaceOnFloor(Context, X, Z, true))
					{
						return;
					}
					SyncRecordPosition(Context);
					State.Flags0 = (State.Flags0 & ~(EventActorFlags0::MapSprite | EventActorFlags0::GearPilot)) | EventActorFlags0::PartyMember;
					if (!bShown)
					{
						Record(Context).Flags |= FieldActorFlag::Hidden;
					}
					if (static_cast<int16>(State.Layer) > static_cast<int32>(Context.Walkmesh->Layers.size()) - 1)
					{
						State.Layer = 0;
					}
					State.Flags0 |= EventActorFlags0::NoTalk;
					State.Flags1 |= EventActorFlags1::KeepsRunningOffScreen | EventActorFlags1::NotFloorAligned;
					Advance(Context, 3);
					return;
				}
			}
			if (!GiveHiddenMapSprite(Context))
			{
				return;
			}
			Advance(Context, 3);
			State.Flags1 |= EventActorFlags1::NotFloorAligned;
		}

		/**
		 * FE C1: the signal the sprite of a party slot's actor holds (its animation scripts set it) and
		 * the actor; a signal other than 1 is taken, cleared. An empty slot gives 0 and 0. The
		 * instruction does not count against the actor's turn.
		 */
		void GetPartySpriteSignal(FEventContext& Context)
		{
			const int32 Slot = ReadValue(Context, 5);
			if (Slot < 0 || Slot > LastPartySlot)
			{
				// The original reads past the table of the slots' actors.
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Slot));
				return;
			}
			const uint8 Actor = Context.PartyActors[static_cast<std::size_t>(Slot)];
			Context.Run.Budget += 4;
			if (Actor == FEventContext::NoActor)
			{
				SetVar(Context, ReadU16(Context, 1), 0);
				SetVar(Context, ReadU16(Context, 3), 0);
			}
			else
			{
				FSprite* Sprite = ActorSprite(Context, Actor);
				if (Sprite == nullptr)
				{
					Fail(Context, EEventFault::ActorWithoutSprite, Actor);
					return;
				}
				SetVar(Context, ReadU16(Context, 1), Sprite->Extra.Signal);
				SetVar(Context, ReadU16(Context, 3), Actor);
				if (static_cast<int16>(Sprite->Extra.Signal) != 1)
				{
					Sprite->Extra.Signal = 0;
				}
			}
			Advance(Context, 7);
		}
	}

	namespace
	{
		/** FE 1F: the running actor, when it is a party slot's gear and the slot's member walks, is boarded by the member. */
		void BoardGear(FEventContext& Context)
		{
			const auto Gear = std::find(Context.GearActors.begin(), Context.GearActors.end(), static_cast<uint8>(Context.Run.Actor));
			if (Gear != Context.GearActors.end())
			{
				const int32 Slot = static_cast<int32>(Gear - Context.GearActors.begin());
				FGameState* Game = GameStateOf(Context);
				if (Game == nullptr || (Game->PartySlotInGear(Slot) == 0 && !BoardFieldGear(Context, Slot)))
				{
					return;
				}
			}
			Advance(Context, 1);
		}

		/** FE 20: a party slot's member, when the slot has a character and the member rides, leaves the gear. */
		void LeaveGear(FEventContext& Context)
		{
			const int32 Slot = CodeByte(Context, 1);
			if (Slot > LastPartySlot)
			{
				// The original reads past the slots' tables.
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Slot));
				return;
			}
			if (Context.PartyCharacters[static_cast<std::size_t>(Slot)] != FEventContext::NoActor)
			{
				FGameState* Game = GameStateOf(Context);
				if (Game == nullptr || (Game->PartySlotInGear(Slot) != 0 && !LeaveFieldGear(Context, Slot)))
				{
					return;
				}
			}
			Advance(Context, 2);
		}
	}

	void RegisterPartySprites(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0x16] = &SetPartyCharacter;
		Base[0x5C] = &ParkedGear;
		Extended[0x1F] = &BoardGear;
		Extended[0x20] = &LeaveGear;
		Extended[0x21] = &BindPartySprite;
		Extended[0xC1] = &GetPartySpriteSignal;
	}
}
