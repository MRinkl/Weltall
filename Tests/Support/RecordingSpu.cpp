#include "Support/RecordingSpu.h"

namespace Weltall::Testing
{
	namespace
	{
		constexpr uint32 TransferAddress = 0x1A6;
		constexpr uint32 Control = 0x1AA;
		constexpr uint16 TransferMode = 0x30;
		constexpr std::size_t SoundRamSize = 0x80000;
	}

	FRecordingSpu::FRecordingSpu()
		: SoundRam(SoundRamSize)
	{
	}

	void FRecordingSpu::Write(uint32 Offset, uint16 Value)
	{
		const std::size_t Index = (Offset & 0x3FE) / 2;
		const uint16 Before = Registers[Index];
		Registers[Index] = Value;
		const uint16 Changed = static_cast<uint16>(Before ^ Value);
		if (Offset == TransferAddress || (Offset == Control && Changed != 0 && (Changed & ~TransferMode) == 0))
		{
			return;
		}
		const std::array<uint8, 4> Bytes{static_cast<uint8>(Offset), static_cast<uint8>(Offset >> 8), static_cast<uint8>(Value), static_cast<uint8>(Value >> 8)};
		Hasher.Update(Bytes);
		++Count;
	}

	uint16 FRecordingSpu::Read(uint32 Offset) const
	{
		return Registers[(Offset & 0x3FE) / 2];
	}

	void FRecordingSpu::WriteDma(std::span<const uint8> Data)
	{
		std::size_t At = static_cast<std::size_t>(Registers[TransferAddress / 2]) * 8;
		for (const uint8 Byte : Data)
		{
			SoundRam[At] = Byte;
			At = (At + 1) % SoundRamSize;
		}
		++Transfers;
	}

	void FRecordingSpu::Restart()
	{
		Hasher = FSha1();
		Count = 0;
		Transfers = 0;
	}

	std::string FRecordingSpu::Digest()
	{
		return FSha1::ToHex(Hasher.Finish());
	}
}
