#include "Media/MemoryCard.h"

#include "Foundation/LittleEndian.h"

#include <algorithm>
#include <string_view>

namespace Weltall
{
	namespace
	{
		constexpr std::size_t DexDriveHeader = 0xF40;
		constexpr std::size_t VirtualGameStationHeader = 0x40;
		constexpr std::size_t PsvHeader = 0x100;
		constexpr std::size_t XploderHeader = 0x36;
		constexpr std::size_t XploderNameBytes = 0x15;
		constexpr uint32 FirstBlockInUse = 0x51;
		constexpr uint16 LastBlock = 0xFFFF;
		constexpr std::size_t NameOffset = 0x0A;
		constexpr std::size_t NameBytes = 20;
		constexpr int32 DirectoryEntries = 15;
		constexpr uint32 MiddleBlockInUse = 0x52;
		constexpr uint32 LastBlockInUse = 0x53;
		constexpr uint32 FreeBlock = 0xA0;
		constexpr std::size_t LinkOffset = 8;
		/** The first block's frames: the header, the directory, the broken sectors' list, their replacements and unused ones, and the write test's copy of the header. */
		constexpr int32 HeaderFrame = 0;
		constexpr int32 FirstBrokenSectorFrame = 16;
		constexpr int32 FirstUnusedFrame = 36;
		constexpr int32 WriteTestFrame = 63;
		constexpr uint32 NoBrokenSector = 0xFFFFFFFF;
		constexpr uint8 Unused = 0xFF;

		bool StartsWith(std::span<const uint8> Bytes, std::string_view Text)
		{
			return Bytes.size() >= Text.size() && std::equal(Text.begin(), Text.end(), Bytes.begin(),
				[](char Expected, uint8 Byte) { return static_cast<uint8>(Expected) == Byte; });
		}

		std::string ReadName(std::span<const uint8> Bytes)
		{
			std::string Name;
			for (const uint8 Byte : Bytes)
			{
				if (Byte == 0)
				{
					break;
				}
				Name.push_back(static_cast<char>(Byte));
			}
			return Name;
		}

		/** A save's blocks begin with its title frame, "SC". */
		bool IsSaveData(std::span<const uint8> Blocks)
		{
			return Blocks.size() >= MemoryCard::BlockSize && StartsWith(Blocks, "SC");
		}

		/** Every save a card holds, its blocks followed through the directory's links. */
		std::vector<FCardSave> ReadCard(std::span<const uint8> Card)
		{
			std::vector<FCardSave> Saves;
			const std::size_t Blocks = Card.size() / MemoryCard::BlockSize;
			for (int32 Slot = 1; Slot <= DirectoryEntries; ++Slot)
			{
				const std::span<const uint8> Entry = Card.subspan(static_cast<std::size_t>(Slot) * MemoryCard::FrameSize, MemoryCard::FrameSize);
				if (LittleEndian::ReadU32(Entry, 0) != FirstBlockInUse)
				{
					continue;
				}
				const uint32 Size = LittleEndian::ReadU32(Entry, 4);
				FCardSave Save;
				Save.FileName = ReadName(Entry.subspan(NameOffset, NameBytes));
				std::vector<int32> Chain;
				int32 Block = Slot;
				while (Block >= 1 && Block <= DirectoryEntries && Chain.size() * MemoryCard::BlockSize < Size
					&& std::find(Chain.begin(), Chain.end(), Block) == Chain.end() && static_cast<std::size_t>(Block) < Blocks)
				{
					Chain.push_back(Block);
					const uint16 Next = LittleEndian::ReadU16(Card, static_cast<std::size_t>(Block) * MemoryCard::FrameSize + 8);
					Block = Next == LastBlock ? 0 : Next + 1;
				}
				for (const int32 Used : Chain)
				{
					const std::span<const uint8> Data = Card.subspan(static_cast<std::size_t>(Used) * MemoryCard::BlockSize, MemoryCard::BlockSize);
					Save.Blocks.insert(Save.Blocks.end(), Data.begin(), Data.end());
				}
				if (!Save.Blocks.empty())
				{
					Saves.push_back(std::move(Save));
				}
			}
			return Saves;
		}

		/** A whole card after a header of HeaderBytes, possibly without its trailing unused blocks. */
		bool IsCardAt(std::span<const uint8> File, std::size_t HeaderBytes)
		{
			if (File.size() < HeaderBytes + MemoryCard::BlockSize || File.size() > HeaderBytes + MemoryCard::CardSize)
			{
				return false;
			}
			const std::size_t CardBytes = File.size() - HeaderBytes;
			return CardBytes % MemoryCard::BlockSize == 0 && StartsWith(File.subspan(HeaderBytes), "MC");
		}
	}

	TResult<std::vector<FCardSave>> ReadMemoryCardImage(std::span<const uint8> File)
	{
		if (IsCardAt(File, 0))
		{
			return ReadCard(File);
		}
		// Some DexDrive images have their header zeroed, signature included: a whole card behind it
		// still tells them apart.
		if ((StartsWith(File, "123-456-STD") || File.size() == DexDriveHeader + MemoryCard::CardSize) && IsCardAt(File, DexDriveHeader))
		{
			return ReadCard(File.subspan(DexDriveHeader));
		}
		if (StartsWith(File, "VgsM") && IsCardAt(File, VirtualGameStationHeader))
		{
			return ReadCard(File.subspan(VirtualGameStationHeader));
		}
		if (StartsWith(File, "PSV") && IsCardAt(File, PsvHeader))
		{
			return ReadCard(File.subspan(PsvHeader));
		}
		// One save: MCS files open with the save's directory frame, Xploder and Action Replay files
		// with its name and title, then the blocks.
		if (File.size() > MemoryCard::FrameSize && LittleEndian::ReadU32(File, 0) == FirstBlockInUse
			&& IsSaveData(File.subspan(MemoryCard::FrameSize)))
		{
			FCardSave Save;
			Save.FileName = ReadName(File.subspan(NameOffset, NameBytes));
			Save.Blocks.assign(File.begin() + MemoryCard::FrameSize, File.end());
			return std::vector<FCardSave>{std::move(Save)};
		}
		if (File.size() > XploderHeader && IsSaveData(File.subspan(XploderHeader)))
		{
			FCardSave Save;
			Save.FileName = ReadName(File.subspan(0, XploderNameBytes));
			Save.Blocks.assign(File.begin() + XploderHeader, File.end());
			return std::vector<FCardSave>{std::move(Save)};
		}
		return MakeError("not a memory card image");
	}

	TResult<std::vector<uint8>> WriteMemoryCardImage(std::span<const FCardSave> Saves)
	{
		std::vector<uint8> Card(MemoryCard::CardSize, 0);
		const auto Frame = [&Card](int32 Number) { return std::span<uint8>(Card).subspan(static_cast<std::size_t>(Number) * MemoryCard::FrameSize, MemoryCard::FrameSize); };
		// The first block as formatting leaves it: the header, a free directory, no broken sectors, the unused frames and the header again.
		Frame(HeaderFrame)[0] = 'M';
		Frame(HeaderFrame)[1] = 'C';
		for (int32 Entry = 1; Entry <= DirectoryEntries; ++Entry)
		{
			LittleEndian::WriteU32(Frame(Entry), 0, FreeBlock);
			LittleEndian::WriteU16(Frame(Entry), LinkOffset, LastBlock);
		}
		for (int32 Broken = FirstBrokenSectorFrame; Broken < FirstUnusedFrame; ++Broken)
		{
			LittleEndian::WriteU32(Frame(Broken), 0, NoBrokenSector);
			LittleEndian::WriteU16(Frame(Broken), LinkOffset, LastBlock);
		}
		std::fill(Frame(FirstUnusedFrame).begin(), Frame(WriteTestFrame).begin(), Unused);
		Frame(WriteTestFrame)[0] = 'M';
		Frame(WriteTestFrame)[1] = 'C';

		int32 Block = 1;
		for (const FCardSave& Save : Saves)
		{
			const int32 Count = static_cast<int32>(Save.Blocks.size() / MemoryCard::BlockSize);
			if (Save.Blocks.empty() || Save.Blocks.size() % MemoryCard::BlockSize != 0)
			{
				return MakeError("the save " + Save.FileName + " is not whole blocks");
			}
			if (Save.FileName.size() > NameBytes)
			{
				return MakeError("the save's name " + Save.FileName + " is too long for a memory card");
			}
			if (Block + Count - 1 > DirectoryEntries)
			{
				return MakeError("a memory card cannot hold these saves");
			}
			for (int32 Part = 0; Part < Count; ++Part, ++Block)
			{
				const bool bLast = Part + 1 == Count;
				const std::span<uint8> Entry = Frame(Block);
				LittleEndian::WriteU32(Entry, 0, Part == 0 ? FirstBlockInUse : bLast ? LastBlockInUse : MiddleBlockInUse);
				LittleEndian::WriteU32(Entry, 4, Part == 0 ? static_cast<uint32>(Save.Blocks.size()) : 0);
				// A link names the block after this one by its number from 0.
				LittleEndian::WriteU16(Entry, LinkOffset, bLast ? LastBlock : static_cast<uint16>(Block));
				if (Part == 0)
				{
					std::copy(Save.FileName.begin(), Save.FileName.end(), Entry.begin() + NameOffset);
				}
				std::copy_n(Save.Blocks.begin() + static_cast<std::ptrdiff_t>(Part * MemoryCard::BlockSize), MemoryCard::BlockSize,
					Card.begin() + static_cast<std::ptrdiff_t>(Block * MemoryCard::BlockSize));
			}
		}
		// Each frame of the header, the directory and the broken-sector list ends with the exclusive-or of its other bytes.
		for (int32 Checked = HeaderFrame; Checked <= WriteTestFrame; ++Checked)
		{
			if (Checked >= FirstUnusedFrame && Checked != WriteTestFrame)
			{
				continue;
			}
			const std::span<uint8> Bytes = Frame(Checked);
			uint8 Sum = 0;
			for (std::size_t Byte = 0; Byte + 1 < Bytes.size(); ++Byte)
			{
				Sum ^= Bytes[Byte];
			}
			Bytes.back() = Sum;
		}
		return Card;
	}
}
