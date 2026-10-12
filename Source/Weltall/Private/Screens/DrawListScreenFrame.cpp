#include "Screens/DrawListScreenFrame.h"

#include "Presentation/ScreenLayout.h"
#include "Rendering/DrawListRendering.h"
#include "Rendering/DrawListTextures.h"

FDrawListScreenFrame::FDrawListScreenFrame(Weltall::FScreenSize InScreen, TSharedRef<FDrawListTextures> InTextures,
	std::shared_ptr<const Weltall::FDrawList> InLatest, std::shared_ptr<const Weltall::FDrawList> InPrevious,
	std::shared_ptr<const std::vector<int32>> InMatches, float InAlpha)
	: Screen(InScreen), Textures(MoveTemp(InTextures)), Latest(std::move(InLatest)), Previous(std::move(InPrevious)),
	  Matches(std::move(InMatches)), Alpha(InAlpha)
{
}

void FDrawListScreenFrame::Set(std::shared_ptr<const Weltall::FDrawList> InLatest, std::shared_ptr<const Weltall::FDrawList> InPrevious,
	std::shared_ptr<const std::vector<int32>> InMatches, float InAlpha)
{
	Latest = std::move(InLatest);
	Previous = std::move(InPrevious);
	Matches = std::move(InMatches);
	Alpha = InAlpha;
}

void FDrawListScreenFrame::AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FIntRect& Area) const
{
	const Weltall::FScreenLayout Layout = Weltall::FScreenLayout::Fit(Area.Width(), Area.Height(), Screen);
	const Weltall::FDisplayRect Placed = Layout.ToDisplay({0, 0, static_cast<int16>(Screen.Width), static_cast<int16>(Screen.Height)});
	const FIntPoint Origin = Area.Min + FIntPoint(Placed.X, Placed.Y);
	const FDrawListFrame Frame{Latest.get(), Previous.get(), Matches.get(), Alpha};
	AddDrawListPasses(GraphBuilder, *Textures, Frame, Output, FIntRect(Origin, Origin + FIntPoint(Placed.Width, Placed.Height)), Screen);
}
