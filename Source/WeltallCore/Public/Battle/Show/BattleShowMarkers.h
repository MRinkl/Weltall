#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/SpriteSystem.h"

#include <array>
#include <vector>

namespace Weltall
{
	class FBattleMarkers;
	class FPsxExecutable;

	/** The model a target marker draws: the kernel's embedded pyramid, its vertices and its unlit triangles, flat or gouraud. */
	struct FBattleMarkerModel
	{
		struct FTriangle
		{
			std::array<uint16, 3> Vertices{};
			/** Each corner's colour (a flat triangle's three the same) and the GPU code, as its packet takes them. */
			std::array<std::array<uint8, 3>, 3> Colors{};
			uint8 Code = 0;
		};

		std::vector<FGteVector> Vertices;
		std::vector<FTriangle> Triangles;

		/** From the boot executable; an error if the model holds a primitive other than an unlit triangle, flat or gouraud. */
		WELTALLCORE_API static TResult<FBattleMarkerModel> Read(const FPsxExecutable& Executable);
	};

	/**
	 * A target marker, a task under its combatant's: the pyramid spinning 32
	 * units over the combatant's head, the combatant's colour pulsing while it stands.
	 */
	class FBattleMarker final : public IKernelTask
	{
	public:
		void Update() override;
		void OnKilled() override;

		bool bLive = false;
		/** Counts the markers made: the same for one marker while it lives. */
		uint32 Serial = 0;
		FSprite* Target = nullptr;
		/** Where the pyramid stands, whole units, and its turn. */
		std::array<int32, 3> Position{};
		FGteVector Rotation{};
		/** The combatant's frame's height as the marker was made, which it stands above. */
		int32 Height = 0;
		/** Steps so far, which drive the pulse. */
		int32 Count = 0;

	private:
		friend class FBattleMarkers;
		FBattleMarkers* Owner = nullptr;
	};

	/** The acting member's glow: a task under its sprite's, pulsing its red against its green and blue. */
	class FBattleActorGlow final : public IKernelTask
	{
	public:
		void Update() override;
		void OnKilled() override;

		bool bLive = false;
		FSprite* Sprite = nullptr;
		int32 Actor = 0;
		int32 Count = 0;

	private:
		friend class FBattleMarkers;
		FBattleMarkers* Owner = nullptr;
	};

	/** The target markers over the combatants of a mask, and the acting member's glow while any is shown. */
	class FBattleMarkers
	{
	public:
		static constexpr int32 SlotCount = 11;

		FSpriteSystem* Sprites = nullptr;

		/** Every marker and the glow gone, the mask cleared, for a new battle. */
		WELTALLCORE_API void Reset();
		/**
		 * The markers shown over the combatants of Slots and taken from the others, the glow on
		 * Actor while any is shown (a sprite of none: none).
		 */
		WELTALLCORE_API void Set(uint16 Slots, uint8 Actor, const std::array<FSprite*, SlotCount>& SlotSprites);
		/** The marker a task is, if it is one. */
		WELTALLCORE_API const FBattleMarker* Find(const IKernelTask* Task) const;

		uint16 Mask = 0;
		std::array<FBattleMarker, SlotCount> Markers{};
		FBattleActorGlow Glow;

	private:
		friend class FBattleMarker;
		friend class FBattleActorGlow;

		void Create(int32 Slot, FSprite& Target);
		void End(FBattleMarker& Marker);
		void StartGlow(uint8 Actor, const std::array<FSprite*, SlotCount>& SlotSprites);
		void StopGlow();

		uint32 Made = 0;
	};
}
