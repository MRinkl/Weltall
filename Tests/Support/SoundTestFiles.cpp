#include "Support/SoundTestFiles.h"

#include "Foundation/LittleEndian.h"
#include "Support/ByteWriter.h"

#include <cmath>
#include <cstring>

namespace Weltall::Testing
{
	namespace
	{
		constexpr std::array<uint32, FSoundDriverTables::ReverbTypes> ReverbAreaSizes{0x80, 0x26C0, 0x1F40, 0x4840, 0x6FE0, 0xADE0, 0xF6C0, 0x18040, 0x18040, 0x3C00};

		void Put16(std::vector<uint8>& Bytes, std::size_t Offset, uint16 Value)
		{
			LittleEndian::WriteU16(Bytes, Offset, Value);
		}
	}

	FSoundDriverTables MakeSoundDriverTables()
	{
		FSoundDriverTables Tables;
		for (std::size_t Index = 0; Index < Tables.Pitches.size(); ++Index)
		{
			Tables.Pitches[Index] = static_cast<int16>(std::lround(8192.0 * std::exp2(static_cast<double>(Index) / 3072.0)));
		}
		// Keys 0-116 as octave and note; then three pad bytes and the pitch table's first bytes, as in RAM.
		constexpr int32 Keys = 117;
		for (int32 Key = 0; Key < Keys; ++Key)
		{
			Tables.Octaves[static_cast<std::size_t>(Key)] = static_cast<uint8>((Key / 12) << 4 | Key % 12);
		}
		for (std::size_t Index = Keys + 3; Index < Tables.Octaves.size(); ++Index)
		{
			const uint16 Pitch = static_cast<uint16>(Tables.Pitches[(Index - Keys - 3) / 2]);
			Tables.Octaves[Index] = static_cast<uint8>((Index - Keys - 3) % 2 == 0 ? Pitch : Pitch >> 8);
		}
		for (int32 Code = 0x80; Code < 0x100; ++Code)
		{
			const int32 Operands = SoundFormat::CommandOperands(static_cast<uint8>(Code));
			Tables.CommandLengths[static_cast<std::size_t>(Code - 0x80)] = static_cast<uint8>(Operands < 0 ? 0 : Operands + 1);
		}
		// The driver's length table counts one byte more than two handlers take.
		Tables.CommandLengths[0x9D - 0x80] = 4;
		Tables.CommandLengths[0xF5 - 0x80] = 2;
		Tables.ReverbAreaSizes = ReverbAreaSizes;
		Tables.SilentBlock.fill(0x07);
		return Tables;
	}

	std::vector<uint8> MakeSequenceFile(const std::vector<std::vector<uint8>>& Tracks, const std::vector<uint8>& Percussion)
	{
		std::vector<uint8> Bytes(0x2A + 2 * Tracks.size());
		std::memcpy(Bytes.data(), "smds", 4);
		Put16(Bytes, 0x12, 0x0102);
		Bytes[0x14] = static_cast<uint8>(Tracks.size());
		Bytes[0x15] = static_cast<uint8>(Percussion.size() / 5);
		Put16(Bytes, 0x18, 100);
		Bytes[0x1A] = 4;
		Bytes[0x1B] = 0x20;
		Put16(Bytes, 0x20, static_cast<uint16>(Bytes.size()));
		Bytes.insert(Bytes.end(), Percussion.begin(), Percussion.end());
		for (std::size_t Track = 0; Track < Tracks.size(); ++Track)
		{
			if (!Tracks[Track].empty())
			{
				Put16(Bytes, 0x22 + 2 * Track, static_cast<uint16>(Bytes.size()));
				Bytes.insert(Bytes.end(), Tracks[Track].begin(), Tracks[Track].end());
			}
		}
		LittleEndian::WriteU32(Bytes, 8, static_cast<uint32>(Bytes.size()));
		Put16(Bytes, 0x0C, 0x0101);
		return Bytes;
	}

	std::vector<uint8> MakeEffectFile(uint16 FileId, uint16 BankId, const std::vector<std::pair<std::vector<uint8>, std::vector<uint8>>>& Effects)
	{
		const std::size_t VolumeTable = 0x20 + Effects.size() * 4;
		std::vector<uint8> Bytes(VolumeTable + Effects.size(), 0x7F);
		std::fill(Bytes.begin(), Bytes.begin() + static_cast<std::ptrdiff_t>(VolumeTable), 0);
		std::memcpy(Bytes.data(), "seds", 4);
		Put16(Bytes, 0x0C, 0x0101);
		Put16(Bytes, 0x12, static_cast<uint16>(Effects.size()));
		Put16(Bytes, 0x14, FileId);
		Put16(Bytes, 0x16, BankId);
		Put16(Bytes, 0x18, static_cast<uint16>(VolumeTable));
		for (std::size_t Effect = 0; Effect < Effects.size(); ++Effect)
		{
			const std::vector<uint8>* Channels[2] = {&Effects[Effect].first, &Effects[Effect].second};
			for (std::size_t Channel = 0; Channel < 2; ++Channel)
			{
				if (!Channels[Channel]->empty())
				{
					Put16(Bytes, 0x20 + (Effect * 2 + Channel) * 2, static_cast<uint16>(Bytes.size()));
					Bytes.insert(Bytes.end(), Channels[Channel]->begin(), Channels[Channel]->end());
				}
			}
		}
		Bytes.resize((Bytes.size() + 3) & ~std::size_t{3});
		LittleEndian::WriteU32(Bytes, 8, static_cast<uint32>(Bytes.size()));
		uint32 Sum = 0;
		for (std::size_t Word = 0; Word < Bytes.size(); Word += 4)
		{
			Sum += LittleEndian::ReadU32(Bytes, Word);
		}
		LittleEndian::WriteU32(Bytes, 4, 0u - Sum);
		return Bytes;
	}

	std::vector<uint8> MakeBankFile(uint16 BankId, uint32 Address, const std::vector<FSoundInstrument>& Instruments, const std::vector<uint8>& Samples)
	{
		FByteWriter Writer;
		const uint32 Header = static_cast<uint32>(0x30 + Instruments.size() * 16);
		Writer.U8s({'w', 'd', 's', ' '}).U32(0).U32(Header).U16(0x0101).U16(0);
		Writer.U32(Header).U32(static_cast<uint32>(Samples.size())).U32(Header).U16(static_cast<uint16>(Instruments.size() - 1)).U16(0);
		Writer.U16(BankId).U16(0).U32(0).U32(Address).U32(0);
		for (const FSoundInstrument& Instrument : Instruments)
		{
			Writer.U32(Instrument.SampleStart).U16(Instrument.LoopStart).S16(Instrument.Tuning).U32(Instrument.Rates).U16(Instrument.Modes).U16(0x7F);
		}
		Writer.Bytes.insert(Writer.Bytes.end(), Samples.begin(), Samples.end());
		return Writer.Bytes;
	}

	std::vector<uint8> HexBytes(const char* Text)
	{
		std::vector<uint8> Bytes;
		int32 High = -1;
		for (const char* Character = Text; *Character != '\0'; ++Character)
		{
			const char C = *Character;
			const int32 Digit = C >= '0' && C <= '9' ? C - '0' : (C >= 'a' && C <= 'f' ? C - 'a' + 10 : (C >= 'A' && C <= 'F' ? C - 'A' + 10 : -1));
			if (Digit < 0)
			{
				continue;
			}
			if (High < 0)
			{
				High = Digit;
			}
			else
			{
				Bytes.push_back(static_cast<uint8>(High << 4 | Digit));
				High = -1;
			}
		}
		return Bytes;
	}
}
