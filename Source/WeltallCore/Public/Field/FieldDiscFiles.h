#pragma once

#include "Field/EventContext.h"

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/** The field's files as the disc holds them, read whole and at once. */
	class FFieldDiscFiles final : public IFieldFiles
	{
	public:
		/** Either may be null: nothing can then be read. Both must outlive the files. */
		FFieldDiscFiles(const FCdImage* InImage, const FGameFileIndex* InIndex) : Image(InImage), Index(InIndex) {}

		WELTALLCORE_API bool ReadFile(int32 Directory, int32 File, std::vector<uint8>& Bytes) override;

	private:
		const FCdImage* Image = nullptr;
		const FGameFileIndex* Index = nullptr;
	};
}
