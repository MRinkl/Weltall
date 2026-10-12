#include "Media/GameFileIndex.h"

#include "Foundation/LittleEndian.h"

#include <string>

namespace Weltall
{
	namespace
	{
		/** Where the executable keeps the two tables (both discs of the USA release). */
		constexpr uint32 IndexAddress = 0x80010004;
		constexpr uint32 IndexBytes = 0x7FFC;
		constexpr uint32 DirectoryAddress = 0x80018004;

		constexpr std::size_t RecordBytes = 7;
		constexpr uint32 EndOfIndex = 0xFFFFFF;
		constexpr uint16 NoDirectory = 0xFFFF;
	}

	TResult<FGameFileIndex> FGameFileIndex::Read(const FPsxExecutable& Executable)
	{
		const std::span<const uint8> Index = Executable.Read(IndexAddress, IndexBytes);
		const std::span<const uint8> Directories = Executable.Read(DirectoryAddress, DirectoryCount * 2);
		if (Index.empty() || Directories.empty())
		{
			return MakeError("the executable holds no file index");
		}
		FGameFileIndex Result;
		for (std::size_t Offset = 0; Offset + RecordBytes <= Index.size(); Offset += RecordBytes)
		{
			const uint32 Lba = Index[Offset] | Index[Offset + 1] << 8 | Index[Offset + 2] << 16;
			if (Lba == EndOfIndex)
			{
				break;
			}
			Result.AllRecords.push_back({Lba, static_cast<int32>(LittleEndian::ReadU32(Index, Offset + 3))});
		}
		for (int32 Directory = 0; Directory < DirectoryCount; ++Directory)
		{
			Result.DirectoryStarts[static_cast<std::size_t>(Directory)] = LittleEndian::ReadU16(Directories, static_cast<std::size_t>(Directory) * 2);
		}
		return Result;
	}

	const FIndexRecord* FGameFileIndex::Find(int32 Directory, int32 Number) const
	{
		if (Directory < 0 || Directory >= DirectoryCount || Number < 1)
		{
			return nullptr;
		}
		const uint16 Start = DirectoryStarts[static_cast<std::size_t>(Directory)];
		if (Start == 0 || Start == NoDirectory)
		{
			return nullptr;
		}
		// File 1 is the directory's first record itself, so file n is n - 1 records after it.
		const std::size_t Record = static_cast<std::size_t>(Start - 1) + static_cast<std::size_t>(Number - 1);
		return Record < AllRecords.size() ? &AllRecords[Record] : nullptr;
	}

	TResult<std::vector<uint8>> ReadFileSectors(const FCdImage& Image, const FIndexRecord& Record)
	{
		if (!Record.IsFile())
		{
			return MakeError("index record at sector " + std::to_string(Record.Lba) + " holds no file");
		}
		const uint64 Sectors = (static_cast<uint64>(Record.Size) + CdSector::Form1DataSize - 1) / CdSector::Form1DataSize;
		return Image.ReadForm1(Record.Lba, Sectors * CdSector::Form1DataSize);
	}
}
