#include "Chd/ChdHunkDecoder.h"

#include "Chd/ChdBitReader.h"
#include "Chd/ChdCdSector.h"
#include "Chd/ChdHeader.h"

#include <algorithm>

namespace Chd
{
	namespace
	{
		/** The properties chdman packs every LZMA hunk with: lc=3, lp=0, pb=2. */
		constexpr uint32 LzmaLc = 3;
		constexpr uint32 LzmaLp = 0;
		constexpr uint32 LzmaPb = 2;
		/** The Huffman codec codes bytes with codes of up to 16 bits. */
		constexpr uint32 HuffmanSymbols = 256;
		constexpr uint32 HuffmanMaxBits = 16;
	}

	FHunkDecoder::FHunkDecoder() : Lzma(LzmaLc, LzmaLp, LzmaPb), Huffman(HuffmanSymbols, HuffmanMaxBits)
	{
	}

	bool FHunkDecoder::Decode(uint32 CodecTag, std::span<const uint8> Bytes, std::span<uint8> Destination)
	{
		switch (CodecTag)
		{
		case Codec::Zlib:
			return Inflater.Inflate(Bytes, Destination);
		case Codec::Lzma:
			return Lzma.Decode(Bytes, Destination);
		case Codec::Huffman:
			return DecodeHuffman(Bytes, Destination);
		case Codec::Flac:
			return DecodeFlac(Bytes, Destination);
		case Codec::Zstd:
			return ZstdDecoder.Decode(Bytes, Destination);
		case Codec::CdLzma:
			return DecodeCd(Codec::Lzma, Codec::Zlib, Bytes, Destination);
		case Codec::CdZlib:
			return DecodeCd(Codec::Zlib, Codec::Zlib, Bytes, Destination);
		case Codec::CdFlac:
			return DecodeCdFlac(Bytes, Destination);
		case Codec::CdZstd:
			return DecodeCd(Codec::Zstd, Codec::Zstd, Bytes, Destination);
		default:
			return false;
		}
	}

	bool FHunkDecoder::DecodeHuffman(std::span<const uint8> Bytes, std::span<uint8> Destination)
	{
		FMsbBitReader Reader(Bytes);
		if (!Huffman.ImportHuffman(Reader))
		{
			return false;
		}
		for (uint8& Byte : Destination)
		{
			const uint32 Symbol = Huffman.Decode(Reader);
			if (Symbol == FHuffmanDecoder::Invalid)
			{
				return false;
			}
			Byte = static_cast<uint8>(Symbol);
		}
		return !Reader.Overran();
	}

	bool FHunkDecoder::DecodeFlac(std::span<const uint8> Bytes, std::span<uint8> Destination)
	{
		// The first byte says the order the samples' bytes are kept in: whichever packed better.
		if (Bytes.empty() || (Bytes[0] != 'L' && Bytes[0] != 'B') || Destination.size() % 4 != 0)
		{
			return false;
		}
		return Flac.DecodeStereo16(Bytes.subspan(1), Destination, Bytes[0] == 'B').has_value();
	}

	bool FHunkDecoder::DecodeCd(uint32 SectorCodec, uint32 SubchannelCodec, std::span<const uint8> Bytes, std::span<uint8> Destination)
	{
		if (Destination.size() % CdFrameBytes != 0)
		{
			return false;
		}
		// First a bit for each frame whose sync and error-correction bytes were left out, then the sectors' packed length.
		const std::size_t Frames = Destination.size() / CdFrameBytes;
		const std::size_t FlagBytes = (Frames + 7) / 8;
		const std::size_t LengthBytes = Destination.size() < 65536 ? 2 : 3;
		const std::size_t HeaderBytes = FlagBytes + LengthBytes;
		if (Bytes.size() < HeaderBytes)
		{
			return false;
		}
		std::size_t SectorsLength = 0;
		for (std::size_t Index = 0; Index < LengthBytes; ++Index)
		{
			SectorsLength = SectorsLength << 8 | Bytes[FlagBytes + Index];
		}
		if (Bytes.size() - HeaderBytes < SectorsLength)
		{
			return false;
		}
		CdParts.resize(Destination.size());
		const std::span<uint8> Sectors(CdParts.data(), Frames * CdSectorBytes);
		const std::span<uint8> Subchannels(CdParts.data() + Sectors.size(), Frames * CdSubchannelBytes);
		if (!Decode(SectorCodec, Bytes.subspan(HeaderBytes, SectorsLength), Sectors)
			|| !Decode(SubchannelCodec, Bytes.subspan(HeaderBytes + SectorsLength), Subchannels))
		{
			return false;
		}
		JoinCdFrames(Destination);
		for (std::size_t Frame = 0; Frame < Frames; ++Frame)
		{
			if ((Bytes[Frame / 8] >> (Frame % 8) & 1) != 0)
			{
				RestoreSyncAndEcc(std::span<uint8, CdSectorBytes>(Destination.data() + Frame * CdFrameBytes, CdSectorBytes));
			}
		}
		return true;
	}

	bool FHunkDecoder::DecodeCdFlac(std::span<const uint8> Bytes, std::span<uint8> Destination)
	{
		if (Destination.size() % CdFrameBytes != 0)
		{
			return false;
		}
		const std::size_t Frames = Destination.size() / CdFrameBytes;
		CdParts.resize(Destination.size());
		const std::span<uint8> Sectors(CdParts.data(), Frames * CdSectorBytes);
		const std::span<uint8> Subchannels(CdParts.data() + Sectors.size(), Frames * CdSubchannelBytes);
		const auto Taken = Flac.DecodeStereo16(Bytes, Sectors, true);
		if (!Taken || !Inflater.Inflate(Bytes.subspan(*Taken), Subchannels))
		{
			return false;
		}
		JoinCdFrames(Destination);
		return true;
	}

	void FHunkDecoder::JoinCdFrames(std::span<uint8> Destination) const
	{
		const std::size_t Frames = Destination.size() / CdFrameBytes;
		for (std::size_t Frame = 0; Frame < Frames; ++Frame)
		{
			uint8* Out = Destination.data() + Frame * CdFrameBytes;
			std::copy_n(CdParts.data() + Frame * CdSectorBytes, CdSectorBytes, Out);
			std::copy_n(CdParts.data() + Frames * CdSectorBytes + Frame * CdSubchannelBytes, CdSubchannelBytes, Out + CdSectorBytes);
		}
	}
}
