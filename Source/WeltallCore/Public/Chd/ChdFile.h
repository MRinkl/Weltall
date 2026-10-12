#pragma once

#include "Chd/ChdHeader.h"
#include "Chd/ChdHunkDecoder.h"
#include "Chd/ChdMap.h"
#include "Chd/ChdMetadata.h"
#include "Chd/ChdTypes.h"

#include <memory>
#include <vector>

namespace Chd
{
	/** An open CHD: its header, map and metadata, and its hunks unpacked on request. */
	class FFile
	{
	public:
		/**
		 * Reads the header, map and metadata, and checks that every hunk can be found: within the file, by a codec this
		 * reader knows, without a parent. The input's Read may then be called from several threads at once.
		 */
		static TResult<FFile> Open(std::shared_ptr<const IInput> Input);

		const FHeader& Header() const { return FileHeader; }
		const std::vector<FMetadata>& Metadata() const { return AllMetadata; }

		/** Where each hunk lies and how it is kept. */
		const std::vector<FHunkEntry>& Map() const { return HunkMap; }

		/** The Index-th metadata entry with Tag, counting from 0; null when there is none. */
		const FMetadata* FindMetadata(uint32 Tag, uint32 Index = 0) const;

		/**
		 * Unpacks a hunk into Destination, which holds HunkBytes; false when its data cannot be read or does not unpack
		 * to what its CRC says. Several threads may read at once, each with its own Decoder.
		 */
		bool ReadHunk(uint32 Hunk, std::span<uint8> Destination, FHunkDecoder& Decoder) const;

	private:
		FFile(std::shared_ptr<const IInput> InInput, FHeader InHeader, std::vector<FHunkEntry> InMap, std::vector<FMetadata> InMetadata);

		std::shared_ptr<const IInput> FileInput;
		FHeader FileHeader;
		std::vector<FHunkEntry> HunkMap;
		std::vector<FMetadata> AllMetadata;
	};
}
