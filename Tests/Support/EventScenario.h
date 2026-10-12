#pragma once

#include "Field/FieldActorSounds.h"
#include "Field/FieldMap.h"
#include "Foundation/Types.h"
#include "Support/EventScenarioProgram.h"
#include "Support/EventStateLayout.h"

#include <array>
#include <vector>

namespace Weltall::Testing
{
	/**
	 * A random map for the event machine: a script section whose entry points all land in one random
	 * program of the implemented opcodes, with random script states, variables, zones, walkmesh
	 * attributes, party, pad and random seed. It is drawn from XorShift32 exactly as the oracle
	 * that ran the original interpreter on the same scenarios draws it, operand for operand.
	 */
	struct FEventScenario
	{
		std::vector<uint8> Section;
		/** Each script actor's record as the original lays it out (0x5C bytes). */
		std::vector<std::array<uint8, 0x5C>> Records;
		std::vector<std::array<uint8, EventStateBytes>> States;
		std::array<uint16, 0x400> Variables{};
		std::vector<FFieldZone> Zones;
		/** Two walking layers of a grid of triangles, then the camera's. */
		FFieldWalkmesh Walkmesh;
		std::array<uint8, 3> PartyActors{};
		std::array<uint8, 3> PartyCharacters{};
		uint8 Leader = 0;
		uint16 PadHeld = 0;
		uint16 PadLatched = 0;
		uint32 RandomSeed = 0;
		bool bMovieOverField = false;
		bool bPartyWalking = false;
		int32 ClockTicks = 0;
		uint8 ClockMode = 0;
		/** The game state block. */
		std::vector<uint8> GameState;
		int32 MapNumber = 0;
		/** The actors reset and placed on the floor before the map's init. */
		std::vector<int32> Resets;
		/** Each actor has a sprite of its own (the moves' scenarios), whose motion the digest reads. */
		bool bStandInSprites = false;
		/** Each stand-in sprite's gravity, which bends a jump. */
		std::vector<int32> StandInGravity;
		/** A song has loaded and plays until the music stops, so the scripts' changes to it apply. */
		bool bMusicLoaded = false;
		/** Where the camera's eye and target are (16.16). */
		std::array<int32, 3> CameraEye{};
		std::array<int32, 3> CameraTarget{};
		/** The positional sounds' channels as the actors hold them, and the effect player's tracks playing. */
		std::array<FFieldActorSoundChannel, FFieldActorSounds::ChannelCount> ActorSoundChannels{};
		uint32 PlayingEffects = 0;
	};

	FEventScenario GenerateEventScenario(uint32 Seed, EEventScenarioFamily Family = EEventScenarioFamily::Machine);
}
