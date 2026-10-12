#include "Formats/StrVideo.h"

#include "Foundation/LittleEndian.h"

namespace Weltall
{
	namespace
	{
		constexpr uint16 StrMagic = 0x0160;
		constexpr uint16 MdecType = 0x8001;
	}

	std::optional<FStrSectorHeader> ParseStrSector(std::span<const uint8> UserData)
	{
		if (UserData.size() < StrSector::HeaderBytes + StrSector::PayloadBytes
			|| LittleEndian::ReadU16(UserData, 0) != StrMagic || LittleEndian::ReadU16(UserData, 2) != MdecType)
		{
			return std::nullopt;
		}
		FStrSectorHeader Header;
		Header.SectorIndex = LittleEndian::ReadU16(UserData, 4);
		Header.SectorCount = LittleEndian::ReadU16(UserData, 6);
		Header.FrameNumber = LittleEndian::ReadU32(UserData, 8);
		Header.FrameBytes = LittleEndian::ReadU32(UserData, 12);
		Header.Width = LittleEndian::ReadU16(UserData, 16);
		Header.Height = LittleEndian::ReadU16(UserData, 18);
		if (Header.SectorCount == 0 || Header.SectorIndex >= Header.SectorCount
			|| Header.FrameBytes > static_cast<uint32>(Header.SectorCount) * StrSector::PayloadBytes)
		{
			return std::nullopt;
		}
		return Header;
	}

	std::optional<FStrFrame> FStrFrameAssembler::Add(std::span<const uint8> UserData)
	{
		const std::optional<FStrSectorHeader> Header = ParseStrSector(UserData);
		if (!Header)
		{
			return std::nullopt;
		}
		const bool bContinues = Header->SectorIndex == NextSector && Header->SectorIndex > 0
			&& Header->FrameNumber == Pending.FrameNumber && Header->SectorCount == SectorCount;
		if (!bContinues)
		{
			if (Header->SectorIndex != 0)
			{
				NextSector = 0;
				return std::nullopt;
			}
			Pending.FrameNumber = Header->FrameNumber;
			Pending.Width = Header->Width;
			Pending.Height = Header->Height;
			Pending.Bitstream.clear();
			SectorCount = Header->SectorCount;
			FrameBytes = Header->FrameBytes;
		}
		const std::span<const uint8> Payload = UserData.subspan(StrSector::HeaderBytes, StrSector::PayloadBytes);
		Pending.Bitstream.insert(Pending.Bitstream.end(), Payload.begin(), Payload.end());
		NextSector = static_cast<uint16>(Header->SectorIndex + 1);
		if (NextSector < SectorCount)
		{
			return std::nullopt;
		}
		NextSector = 0;
		Pending.Bitstream.resize(FrameBytes);
		return Pending;
	}
}
