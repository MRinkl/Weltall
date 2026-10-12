#include "Chd/ChdFile.h"

#include "Chd/ChdCrc16.h"

#include <algorithm>

namespace Chd
{
	TResult<FFile> FFile::Open(std::shared_ptr<const IInput> Input)
	{
		auto HeaderRead = ReadHeader(*Input);
		if (!HeaderRead)
		{
			return HeaderRead.Error();
		}
		auto MapRead = ReadMap(*Input, *HeaderRead);
		if (!MapRead)
		{
			return MapRead.Error();
		}
		auto MetadataRead = ReadMetadata(*Input, *HeaderRead);
		if (!MetadataRead)
		{
			return MetadataRead.Error();
		}
		return FFile(std::move(Input), std::move(*HeaderRead), std::move(*MapRead), std::move(*MetadataRead));
	}

	FFile::FFile(std::shared_ptr<const IInput> InInput, FHeader InHeader, std::vector<FHunkEntry> InMap, std::vector<FMetadata> InMetadata)
		: FileInput(std::move(InInput)), FileHeader(std::move(InHeader)), HunkMap(std::move(InMap)), AllMetadata(std::move(InMetadata))
	{
	}

	const FMetadata* FFile::FindMetadata(uint32 Tag, uint32 Index) const
	{
		for (const FMetadata& Entry : AllMetadata)
		{
			if (Entry.Tag == Tag && Index-- == 0)
			{
				return &Entry;
			}
		}
		return nullptr;
	}

	bool FFile::ReadHunk(uint32 Hunk, std::span<uint8> Destination, FHunkDecoder& Decoder) const
	{
		if (Hunk >= HunkMap.size() || Destination.size() != FileHeader.HunkBytes)
		{
			return false;
		}
		const FHunkEntry& Entry = HunkMap[Hunk];
		switch (Entry.Kind())
		{
		case EHunkKind::Copy:
			return ReadHunk(Entry.Source(), Destination, Decoder);
		case EHunkKind::Zeros:
			std::fill(Destination.begin(), Destination.end(), uint8{0});
			return true;
		case EHunkKind::Plain:
			return FileInput->Read(Entry.Offset(), Destination);
		case EHunkKind::Stored:
			return FileInput->Read(Entry.Offset(), Destination) && Crc16(Destination) == Entry.ExpectedCrc();
		case EHunkKind::Codec0:
		case EHunkKind::Codec1:
		case EHunkKind::Codec2:
		case EHunkKind::Codec3:
			break;
		}
		std::vector<uint8>& Packed = Decoder.PackedBuffer();
		Packed.resize(Entry.Length());
		return FileInput->Read(Entry.Offset(), Packed)
			&& Decoder.Decode(FileHeader.Codecs[static_cast<std::size_t>(Entry.Kind())], Packed, Destination)
			&& Crc16(Destination) == Entry.ExpectedCrc();
	}
}
