#include "EventSprites.h"

#include "EventOps.h"

#include "Field/EventPlacement.h"
#include "Graphics/Sprite.h"

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint32 PaletteSetMask = 0xF;
		constexpr uint32 DeferredBit = 0x10;
		constexpr uint32 KindShift = 28;
		constexpr uint32 KindMask = 0x3u << KindShift;
		/** The height a party member's actor has, whatever its sprite. */
		constexpr uint16 PartyMemberHeight = 0x40;
		constexpr uint16 NoAnimationRequest = 0xFF;
		constexpr uint32 KindFieldMask = 0xF07F;
		constexpr uint16 SpriteActorKind = 0x200;
		/** A map sprite's id: its file with bit 7. */
		constexpr uint8 MapSpriteId = 0x80;

		/** The record holds the actor's whole position, each part sign-extended. */
		void MoveRecord(FFieldActor& Record, const FEventActorState& State)
		{
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Record.Translation[Axis] = static_cast<int16>(State.Position[Axis] >> 16);
				Record.Position[Axis] = Record.Translation[Axis];
			}
		}
	}

	bool LoadActorSprite(FEventContext& Context, int32 Actor, EFieldSpriteKind Kind, int32 Slot, int32 PaletteSet, uint8 Id, bool bDeferred)
	{
		if (Context.Models == nullptr)
		{
			Fail(Context, EEventFault::NoActorModels, static_cast<uint32>(Actor));
			return false;
		}
		FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
		FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Actor)];
		State.ModelIndex = static_cast<uint8>(Slot);
		State.ModelId = Id;
		State.SpriteDraw = (State.SpriteDraw & ~(PaletteSetMask | DeferredBit)) | (static_cast<uint32>(PaletteSet) & PaletteSetMask) | (bDeferred ? DeferredBit : 0);
		State.SpriteTexture = (State.SpriteTexture & ~KindMask) | static_cast<uint32>(Kind) << KindShift;

		FFieldSpriteRequest Request;
		Request.Actor = Actor;
		Request.Kind = Kind;
		Request.Slot = Slot;
		Request.PaletteSet = PaletteSet;
		Request.bDeferred = bDeferred;
		Request.Position = State.Position;
		Request.RecordHeight = static_cast<int16>(Record.Translation[1]);
		Request.bFog = Context.Render.Fog.bOn;
		Request.bReturning = Context.bReturning;
		const std::optional<FFieldSpriteResult> Result = Context.Models->LoadSprite(Request);
		if (!Result)
		{
			Fail(Context, EEventFault::NoSpriteFile, static_cast<uint32>(Slot));
			return false;
		}
		if (!Context.bReturning)
		{
			State.Height = Kind == EFieldSpriteKind::Map ? static_cast<uint16>(Result->FrameTop * 2) : PartyMemberHeight;
		}
		if (!bDeferred && Result->bNoAnimations)
		{
			State.AnimationRequest = NoAnimationRequest;
			State.Flags1 |= EventActorFlags1::NoAnimations;
		}
		MoveRecord(Record, State);
		++Context.ModelsCreated;
		return true;
	}

	void MarkSpriteActor(FEventContext& Context)
	{
		FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Context.Run.Actor)];
		Record.Flags = static_cast<uint16>((Record.Flags & KindFieldMask) | SpriteActorKind);
	}

	bool GiveHiddenMapSprite(FEventContext& Context)
	{
		if (!LoadActorSprite(Context, Context.Run.Actor, EFieldSpriteKind::Map, 0, 0, MapSpriteId, true))
		{
			return false;
		}
		SyncRecordPosition(Context);
		FEventActorState& State = Self(Context);
		State.Flags0 |= EventActorFlags0::MapSprite;
		State.Flags1 |= EventActorFlags1::NotFloorAligned;
		return true;
	}

	int32 PartySlotOf(const FEventContext& Context, int32 Character)
	{
		if (Character == 0xFF)
		{
			return -1;
		}
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			const uint8 InSlot = Context.PartyCharacters[static_cast<std::size_t>(Slot)];
			if (InSlot == FEventContext::NoActor)
			{
				return -1;
			}
			if (InSlot == Character)
			{
				return Slot;
			}
		}
		return -1;
	}

	void SyncRecordPosition(FEventContext& Context)
	{
		FEventActorState& State = Self(Context);
		MoveRecord(Context.Actors[static_cast<std::size_t>(Context.Run.Actor)], State);
		State.FloorHeight = static_cast<int16>(State.Position[1] >> 16);
		if (FSprite* Sprite = ActorSprite(Context, Context.Run.Actor))
		{
			Sprite->Position = State.Position;
			Sprite->Velocity[1] = 0;
			Sprite->FloorY = State.FloorHeight;
		}
	}

	FSprite* ActorSprite(FEventContext& Context, int32 Actor)
	{
		return Context.Models != nullptr ? Context.Models->ActorSprite(Actor) : nullptr;
	}

	bool PlaceOnFloor(FEventContext& Context, int32 X, int32 Z, bool bAnyLayer)
	{
		return PlaceActorOnFloor(Context, Context.Run.Actor, X, Z, bAnyLayer);
	}

	bool PlaceActorOnFloor(FEventContext& Context, int32 Actor, int32 X, int32 Z, bool bAnyLayer)
	{
		FEventActorState* State = StateOf(Context, static_cast<uint32>(Actor));
		if (State == nullptr)
		{
			return false;
		}
		const int32 Layer = static_cast<int16>(State->Layer);
		if (Context.Walkmesh != nullptr && !bAnyLayer && (Layer < 0 || Layer >= static_cast<int32>(Context.Walkmesh->Layers.size())))
		{
			Fail(Context, EEventFault::FloorFromStaleStack, static_cast<uint16>(State->Layer));
			return false;
		}
		if (Context.Walkmesh == nullptr || !PlaceActor(*State, Context.Actors[static_cast<std::size_t>(Actor)], *Context.Walkmesh, X, Z))
		{
			Fail(Context, EEventFault::NoFloorTriangle, static_cast<uint16>(State->Layer));
			return false;
		}
		if (FSprite* Sprite = ActorSprite(Context, Actor))
		{
			Sprite->FloorY = State->FloorHeight;
			Sprite->Position = State->Position;
			Sprite->Velocity = {};
		}
		return true;
	}
}
