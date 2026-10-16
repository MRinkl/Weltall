#pragma once

#include "CoreMinimal.h"
#include "Foundation/Result.h"
#include "Media/GameFileIndex.h"
#include "Media/Iso9660.h"

#include <filesystem>
#include <memory>

class IImageWrapperModule;
namespace Weltall
{
	class FDiscTextureCatalogue;
	struct FTextureSheetPicture;
}

/**
 * Every original texture picture of a disc (Weltall::FDiscTextureCatalogue) saved as a PNG below a textures folder's
 * originals, named by its key, in the folder of the item it was first found in and linked into the folder of each later
 * item that draws it, or on a sheet of a streamed sprite's cells with its layout beside it (Weltall::FTextureSheetLayout),
 * with an index listing every item each picture appears in. Existing files are kept, so an export
 * cancelled or run again goes on where it stopped. The disc is a detached copy, read without replacements. One thread
 * at a time.
 */
class WELTALL_API FTextureExport
{
public:
	static Weltall::TResult<TUniquePtr<FTextureExport>> Open(const Weltall::FIso9660& Disc, const Weltall::FGameFileIndex& Index, const FString& TexturesFolder);
	~FTextureExport();

	/** Saves the next item's new pictures; true once every item is done and the index is written. An unreadable item is logged and passed. */
	Weltall::TResult<bool> Step();
	uint32 CompletedItems() const;
	uint32 TotalItems() const;

	/** The index's name, below the originals. */
	static const TCHAR* IndexName;

private:
	FTextureExport(const Weltall::FIso9660& Disc, const Weltall::FGameFileIndex& Index, const FString& TexturesFolder);
	bool WriteIndex() const;
	/** A sheet's PNG and its layout beside it, unless its PNG is there already. */
	bool WriteSheet(const FString& Path, TConstArrayView<Weltall::FTextureSheetPicture> Cells) const;
	FString FolderOf(std::size_t Item) const;
	static FString PictureName(uint64 Key);

	Weltall::FIso9660 Disc;
	Weltall::FGameFileIndex Index;
	std::unique_ptr<Weltall::FDiscTextureCatalogue> Catalogue;
	FString Originals;
	std::filesystem::path Root;
	IImageWrapperModule* Wrappers = nullptr;
	/** The item whose folder holds each picture's PNG. */
	TMap<uint64, uint32> FirstItems;
	bool bDone = false;
};
