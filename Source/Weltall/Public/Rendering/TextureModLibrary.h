#pragma once

#include "CoreMinimal.h"
#include "Graphics/Texture.h"

/** A replacement picture in RGBA byte order, ready for upload on the rendering thread. */
struct FTextureModImage
{
	int32 Width = 0;
	int32 Height = 0;
	TArray<uint8> Rgba;
};

/** What the library has of a key's picture. */
struct FTextureModLookup
{
	TSharedPtr<const FTextureModImage> Image;
	/** Being read in the background: asked again later, it may be there. */
	bool bPending = false;
	/** Larger than the pixels it was asked within. */
	bool bOverBudget = false;
};

/**
 * Local texture replacements and captures in a textures folder, named by the key of the picture they stand for
 * (Weltall::TextureRegionKey). Replacements are PNGs anywhere below the folder except its originals folder, their
 * names ending in the key; originals, exported or captured, are below "original". Pictures are decoded on worker
 * threads, many at once, and arrive at a later Find. Game thread only.
 */
class WELTALL_API FTextureModLibrary
{
public:
	/** The folder below the textures folder that holds original pictures, which are never read as replacements. */
	static const TCHAR* OriginalsFolder;

	/** Takes the folder and the switches; while replacements are on, finds the PNGs the folder holds again. */
	static void Configure(FString Folder, bool bEnabled, bool bCapture);
	static uint64 Generation();
	/** Replacing or capturing. */
	static bool IsActive();
	static bool IsReplacing();
	static bool IsCapturing();
	/**
	 * The picture standing for Key, a region Width x Height texels: a picture of any size up to 16 times the region's
	 * each way, decoded once and kept while it is drawn. Pending while it is read, the first time it is asked for and
	 * until it arrives; none when there is none, it fails to load, or it would take more than MaxPixels.
	 */
	static FTextureModLookup Find(uint64 Key, int32 Width, int32 Height, int64 MaxPixels);
	/** How many readings have been started: a change means a later Find may have something new. */
	static uint64 DecodesStarted();
	/**
	 * Waits for the pictures being read to arrive, at most Seconds when that is not negative; true when every one has.
	 * The arrived ones are there for the next Find.
	 */
	static bool WaitForDecodes(double Seconds = -1.0);
	/** Takes in the pictures that have arrived. */
	static void CollectDecodes();
	/** Whether Key's original is still to be saved: capturing, and no original of it is saved or queued. */
	static bool NeedsCapture(uint64 Key);
	/** Whether the queue of background writes takes another. */
	static bool HasCaptureRoom();
	/** Saves an original picture under original/captured, named by its key, in the background, when NeedsCapture. */
	static void Capture(uint64 Key, const Weltall::FImageRgba8& Original);
	/** Finishes the small queue of PNG writes before exit, and for deterministic verification. */
	static void WaitForCaptures();
};
