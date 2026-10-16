#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>
#include <memory>
#include <span>
#include <vector>

namespace Weltall
{
	struct FWave;

	/**
	 * The files of the game's sound driver, checked as far as the driver needs to read them safely.
	 * All three start with a four-letter magic; sequences and effects are command streams in which a
	 * byte below 80h starts a note (velocity, then a note byte carrying the key and the length, then
	 * an explicit length when the note byte has none) and 80h-FFh are commands with fixed operands.
	 */
	namespace SoundFormat
	{
		/** A whole note lasts 192 ticks; a note byte is note (0-11) * 19 + length index (0: explicit). */
		inline constexpr std::array<uint8, 19> NoteLengths{0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2};
		inline constexpr int32 NoteBytes = 12 * 19;

		/** The operand bytes command Code (80h-FFh) takes, or -1 for the bytes that are no command. */
		WELTALLCORE_API int32 CommandOperands(uint8 Code);
	}

	/** An instrument of a bank as the file stores it. */
	struct FSoundInstrument
	{
		/** In 8-byte units from the bank's sample data. */
		uint32 SampleStart = 0;
		/** In 8-byte units from the sample start. */
		uint16 LoopStart = 0;
		/** Added to every note's pitch, in 1/256 semitone. */
		int16 Tuning = 0;
		/** Attack rate (bits 0-6), decay rate (8-11), sustain level (12-15), sustain rate (16-22), release rate (24-28). */
		uint32 Rates = 0;
		/** Attack (bits 0-2), sustain (4-6) and release (8-10) modes: 1, 3, 5, 7 for linear up, linear down, exponential up and down. */
		uint16 Modes = 0;
	};

	/** A `wds` file: a bank of instruments and the ADPCM sample data they play. */
	class FSoundBankFile
	{
	public:
		/** Instrument numbers are bytes; numbers past the bank's table read as empty instruments. */
		static constexpr std::size_t InstrumentSlots = 256;

		WELTALLCORE_API static TResult<FSoundBankFile> Parse(std::vector<uint8> File);

		uint16 Id() const { return BankId; }
		/** Where in sound RAM the bank asks to be put; 0 for anywhere. */
		uint32 RequestedAddress() const { return Address; }
		std::span<const FSoundInstrument> Instruments() const { return Table; }
		WELTALLCORE_API std::span<const uint8> Samples() const;

	private:
		std::vector<uint8> Bytes;
		std::vector<FSoundInstrument> Table;
		uint16 BankId = 0;
		uint32 Address = 0;
		std::size_t SampleOffset = 0;
		std::size_t SampleSize = 0;
	};

	/** What a percussion key plays: an instrument at a key, panned. */
	struct FSoundPercussion
	{
		uint8 Instrument = 0;
		uint8 Key = 0;
		uint8 Unused = 0;
		uint8 Pan = 0;
	};

	/** An `smds` file: a sequence of up to 32 tracks, track 0 conducting and track n playing on voice n - 1. */
	class FSoundSequenceFile
	{
	public:
		static constexpr int32 MaxTracks = 32;
		static constexpr int32 PercussionKeys = 96;

		WELTALLCORE_API static TResult<FSoundSequenceFile> Parse(std::vector<uint8> File);

		std::span<const uint8> Bytes() const { return Data; }
		uint16 Id() const { return SequenceId; }
		uint16 BankId() const { return Bank; }
		uint16 Volume() const { return HeaderVolume; }
		int32 TrackCount() const { return static_cast<int32>(TrackOffsets.size()); }
		/** Where track Track's commands start; 0 for a track without any. */
		uint16 TrackOffset(int32 Track) const { return TrackOffsets[static_cast<std::size_t>(Track)]; }
		bool HasPercussion() const { return bPercussion; }
		const FSoundPercussion& Percussion(int32 Key) const { return PercussionTable[static_cast<std::size_t>(Key)]; }
		/** The reverb the sequence asks for: its type (0-9), depth, delay and feedback. */
		uint8 ReverbType() const { return Data[0x1A]; }
		uint8 ReverbDepth() const { return Data[0x1B]; }
		uint8 ReverbDelay() const { return Data[0x1C]; }
		uint8 ReverbFeedback() const { return Data[0x1D]; }
		/** Optional mixed music from the player's assets folder; the native sequence still drives timing. */
		std::shared_ptr<const FWave> Replacement;

	private:
		std::vector<uint8> Data;
		std::vector<uint16> TrackOffsets;
		std::array<FSoundPercussion, PercussionKeys> PercussionTable{};
		uint16 SequenceId = 0;
		uint16 Bank = 0;
		uint16 HeaderVolume = 0;
		bool bPercussion = false;
	};

	/** A `seds` file: sound effects, each a pair of command streams (its two channels) and a volume. */
	class FSoundEffectFile
	{
	public:
		WELTALLCORE_API static TResult<FSoundEffectFile> Parse(std::vector<uint8> File);

		std::span<const uint8> Bytes() const { return Data; }
		/** Effect ids are (file id << 16) | effect number. */
		uint16 FileId() const { return Id; }
		uint16 BankId() const { return Bank; }
		/** Bit 0 marks the effects' tracks as the file's (reverb follows the driver's setting for them). */
		uint16 Flags() const { return FileFlags; }
		int32 EffectCount() const { return static_cast<int32>(Volumes.size()); }
		uint8 Volume(int32 Effect) const { return Volumes[static_cast<std::size_t>(Effect)]; }
		/** Where channel Index (effect * 2 + channel) starts; 0 for a silent channel. */
		uint16 ChannelOffset(int32 Index) const { return Channels[static_cast<std::size_t>(Index)]; }
		/** Whether commands can go on three bytes into channel Index, as a jump from another track does. */
		bool CanContinue(int32 Index) const { return Continuable[static_cast<std::size_t>(Index)]; }

	private:
		std::vector<uint8> Data;
		std::vector<uint8> Volumes;
		std::vector<uint16> Channels;
		std::vector<bool> Continuable;
		uint16 Id = 0;
		uint16 Bank = 0;
		uint16 FileFlags = 0;
	};
}
