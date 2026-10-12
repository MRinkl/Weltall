#include "EventOps.h"
#include "EventSprites.h"

#include "Graphics/Sprite.h"
#include "Graphics/SpriteSystem.h"

#include <array>

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint32 GearModelSlotShift = 13;
		constexpr uint32 GearModelSlotMask = 0x7u << GearModelSlotShift;
		/** The extra animation sets' first file in directory 4. */
		constexpr int32 FirstExtraAnimationFile = 0x77A;

		FFieldActor& Record(FEventContext& Context)
		{
			return Context.Actors[static_cast<std::size_t>(Context.Run.Actor)];
		}

		/** The running actor, shown with a sprite of the map's, and colliding with others. */
		void ShowMapSprite(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.Flags0 = (State.Flags0 & ~EventActorFlags0::PassesThroughActors) | EventActorFlags0::MapSprite;
			State.Flags1 &= ~EventActorFlags1::NotFloorAligned;
			Record(Context).Flags &= static_cast<uint16>(~FieldActorFlag::Hidden);
		}

		/** A sprite file of the map's for the running actor, or a fault when the scripts name none. */
		bool LoadMapSprite(FEventContext& Context, int32 Slot, int32 PaletteSet, bool bDeferred)
		{
			return LoadActorSprite(Context, Context.Run.Actor, EFieldSpriteKind::Map, Slot, PaletteSet, static_cast<uint8>(Slot | 0x80), bDeferred);
		}

		/** 0B: the actor gets a sprite of the map's and plays its first step at once. */
		void SetModel(FEventContext& Context)
		{
			MarkSpriteActor(Context);
			if (!LoadMapSprite(Context, ReadValue(Context, 1), 0, false))
			{
				return;
			}
			SyncRecordPosition(Context);
			Advance(Context, 3);
			ShowMapSprite(Context);
		}

		/** BC: the actor gets the map's first sprite, kept hidden: the scripts show it later or never. */
		void SetHiddenModel(FEventContext& Context)
		{
			if (GiveHiddenMapSprite(Context))
			{
				Advance(Context, 1);
			}
		}

		/**
		 * 93: the actor becomes a gear, its model registered for the map to load once its scripts
		 * have set up; until then it has the map's first sprite.
		 */
		void SetGearModel(FEventContext& Context)
		{
			MarkSpriteActor(Context);
			const int32 Model = ReadValue(Context, 1);
			FFieldGears& Gears = Context.Gears;
			if (Gears.Count >= FFieldGears::Capacity)
			{
				Fail(Context, EEventFault::GearArraysFull, static_cast<uint32>(Gears.Count));
				return;
			}
			if (!LoadMapSprite(Context, 0, 0, true))
			{
				return;
			}
			SyncRecordPosition(Context);
			Advance(Context, 3);
			FEventActorState& State = Self(Context);
			State.Flags0 |= EventActorFlags0::MapSprite;
			Record(Context).Flags &= static_cast<uint16>(~FieldActorFlag::Hidden);
			State.Flags1 = (State.Flags1 & ~EventActorFlags1::NotFloorAligned) | EventActorFlags1::Gear;
			const std::size_t Slot = static_cast<std::size_t>(Gears.Count);
			Gears.FileNumbers[Slot] = static_cast<uint16>(Model * 2);
			Gears.TextureShifts[Slot] = 0;
			State.Status = (State.Status & ~GearModelSlotMask) | static_cast<uint32>(Gears.Count & 7) << GearModelSlotShift;
			++Gears.Count;
		}

		/**
		 * FE A6 and FE A7: the running actor's sprite takes the cells of its next page columns from other
		 * places in VRAM, one or two, each a page column (bits 0-3 of its first operand) and a row (bits
		 * 4 and up, times 256, plus its second); the script state keeps them for the return from a battle.
		 */
		template <int32 Slots>
		void SetTextureSlots(FEventContext& Context)
		{
			FSprite* Sprite = ActorSprite(Context, Context.Run.Actor);
			if (Sprite == nullptr)
			{
				Fail(Context, EEventFault::ActorWithoutSprite, static_cast<uint32>(Context.Run.Actor));
				return;
			}
			std::array<uint32, Slots> Columns{};
			std::array<uint32, Slots> Rows{};
			for (int32 Slot = 0; Slot < Slots; ++Slot)
			{
				const int32 Column = ReadValue(Context, 1 + 4 * Slot);
				Columns[static_cast<std::size_t>(Slot)] = static_cast<uint32>(Column);
				Rows[static_cast<std::size_t>(Slot)] = static_cast<uint32>((Column >> 4) * 0x100 + ReadValue(Context, 3 + 4 * Slot));
			}
			Context.Models->SpriteSystem()->AllocTextureSlots(*Sprite, Slots + 1);
			FEventActorState& State = Self(Context);
			for (int32 Slot = 0; Slot < Slots; ++Slot)
			{
				const uint32 Column = Columns[static_cast<std::size_t>(Slot)] & 0xF;
				const uint32 Row = Rows[static_cast<std::size_t>(Slot)];
				Sprite->Extra.TextureSlots[static_cast<std::size_t>(Slot + 1)] = {static_cast<int16>(Column << 6), static_cast<int16>(Row)};
				if (Slot == 0)
				{
					State.Status = (State.Status & 0xF003FFFF) | Column << 24;
					State.SpriteTexture = (State.SpriteTexture & ~0x1FFu) | (Row & 0x1FF);
				}
				else
				{
					State.SpriteTexture = (State.SpriteTexture & 0xFFF801FF) | Column << 15;
					State.SpriteTexture = (State.SpriteTexture & 0xF007FFFF) | (Row & 0x1FF) << 19;
				}
			}
			State.Status = (State.Status & ~0x30000u) | static_cast<uint32>(Slots) << 16;
			Advance(Context, 1 + 4 * Slots);
		}

		/**
		 * FE 4A: the running actor reads an extra animation set, file 0x77A + the operand of directory 4
		 * (the set it read before let go); its turn ends. It waits while another actor's set is on its
		 * way to FE 4B.
		 */
		void LoadExtraAnimations(FEventContext& Context)
		{
			Context.Run.bYield = true;
			if (Context.bExtraAnimationsLoading)
			{
				Advance(Context, -1);
				return;
			}
			FEventActorState& State = Self(Context);
			const int32 File = ReadValue(Context, 1) + FirstExtraAnimationFile;
			State.ExternalAnimationFile = static_cast<int16>(File);
			if (Context.Models == nullptr)
			{
				Fail(Context, EEventFault::NoActorModels, static_cast<uint32>(Context.Run.Actor));
				return;
			}
			if (!Context.Models->LoadExtraAnimations(Context.Run.Actor, File))
			{
				Fail(Context, EEventFault::NoSpriteFile, static_cast<uint32>(File));
				return;
			}
			Context.bExtraAnimationsLoading = true;
			Advance(Context, 3);
		}

		/** FE 4B: the running actor's sprite plays the set it read as its negative animations; the turn ends. */
		void UseExtraAnimations(FEventContext& Context)
		{
			if (Context.Models == nullptr)
			{
				Fail(Context, EEventFault::NoActorModels, static_cast<uint32>(Context.Run.Actor));
				return;
			}
			Context.bExtraAnimationsLoading = false;
			Context.Models->UseExtraAnimations(Context.Run.Actor);
			Context.Run.bYield = true;
			Advance(Context, 1);
		}

		/** FE 4D: the actor's next animation is animation k of its extra set (~k); FE 4C also clears its ended flag. */
		template <bool bClearsEnded>
		void PlayExtraAnimation(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.AnimationRequest = static_cast<uint16>(~CodeByte(Context, 1));
			Advance(Context, 2);
			if (bClearsEnded)
			{
				State.Flags1 &= ~EventActorFlags1::AnimationFinished;
			}
		}

		/** FE 4E: the running actor lets its extra animation set go (its sprite keeps playing it); the turn ends. */
		void FreeExtraAnimations(FEventContext& Context)
		{
			Self(Context).ExternalAnimationFile = -1;
			Context.Run.bYield = true;
			Advance(Context, 1);
		}

		/** FE 15: the actor gets a sprite of the map's in one of its colour sets, its first step not yet run. */
		void SetSprite(FEventContext& Context)
		{
			MarkSpriteActor(Context);
			const int32 Slot = ReadValue(Context, 1);
			if (!LoadMapSprite(Context, Slot, ReadValue(Context, 3), true))
			{
				return;
			}
			SyncRecordPosition(Context);
			Advance(Context, 5);
			ShowMapSprite(Context);
		}
	}

	void RegisterSprites(FBaseTable& Base, FExtendedTable& Extended)
	{
		Extended[0xA6] = &SetTextureSlots<1>;
		Extended[0xA7] = &SetTextureSlots<2>;
		Base[0x0B] = &SetModel;
		Base[0x93] = &SetGearModel;
		Base[0xBC] = &SetHiddenModel;
		Extended[0x15] = &SetSprite;
		Extended[0x4A] = &LoadExtraAnimations;
		Extended[0x4B] = &UseExtraAnimations;
		Extended[0x4C] = &PlayExtraAnimation<true>;
		Extended[0x4D] = &PlayExtraAnimation<false>;
		Extended[0x4E] = &FreeExtraAnimations;
	}
}
