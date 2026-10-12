#pragma once

#include "CoreMinimal.h"
#include "Presentation/DrawList.h"
#include "RenderResource.h"
#include "RHIResources.h"

#include <span>

namespace Weltall
{
	class FVram;
}

/**
 * The textures a screen's draw lists sample, on the GPU: each (VRAM state, page, CLUT) they name
 * decoded into one layer of a texture array, semi-transparent texels marked. A set is made for the
 * VRAM states as they are; when an upload changes one, a new set takes over from it.
 */
class WELTALL_API FDrawListTextures final : public FRenderResource
{
public:
	static constexpr uint32 NoLayer = 0xFFFF;

	/**
	 * Decodes the textures on the calling thread, taking the layers of VRAM states that have not
	 * changed since Previous decoded them from it; the GPU resources are created by InitResource.
	 */
	FDrawListTextures(std::span<const Weltall::FVram> Vrams, std::span<const Weltall::FDrawTexture> Textures,
		const FDrawListTextures* Previous = nullptr);

	/** The layer a texture was decoded into, or NoLayer if it is not one of this set's. */
	uint32 LayerOf(const Weltall::FDrawTexture& Texture) const;

	/** Whether these are the textures asked for, decoded from the VRAM states as they are now. */
	bool IsCurrent(std::span<const Weltall::FVram> Vrams, std::span<const Weltall::FDrawTexture> Textures) const;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	FRHITexture* TextureArray() const { return Array; }

private:
	static constexpr int32 LayerSize = 256;

	TArray<Weltall::FDrawTexture> Requested;
	/** Each VRAM state's revision when the set was decoded. */
	TArray<uint32> Revisions;
	TArray<Weltall::FDrawTexture> Sources;
	/** RGBA8 texels, one 256 x 256 layer per source. */
	TArray<TArray<uint8>> Layers;
	FTextureRHIRef Array;
};
