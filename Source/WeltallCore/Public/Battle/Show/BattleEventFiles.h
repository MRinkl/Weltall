#pragma once

#include "Battle/BattleProgram.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/**
	 * What a battle event's show reads from the disc beside its scripts: its objects' sprite data (the event
	 * program's directory: an archive of packed entries), its own sound effects (a `seds`), the portraits its
	 * messages show (images of the field's directory, two a portrait by the event program's table: facing right
	 * of the window and facing left) and the voices (the party's directory: a set's sound effects and its banks,
	 * by the Battle program's table).
	 */
	class FBattleEventFiles
	{
	public:
		static constexpr int32 PortraitCount = 90;

		/** Reads the objects' archive, the sound effects and the event program's portrait table; Voices is the Battle program's table. */
		WELTALLCORE_API static TResult<FBattleEventFiles> Load(const FCdImage& Image, const FGameFileIndex& Index,
			const std::array<FBattleProgramTables::FVoiceSet, FBattleProgramTables::VoiceSetCount>& Voices);
		/** Selects compatible USA or undub voice files, leaving event text and portraits on their gameplay source. */
		void SetVoiceFiles(const FCdImage& InImage, const FGameFileIndex& InIndex) { VoiceImage = &InImage; VoiceIndex = &InIndex; }

		int32 ObjectCount() const { return static_cast<int32>(ObjectOffsets.size()); }
		/** An object's sprite data, unpacked afresh as each load of the object unpacks it. */
		WELTALLCORE_API TResult<std::vector<uint8>> UnpackObject(int32 Entry) const;
		/** The event's own sound effects. */
		std::span<const uint8> Sounds() const { return SoundEffects; }
		/** A portrait's image (a TIM): the one drawn at the window's left, or at its right. */
		WELTALLCORE_API TResult<std::vector<uint8>> ReadPortrait(int32 Portrait, bool bAtRight) const;
		/** A voice set's sound effects, the bank of voice Number of the set, and the effect that voice plays. */
		WELTALLCORE_API TResult<std::vector<uint8>> ReadVoiceSounds(int32 Set) const;
		WELTALLCORE_API TResult<std::vector<uint8>> ReadVoiceBank(int32 Set, int32 Number) const;
		int32 VoiceEffect(int32 Set, int32 Number) const { return VoiceSets[static_cast<std::size_t>(Set)].FirstEffect + Number; }
		static bool IsVoiceSet(int32 Set) { return Set >= 0 && Set < FBattleProgramTables::VoiceSetCount; }

	private:
		const FCdImage* Image = nullptr;
		const FGameFileIndex* Index = nullptr;
		const FCdImage* VoiceImage = nullptr;
		const FGameFileIndex* VoiceIndex = nullptr;
		/** The archive as the disc holds it, padded for the unpacker's reads past its last entry, and each entry's place in it. */
		std::vector<uint8> ObjectArchive;
		std::vector<uint32> ObjectOffsets;
		std::vector<uint8> SoundEffects;
		std::array<std::array<uint8, 2>, PortraitCount> PortraitFiles{};
		std::array<FBattleProgramTables::FVoiceSet, FBattleProgramTables::VoiceSetCount> VoiceSets{};
	};
}
