#include "Field/FieldMemberStandIns.h"

#include "Field/EventContext.h"
#include "Field/FieldSpriteFiles.h"
#include "Field/FieldSprites.h"
#include "Game/GameState.h"
#include "Game/PartyOrder.h"
#include "Graphics/Vram.h"

namespace Weltall
{
	namespace
	{
		constexpr int32 PartySpriteDirectory = 4;
		/** The animations every member's own sprite has, numbered alike; and those a gear's or a pilot's shares with the others'. */
		constexpr int32 MemberAnimations = 15;
		constexpr int32 MovingAnimations = 4;
		/** The stand-ins' frames in the port rows below the pilots' stand-ins', their CLUTs beside the pilots' CLUTs. */
		constexpr int16 AreaX = 0;
		constexpr int16 AreaY = FVram::Height + 0x40;
		constexpr int16 ClutX = 0x140;
		constexpr int16 ClutRow = FVram::Height;
		constexpr int32 Slots = FPartySpriteFiles::Slots;
		constexpr int32 WholeScale = 0x1000;

		/** The party slot whose actor, or whose gear's actor, the actor is. */
		std::optional<int32> SlotOf(const FEventContext& Context, int32 Actor)
		{
			for (int32 Slot = 0; Slot < Slots; ++Slot)
			{
				if (Context.PartyActors[static_cast<std::size_t>(Slot)] == Actor || Context.GearActors[static_cast<std::size_t>(Slot)] == Actor)
				{
					return Slot;
				}
			}
			return std::nullopt;
		}
	}

	FFieldMemberStandIns::FFieldMemberStandIns(const FCdImage& InImage, const FGameFileIndex& InIndex)
		: Image(InImage), Index(InIndex), StandIns(2 * Slots, AreaX, AreaY, ClutX, ClutRow)
	{
	}

	void FFieldMemberStandIns::Clear()
	{
		StandIns.Clear();
	}

	const FSpriteFile* FFieldMemberStandIns::DiscFile(int32 Number)
	{
		auto [Found, bNew] = Files.try_emplace(Number);
		if (bNew)
		{
			TResult<FSpriteFile> File = ReadSpriteFile(Image, Index, PartySpriteDirectory, Number);
			if (File && !File->IsResident(File->Tables()))
			{
				Found->second = std::make_unique<FSpriteFile>(std::move(*File));
			}
		}
		return Found->second.get();
	}

	void FFieldMemberStandIns::Arrange(const FEventContext& Context, const FPartyOrder& Order, bool bShown)
	{
		Looks.fill(-1);
		if (!bShown)
		{
			return;
		}
		const std::array<uint8, Slots>& Party = Context.PartyCharacters;
		// A slot whose own sprite is its gear's rides: its member is shown among those who ride, the others among those on foot.
		std::array<bool, Slots> bRides{};
		for (int32 Slot = 0; Slot < Slots && Context.PartySprites != nullptr; ++Slot)
		{
			const int32 Own = Context.PartySprites->File(Slot);
			bRides[static_cast<std::size_t>(Slot)] = Own != FPartySpriteFiles::NoFile && Own >= FPartySpriteFiles::FirstGearFile;
		}
		const std::array<uint8, Slots> Members = Order.ShownAsTheyRide(Party, bRides);
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			const uint8 Shown = Members[static_cast<std::size_t>(Slot)];
			if (Shown != Party[static_cast<std::size_t>(Slot)] && Shown < FGameState::CharacterCount)
			{
				Looks[static_cast<std::size_t>(Slot)] = Shown;
			}
		}
	}

	std::optional<int32> FFieldMemberStandIns::ShownFor(const FEventContext& Context, int32 Actor) const
	{
		const std::optional<int32> Slot = SlotOf(Context, Actor);
		return Slot ? ShownIn(*Slot) : std::nullopt;
	}

	FFieldMemberStandIns::FLook FFieldMemberStandIns::Follow(const FEventContext& Context, const FFieldSprites& Sprites, int32 Actor, const FSprite& Sprite,
		bool bPilotsDrawn, uint32 Tick, FVram& Vram)
	{
		const std::optional<int32> Slot = SlotOf(Context, Actor);
		const std::optional<int32> Shown = Slot ? ShownIn(*Slot) : std::nullopt;
		if (!Shown || Sprite.PartCount() == 0 || Sprite.PrimaryFile == nullptr || Context.PartySprites == nullptr || Context.GameState == nullptr)
		{
			return {};
		}
		const FPartySpriteFiles& Party = *Context.PartySprites;
		const FSpriteFile* File = nullptr;
		int32 Place = *Slot;
		int32 Animations = MemberAnimations;
		FLook Look;
		if (Sprite.PrimaryFile == &Party.Buffer(*Slot))
		{
			// The slot's own sprite: its member's, or in gears its gear's.
			const int32 Own = Party.File(*Slot);
			if (Own == FPartySpriteFiles::NoFile)
			{
				return {};
			}
			const bool bGear = Own >= FPartySpriteFiles::FirstGearFile;
			const std::optional<int32> ShownFile = bGear ? FPartySpriteFiles::FileFor(*Context.GameState, *Shown, true) : Shown;
			File = ShownFile ? DiscFile(*ShownFile + FPartySpriteFiles::DiscFileOffset) : nullptr;
			Animations = bGear ? MovingAnimations : MemberAnimations;
			if (bGear && ShownFile)
			{
				Look.Gear = *ShownFile - FPartySpriteFiles::FirstGearFile;
			}
		}
		else if (!bPilotsDrawn && Context.PilotSpriteBase != 0 && Context.PartyActors[static_cast<std::size_t>(*Slot)] == Actor)
		{
			// Its member's pilot, drawn with the map's own small figure of the member shown.
			const std::optional<int32> Own = PilotSpriteFile(Context.PartyCharacters[static_cast<std::size_t>(*Slot)], Context.PilotSpriteBase);
			const std::optional<int32> ShownPilot = PilotSpriteFile(*Shown, Context.PilotSpriteBase);
			if (!Own || !ShownPilot || Sprite.PrimaryFile != Sprites.MapFile(*Own))
			{
				return {};
			}
			File = Sprites.MapFile(*ShownPilot);
			Place = Slots + *Slot;
			Animations = MovingAnimations;
		}
		if (File == nullptr)
		{
			return {};
		}
		// The sprite's own animations only, by the number both files give the same meaning.
		const int32 Animation = Sprite.Animation();
		if (Sprite.ScriptFile != Sprite.PrimaryFile || Animation < 0 || Animation >= Animations
			|| Animation >= Sprite.PrimaryFile->AnimationCount(Sprite.PrimaryFile->Tables()) || Animation >= File->AnimationCount(File->Tables()))
		{
			return {};
		}
		const FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
		// A drawn actor's animation runs after the drawing unless it is paused or frozen.
		const bool bAnimates = (State.Flags1 & EventActorFlags1::AnimationPaused) == 0 && (State.Flags0 & EventActorFlags0::Frozen) == 0;
		Look.StandIn = StandIns.Follow(Place, *File, Animation, Sprite, bAnimates, WholeScale, Tick, Context.Arctangents, Vram);
		if (Look.StandIn == nullptr)
		{
			return {};
		}
		return Look;
	}

	FFieldSpritePass FFieldMemberStandIns::PassFor(const FSprite& Sprite, const FSprite& StandIn, const FFieldSpritePass& Pass)
	{
		return FFieldSpriteStandIns::PassFor(Sprite, StandIn, Pass, WholeScale);
	}
}
