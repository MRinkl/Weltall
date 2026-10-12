#pragma once

#include "Battle/BattleCombatant.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/** A battle's formation: the scene and stage it is fought on, who stands where, and how it may go. */
	struct FBattleFormation
	{
		static constexpr std::size_t RecordSize = 0x20;
		static constexpr uint8 EmptyType = 0x7F;

		uint8 Scene = 0;
		/** FormationFlags bits. */
		uint8 Flags = 0;
		uint8 Stage = 0;
		/** The battle event's script, for an event battle. */
		uint8 EventScript = 0;
		/** Each party slot's group on the stage (bits 0-6). */
		std::array<uint8, 3> PartyGroups{};
		/** Each enemy slot's type in the scene (bits 0-6, EmptyType for none) and bit 7 for a gear-sized enemy. */
		std::array<uint8, 8> EnemyTypes{};
		/** Each enemy slot's bit 7: hidden until its script shows it; bit 0 kept beside it. */
		std::array<uint8, 8> EnemyFlags{};
		std::array<uint8, 8> EnemyGroups{};

		WELTALLCORE_API static FBattleFormation FromRecord(std::span<const uint8, RecordSize> Record);
	};

	namespace FormationFlags
	{
		inline constexpr uint8 NoResultScreen = 0x08;
		/** Slot 0 the party's leader, slots 1 and 2 the adult Emeralda in gear 17. */
		inline constexpr uint8 SpecialParty = 0x10;
		inline constexpr uint8 EventBattle = 0x20;
		inline constexpr uint8 NoBoarding = 0x40;
		inline constexpr uint8 NoEscape = 0x80;
	}

	/** The enemies of a battle scene: up to eight types, each a whole combatant record with its scripts, and their names. */
	class FBattleScene
	{
	public:
		static constexpr int32 TypeCount = 8;

		/** From the scene's enemy file; an error if a record or a type's script offsets lie past its end. */
		WELTALLCORE_API static TResult<FBattleScene> Parse(std::vector<uint8> File);

		/** Scene Number's enemy file, read from the disc as the battle's setup reads it. */
		WELTALLCORE_API static TResult<FBattleScene> Load(const FCdImage& Image, const FGameFileIndex& Index, int32 Number);

		const FBattleCombatant& Enemy(int32 Type) const { return Enemies[static_cast<std::size_t>(Type)]; }

		/** Where type Type's script block starts in the file, and its four scripts' offsets within the block (FFFFh none). */
		uint16 ScriptBlock(int32 Type) const { return ScriptBlocks[static_cast<std::size_t>(Type)]; }
		WELTALLCORE_API uint16 ScriptOffset(int32 Type, int32 Script) const;

		std::span<const uint8> Bytes() const { return File; }

	private:
		std::vector<uint8> File;
		std::array<FBattleCombatant, TypeCount> Enemies{};
		std::array<uint16, TypeCount> ScriptBlocks{};
	};

	/** A position on a battle stage's floor. */
	struct FBattleStagePoint
	{
		uint16 X = 0;
		uint16 Z = 0;
	};

	/**
	 * A battle stage's layout: where the party's and the enemies' members stand in each of its eight
	 * groups, on foot and in gears, the routes between the groups, and its kinds of ground. The places
	 * are read as the game reads them, so a fifth member of a group stands where the next group's data
	 * says.
	 */
	class FBattleStage
	{
	public:
		static constexpr int32 GroupCount = 8;
		static constexpr std::size_t LayoutSize = 0x344;

		/** From the stage's layout file. */
		WELTALLCORE_API static TResult<FBattleStage> Parse(std::span<const uint8> File);

		/** Stage Number's layout, read from the disc as the kernel reads it for a formation. */
		WELTALLCORE_API static TResult<FBattleStage> Load(const FCdImage& Image, const FGameFileIndex& Index, int32 Number);

		/** Where a group stands on the stage (the routes' waypoints). */
		WELTALLCORE_API FBattleStagePoint GroupPoint(int32 Group) const;
		WELTALLCORE_API FBattleStagePoint PartyPlace(int32 Group, int32 Place) const;
		WELTALLCORE_API FBattleStagePoint EnemyPlace(int32 Group, int32 Place) const;
		WELTALLCORE_API FBattleStagePoint PartyGearPlace(int32 Group) const;
		WELTALLCORE_API FBattleStagePoint EnemyGearPlace(int32 Group) const;
		/** Byte 0 of a route is a value the enemies' scripts read and set; bytes 1-7 its waypoints, FFh ending them. */
		WELTALLCORE_API uint8& Route(int32 From, int32 To, int32 Byte);
		/** The kinds of ground the stage has, which change the gears' agility; 0 for none. */
		uint8 Terrain(int32 Kind) const { return Layout[TerrainOffset + static_cast<std::size_t>(Kind)]; }
		/** The layout's bytes as the file holds them (with the routes as the enemies' scripts have changed them). */
		std::span<const uint8, LayoutSize> Bytes() const { return Layout; }

	private:
		static constexpr std::size_t TerrainOffset = 0x340;

		FBattleStagePoint Point(std::size_t Offset) const;

		std::array<uint8, LayoutSize> Layout{};
	};
}
