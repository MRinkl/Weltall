#include "Rendering/VramTexture.h"

#include "Graphics/TextureRegion.h"
#include "Rendering/TextureModLibrary.h"
#include "TextureReplacementBindings.h"
#include "TextureReplacementGpu.h"

FVramTexture::FVramTexture()
	: ReplacementBindings(MakeUnique<FTextureReplacementBindings>()), ReplacementGpu(MakeUnique<FTextureReplacementGpu>())
{
}

FVramTexture::~FVramTexture() = default;

const FTextureReplacementResources& FVramTexture::Replacements() const
{
	return ReplacementGpu->Resources();
}

void FVramTexture::FollowReplacements(const Weltall::FVram& Vram, std::span<const Weltall::FTextureRegion> Regions, std::span<const Weltall::FTexturePictureSeed> Seeds)
{
	if (!FTextureModLibrary::IsActive())
	{
		if (!bReplacementsBound) return;
		ReplacementBindings->Reset();
		ReplacementGpu->Update({});
		bReplacementsBound = false;
		return;
	}
	if (ReplacementBindings->Follow(Vram, Regions, Seeds, [this](const Weltall::FTextureRegion& Region, bool bOwnColours) { return IsReplacementExcluded(Region, bOwnColours); }))
	{
		ReplacementGpu->Update(ReplacementBindings->Bound());
	}
	bReplacementsBound = !ReplacementBindings->Bound().IsEmpty();
}
