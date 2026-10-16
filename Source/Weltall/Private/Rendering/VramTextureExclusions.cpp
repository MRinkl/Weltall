#include "Rendering/VramTexture.h"

#include "Graphics/TextureRegion.h"

#include <algorithm>

namespace
{
	bool OverlapsWrapped(int32 StartA, int32 LengthA, int32 StartB, int32 LengthB, int32 Limit)
	{
		if (LengthA <= 0 || LengthB <= 0) return false;
		StartA &= Limit - 1;
		StartB &= Limit - 1;
		LengthA = std::min(LengthA, Limit);
		LengthB = std::min(LengthB, Limit);
		const auto Overlap = [](int32 LowA, int32 HighA, int32 LowB, int32 HighB) { return LowA < HighB && LowB < HighA; };
		return Overlap(StartA, StartA + LengthA, StartB, StartB + LengthB) ||
			Overlap(StartA + Limit, StartA + LengthA + Limit, StartB, StartB + LengthB) ||
			Overlap(StartA, StartA + LengthA, StartB + Limit, StartB + LengthB + Limit);
	}

	bool Overlaps(const Weltall::FVramRect& Source, const Weltall::FVramRect& Written)
	{
		if (Source.Y >= Weltall::FVram::Height) return false;
		return OverlapsWrapped(Source.X, Source.Width, Written.X, Written.Width, Weltall::FVram::Width) &&
			OverlapsWrapped(Source.Y, Source.Height, Written.Y, Written.Height, Weltall::FVram::Height);
	}
}

void FVramTexture::SetReplacementExclusions(std::span<const Weltall::FVramRect> Rectangles)
{
	if (ReplacementExclusions.size() == Rectangles.size() && std::equal(Rectangles.begin(), Rectangles.end(), ReplacementExclusions.begin())) return;
	ReplacementExclusions.assign(Rectangles.begin(), Rectangles.end());
}

bool FVramTexture::IsReplacementExcluded(const Weltall::FTextureRegion& Region, bool bOwnColours) const
{
	if (ReplacementExclusions.empty()) return false;
	const Weltall::FTexturePage Page = Weltall::TextureViewPage(Region.View);
	const Weltall::FClutPosition Clut = Weltall::TextureViewClut(Region.View);
	const Weltall::FVramRect Texels = Weltall::TextureRegionUnits(Region);
	const Weltall::FVramRect Colors{static_cast<int16>(Clut.X), static_cast<int16>(Clut.Y),
		static_cast<int16>(Page.Depth == Weltall::ETextureDepth::Indexed4 ? 16 : 256), 1};
	const bool bClut = !bOwnColours && Page.Depth != Weltall::ETextureDepth::Direct15;
	for (const auto& Written : ReplacementExclusions)
	{
		if (Overlaps(Texels, Written) || (bClut && Overlaps(Colors, Written))) return true;
	}
	return false;
}
