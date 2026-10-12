#include "Rendering/DrawListTextures.h"

#include "Graphics/Texture.h"
#include "Graphics/Vram.h"
#include "TextureArrayUpload.h"

FDrawListTextures::FDrawListTextures(std::span<const Weltall::FVram> Vrams, std::span<const Weltall::FDrawTexture> Textures,
	const FDrawListTextures* Previous)
{
	Requested.Append(Textures.data(), static_cast<int32>(Textures.size()));
	for (const Weltall::FVram& Vram : Vrams)
	{
		Revisions.Add(Vram.Revision());
	}
	for (const Weltall::FDrawTexture& Texture : Textures)
	{
		if (Texture.Vram >= Vrams.size() || LayerOf(Texture) != NoLayer)
		{
			continue;
		}
		Sources.Add(Texture);
		const bool bUnchanged = Previous != nullptr && Previous->Revisions.IsValidIndex(Texture.Vram) && Previous->Revisions[Texture.Vram] == Revisions[Texture.Vram];
		const uint32 Decoded = bUnchanged ? Previous->LayerOf(Texture) : NoLayer;
		if (Decoded != NoLayer)
		{
			Layers.Add(Previous->Layers[Decoded]);
			continue;
		}
		const Weltall::FImageRgba8 Image = Weltall::DecodeTexture(Vrams[Texture.Vram], Texture.Page, Texture.Clut);
		check(Image.Width == LayerSize && Image.Height == LayerSize);
		Layers.Emplace(Image.Pixels.data(), static_cast<int32>(Image.Pixels.size()));
	}
}

bool FDrawListTextures::IsCurrent(std::span<const Weltall::FVram> Vrams, std::span<const Weltall::FDrawTexture> Textures) const
{
	if (static_cast<int32>(Vrams.size()) != Revisions.Num() || static_cast<int32>(Textures.size()) != Requested.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Revisions.Num(); ++Index)
	{
		if (Vrams[static_cast<std::size_t>(Index)].Revision() != Revisions[Index])
		{
			return false;
		}
	}
	for (int32 Index = 0; Index < Requested.Num(); ++Index)
	{
		if (!(Textures[static_cast<std::size_t>(Index)] == Requested[Index]))
		{
			return false;
		}
	}
	return true;
}

uint32 FDrawListTextures::LayerOf(const Weltall::FDrawTexture& Texture) const
{
	// The page's blend mode does not change the texels, so it does not select a layer.
	const int32 Index = Sources.IndexOfByPredicate([&Texture](const Weltall::FDrawTexture& Source)
	{
		return Source.Vram == Texture.Vram && Source.Page.X == Texture.Page.X && Source.Page.Y == Texture.Page.Y && Source.Page.Depth == Texture.Page.Depth
			&& Source.Clut == Texture.Clut;
	});
	return Index == INDEX_NONE ? NoLayer : static_cast<uint32>(Index);
}

void FDrawListTextures::InitRHI(FRHICommandListBase& RHICmdList)
{
	Array = CreateLayeredTexture(RHICmdList, TEXT("ScreenDrawTextures"), LayerSize, Layers);
}

void FDrawListTextures::ReleaseRHI()
{
	Array.SafeRelease();
}
