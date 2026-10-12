#pragma once

#include "Sound/SoundDriverTables.h"
#include "Sound/SoundFiles.h"

#include <utility>
#include <vector>

namespace Weltall::Testing
{
	/**
	 * The sound driver's tables as the executable holds them, built from their definitions: the pitch
	 * table is 8192 * 2^(n / 3072) rounded (a content test checks the executable's against it), the
	 * reverb presets are left empty.
	 */
	FSoundDriverTables MakeSoundDriverTables();

	/** A sequence file with the given tracks' commands, laid out as the oracle's crafted one. */
	std::vector<uint8> MakeSequenceFile(const std::vector<std::vector<uint8>>& Tracks, const std::vector<uint8>& Percussion = {});

	/** An effect file (checksum included) whose effects are pairs of channel commands; empty channels are silent. */
	std::vector<uint8> MakeEffectFile(uint16 FileId, uint16 BankId, const std::vector<std::pair<std::vector<uint8>, std::vector<uint8>>>& Effects);

	/** A bank of the given instruments over the given sample data. */
	std::vector<uint8> MakeBankFile(uint16 BankId, uint32 Address, const std::vector<FSoundInstrument>& Instruments, const std::vector<uint8>& Samples);

	/** Bytes from a string of hex digits, spaces ignored. */
	std::vector<uint8> HexBytes(const char* Text);
}
