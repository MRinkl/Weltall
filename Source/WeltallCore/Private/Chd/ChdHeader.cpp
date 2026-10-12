#include "Chd/ChdHeader.h"

#include "Chd/ChdBytes.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace Chd
{
	namespace
	{
		constexpr char Signature[8] = {'M', 'C', 'o', 'm', 'p', 'r', 'H', 'D'};
		constexpr uint32 SupportedVersion = 5;
		/** chdman makes hunks of at most 1 MiB; a map entry can give a hunk at most 16 MiB. */
		constexpr uint32 MostHunkBytes = (16u << 20) - 1;
	}

	bool FHeader::HasParent() const
	{
		return std::any_of(ParentSha1.begin(), ParentSha1.end(), [](uint8 Byte) { return Byte != 0; });
	}

	bool IsKnownCodec(uint32 Tag)
	{
		constexpr std::array<uint32, 9> Known{Codec::Zlib, Codec::Lzma, Codec::Huffman, Codec::Flac, Codec::Zstd, Codec::CdZlib,
			Codec::CdLzma, Codec::CdFlac, Codec::CdZstd};
		return std::find(Known.begin(), Known.end(), Tag) != Known.end();
	}

	TResult<FHeader> ReadHeader(const IInput& Input)
	{
		std::array<uint8, FHeader::Bytes> Raw{};
		if (Input.Size() < 16)
		{
			return EError::NotChd;
		}
		if (!Input.Read(0, std::span(Raw).first(16)))
		{
			return EError::CannotRead;
		}
		if (std::memcmp(Raw.data(), Signature, sizeof(Signature)) != 0)
		{
			return EError::NotChd;
		}
		if (BigEndian::ReadU32(Raw, 12) != SupportedVersion)
		{
			return EError::UnsupportedVersion;
		}
		if (BigEndian::ReadU32(Raw, 8) != FHeader::Bytes)
		{
			return EError::Corrupt;
		}
		if (Input.Size() < FHeader::Bytes)
		{
			return EError::CutShort;
		}
		if (!Input.Read(0, Raw))
		{
			return EError::CannotRead;
		}

		FHeader Header;
		for (std::size_t Index = 0; Index < Header.Codecs.size(); ++Index)
		{
			Header.Codecs[Index] = BigEndian::ReadU32(Raw, 16 + Index * 4);
		}
		Header.LogicalBytes = BigEndian::ReadU64(Raw, 32);
		Header.MapOffset = BigEndian::ReadU64(Raw, 40);
		Header.MetadataOffset = BigEndian::ReadU64(Raw, 48);
		Header.HunkBytes = BigEndian::ReadU32(Raw, 56);
		Header.UnitBytes = BigEndian::ReadU32(Raw, 60);
		std::copy_n(Raw.begin() + 64, Header.RawSha1.size(), Header.RawSha1.begin());
		std::copy_n(Raw.begin() + 84, Header.Sha1.size(), Header.Sha1.begin());
		std::copy_n(Raw.begin() + 104, Header.ParentSha1.size(), Header.ParentSha1.begin());

		if (Header.HunkBytes == 0 || Header.HunkBytes > MostHunkBytes || Header.UnitBytes == 0 || Header.HunkBytes % Header.UnitBytes != 0
			|| Header.LogicalBytes / Header.HunkBytes >= std::numeric_limits<uint32>::max())
		{
			return EError::Corrupt;
		}
		for (const uint32 Tag : Header.Codecs)
		{
			if (Tag != Codec::None && !IsKnownCodec(Tag))
			{
				return EError::UnknownCodec;
			}
		}
		if (Header.HasParent())
		{
			return EError::NeedsParent;
		}
		return Header;
	}
}
