#include "Chd/ChdMap.h"

#include "Chd/ChdBitReader.h"
#include "Chd/ChdBytes.h"
#include "Chd/ChdCrc16.h"
#include "Chd/ChdHuffman.h"

#include <array>

namespace Chd
{
	namespace
	{
		/** The kinds a packed map codes: EHunkKind's first six, a parent's hunk, and shorthands for runs and copies. */
		enum ECodedKind : uint32
		{
			CodedStored = 4,
			CodedCopy = 5,
			CodedParent = 6,
			/** The last kind again for 3 to 18 hunks, or for 19 to 274. */
			ShortRun = 7,
			LongRun = 8,
			/** A copy of the hunk the last copy copied, or of the one after it. */
			CopyOfSame = 9,
			CopyOfNext = 10,
			/** Parent hunks, which only a CHD with a parent has. */
			ParentOfSame = 11,
			ParentOfLast = 12,
			ParentOfNext = 13,
		};

		constexpr uint32 MapHeaderBytes = 16;
		/** The kinds are coded with a Huffman code of 16 symbols, none longer than 8 bits. */
		constexpr uint32 KindSymbols = 16;
		constexpr uint32 KindMaxBits = 8;

		bool Within(uint64 Offset, uint64 Length, uint64 Size)
		{
			return Offset <= Size && Length <= Size - Offset;
		}

		/** The map of a CHD that packs nothing: each hunk's place in the file in hunks, 0 for none. */
		TResult<std::vector<FHunkEntry>> ReadPlainMap(const IInput& Input, const FHeader& Header)
		{
			const uint32 HunkCount = Header.HunkCount();
			if (!Within(Header.MapOffset, static_cast<uint64>(HunkCount) * 4, Input.Size()))
			{
				return EError::CutShort;
			}
			std::vector<uint8> Raw(static_cast<std::size_t>(HunkCount) * 4);
			if (!Input.Read(Header.MapOffset, Raw))
			{
				return EError::CannotRead;
			}
			std::vector<FHunkEntry> Entries;
			Entries.reserve(HunkCount);
			for (uint32 Hunk = 0; Hunk < HunkCount; ++Hunk)
			{
				const uint64 Offset = static_cast<uint64>(BigEndian::ReadU32(Raw, Hunk * 4)) * Header.HunkBytes;
				if (Offset == 0)
				{
					Entries.push_back(FHunkEntry(EHunkKind::Zeros, 0, 0, 0));
					continue;
				}
				if (!Within(Offset, Header.HunkBytes, Input.Size()))
				{
					return EError::CutShort;
				}
				Entries.push_back(FHunkEntry(EHunkKind::Plain, Offset, Header.HunkBytes, 0));
			}
			return Entries;
		}

		/** Each hunk's coded kind, a Huffman code with runs of the last kind. */
		TResult<std::vector<uint8>> ReadCodedKinds(FMsbBitReader& Reader, uint32 HunkCount)
		{
			FHuffmanDecoder Code(KindSymbols, KindMaxBits);
			if (!Code.ImportRle(Reader))
			{
				return EError::Corrupt;
			}
			std::vector<uint8> Kinds(HunkCount);
			uint8 Last = 0;
			uint32 Repeats = 0;
			for (uint32 Hunk = 0; Hunk < HunkCount; ++Hunk)
			{
				if (Repeats > 0)
				{
					Kinds[Hunk] = Last;
					--Repeats;
					continue;
				}
				const uint32 Kind = Code.Decode(Reader);
				if (Kind == FHuffmanDecoder::Invalid || Reader.Overran())
				{
					return EError::Corrupt;
				}
				if (Kind == ShortRun || Kind == LongRun)
				{
					const uint32 High = Code.Decode(Reader);
					const uint32 Low = Kind == LongRun ? Code.Decode(Reader) : 0;
					if (High == FHuffmanDecoder::Invalid || Low == FHuffmanDecoder::Invalid)
					{
						return EError::Corrupt;
					}
					Kinds[Hunk] = Last;
					Repeats = Kind == ShortRun ? 2 + High : 2 + 16 + (High << 4) + Low;
					continue;
				}
				Last = static_cast<uint8>(Kind);
				Kinds[Hunk] = Last;
			}
			return Kinds;
		}

		/** The CRC-16 of an entry as MAME lays the map out unpacked: kind, 24-bit length, 48-bit offset, CRC. */
		uint16 ContinueMapCrc(uint16 Crc, EHunkKind Kind, uint32 Length, uint64 Offset, uint16 HunkCrc)
		{
			const std::array<uint8, 12> Raw{
				static_cast<uint8>(Kind),
				static_cast<uint8>(Length >> 16), static_cast<uint8>(Length >> 8), static_cast<uint8>(Length),
				static_cast<uint8>(Offset >> 40), static_cast<uint8>(Offset >> 32), static_cast<uint8>(Offset >> 24),
				static_cast<uint8>(Offset >> 16), static_cast<uint8>(Offset >> 8), static_cast<uint8>(Offset),
				static_cast<uint8>(HunkCrc >> 8), static_cast<uint8>(HunkCrc),
			};
			return Crc16(Raw, Crc);
		}

		TResult<std::vector<FHunkEntry>> ReadPackedMap(const IInput& Input, const FHeader& Header)
		{
			std::array<uint8, MapHeaderBytes> MapHeader{};
			if (!Within(Header.MapOffset, MapHeader.size(), Input.Size()))
			{
				return EError::CutShort;
			}
			if (!Input.Read(Header.MapOffset, MapHeader))
			{
				return EError::CannotRead;
			}
			const uint32 MapBytes = BigEndian::ReadU32(MapHeader, 0);
			const uint64 FirstOffset = BigEndian::ReadU48(MapHeader, 4);
			const uint16 MapCrc = BigEndian::ReadU16(MapHeader, 10);
			const uint32 LengthBits = MapHeader[12];
			const uint32 CopyBits = MapHeader[13];
			if (LengthBits > 24 || CopyBits > 32)
			{
				return EError::Corrupt;
			}
			if (!Within(Header.MapOffset + MapHeader.size(), MapBytes, Input.Size()))
			{
				return EError::CutShort;
			}
			std::vector<uint8> Packed(MapBytes);
			if (!Input.Read(Header.MapOffset + MapHeader.size(), Packed))
			{
				return EError::CannotRead;
			}

			FMsbBitReader Reader(Packed);
			const uint32 HunkCount = Header.HunkCount();
			auto Kinds = ReadCodedKinds(Reader, HunkCount);
			if (!Kinds)
			{
				return Kinds.Error();
			}

			std::vector<FHunkEntry> Entries;
			Entries.reserve(HunkCount);
			uint64 Next = FirstOffset;
			uint32 LastCopied = 0;
			uint16 Crc = 0xFFFF;
			for (uint32 Hunk = 0; Hunk < HunkCount; ++Hunk)
			{
				const uint32 Coded = (*Kinds)[Hunk];
				switch (Coded)
				{
				case CodedStored:
				case static_cast<uint32>(EHunkKind::Codec0):
				case static_cast<uint32>(EHunkKind::Codec1):
				case static_cast<uint32>(EHunkKind::Codec2):
				case static_cast<uint32>(EHunkKind::Codec3):
				{
					const EHunkKind Kind = static_cast<EHunkKind>(Coded);
					const uint32 Length = Coded == CodedStored ? Header.HunkBytes : Reader.Read(LengthBits);
					const uint16 HunkCrc = static_cast<uint16>(Reader.Read(16));
					if (Kind != EHunkKind::Stored && Header.Codecs[Coded] == Codec::None)
					{
						return EError::Corrupt;
					}
					if (!Within(Next, Length, Input.Size()))
					{
						return EError::CutShort;
					}
					Crc = ContinueMapCrc(Crc, Kind, Length, Next, HunkCrc);
					Entries.emplace_back(Kind, Next, Length, HunkCrc);
					Next += Length;
					break;
				}
				case CodedCopy:
				case CopyOfSame:
				case CopyOfNext:
				{
					if (Coded == CodedCopy)
					{
						LastCopied = Reader.Read(CopyBits);
					}
					else if (Coded == CopyOfNext)
					{
						++LastCopied;
					}
					if (LastCopied >= Hunk)
					{
						return EError::Corrupt;
					}
					Crc = ContinueMapCrc(Crc, EHunkKind::Copy, 0, LastCopied, 0);
					const FHunkEntry& Copied = Entries[LastCopied];
					Entries.push_back(FHunkEntry(EHunkKind::Copy, Copied.Kind() == EHunkKind::Copy ? Copied.Source() : LastCopied, 0, 0));
					break;
				}
				case CodedParent:
				case ParentOfSame:
				case ParentOfLast:
				case ParentOfNext:
					return EError::NeedsParent;
				default:
					return EError::Corrupt;
				}
			}
			if (Reader.Overran() || Crc != MapCrc)
			{
				return EError::Corrupt;
			}
			return Entries;
		}
	}

	TResult<std::vector<FHunkEntry>> ReadMap(const IInput& Input, const FHeader& Header)
	{
		return Header.IsCompressed() ? ReadPackedMap(Input, Header) : ReadPlainMap(Input, Header);
	}
}
