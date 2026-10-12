#include "Field/FieldSprites.h"

#include "Field/FieldSpriteFiles.h"

#include <utility>

namespace Weltall
{
	namespace
	{
		constexpr int32 MapClutX = 0x100;
		constexpr int32 MapClutY = 0x1E0;
		constexpr int32 PartyClutY = 0xE0;
		constexpr int32 PartyCopyClutY = 0xE3;
		constexpr int32 PartyTextureX = 0x280;
		constexpr int32 PartyCopyTextureX = 0x2A0;
		constexpr int32 PartyTextureY = 0x100;
		constexpr int32 PartyTextureRows = 0x40;
		constexpr int32 PartyPartRecords = 0x20;
		/** Part coordinates in texels << 3, the field's sprite scale and weight. */
		constexpr int32 FieldSizeShift = 3;
		constexpr int16 FieldSpriteScale = 0xC00;
		constexpr int16 FieldSpriteWeight = 0x2000;
		constexpr int32 FieldSpriteGravity = 0x10000;
		constexpr uint16 NoAnimationsSignal = 0xFF;
		constexpr int32 ExtraAnimationDirectory = 4;
	}

	FFieldSprites::FFieldSprites(std::span<const FSpriteFile> InMapFiles, const std::array<FFieldSpriteSlot, FieldSpriteSlotCount>& InSlots,
		const FPartySpriteFiles& InPartyFiles, std::span<FEventActorState> InStates, const FCdImage& InImage, const FGameFileIndex& InIndex)
		: MapFiles(InMapFiles), Slots(InSlots), PartyFiles(InPartyFiles), States(InStates), Sprites(InStates.size()), HasSprite(InStates.size(), 0),
		  SpriteIndex(InStates.size()), Image(InImage), FileIndex(InIndex), ActorExtraFiles(InStates.size(), nullptr)
	{
		for (std::size_t Actor = 0; Actor < SpriteIndex.size(); ++Actor)
		{
			SpriteIndex[Actor] = Actor;
		}
	}

	std::optional<FFieldSpriteResult> FFieldSprites::LoadSprite(const FFieldSpriteRequest& Request)
	{
		if (Request.Actor < 0 || static_cast<std::size_t>(Request.Actor) >= Sprites.size())
		{
			return std::nullopt;
		}
		const FSpriteFile* File = nullptr;
		int32 ClutX = MapClutX;
		int32 ClutY = 0;
		int32 TextureX = 0;
		int32 TextureY = 0;
		if (Request.Kind == EFieldSpriteKind::Map)
		{
			if (Request.Slot < 0 || static_cast<std::size_t>(Request.Slot) >= MapFiles.size() || static_cast<std::size_t>(Request.Slot) >= Slots.size())
			{
				return std::nullopt;
			}
			File = &MapFiles[static_cast<std::size_t>(Request.Slot)];
			ClutX = MapClutX + 16 * Request.PaletteSet;
			ClutY = MapClutY + Request.Slot;
			TextureX = Slots[static_cast<std::size_t>(Request.Slot)].X;
			TextureY = Slots[static_cast<std::size_t>(Request.Slot)].Y;
		}
		else
		{
			if (Request.Slot < 0 || Request.Slot >= PartySlots)
			{
				return std::nullopt;
			}
			File = &PartyFiles.Buffer(Request.Slot);
			if (File->Size() == 0)
			{
				return std::nullopt;
			}
			const bool bCopy = Request.Kind == EFieldSpriteKind::PartyCopy;
			ClutY = (bCopy ? PartyCopyClutY : PartyClutY) + Request.Slot;
			TextureX = bCopy ? PartyCopyTextureX : PartyTextureX;
			TextureY = PartyTextureY + PartyTextureRows * Request.Slot;
		}

		const std::size_t Index = SpriteIndex[static_cast<std::size_t>(Request.Actor)];
		FSprite& Sprite = Sprites[Index];
		if (HasSprite[Index] != 0)
		{
			Kernel.Free(Sprite);
		}
		Kernel.Create(Sprite, *File, ClutX, static_cast<int16>(ClutY), TextureX, TextureY, Request.Kind == EFieldSpriteKind::Map ? Request.PaletteSet : 0);
		if (Request.Kind != EFieldSpriteKind::Map)
		{
			Kernel.SetPartCapacity(Sprite, PartyPartRecords);
		}
		HasSprite[Index] = 1;
		const FSpriteFrameSize Size = Kernel.FirstFrameSize(Sprite);
		Kernel.SetSizeShift(Sprite, FieldSizeShift);
		Sprite.Scale = FieldSpriteScale;
		Sprite.Weight = FieldSpriteWeight;
		// Made as the field comes back from a battle, a sprite keeps the floor and gravity it is made with;
		// its place and motion are the fresh start's, where the original leaves what its memory held.
		Sprite.Position = Request.Position;
		Sprite.Velocity = {};
		if (!Request.bReturning)
		{
			Sprite.Gravity = FieldSpriteGravity;
			Sprite.FloorY = Request.RecordHeight;
		}
		if (Request.bFog)
		{
			Sprite.BuildFlags |= SpriteBuildFlags::Fog;
		}
		Kernel.SetAnimation(Sprite, 0);
		Kernel.SetDirection(Sprite, 0);
		Sprite.Extra.Owner = static_cast<int16>(Request.Actor);
		Sprite.Listener = this;
		FFieldSpriteResult Result;
		Result.FrameTop = Size.Top;
		if (!Request.bDeferred)
		{
			Kernel.TickAnimation(Sprite);
			Kernel.RunTasks();
			Result.bNoAnimations = Sprite.Extra.Signal == NoAnimationsSignal;
		}
		Sprite.FloorY = static_cast<int16>(Request.Position[1] >> 16);
		Sprite.Position = Request.Position;
		return Result;
	}

	FSprite* FFieldSprites::ActorSprite(int32 Actor)
	{
		return const_cast<FSprite*>(SpriteOf(Actor));
	}

	void FFieldSprites::OnAnimationEvent(FSprite& Sprite)
	{
		const std::size_t Actor = static_cast<std::size_t>(Sprite.Extra.Owner);
		if (Actor < States.size())
		{
			States[Actor].Flags1 |= EventActorFlags1::AnimationFinished;
		}
	}

	void FFieldSprites::SwapSprites(int32 First, int32 Second)
	{
		if (First >= 0 && Second >= 0 && static_cast<std::size_t>(First) < SpriteIndex.size() && static_cast<std::size_t>(Second) < SpriteIndex.size())
		{
			std::swap(SpriteIndex[static_cast<std::size_t>(First)], SpriteIndex[static_cast<std::size_t>(Second)]);
		}
	}

	bool FFieldSprites::LoadExtraAnimations(int32 Actor, int32 File)
	{
		if (Actor < 0 || static_cast<std::size_t>(Actor) >= ActorExtraFiles.size())
		{
			return false;
		}
		auto Loaded = ReadSpriteFile(Image, FileIndex, ExtraAnimationDirectory, File);
		if (!Loaded)
		{
			return false;
		}
		ExtraFiles.push_back(std::make_unique<FSpriteFile>(std::move(*Loaded)));
		ActorExtraFiles[static_cast<std::size_t>(Actor)] = ExtraFiles.back().get();
		return true;
	}

	void FFieldSprites::UseExtraAnimations(int32 Actor)
	{
		if (FSprite* Sprite = ActorSprite(Actor))
		{
			Sprite->AlternateFile = ActorExtraFiles[static_cast<std::size_t>(Actor)];
		}
	}

	const FSprite* FFieldSprites::SpriteOf(int32 Actor) const
	{
		if (Actor < 0 || static_cast<std::size_t>(Actor) >= Sprites.size())
		{
			return nullptr;
		}
		const std::size_t Index = SpriteIndex[static_cast<std::size_t>(Actor)];
		return HasSprite[Index] != 0 ? &Sprites[Index] : nullptr;
	}
}
