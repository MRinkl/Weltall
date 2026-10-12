#pragma once

#include "CoreMinimal.h"
#include "Graphics/Vram.h"
#include "RenderGraphFwd.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "RHIResources.h"

namespace Weltall
{
	struct FTexturePage;
	struct FClutPosition;
}

/**
 * A VRAM state on the GPU, 1024 x 512 16-bit units as the PlayStation keeps them and the port's own rows
 * below them, which the shaders decode through a page and a CLUT as its GPU does. It follows the VRAM it
 * is given: each change is copied once, and the frames queued after it draw with it. Upscaled, it also
 * keeps the console's rows' indexed texels upscaled (Graphics/TextureUpscale.h), made again where VRAM changed.
 */
class WELTALL_API FVramTexture final : public FRenderResource
{
public:
	/**
	 * A texture and CLUT packed as the shaders unpack them (PsxVram.ush), a page or CLUT of the port's rows included;
	 * textureless primitives have NoTexture.
	 */
	static constexpr uint32 NoTexture = 0xFFFFFFFF;
	static uint32 Pack(const Weltall::FTexturePage& Page, const Weltall::FClutPosition& Clut);

	/**
	 * Whether the scenes drawn from this VRAM upscale its textures: from the next Follow on, the units' edges go
	 * with them and the upscaled texels are kept; off, the upscaled texels are let go. Game thread.
	 */
	void SetUpscaled(bool bUpscaled);

	/**
	 * Sends the VRAM's units to the GPU unless they are the ones sent last; true when it sent them. Game
	 * thread; the resource must be initialised.
	 */
	bool Follow(const Weltall::FVram& Vram);

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	/** The units, which passes may also draw into (a render target). */
	FRHITexture* Texture() const { return Units; }

	/** VRAM read as 4-bit and as 8-bit indices, each TextureUpscale::Factor times finer. */
	struct FUpscaled
	{
		FRDGTextureRef Indexed4 = nullptr;
		FRDGTextureRef Indexed8 = nullptr;
	};

	/**
	 * The upscaled texels, their passes added to the graph where VRAM changed since they were last asked for;
	 * none before a Follow after SetUpscaled. Rendering thread.
	 */
	FUpscaled Upscaled(FRDGBuilder& GraphBuilder) const;

private:
	/** A copy of the VRAM on its way to the rendering thread, and which tiles changed since the one before. */
	struct FCopy
	{
		TArray<uint16> Units;
		TArray<uint8> Edges;
		Weltall::FVramTiles Changed{};
	};

	FTextureRHIRef Units;
	FTextureRHIRef Edges;
	uint32 SentRevision = 0;
	uint32 SentLineage = 0;
	bool bSent = false;
	bool bUpscaledOnGameThread = false;
	/** The next Follow sends everything, edges included, though the VRAM did not change. */
	bool bSendAll = false;
	/** Copies on their way to the rendering thread; one it no longer holds is filled again. */
	TArray<TSharedRef<FCopy>> Staging;

	/** The rendering thread's: whether edges came, the tiles changed since the upscaled texels were made, and those texels (2x and 4x, per depth). */
	mutable bool bEdgesSent = false;
	mutable Weltall::FVramTiles Pending{};
	mutable TRefCountPtr<IPooledRenderTarget> DoubledTexels[2];
	mutable TRefCountPtr<IPooledRenderTarget> UpscaledTexels[2];
};
