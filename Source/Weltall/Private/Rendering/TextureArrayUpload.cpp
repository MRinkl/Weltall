#include "TextureArrayUpload.h"

#include "RHICommandList.h"
#include "RHITextureInitializer.h"

FTextureRHIRef CreateLayeredTexture(FRHICommandListBase& RHICmdList, const TCHAR* Name, int32 LayerSize, const TArray<TArray<uint8>>& Layers)
{
	constexpr int32 BytesPerTexel = 4;
	// An array needs a layer even when nothing samples it.
	const int32 LayerCount = FMath::Max(Layers.Num(), 1);
	const FRHITextureCreateDesc Desc = FRHITextureCreateDesc::Create2DArray(Name, FIntPoint(LayerSize, LayerSize), static_cast<uint16>(LayerCount), PF_R8G8B8A8)
		.SetFlags(ETextureCreateFlags::ShaderResource)
		.SetInitialState(ERHIAccess::SRVMask)
		.SetInitActionInitializer();
	FRHITextureInitializer Initializer = RHICmdList.CreateTextureInitializer(Desc);
	for (int32 Layer = 0; Layer < LayerCount; ++Layer)
	{
		const FRHITextureSubresourceInitializer Subresource = Initializer.GetTexture2DArraySubresource(Layer, 0);
		for (int32 Row = 0; Row < LayerSize; ++Row)
		{
			uint8* Destination = static_cast<uint8*>(Subresource.Data) + Row * Subresource.Stride;
			if (Layers.IsValidIndex(Layer))
			{
				FMemory::Memcpy(Destination, Layers[Layer].GetData() + Row * LayerSize * BytesPerTexel, LayerSize * BytesPerTexel);
			}
			else
			{
				FMemory::Memzero(Destination, LayerSize * BytesPerTexel);
			}
		}
	}
	return Initializer.Finalize();
}
