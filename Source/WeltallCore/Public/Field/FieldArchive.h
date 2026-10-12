#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	inline constexpr int32 FieldSpriteSlotCount = 32;

	/** Where a script actor's sprite images go in VRAM; its palette row is 480 plus the slot's number. */
	struct FFieldSpriteSlot
	{
		int16 X = 0;
		int16 Y = 0;
		/** The slot's images come with the archive (section 4) rather than in the texture stream. */
		bool bImagesInArchive = false;
	};

	struct FFieldLight
	{
		/** The kernel lights along its opposite, normalized. */
		FGteVector Direction{};
		/** As stored: the GTE's colour matrix takes 8 times it (4096 is full). */
		FGteVector Color{};
	};

	/** The lights the map's lit models are built and drawn with; the kernel's third light copies the first. */
	struct FFieldLights
	{
		std::array<FFieldLight, 2> Directional{};
		/** As stored: the GTE's back colour takes its low 12 bits (255 is full). */
		FGteVector Ambient{};
	};

	/** Bits of an actor definition's flags. */
	namespace FieldActorFlag
	{
		/** 0: the definition's rotation under the camera; 1: the global model scale times the actor's own matrix; 2: the rotation scaled by the actor's scale. */
		constexpr uint16 MatrixMode = 0x0003;
		/** Shifted down by 2, the light mode the model's packets are built with; 0 keeps their colours as stored. */
		constexpr uint16 LightMode = 0x000C;
		/** The model sorts by its farthest vertex instead of its average depth. */
		constexpr uint16 SortByFarthestVertex = 0x0010;
		/** Neither drawn nor animated. */
		constexpr uint16 Hidden = 0x0020;
		/** No static model: a script actor drawn as a sprite. */
		constexpr uint16 NoModel = 0x0040;
		/** The actor walks on the walkmesh when these bits are exactly WalkingKind. */
		constexpr uint16 KindBits = 0x0F80;
		constexpr uint16 WalkingKind = 0x0200;
		/** The model has a MIMe vertex animation. */
		constexpr uint16 VertexAnimation = 0x2000;
		/** Drawn into the background ordering table, behind the rest of the scene (sky domes). */
		constexpr uint16 Background = 0x8000;
	}

	/** One of the map's actors as the archive places it: a static model, or a script actor. */
	struct FFieldActorDefinition
	{
		uint16 Flags = 0;
		/** 4096 is a turn; the matrix is Rx * Ry * Rz. */
		FGteVector Rotation{};
		FGteVector Position{};
		/** The model's number in the map's model list; meaningless without one. */
		uint16 Model = 0;

		bool HasModel() const { return (Flags & FieldActorFlag::NoModel) == 0; }
		bool IsHidden() const { return (Flags & FieldActorFlag::Hidden) != 0; }
		int32 MatrixMode() const { return Flags & FieldActorFlag::MatrixMode; }
		int32 LightMode() const { return (Flags & FieldActorFlag::LightMode) >> 2; }
		bool WalksOnWalkmesh() const { return (Flags & FieldActorFlag::KindBits) == FieldActorFlag::WalkingKind; }

		/** The kernel's draw mode for the model, as the field sets it when the map loads (FieldModelDrawMode). */
		WELTALLCORE_API int32 ModelDrawMode(bool bFog) const;
	};

	/**
	 * The kernel's draw mode for a model with these actor flags, as the field sets it after the map's
	 * init and when the scripts turn fog on: without fog 1 (relit every frame) for a light mode, else
	 * 3 or 2 (farthest-vertex sort) by the flags, else 0; with fog 5 (farthest-vertex sort) or 4, both
	 * depth-cued.
	 */
	WELTALLCORE_API int32 FieldModelDrawMode(uint16 Flags, bool bFog);

	/** The archive's sections, in their order in the header. */
	enum class EFieldSection : uint8
	{
		Tims,
		Walkmesh,
		Models,
		SpriteSets,
		SpriteImages,
		EventScripts,
		Encounters,
		Messages,
		Zones,
	};

	inline constexpr int32 FieldSectionCount = 9;

	/** A field map's first file: a header and nine LZSS-packed sections. */
	struct FFieldArchive
	{
		std::array<FFieldSpriteSlot, FieldSpriteSlotCount> SpriteSlots{};
		FFieldLights Lights;
		std::vector<FFieldActorDefinition> Actors;
		/**
		 * Each section at the size the header records for it. The field unpacks a whole stream, which
		 * can run up to 7 bytes longer, into slack nothing reads.
		 */
		std::array<std::vector<uint8>, FieldSectionCount> Sections;

		std::vector<uint8>& Section(EFieldSection Which) { return Sections[static_cast<uint8>(Which)]; }
		const std::vector<uint8>& Section(EFieldSection Which) const { return Sections[static_cast<uint8>(Which)]; }
	};

	/** Reads the archive from its file's sectors, as the game reads them. */
	WELTALLCORE_API TResult<FFieldArchive> ParseFieldArchive(std::span<const uint8> Sectors);
}
