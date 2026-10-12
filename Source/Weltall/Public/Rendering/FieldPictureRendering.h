#pragma once

#include "CoreMinimal.h"
#include "RenderGraphFwd.h"

/** How a picture is drawn over what the target holds. */
enum class EFieldPictureBlend : uint8
{
	Opaque,
	/** Opaque but for its black pixels, as the GPU lets a texture's texel 0 through. */
	OpaqueBlackTransparent,
	/** Added to what is there. */
	Add,
	/** Half of it and half of what is there. */
	Average,
};

/** A picture drawn at the display's resolution, drawn again over triangles (FieldPicture.usf). */
struct FFieldPictureDraw
{
	/** Per vertex, three a triangle: where it is drawn (xy) and the place of the picture it shows (zw). */
	TConstArrayView<FVector4f> Vertices;
	/** Maps where vertices are drawn to the target's pixels, and the places they show to the picture's UV: scale xy, then offset zw. */
	FVector4f Placement = FVector4f(1.0f, 1.0f, 0.0f, 0.0f);
	FVector4f SourcePlacement = FVector4f(1.0f, 1.0f, 0.0f, 0.0f);
	/** The texels' modulation (a brightness of 128 is 1). */
	float ColorScale = 1.0f;
	/** Per vertex, a modulation of the texels besides ColorScale, graded across its triangle; none: 1 for all. */
	TConstArrayView<float> Shades;
	EFieldPictureBlend Blend = EFieldPictureBlend::Opaque;
	/** Clears the target to black before drawing. */
	bool bClearFirst = false;
	/** Draws only within Scissor (target pixels). */
	bool bScissor = false;
	FIntRect Scissor;
};

/** Adds a pass drawing Source over Output as Picture says. */
WELTALL_API void AddFieldPicturePass(FRDGBuilder& GraphBuilder, FRDGTextureRef Source, FRDGTextureRef Output, const FFieldPictureDraw& Picture);
