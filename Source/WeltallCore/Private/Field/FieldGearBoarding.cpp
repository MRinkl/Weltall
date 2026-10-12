#include "Field/FieldGearBoarding.h"

#include "EventOps.h"
#include "EventSprites.h"

#include "Field/EventContext.h"
#include "Field/FieldActorMotion.h"
#include "Game/GameState.h"
#include "Graphics/Sprite.h"

#include <string>

namespace Weltall
{
	namespace
	{
		constexpr int32 PartyGearSlots = 3;
		/** What a gear plays once its pilot has left it. */
		constexpr int32 LeftGearAnimation = 6;
		/** Where each party slot's gear was parked: the map (with its walkmesh layer in bits 14-15), x and z, six bytes a slot. */
		constexpr uint32 ParkedGearVariables = 0x02A;
		constexpr uint32 ParkedGearStride = 6;
		constexpr int32 MapNumberMask = 0xFFF;
		constexpr int32 ParkedLayerBits = 3;
		constexpr int32 ParkedLayerShift = 14;
		constexpr uint32 SpriteModelBits = EventActorFlags0::MapSprite | EventActorFlags0::GearPilot | EventActorFlags0::PartyMember;

		/** A party slot's member and gear: their actors, script states, records and sprites. */
		struct FBoardingActors
		{
			int32 Pilot = 0;
			int32 Gear = 0;
			FEventActorState* PilotState = nullptr;
			FEventActorState* GearState = nullptr;
			FFieldActor* PilotRecord = nullptr;
			FFieldActor* GearRecord = nullptr;
		};

		/** The slot's actors, or nothing with the context's fault when one is missing or has no sprite to hand over. */
		std::optional<FBoardingActors> BoardingActors(FEventContext& Context, int32 Slot)
		{
			if (Slot < 0 || Slot >= PartyGearSlots)
			{
				// The original indexes past the three slots' tables.
				EventOps::Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Slot));
				return std::nullopt;
			}
			FBoardingActors Actors;
			Actors.Pilot = Context.PartyActors[static_cast<std::size_t>(Slot)];
			Actors.Gear = Context.GearActors[static_cast<std::size_t>(Slot)];
			Actors.PilotState = EventOps::StateOf(Context, static_cast<uint32>(Actors.Pilot));
			Actors.GearState = EventOps::StateOf(Context, static_cast<uint32>(Actors.Gear));
			if (Actors.PilotState == nullptr || Actors.GearState == nullptr)
			{
				return std::nullopt;
			}
			Actors.PilotRecord = &Context.Actors[static_cast<std::size_t>(Actors.Pilot)];
			Actors.GearRecord = &Context.Actors[static_cast<std::size_t>(Actors.Gear)];
			for (const int32 Actor : {Actors.Pilot, Actors.Gear})
			{
				if (EventOps::ActorSprite(Context, Actor) == nullptr)
				{
					EventOps::Fail(Context, EEventFault::ActorWithoutSprite, static_cast<uint32>(Actor));
					return std::nullopt;
				}
			}
			return Actors;
		}

		void MoveSprite(FEventContext& Context, int32 Actor, const std::array<int32, 3>& Position)
		{
			EventOps::ActorSprite(Context, Actor)->Position = Position;
		}

		/** The gear stands where its pilot stands: the pilot's floor, place and model placement. */
		void PlaceGearAtPilot(FEventContext& Context, const FBoardingActors& Actors)
		{
			FEventActorState& Gear = *Actors.GearState;
			const FEventActorState& Pilot = *Actors.PilotState;
			Gear.FloorTriangles = Pilot.FloorTriangles;
			Gear.Layer = Pilot.Layer;
			Gear.TargetHeight = Pilot.TargetHeight;
			Gear.FloorHeight = Pilot.FloorHeight;
			Gear.FloorNormal = Pilot.FloorNormal;
			Gear.Position = Pilot.Position;
			Actors.GearRecord->Matrix = Actors.PilotRecord->Matrix;
			Actors.GearRecord->Translation = Actors.PilotRecord->Translation;
			MoveSprite(Context, Actors.Gear, Pilot.Position);
		}

		/** Both actors land and play their idle animations again, the gear the one given. */
		void RestartAnimations(FEventContext& Context, const FBoardingActors& Actors, int32 GearAnimation)
		{
			Actors.GearState->Flags0 &= ~EventActorFlags0::InTheAir;
			Actors.PilotState->Flags0 &= ~EventActorFlags0::InTheAir;
			Actors.GearState->Animation = Actors.GearState->IdleAnimation;
			Actors.PilotState->Animation = Actors.PilotState->IdleAnimation;
			PlayActorAnimation(Context, Actors.Gear, GearAnimation);
			PlayActorAnimation(Context, Actors.Pilot, static_cast<int16>(Actors.PilotState->IdleAnimation));
		}
	}

	bool BoardFieldGear(FEventContext& Context, int32 Slot)
	{
		FGameState* Game = EventOps::GameStateOf(Context);
		const std::optional<FBoardingActors> Found = Game != nullptr ? BoardingActors(Context, Slot) : std::nullopt;
		if (!Found)
		{
			return false;
		}
		const FBoardingActors& Actors = *Found;
		FEventActorState& Pilot = *Actors.PilotState;
		Context.Models->SwapSprites(Actors.Gear, Actors.Pilot);
		MoveSprite(Context, Actors.Pilot, Pilot.Position);
		Actors.GearRecord->Flags |= FieldActorFlag::Hidden;
		Pilot.Flags0 = (Pilot.Flags0 & ~SpriteModelBits) | EventActorFlags0::GearPilot;
		Game->SetPartySlotInGear(Slot, 1);
		RestartAnimations(Context, Actors, static_cast<int16>(Actors.GearState->IdleAnimation));
		Context.Particles.Stop(static_cast<int16>(Actors.Gear), false);
		ParkFieldGear(Context, Slot);
		return true;
	}

	bool LeaveFieldGear(FEventContext& Context, int32 Slot)
	{
		FGameState* Game = EventOps::GameStateOf(Context);
		const std::optional<FBoardingActors> Found = Game != nullptr ? BoardingActors(Context, Slot) : std::nullopt;
		if (!Found)
		{
			return false;
		}
		const FBoardingActors& Actors = *Found;
		FEventActorState& Pilot = *Actors.PilotState;
		FEventActorState& Gear = *Actors.GearState;
		Game->SetPartySlotInGear(Slot, 0);
		Context.Models->SwapSprites(Actors.Pilot, Actors.Gear);
		FFieldActor& GearRecord = *Actors.GearRecord;
		GearRecord.Flags = static_cast<uint16>(((GearRecord.Flags & ~FieldActorFlag::KindBits) | FieldActorFlag::WalkingKind) & ~FieldActorFlag::Hidden);
		Gear.Flags0 &= ~EventActorFlags0::Frozen;
		PlaceGearAtPilot(Context, Actors);
		MoveSprite(Context, Actors.Pilot, Pilot.Position);
		Pilot.Flags0 = (Pilot.Flags0 & ~SpriteModelBits) | EventActorFlags0::PartyMember;
		Gear.Yaw = Pilot.Yaw;
		Gear.Facing = Pilot.Facing;
		RestartAnimations(Context, Actors, LeftGearAnimation);
		ParkFieldGear(Context, Slot);
		Context.Particles.Stop(static_cast<int16>(Actors.Gear), false);
		return true;
	}

	void ParkFieldGear(FEventContext& Context, int32 Slot)
	{
		if (Slot < 0 || Slot >= PartyGearSlots)
		{
			return;
		}
		const FEventActorState* Member = Context.StateOf(Context.PartyActors[static_cast<std::size_t>(Slot)]);
		if (Member == nullptr)
		{
			return;
		}
		const uint32 Parked = ParkedGearVariables + ParkedGearStride * static_cast<uint32>(Slot);
		Context.Variables.Set(Parked, (Context.MapNumber & MapNumberMask) | (Member->Layer & ParkedLayerBits) << ParkedLayerShift);
		Context.Variables.Set(Parked + 2, static_cast<int16>(Member->Position[0] >> 16));
		Context.Variables.Set(Parked + 4, static_cast<int16>(Member->Position[2] >> 16));
	}

	std::optional<FError> BoardOrLeaveFieldGears(FEventContext& Context, const std::array<bool, 3>& Slots, bool bByButtons)
	{
		if (Context.PilotSpriteBase == 0)
		{
			return std::nullopt;
		}
		if (Context.GameState == nullptr)
		{
			return MakeError("field map " + std::to_string(Context.RunningMap) + ": the gears' boarding has no game state");
		}
		for (int32 Slot = 0; Slot < PartyGearSlots; ++Slot)
		{
			if (Context.PartyActors[static_cast<std::size_t>(Slot)] == FEventContext::NoActor || !Slots[static_cast<std::size_t>(Slot)])
			{
				continue;
			}
			const bool bRiding = Context.GameState->PartySlotInGear(Slot) != 0;
			if (bRiding == bByButtons)
			{
				LeaveFieldGear(Context, Slot);
			}
			else
			{
				BoardFieldGear(Context, Slot);
			}
			if (Context.Run.Fault != EEventFault::None)
			{
				const EEventFault Fault = Context.Run.Fault;
				Context.Run.Fault = EEventFault::None;
				return MakeError("field map " + std::to_string(Context.RunningMap) + ", the gears' boarding of party slot " + std::to_string(Slot) + ": "
								 + EventOps::DescribeFault(Context, Fault, Context.Run.FaultValue));
			}
		}
		return std::nullopt;
	}
}
