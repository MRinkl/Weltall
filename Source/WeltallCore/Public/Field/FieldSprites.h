#pragma once

#include "Field/EventContext.h"
#include "Field/FieldArchive.h"
#include "Field/FieldPartySprites.h"
#include "Graphics/Sprite.h"
#include "Graphics/SpriteCells.h"
#include "Graphics/SpriteFile.h"
#include "Graphics/SpriteSystem.h"

#include <array>
#include <memory>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/** Where map sprite Number takes its textures and palettes: its slot's place, its palette row from palette set PaletteSet on. */
	WELTALLCORE_API FSpriteFilePlace FieldMapSpritePlace(const FFieldSpriteSlot& Slot, int32 Number, int32 PaletteSet);

	/** Where the sprite of the party's first member takes them: a streamed area of its own. */
	WELTALLCORE_API FSpriteFilePlace FieldPartySpritePlace();

	/**
	 * The field's actors' sprites: the map's sprite files and the party's, one kernel sprite per
	 * actor that has one, and the sprite system they share. The sprites the event scripts ask for are
	 * made here as the field makes them; an ended animation marks its actor's script state.
	 */
	class FFieldSprites final : public IFieldActorModels, public ISpriteListener
	{
	public:
		static constexpr int32 PartySlots = FPartySpriteFiles::Slots;

		/**
		 * A map's sprites: its sprite files, textured at its sprite slots' places in VRAM; the party's
		 * sprite files, read by slot; the actors' script states, one per script actor; and the disc the
		 * extra animation sets are read from. The files, states and disc must outlive the sprites.
		 */
		WELTALLCORE_API FFieldSprites(std::span<const FSpriteFile> MapFiles, const std::array<FFieldSpriteSlot, FieldSpriteSlotCount>& Slots,
			const FPartySpriteFiles& PartyFiles, std::span<FEventActorState> States, const FCdImage& Image, const FGameFileIndex& Index);

		WELTALLCORE_API std::optional<FFieldSpriteResult> LoadSprite(const FFieldSpriteRequest& Request) override;
		WELTALLCORE_API FSprite* ActorSprite(int32 Actor) override;
		FSpriteSystem* SpriteSystem() override { return &Kernel; }
		WELTALLCORE_API void SwapSprites(int32 First, int32 Second) override;
		WELTALLCORE_API bool LoadExtraAnimations(int32 Actor, int32 File) override;
		WELTALLCORE_API void UseExtraAnimations(int32 Actor) override;
		void OnAnimationEvent(FSprite& Sprite) override;

		/** The actor's sprite, or null when it has none. */
		WELTALLCORE_API const FSprite* SpriteOf(int32 Actor) const;
		/** The map's sprite file in a slot, or null past them. */
		const FSpriteFile* MapFile(int32 Slot) const
		{
			return Slot >= 0 && static_cast<std::size_t>(Slot) < MapFiles.size() ? &MapFiles[static_cast<std::size_t>(Slot)] : nullptr;
		}
		/** The extra animation set the actor read last, or null. */
		const FSpriteFile* ExtraAnimationsOf(int32 Actor) const { return ActorExtraFiles[static_cast<std::size_t>(Actor)]; }
		FSpriteSystem& System() { return Kernel; }
		const FSpriteSystem& System() const { return Kernel; }

	private:
		std::span<const FSpriteFile> MapFiles;
		std::array<FFieldSpriteSlot, FieldSpriteSlotCount> Slots;
		const FPartySpriteFiles& PartyFiles;
		std::span<FEventActorState> States;
		/** One per script actor, made in place: a sprite may point at its own resource block. */
		std::vector<FSprite> Sprites;
		std::vector<uint8> HasSprite;
		/** The sprite each actor has: actors hand theirs to each other. */
		std::vector<std::size_t> SpriteIndex;
		const FCdImage& Image;
		const FGameFileIndex& FileIndex;
		/** Every extra animation set read on the map, kept for the sprites that play one an actor read again. */
		std::vector<std::unique_ptr<FSpriteFile>> ExtraFiles;
		/** The set each actor read last, null for none. */
		std::vector<const FSpriteFile*> ActorExtraFiles;
		FSpriteSystem Kernel;
	};
}
