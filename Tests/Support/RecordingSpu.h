#pragma once

#include "Foundation/Sha1.h"
#include "Sound/SpuBus.h"

#include <array>
#include <string>
#include <vector>

namespace Weltall::Testing
{
	/**
	 * An SPU that only remembers: a register reads back what was last written to it, as in the
	 * oracle harness, and sound RAM keeps what DMA put there. The writes are digested in order the
	 * way the sound driver's oracle takes them: each as its
	 * offset and value (16-bit little-endian), leaving out writes that start or end a transfer into
	 * sound RAM (the transfer address, and control writes changing only the transfer mode).
	 */
	class FRecordingSpu final : public ISpuBus
	{
	public:
		FRecordingSpu();

		void Write(uint32 Offset, uint16 Value) override;
		uint16 Read(uint32 Offset) const override;
		void WriteDma(std::span<const uint8> Data) override;

		/** Starts a new digest; the registers and sound RAM stay as they are. */
		void Restart();
		/** The digest of the writes since the start or the last Restart. */
		std::string Digest();
		int32 WriteCount() const { return Count; }
		/** DMA transfers since the last Restart. */
		int32 TransferCount() const { return Transfers; }
		std::span<const uint8> Ram() const { return SoundRam; }

	private:
		std::array<uint16, 0x200> Registers{};
		std::vector<uint8> SoundRam;
		FSha1 Hasher;
		int32 Count = 0;
		int32 Transfers = 0;
	};
}
