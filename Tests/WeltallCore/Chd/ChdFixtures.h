#pragma once

#include "Chd/ChdTypes.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace Chd::Testing
{
	/** A CHD made by chdman from a synthetic image, and what that image was. */
	struct FFixture
	{
		std::string_view Name;
		uint64 ImageBytes;
		/** The image's FNV-1a hash, 64 bits. */
		uint64 ImageHash;
		/** The CHD file, two hexadecimal digits a byte. */
		std::string_view Hex;
	};

	/** Hunks of 2048 bytes, each packed by the one codec: an image with text, noise, copied hunks, fills and zeros. */
	extern const FFixture LzmaFixture;
	extern const FFixture ZlibFixture;
	extern const FFixture HuffmanFixture;
	extern const FFixture ZstdFixture;
	/** Hunks of 2048 bytes packed by FLAC: stereo sound of several kinds, with a fill, zeros and noise. */
	extern const FFixture FlacFixture;
	/** Both images, in hunks of 4096 bytes, packed by chdman's default codecs. */
	extern const FFixture MixedFixture;
	/** A CHD that packs nothing: its zero hunks are not written at all. */
	extern const FFixture PlainFixture;

	/** A CD track's own sectors as a BIN keeps them. */
	struct FCdTrackExpected
	{
		uint32 Number;
		uint32 SectorBytes;
		uint32 Sectors;
		/** The sectors' FNV-1a hash, 64 bits. */
		uint64 Hash;
	};

	/** A CHD chdman made of a synthetic CD from its CUE sheet, and each of the CD's tracks. */
	struct FCdFixture
	{
		std::string_view Name;
		std::array<FCdTrackExpected, 3> Tracks;
		std::size_t TrackCount;
		std::string_view Hex;
	};

	/** One Mode 1 track of raw sectors: most with error codes chdman leaves out, some with wrong ones, some zeros. */
	extern const FCdFixture CdDataFixture;
	/** Sound; a Mode 2 track of both forms whose pregap is in the image; sound after a pregap that is not. */
	extern const FCdFixture CdTracksFixture;
	/** One Mode 1 track of 2048-byte sectors, packed by the CD Deflate codec. */
	extern const FCdFixture CdCookedFixture;
	/** CdData's track, packed by the CD Zstandard codec. */
	extern const FCdFixture CdZstdFixture;

	inline uint64 Fnv1a(std::span<const uint8> Bytes)
	{
		uint64 Hash = 0xCBF29CE484222325ull;
		for (const uint8 Byte : Bytes)
		{
			Hash = (Hash ^ Byte) * 0x100000001B3ull;
		}
		return Hash;
	}

	inline std::vector<uint8> FromHex(std::string_view Hex)
	{
		const auto Digit = [](char Character) { return static_cast<uint8>(Character <= '9' ? Character - '0' : Character - 'a' + 10); };
		std::vector<uint8> Bytes(Hex.size() / 2);
		for (std::size_t Index = 0; Index < Bytes.size(); ++Index)
		{
			Bytes[Index] = static_cast<uint8>(Digit(Hex[Index * 2]) << 4 | Digit(Hex[Index * 2 + 1]));
		}
		return Bytes;
	}

	/** Bytes in memory as a CHD's input. */
	class FMemoryInput final : public IInput
	{
	public:
		explicit FMemoryInput(std::vector<uint8> InBytes) : Bytes(std::move(InBytes)) {}

		uint64 Size() const override { return Bytes.size(); }

		bool Read(uint64 Offset, std::span<uint8> Destination) const override
		{
			if (Offset > Bytes.size() || Destination.size() > Bytes.size() - Offset)
			{
				return false;
			}
			std::copy_n(Bytes.begin() + static_cast<std::ptrdiff_t>(Offset), Destination.size(), Destination.begin());
			return true;
		}

	private:
		std::vector<uint8> Bytes;
	};
}
