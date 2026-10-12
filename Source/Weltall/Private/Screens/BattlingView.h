#pragma once

#include "CoreMinimal.h"
#include "Menu/MenuButtonIcons.h"
#include "Menu/MenuDrawing.h"
#include "Rendering/WorldMapRendering.h"
#include "Screens/GameScreen.h"

class FBattlingScreenFrame;
class FBattlingTrails;
class FVramTexture;

namespace Weltall
{
	class FBattlingGameProgram;
	class IButtonGlyphs;
}

/**
 * The Battling program shown at the display's rate: its latest frame's scene (the sky, the arena's ground and wall, the
 * gears and their shadows) over the frame's clear colour and its labels and HUD over the scene, textured from the program's VRAM,
 * drawn in between the pass before and the latest; a paused frame's layer over the picture the pause captured, blurred; the
 * menus' demonstration's gears over its frames before, dimmed; black before its first frame. The buttons its text shows or
 * names (the tutorial's messages, the menus' help lines) are the player's own, as the glyph service shows them.
 */
class FBattlingView
{
public:
	explicit FBattlingView(const FWorldMapRenderOptions& InOptions);
	~FBattlingView();

	/** How the next frames are drawn (the player's settings changed). */
	void SetOptions(const FWorldMapRenderOptions& InOptions) { Options = InOptions; }

	/** How the player's buttons are shown; none: the game's own text. It must outlive the view. */
	void SetButtonGlyphs(const Weltall::IButtonGlyphs* InGlyphs) { Glyphs = InGlyphs; }

	/** After each of the program's frames: VRAM as it stands, and the two frames' anchors paired. bNewProgram: another run, whose frames start afresh. */
	void Follow(const Weltall::FBattlingGameProgram& Program, bool bNewProgram);

	/** What the display shows Alpha of the way from the program's frame before to its latest; nothing before its first. */
	TSharedPtr<const IScreenFrame> Capture(const Weltall::FBattlingGameProgram& Program, float Alpha) const;

private:
	/** Shows the buttons of the layer the screen's builder just built as the player's own. */
	void ShowTextButtons(Weltall::FDrawList& Layer) const;

	std::vector<int32> AnchorMatches;
	std::vector<std::pair<uint32, int32>> MatchScratch;
	bool bPreviousFollows = false;
	mutable Weltall::FWorldMapFrameBlend Blend;
	/** The two frames' 2D layers as the menus' builder takes them, and the builder, which draws the gauges between the two. */
	mutable Weltall::FMenuFrame ScreenLatest;
	mutable Weltall::FMenuFrame ScreenPrevious;
	/** The paused picture's 2D layer, as the builder takes it. */
	mutable Weltall::FMenuFrame ScreenPicture;
	mutable Weltall::FMenuDrawBuilder ScreenBuilder;
	FWorldMapRenderOptions Options;
	TSharedRef<FVramTexture> Vram;
	const Weltall::IButtonGlyphs* Glyphs = nullptr;
	mutable Weltall::FMenuButtonIcons Icons;
	TSharedRef<FVramTexture> IconVram;
	/** The field scene's passes draw a scene's mesh and the frame's own polygons; the arena's are all its own. */
	TSharedRef<const FFieldSceneMesh> NoMesh;
	/** Frames handed out; one the rendering thread no longer holds is filled again. */
	mutable TArray<TSharedRef<FBattlingScreenFrame>> DisplayFrames;
	/** The demonstration's pictures, its runs counted (each starts them black), and whether the latest frame was one of it. */
	TSharedRef<FBattlingTrails> Trails;
	uint32 TrailsRun = 0;
	bool bTrailsShown = false;
};
