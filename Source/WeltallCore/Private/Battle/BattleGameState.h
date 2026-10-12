#pragma once

#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"

#include <cstddef>
#include <span>

namespace Weltall::BattleGameState
{
	/** The game state's bytes the battle reads and changes, by their offsets in the block. */
	inline uint8& Byte(FGameState& State, std::size_t Offset)
	{
		return State.Block()[Offset];
	}

	inline uint16 Word(const FGameState& State, std::size_t Offset)
	{
		return LittleEndian::ReadU16(State.Block(), Offset);
	}

	inline void SetWord(FGameState& State, std::size_t Offset, uint16 Value)
	{
		LittleEndian::WriteU16(State.Block(), Offset, Value);
	}

	inline std::span<const uint8> CharacterRecord(const FGameState& State, std::size_t Character)
	{
		return std::span<const uint8>(State.Block()).subspan(GameStateLayout::CharacterOffset(Character), GameStateLayout::CharacterSize);
	}

	inline std::span<const uint8> GearRecord(const FGameState& State, std::size_t Gear)
	{
		return std::span<const uint8>(State.Block()).subspan(GameStateLayout::GearOffset(Gear), GameStateLayout::GearSize);
	}

	/** A word of a character's learned skills. */
	inline std::size_t SkillsOffset(std::size_t Character, std::size_t Field)
	{
		return GameStateLayout::LearnedSkillsOffsetOf(Character) + Field;
	}

	/** Whether a character's record and learned skills lie within the game state, as the battle reads them by the id a combatant carries. */
	inline bool HasCharacter(std::size_t Character)
	{
		return GameStateLayout::CharacterOffset(Character) + GameStateLayout::CharacterSize <= FGameState::BlockSize
			&& GameStateLayout::LearnedSkillsOffsetOf(Character) + GameStateLayout::LearnedSkillsSize <= FGameState::BlockSize;
	}

	/** Billy's count of an ammunition, or none when the kind's count would lie past the game state. */
	inline uint8* AmmunitionCount(FGameState& State, uint8 Kind)
	{
		const std::size_t Offset = GameStateLayout::AmmunitionCountsOffset + Kind;
		return Offset < FGameState::BlockSize ? &State.Block()[Offset] : nullptr;
	}

	/** The story's progress: saved event variable 0. */
	inline uint16 Story(const FGameState& State)
	{
		return State.SavedEventVariable(0);
	}
}
