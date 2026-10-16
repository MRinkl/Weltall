#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"
#include "TextureReplacementBindings.h"

/** What the shaders read of the replacements (TextureReplacements.ush). Rendering thread. */
struct FTextureReplacementResources
{
	FTextureRHIRef Views;
	FTextureRHIRef RegionMaps;
	FTextureRHIRef Regions;
	FTextureRHIRef Palettes;
	FTextureRHIRef Atlas;
	uint32 ViewMask = 0;
	uint32 ViewCount = 0;
};

/**
 * Bound replacement pictures on the GPU: their 64-pixel tiles in an atlas that keeps a picture while frames draw it,
 * for each view a map of its texels to the regions drawn there, the regions (their pictures, and where the whole
 * picture lies when a region is part of a larger one) with the colours each was matched with, and a hash lookup from a
 * view to its map. Only what changed is sent.
 */
class FTextureReplacementGpu
{
public:
	/** Sends what changed for the bound regions; the frames queued after it draw with them. Game thread. */
	void Update(TConstArrayView<FBoundTextureRegion> Bound);

	/** Rendering thread. */
	const FTextureReplacementResources& Resources() const { return Gpu; }
	void InitRHI(FRHICommandListBase& RHICmdList);
	void ReleaseRHI();

	static constexpr int32 TileSize = 64;
	static constexpr int32 AtlasColumns = 128;
	static constexpr int32 MaxAtlasRows = 256;
	static constexpr int32 MapsAcross = 16;

private:
	struct FResidentPicture
	{
		TSharedPtr<const FTextureModImage> Image;
		uint32 FirstTile = 0;
		uint32 Tiles = 0;
		uint64 LastFrame = 0;
	};

	struct FRegionKey
	{
		Weltall::FTextureRegion Region;
		const FTextureModImage* Image = nullptr;
		const TArray<uint16>* Palette = nullptr;

		bool operator==(const FRegionKey&) const = default;
		friend uint32 GetTypeHash(const FRegionKey& Key)
		{
			uint32 Hash = HashCombineFast(Key.Region.View, static_cast<uint32>(Key.Region.U | Key.Region.V << 8 | Key.Region.Width << 16));
			Hash = HashCombineFast(Hash, PointerHash(Key.Image));
			return HashCombineFast(HashCombineFast(Hash, Key.Region.Height), PointerHash(Key.Palette));
		}
	};

	struct FSlot
	{
		int32 Number = 0;
		/** The regions drawn on the view, in precedence, as the map was last made from them. */
		TArray<uint32> Regions;
		TArray<uint16> Map;
	};

	/** What one Update sends to the rendering thread. */
	struct FUploads
	{
		int32 AtlasRows = 0;
		/** The rows the atlas had before, copied into a new one. */
		int32 OldAtlasRows = 0;
		bool bNewAtlas = false;
		TArray<TPair<TSharedPtr<const FTextureModImage>, uint32>> Pictures;
		int32 MapRows = 0;
		bool bNewMaps = false;
		TArray<TPair<int32, TArray<uint16>>> Maps;
		TArray<FUintVector4> Regions;
		TArray<uint16> Palettes;
		TArray<FUintVector4> Views;
		uint32 ViewCount = 0;
		bool bViews = false;
	};

	FResidentPicture* MakeResident(const TSharedPtr<const FTextureModImage>& Image, FUploads& Uploads);
	bool AllocateTiles(uint32 Count, uint32& First);
	void FreeTiles(uint32 First, uint32 Count);
	uint32 RegionId(const FBoundTextureRegion& Bound, const FResidentPicture& Picture, bool& bChanged);
	void Send(TSharedRef<FUploads> Uploads);

	TMap<const FTextureModImage*, FResidentPicture> Resident;
	/** Runs of free tiles as first and count, in order. */
	TArray<TPair<uint32, uint32>> FreeRuns;
	int32 AtlasRows = 0;
	TMap<FRegionKey, uint32> RegionIds;
	TArray<uint32> FreeRegionIds;
	TArray<FUintVector4> RegionEntries;
	TArray<uint16> PaletteRows;
	TArray<uint64> RegionLastFrame;
	TMap<uint32, FSlot> Slots;
	TArray<int32> FreeSlots;
	int32 SlotCount = 0;
	int32 MapRows = 0;
	uint64 Frame = 0;
	bool bWarnedBudget = false;

	FTextureReplacementResources Gpu;
};
