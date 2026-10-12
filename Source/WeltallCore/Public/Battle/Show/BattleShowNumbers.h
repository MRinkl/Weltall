#pragma once

#include "Battle/Show/BattleHudSheet.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/SpriteSystem.h"

#include <array>
#include <optional>
#include <vector>

namespace Weltall
{
	class FPsxRandom;
	struct FBattlePerformance;

	/** How a damage number moves: sliding aside, holding, fading, or (a miss) fading from the start. */
	enum class EBattleNumberPhase : uint8
	{
		Slide,
		Hold,
		Fade,
		MissFade,
	};

	class FBattleNumbers;

	/**
	 * A number popped over a combatant: its digits (and a word for EP, fuel or a miss) as HUD sheet parts
	 * in a 3D quad at the combatant's place, sliding 12 units a step to the side it faces for 8 steps,
	 * holding 16, then fading 16.
	 */
	class FBattleNumber final : public IKernelTask
	{
	public:
		void Update() override;

		bool bLive = false;
		/** Counts the numbers popped: the same for one number while it lives. */
		uint32 Serial = 0;
		/** The sprite it was popped over: a new number over the same sprite ends this one. */
		const FSprite* Over = nullptr;
		EBattleNumberPhase Phase = EBattleNumberPhase::Slide;
		/** Steps left in its phase, counted below zero. */
		int32 Count = 0;
		/** 16.16, the sprite's place when popped. */
		std::array<int32, 3> Position{};
		FGteVector Rotation{};
		std::array<int32, 3> Scale{};
		std::array<uint8, 3> Color{};
		/** The GPU code: 2Dh raw, 2Ch modulated by the colour, 2Eh semi-transparent too. */
		uint8 Code = 0;
		/** It slides right (the sprite faces mirrored), else left. */
		bool bRight = false;
		std::vector<FBattleSheetPart> Parts;

	private:
		friend class FBattleNumbers;
		FBattleNumbers* Numbers = nullptr;
	};

	/** The burst of the running total: six ghosts of its new value growing, spinning and fading for 32 steps. */
	class FBattleTotalBurst final : public IKernelTask
	{
	public:
		void Update() override;

		bool bLive = false;
		/** Counts the bursts made: the same for one burst while it lives. */
		uint32 Serial = 0;
		int32 Count = 0;
		/** Added to the rotation about z each step: ((rand() & 3) - 2) * -8, or 6 for 0. */
		int32 Spin = 0;
		FGteVector Rotation{};
		std::array<int32, 3> Scale{};
		std::array<uint8, 3> Color{};
		uint8 Code = 0;
		std::vector<FBattleSheetPart> Parts;

	private:
		friend class FBattleNumbers;
		FBattleNumbers* Numbers = nullptr;
	};

	/** The running total's panel: the combo's damage so far at (90h, 2Ah), coloured by the result's class. */
	class FBattleTotalPanel final : public IKernelTask
	{
	public:
		void Update() override {}

		bool bOpen = false;
		std::array<int16, 2> Place{};
		FGteVector Rotation{};
		std::array<int32, 3> Scale{};
		std::array<uint8, 3> Color{};
		uint8 Code = 0;
		std::vector<FBattleSheetPart> Parts;
	};

	/**
	 * The damage numbers and the running total, as kernel tasks
	 * in the sprite system's list. The show's records hand them each slot's amount, result, running total
	 * and its class; a number shown marks its amount FFFFh.
	 */
	class FBattleNumbers
	{
	public:
		static constexpr int32 NumberCapacity = 16;
		static constexpr int32 BurstCapacity = 32;

		FSpriteSystem* Sprites = nullptr;
		FPsxRandom* Random = nullptr;
		const FBattleHudSheet* Sheet = nullptr;
		/** Whether a playback is under way (numbers are popped only then). */
		bool bPlaying = false;
		/** The rules' flag of a gear's attack press taken, which a number's pop clears (none: nothing to clear). */
		bool* GearPressed = nullptr;
		/** The value the total shows next, and the class that colours it (the record's per-slot bytes). */
		uint32 RunningTotal = 0;
		uint8 TotalClass = 0;
		/** Numbers without the total (the turn's damage summary). */
		bool bWithoutTotal = false;
		/** The value the total last showed; FFFFFFFFh when it opened. */
		uint32 LastTotal = 0;
		/** A fault: more numbers or bursts at once than the pools hold, or a frame past the sheet. */
		bool bFault = false;
		/** The numbers and bursts made so far, which give each its serial. */
		uint32 Made = 0;

		std::array<FBattleNumber, NumberCapacity> Numbers{};
		std::array<FBattleTotalBurst, BurstCapacity> Bursts{};
		FBattleTotalPanel Total;

		/** Gives every task its room for parts, so that popping allocates nothing. */
		WELTALLCORE_API void Reserve();
		/** A number popped over Sprite for Amount and Result (a miss shows its word only); the gear's press taken back. */
		WELTALLCORE_API void Pop(const FSprite& Sprite, uint32 Amount, uint8 Result);
		/**
		 * Slot's number shown from the record, unless it has no result or its amount was
		 * shown; Over is the slot's sprite (none: nothing).
		 */
		WELTALLCORE_API void ShowSlot(FBattlePerformance& Record, int32 Slot, const FSprite* Over);
		/**
		 * Every slot's number, the actor's own left out once a counterattack's result has
		 * been met in the slots before it.
		 */
		WELTALLCORE_API void ShowAll(FBattlePerformance& Record, const std::array<const FSprite*, 11>& SlotSprites, const FSprite* Actor);
		/** The total's panel opened and closed. */
		WELTALLCORE_API void OpenTotal();
		WELTALLCORE_API void CloseTotal();
		/** The counted tasks still running but the total's panel. */
		WELTALLCORE_API int32 Pending() const;

		/** The total's update, which each number's step runs first: a new value bursts and rebuilds the panel. */
		void UpdateTotal();
		void End(FBattleNumber& Number);
		void End(FBattleTotalBurst& Burst);

	private:
		void Burst(uint32 Value);
		/** The value's digits as parts at the centred start, 10 apart. */
		void AddDigits(uint32 Value, int32 Y, std::vector<FBattleSheetPart>& Parts);
	};

	/**
	 * A number formatted: Value's digits (at most Width + 1 of them, the number kept below 10 ^ (Width + 1)),
	 * a '-' first when negative, without leading zeros; the count first.
	 */
	WELTALLCORE_API std::array<uint8, 8> FormatBattleNumber(uint32 Value, int32 Width);
}
