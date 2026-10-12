#pragma once

#include "Foundation/Types.h"

#include <span>

namespace Weltall
{
	/**
	 * The PlayStation's sound processor as software reaches it: its registers, at offsets from
	 * 1F801C00, and the DMA channel that fills its sound RAM from the transfer address its
	 * registers hold.
	 */
	class ISpuBus
	{
	public:
		virtual ~ISpuBus() = default;

		virtual void Write(uint32 Offset, uint16 Value) = 0;
		virtual uint16 Read(uint32 Offset) const = 0;

		/** A DMA write into sound RAM, starting at the transfer address and moving it on. */
		virtual void WriteDma(std::span<const uint8> Data) = 0;
	};
}
