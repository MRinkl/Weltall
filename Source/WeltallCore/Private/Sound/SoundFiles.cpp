#include "Sound/SoundFiles.h"

#include "Foundation/LittleEndian.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		constexpr std::size_t HeaderSize = 0x20;
		constexpr uint16 EffectFileVersion = 0x0101;
		constexpr int32 ReverbTypes = 10;

		constexpr uint8 End = 0x90;
		constexpr uint8 TimeSignature = 0x97;
		constexpr uint8 ContinueInEffect = 0x9E;

		/** Operand bytes of commands 80h-FFh as the driver's handlers consume them; -1: no command. */
		constexpr std::array<int8, 128> Operands{
			1, 1, -1, -1, -1, -1, -1, -1, -1, -1, 0, -1, -1, 1, 3, 0,  // 80
			0, 0, -1, -1, 1, 0, 0, 2, 1, 0, 0, -1, 3, 2, 3, -1,        // 90
			1, 1, 2, -1, 1, 1, 1, 2, -1, 1, 1, -1, 1, 1, 0, 0,         // A0
			0, 0, 0, 0, 1, 1, 0, 0, 3, -1, 0, 0, 3, 0, 0, -1,          // B0
			0, 3, 1, 1, 1, 1, 1, 2, 1, 1, 1, -1, -1, -1, -1, -1,       // C0
			1, 1, 1, 2, 2, 0, 1, 1, 3, 3, 0, 0, 0, -1, -1, -1,         // D0
			1, 1, 2, 1, 3, 3, 0, 0, 1, 1, 2, 1, 3, 3, 0, 0,            // E0
			3, 3, 2, -1, -1, 0, 1, 1, 3, 2, -1, -1, 2, 1, 1, 0,        // F0
		};

		bool HasMagic(std::span<const uint8> Bytes, const char (&Magic)[5])
		{
			return Bytes.size() >= HeaderSize && Bytes[0] == Magic[0] && Bytes[1] == Magic[1] && Bytes[2] == Magic[2] && Bytes[3] == Magic[3];
		}

		/**
		 * Whether commands read from Offset stay whole inside the file up to the end of their stream
		 * (an end, or a jump into an effect's commands), without a byte that is no command and without
		 * a time signature that would divide by zero.
		 */
		bool IsStream(std::span<const uint8> Bytes, std::size_t Offset)
		{
			while (Offset < Bytes.size())
			{
				const uint8 Code = Bytes[Offset];
				if (Code < 0x80)
				{
					if (Offset + 1 >= Bytes.size() || Bytes[Offset + 1] >= SoundFormat::NoteBytes)
					{
						return false;
					}
					const bool bExplicitLength = SoundFormat::NoteLengths[Bytes[Offset + 1] % SoundFormat::NoteLengths.size()] == 0;
					if (bExplicitLength && Offset + 2 >= Bytes.size())
					{
						return false;
					}
					Offset += bExplicitLength ? 3 : 2;
					continue;
				}
				const int32 Count = SoundFormat::CommandOperands(Code);
				if (Count < 0 || Offset + static_cast<std::size_t>(Count) >= Bytes.size())
				{
					return false;
				}
				if (Code == TimeSignature && Bytes[Offset + 2] == 0)
				{
					return false;
				}
				if (Code == End || Code == ContinueInEffect)
				{
					return true;
				}
				Offset += 1 + static_cast<std::size_t>(Count);
			}
			return false;
		}

		/** The header check's sum: the words over the checked size add up to 0 (bytes past the file read as 0). */
		bool HasValidChecksum(std::span<const uint8> Bytes)
		{
			const uint32 Checked = LittleEndian::ReadU32(Bytes, 8);
			if (Checked < HeaderSize || Checked > Bytes.size())
			{
				return false;
			}
			uint32 Sum = 0;
			for (std::size_t Word = 0; Word < (static_cast<std::size_t>(Checked) + 3) / 4; ++Word)
			{
				uint32 Value = 0;
				for (std::size_t Byte = 0; Byte < 4; ++Byte)
				{
					const std::size_t At = Word * 4 + Byte;
					Value |= static_cast<uint32>(At < Bytes.size() ? Bytes[At] : 0) << (8 * Byte);
				}
				Sum += Value;
			}
			return Sum == 0;
		}

		FError Malformed(const char* Kind, const std::string& Why)
		{
			return MakeError(std::string(Kind) + " file: " + Why);
		}
	}

	int32 SoundFormat::CommandOperands(uint8 Code)
	{
		return Code < 0x80 ? -1 : Operands[Code - 0x80];
	}

	TResult<FSoundBankFile> FSoundBankFile::Parse(std::vector<uint8> File)
	{
		if (!HasMagic(File, "wds ") || File.size() < 0x30)
		{
			return Malformed("wds", "not an instrument bank");
		}
		const uint32 Header = LittleEndian::ReadU32(File, 0x10);
		const uint32 SampleSize = LittleEndian::ReadU32(File, 0x14);
		const uint32 SampleOffset = LittleEndian::ReadU32(File, 0x18);
		if (Header < 0x30 || Header > File.size())
		{
			return Malformed("wds", "the header runs past the file");
		}
		if (static_cast<uint64>(SampleOffset) + SampleSize > File.size())
		{
			return Malformed("wds", "the sample data runs past the file");
		}
		FSoundBankFile Bank;
		// The driver copies the whole header and indexes its instruments by number, so every whole
		// entry the header holds is an instrument, whatever its instrument count says.
		const std::size_t Entries = std::min<std::size_t>((Header - 0x30) / 16, InstrumentSlots);
		for (std::size_t Entry = 0; Entry < Entries; ++Entry)
		{
			const std::size_t At = 0x30 + Entry * 16;
			Bank.Table.push_back({LittleEndian::ReadU32(File, At), LittleEndian::ReadU16(File, At + 4), LittleEndian::ReadS16(File, At + 6),
				LittleEndian::ReadU32(File, At + 8), LittleEndian::ReadU16(File, At + 12)});
		}
		Bank.BankId = LittleEndian::ReadU16(File, 0x20);
		Bank.Address = LittleEndian::ReadU32(File, 0x28);
		Bank.SampleOffset = SampleOffset;
		Bank.SampleSize = SampleSize;
		Bank.Bytes = std::move(File);
		return Bank;
	}

	std::span<const uint8> FSoundBankFile::Samples() const
	{
		return std::span<const uint8>(Bytes).subspan(SampleOffset, SampleSize);
	}

	TResult<FSoundSequenceFile> FSoundSequenceFile::Parse(std::vector<uint8> File)
	{
		if (!HasMagic(File, "smds") || File.size() < 0x22)
		{
			return Malformed("smds", "not a sequence");
		}
		const int32 Tracks = File[0x14];
		const int32 Percussion = File[0x15];
		if (Tracks == 0 || Tracks > MaxTracks)
		{
			return Malformed("smds", std::to_string(Tracks) + " tracks");
		}
		if (0x22 + static_cast<std::size_t>(Tracks) * 2 > File.size())
		{
			return Malformed("smds", "the track table runs past the file");
		}
		if (File[0x1A] >= ReverbTypes)
		{
			return Malformed("smds", "reverb type " + std::to_string(File[0x1A]));
		}
		FSoundSequenceFile Sequence;
		for (int32 Track = 0; Track < Tracks; ++Track)
		{
			const uint16 Offset = LittleEndian::ReadU16(File, 0x22 + static_cast<std::size_t>(Track) * 2);
			if (Offset != 0 && !IsStream(File, Offset))
			{
				return Malformed("smds", "track " + std::to_string(Track) + " is not a whole command stream");
			}
			Sequence.TrackOffsets.push_back(Offset);
		}
		const std::size_t Table = LittleEndian::ReadU16(File, 0x20);
		if (Percussion != 0 && Table + static_cast<std::size_t>(Percussion) * 5 > File.size())
		{
			return Malformed("smds", "the percussion table runs past the file");
		}
		for (int32 Entry = 0; Entry < Percussion; ++Entry)
		{
			const std::size_t At = Table + static_cast<std::size_t>(Entry) * 5;
			if (File[At] >= PercussionKeys)
			{
				return Malformed("smds", "percussion key " + std::to_string(File[At]));
			}
			Sequence.PercussionTable[File[At]] = {File[At + 1], File[At + 2], File[At + 3], File[At + 4]};
		}
		Sequence.bPercussion = Percussion != 0;
		Sequence.SequenceId = LittleEndian::ReadU16(File, 0x10);
		Sequence.Bank = LittleEndian::ReadU16(File, 0x16);
		Sequence.HeaderVolume = LittleEndian::ReadU16(File, 0x18);
		Sequence.Data = std::move(File);
		return Sequence;
	}

	TResult<FSoundEffectFile> FSoundEffectFile::Parse(std::vector<uint8> File)
	{
		if (!HasMagic(File, "seds"))
		{
			return Malformed("seds", "not an effect file");
		}
		if (!HasValidChecksum(File))
		{
			return Malformed("seds", "the checksum does not hold");
		}
		if (LittleEndian::ReadU16(File, 0x0C) != EffectFileVersion)
		{
			return Malformed("seds", "an unknown version");
		}
		const std::size_t Count = LittleEndian::ReadU16(File, 0x12);
		const std::size_t VolumeTable = LittleEndian::ReadU16(File, 0x18);
		if (0x20 + Count * 4 > File.size() || VolumeTable + Count > File.size())
		{
			return Malformed("seds", "the effect tables run past the file");
		}
		FSoundEffectFile Effects;
		for (std::size_t Index = 0; Index < Count * 2; ++Index)
		{
			const uint16 Offset = LittleEndian::ReadU16(File, 0x20 + Index * 2);
			if (Offset != 0 && !IsStream(File, Offset))
			{
				return Malformed("seds", "channel " + std::to_string(Index) + " is not a whole command stream");
			}
			Effects.Channels.push_back(Offset);
			Effects.Continuable.push_back(Offset != 0 && IsStream(File, static_cast<std::size_t>(Offset) + 3));
		}
		Effects.Volumes.assign(File.begin() + static_cast<std::ptrdiff_t>(VolumeTable), File.begin() + static_cast<std::ptrdiff_t>(VolumeTable + Count));
		Effects.Id = LittleEndian::ReadU16(File, 0x14);
		Effects.Bank = LittleEndian::ReadU16(File, 0x16);
		Effects.FileFlags = LittleEndian::ReadU16(File, 0x10);
		Effects.Data = std::move(File);
		return Effects;
	}
}
