#include "Battling/BattlingDrawing.h"

#include "Battling/BattlingFight.h"
#include "Battling/BattlingLabels.h"
#include "Text/MesRender.h"

#include <span>
#include <string_view>

namespace Weltall
{
	namespace
	{
		/** The ending's tint covers the screen, blending by its drawing mode, dithered. */
		constexpr int16 TintWidth = FBattlingFrame::ScreenWidth;
		constexpr int16 TintHeight = FBattlingFrame::ScreenHeight;
		constexpr uint8 HalfTile = 0x62;
		constexpr uint32 Dithered = 0x200;
		constexpr int32 BlendAdd = 1;
		constexpr int32 BlendSubtract = 2;
		constexpr int16 PointerSize = 16;
		/** The fire's picture: the texels it spreads over the screen, its palette's place and page. */
		constexpr uint8 FireLastU = 0x6F;
		constexpr uint8 FireLastV = 0x2A;
		constexpr int32 FirePaletteX = 0;
		constexpr int32 FirePaletteY = 0x1FD;
		/** Above this fade the fire darkens the screen under it. */
		constexpr int32 FireDarkensFrom = 0x80;
		constexpr uint8 Unmodulated = 0x80;
		/** The versus screen: the fire at C0h, VS between the names under the portraits, each portrait at rest. */
		constexpr int32 VersusFade = 0xC0;
		constexpr std::array<int32, 3> VersusLabelsX{0x50, 0xA0, 0xF0};
		constexpr int32 VersusLabelsY = 0x78;
		constexpr std::string_view VersusText = "VS";
		constexpr std::array<int16, 2> PortraitLeft{0x33, 0xD3};
		constexpr int16 PortraitWidth = 0x3C;
		constexpr int16 PortraitTop = 0x30;
		constexpr int16 PortraitBottom = 0x70;
		constexpr int32 PortraitLastU = 0x3B;
		constexpr int32 PortraitLastV = 0x3F;
		constexpr int32 PortraitPageMask = 0xFF80;
		/**
		 * A portrait's brightness at rest; below it the portrait is shrunk about the middle of this box, its edges a pixel
		 * further out for every 10h of brightness.
		 */
		constexpr int32 PortraitLit = 0x40;
		constexpr int16 ShrunkLeft = 4;
		constexpr int16 ShrunkTop = 0x34;
		constexpr int16 ShrunkWidth = 0x34;
		constexpr int16 ShrunkBottom = 0x6C;
		/** The arena's menus show the fire at C0h, without darkening the screen. */
		constexpr int32 MenusFade = 0xC0;
		/**
		 * The pause: its menu's box (a tile subtracting this colour), its help lines' image (both lines, at the text's images'
		 * place), the lines' rows and height.
		 */
		constexpr FDrawColor PauseBoxColour{0x20, 0x20, 0x10};
		constexpr FVramRect HelpImagePlace{0x140, 0x30, 0x42, 0x0D};
		constexpr std::array<int16, 2> HelpLineY{0xB4, 0xC3};

		/** A drawing-mode command alone, its texture window word the GPU's no-operation (libgpu's SetDrawTPage, or SetDrawMode without a window). */
		FMenuPrimitive ModeAlone(uint32 Word)
		{
			FMenuPrimitive Mode;
			Mode.Code = MenuGpu::DrawModeCommand;
			Mode.Words = {Word, 0};
			return Mode;
		}

		FMesWindowPlace TextPlace(const FBattlingTutorial& Tutorial)
		{
			return {FBattlingTutorial::ImagesX, FBattlingTutorial::TextImageY, FBattlingTutorial::TextX, Tutorial.TextDrawnY,
				MenuGpu::Clut(FBattlingTutorial::PalettesX, FBattlingTutorial::PalettesY), MenuGpu::Clut(FBattlingTutorial::PalettesX + 0x10, FBattlingTutorial::PalettesY)};
		}

		FMesWindowPlace MenuPlace()
		{
			return {FBattlingTutorial::ImagesX, FBattlingTutorial::MenuImageY, FBattlingTutorial::MenuX, FBattlingTutorial::MenuY,
				MenuGpu::Clut(FBattlingTutorial::PalettesX, FBattlingTutorial::PalettesY), MenuGpu::Clut(FBattlingTutorial::PalettesX + 0x10, FBattlingTutorial::PalettesY)};
		}
	}

	void FBattlingDrawing::AppendFire(int32 Fade, bool bDarkens, std::vector<FMenuPrimitive>& Out)
	{
		Out.push_back(ModeAlone(MenuGpu::DrawMode(MenuGpu::TexturePage(0, BlendSubtract, 0, 0)) | Dithered));
		if (bDarkens && Fade > FireDarkensFrom)
		{
			// Twice the fade in each channel, each channel's carry falling into the next (and the last's into the code).
			const uint32 Word = static_cast<uint32>(Fade << 1 | Fade << 9 | Fade << 17);
			FMenuPrimitive& Darken = Out.emplace_back();
			Darken.Code = static_cast<uint8>(HalfTile | Word >> 24);
			Darken.Corners[0].Color = {static_cast<uint8>(Word), static_cast<uint8>(Word >> 8), static_cast<uint8>(Word >> 16)};
			Darken.Width = TintWidth;
			Darken.Height = TintHeight;
			Darken.bWholeScreen = true;
		}
		FMenuPrimitive& Fire = Out.emplace_back();
		const uint8 Level = static_cast<uint8>(Fade * 2 / 3);
		Fire.Code = MenuGpu::TexturedQuad | MenuGpu::SemiTransparent | (Level == Unmodulated ? MenuGpu::RawTexture : 0);
		for (std::size_t Corner = 0; Corner < 4; ++Corner)
		{
			const bool bRight = (Corner & 1) != 0;
			const bool bDown = (Corner & 2) != 0;
			FMenuCorner& At = Fire.Corners[Corner];
			At.X = bRight ? TintWidth : int16{0};
			At.Y = bDown ? TintHeight : int16{0};
			At.U = bRight ? FireLastU : uint8{0};
			At.V = bDown ? FireLastV : uint8{0};
			At.Color = {Level, Level, Level};
		}
		Fire.Clut = MenuGpu::Clut(FirePaletteX, FirePaletteY);
		Fire.Page = MenuGpu::TexturePage(1, BlendAdd, BattlingFirePicturePlace.X, BattlingFirePicturePlace.Y);
		Fire.bWholeScreen = true;
	}

	void FBattlingDrawing::AppendPortrait(int32 Gear, int32 Side, int32 Offset, int32 Brightness, std::vector<FMenuPrimitive>& Out)
	{
		// At rest, a picture's 60 x 64 at its side's place. The texels' words take x twice into u, whose ninth and tenth bits
		// fall into v: the picture is read from its third or fourth row down, stretched over the 64.
		const FVramRect Place = BattlingPortraitPlace(Gear);
		const FVramRect Palette = BattlingPortraitPalettePlace(Gear);
		const int32 Left = PortraitLeft[static_cast<std::size_t>(Side)] + Offset;
		const bool bLit = Brightness == PortraitLit;
		const int32 Grow = Brightness >> 4;
		const uint8 Level = static_cast<uint8>(Brightness + PortraitLit);
		FMenuPrimitive& Portrait = Out.emplace_back();
		Portrait.Code = MenuGpu::TexturedQuad | (bLit ? MenuGpu::RawTexture : 0);
		for (std::size_t Corner = 0; Corner < 4; ++Corner)
		{
			const bool bRight = (Corner & 1) != 0;
			const bool bDown = (Corner & 2) != 0;
			FMenuCorner& At = Portrait.Corners[Corner];
			if (bLit)
			{
				At.X = static_cast<int16>(Left + (bRight ? PortraitWidth : 0));
				At.Y = bDown ? PortraitBottom : PortraitTop;
			}
			else
			{
				const int32 ShrunkX = Left - (Grow - ShrunkLeft);
				At.X = static_cast<int16>(ShrunkX + (bRight ? ShrunkWidth + 2 * Grow : 0));
				At.Y = static_cast<int16>(bDown ? ShrunkBottom + Grow : ShrunkTop - Grow);
				At.Color = {Level, Level, Level};
			}
			const uint32 Texel = static_cast<uint32>(Place.X * 2 + (bRight ? PortraitLastU : 0)) | static_cast<uint32>(Place.Y + (bDown ? PortraitLastV : 0)) << 8;
			At.U = static_cast<uint8>(Texel);
			At.V = static_cast<uint8>(Texel >> 8);
		}
		Portrait.Clut = MenuGpu::Clut(Palette.X, Palette.Y);
		Portrait.Page = MenuGpu::TexturePage(1, 0, Place.X & PortraitPageMask, Place.Y);
	}

	void FBattlingDrawing::AppendHelpLines(const FBattlingMenus& Menus, std::vector<FMenuPrimitive>& Out, std::vector<FTextButtonCell>* TextButtons) const
	{
		if (Menus.HelpShown[0] != 0 || Menus.HelpShown[1] != 0)
		{
			Out.push_back(ModeAlone(MenuGpu::DrawMode(MenuGpu::TexturePage(0, 0, HelpImagePlace.X, HelpImagePlace.Y))));
		}
		for (std::size_t Line = Menus.HelpShown.size(); Line-- > 0;)
		{
			if (Menus.HelpShown[Line] == 0)
			{
				continue;
			}
			FMenuPrimitive& Help = Out.emplace_back();
			Help.Code = MenuGpu::Sprite | MenuGpu::RawTexture;
			Help.Corners[0].X = static_cast<int16>((FBattlingFrame::ScreenWidth - HelpWidths[Line]) / 2);
			Help.Corners[0].Y = HelpLineY[Line];
			Help.Corners[0].V = static_cast<uint8>(HelpImagePlace.Y);
			Help.Clut = MenuGpu::Clut(FBattlingTutorial::PalettesX + static_cast<int32>(Line) * 0x10, FBattlingTutorial::PalettesY);
			Help.Width = static_cast<int16>(HelpWidths[Line]);
			Help.Height = static_cast<int16>(HelpImagePlace.Height);
			if (TextButtons != nullptr)
			{
				for (FTextButtonCell Cell : HelpButtons[Line])
				{
					Cell.Primitive = static_cast<uint32>(Out.size() - 1);
					TextButtons->push_back(Cell);
				}
			}
		}
	}

	void FBattlingDrawing::DrawScreen(const FBattlingFight& Fight, std::vector<FMenuPrimitive>& Out, std::vector<FTextButtonCell>* TextButtons) const
	{
		const FMesWindowButtons Buttons{TextButtons != nullptr ? &ButtonCells : nullptr, Fight.Text != nullptr ? Fight.Text->Font : nullptr, TextButtons};
		// The labels are linked after the HUD, so drawn under it, the last printed first.
		for (auto Letter = Fight.FrameLabels.rbegin(); Letter != Fight.FrameLabels.rend(); ++Letter)
		{
			Out.push_back(MakeLabelPrimitive(*Letter, Textures.FontPage, Textures.FontClut));
		}
		if (FBattlingHud::IsDrawn(Fight))
		{
			Hud.Draw(Fight, Out);
		}
		// What the pass's tick linked before the frame's drawing lies over it, the first linked on top: the message window
		// (updated as the pass begins), the pointer, the lesson menu, the ending's tint.
		const FBattlingTutorial& Tutorial = Fight.Tutorial;
		// A level below 0 makes the tile's words ones the GPU does nothing with; its drawing mode is sent all the same.
		if (Tutorial.bTintDrawn)
		{
			const int32 Blend = Tutorial.bTintSubtracts ? BlendSubtract : BlendAdd;
			Out.push_back(ModeAlone(MenuGpu::DrawMode(MenuGpu::TexturePage(0, Blend, 0, 0)) | Dithered));
		}
		if (Tutorial.bTintDrawn && Tutorial.Tint >= 0 && Tutorial.Tint <= 0xFF)
		{
			FMenuPrimitive& Tint = Out.emplace_back();
			Tint.Code = HalfTile;
			const uint8 Level = static_cast<uint8>(Tutorial.Tint);
			Tint.Corners[0].Color = {Level, Level, Level};
			Tint.Width = TintWidth;
			Tint.Height = TintHeight;
			Tint.bWholeScreen = true;
		}
		if (Tutorial.bMenuDrawn)
		{
			AppendMesWindowPrimitives(Tutorial.Menu, MenuPlace(), Out, Buttons);
		}
		if (Tutorial.bPointerDrawn)
		{
			Out.push_back(ModeAlone(MenuGpu::DrawMode(Textures.PointerPage)));
			FMenuPrimitive& Pointer = Out.emplace_back();
			Pointer.Code = MenuGpu::Sprite | MenuGpu::RawTexture;
			Pointer.Corners[0].X = static_cast<int16>(Tutorial.PointerDrawn[0]);
			Pointer.Corners[0].Y = static_cast<int16>(Tutorial.PointerDrawn[1]);
			Pointer.Corners[0].U = static_cast<uint8>(Textures.PointerTexel);
			Pointer.Corners[0].V = static_cast<uint8>(Textures.PointerTexel >> 8);
			Pointer.Clut = Textures.PointerClut;
			Pointer.Width = PointerSize;
			Pointer.Height = PointerSize;
		}
		AppendMesWindowPrimitives(Tutorial.Text, TextPlace(Tutorial), Out, Buttons);
		// The fire the bout fades in through, linked as the pass began: over everything else.
		if (Fight.bFireDrawn)
		{
			AppendFire(Fight.FireDrawnFade, true, Out);
		}
	}

	void FBattlingDrawing::DrawVersus(const FBattlingFight& Fight, const std::array<int32, 2>& Opponents, bool bLoading, std::vector<FMenuPrimitive>& Out) const
	{
		// While the gears are read the fire is linked into the scene's table, which the portraits, linked first into the main
		// list, lie over; the load's passes link it after them into the main list, the screen darkened under it.
		AppendFire(VersusFade, bLoading, Out);
		FBattlingLabels Labels(Fight.Tables);
		Labels.SetFlash(false, 0);
		Labels.SetPosition(VersusLabelsX[1], VersusLabelsY);
		Labels.PrintCentered(VersusText, 0);
		for (std::size_t Side = 0; Side < Opponents.size(); ++Side)
		{
			Labels.SetPosition(VersusLabelsX[Side == 0 ? 0 : 2], VersusLabelsY);
			Labels.PrintCentered(Fight.Tables.GearNames[static_cast<std::size_t>(Opponents[Side])], 0);
		}
		const std::span<const FBattlingLabelGlyph> Letters = Labels.Take(0);
		for (auto Letter = Letters.rbegin(); Letter != Letters.rend(); ++Letter)
		{
			Out.push_back(MakeLabelPrimitive(*Letter, Textures.FontPage, Textures.FontClut));
		}
		for (std::size_t Side = Opponents.size(); Side-- > 0;)
		{
			AppendPortrait(Opponents[Side], static_cast<int32>(Side), 0, PortraitLit, Out);
		}
	}

	void FBattlingDrawing::DrawMenus(const FBattlingFight& Fight, bool bLoading, std::vector<FMenuPrimitive>& Out,
		std::vector<FTextButtonCell>* TextButtons) const
	{
		AppendFire(MenusFade, bLoading, Out);
		AppendHelpLines(Fight.Menus, Out, TextButtons);
		for (auto Letter = Fight.FrameLabels.rbegin(); Letter != Fight.FrameLabels.rend(); ++Letter)
		{
			Out.push_back(MakeLabelPrimitive(*Letter, Textures.FontPage, Textures.FontClut));
		}
		const FBattlingGearChoice& Choice = Fight.Menus.Gears;
		for (int32 Shown = Choice.PortraitCount; Shown-- > 0;)
		{
			const FBattlingGearChoice::FShown& Portrait = Choice.Portraits[static_cast<std::size_t>(Shown)];
			AppendPortrait(Portrait.Gear, Portrait.Side, Portrait.Offset, Portrait.Brightness, Out);
		}
	}

	bool FBattlingDrawing::DrawPause(const FBattlingFight& Fight, std::vector<FMenuPrimitive>& Out, std::vector<FTextButtonCell>* TextButtons) const
	{
		// Linked into the scene's table one before another, so the GPU draws them in the reverse: the picture's move, the box's
		// subtracting mode and the box, the help lines' page and the lines (the second first), the letters (the last printed first).
		const FBattlingMenus& Menus = Fight.Menus;
		const bool bMenuShown = Menus.Current != EBattlingMenu::None && Menus.bCaptureShown;
		if (bMenuShown)
		{
			const FBattlingMenuPlace Place = BattlingMenuPlace(Menus.Current, Fight.Labels);
			Out.push_back(ModeAlone(MenuGpu::DrawMode(MenuGpu::TexturePage(0, BlendSubtract, 0, 0))));
			FMenuPrimitive& Box = Out.emplace_back();
			Box.Code = HalfTile;
			Box.Corners[0].X = Place.BoxX;
			Box.Corners[0].Y = Place.BoxY;
			Box.Corners[0].Color = PauseBoxColour;
			Box.Width = Place.BoxWidth;
			Box.Height = Place.BoxHeight;
		}
		AppendHelpLines(Menus, Out, TextButtons);
		for (auto Letter = Fight.FrameLabels.rbegin(); Letter != Fight.FrameLabels.rend(); ++Letter)
		{
			Out.push_back(MakeLabelPrimitive(*Letter, Textures.FontPage, Textures.FontClut));
		}
		return bMenuShown || Menus.bCaptureAfterResume;
	}

	void FBattlingDrawing::TypeWindows(const FBattlingFight& Fight, std::vector<FVramImage>& Uploads)
	{
		Uploads.clear();
		if (Fight.Text == nullptr)
		{
			return;
		}
		// A help line the pass typed goes into its plane (the other plane, and what a longer line left past the new one, stay),
		// and the whole image is uploaded.
		bool bHelpTyped = false;
		for (std::size_t Line = 0; Line < HelpWidths.size(); ++Line)
		{
			const uint8 Message = Fight.Menus.HelpTyped[Line];
			if (Message == 0 || Fight.Messages == nullptr)
			{
				continue;
			}
			if (HelpImage.Units.empty())
			{
				HelpImage.Rect = HelpImagePlace;
				HelpImage.Units.assign(static_cast<std::size_t>(HelpImagePlace.Width * HelpImagePlace.Height), 0);
			}
			HelpWidths[Line] = RenderMesString(*Fight.Text, HelpTyping, Fight.Messages->String(Message), HelpImage, static_cast<int32>(Line));
			HelpButtons[Line].clear();
			ButtonCells.AddTyped(HelpTyping.TypedGlyphs(), HelpWidths[Line], Fight.Text->Font, 0, HelpButtons[Line]);
			bHelpTyped = true;
		}
		if (bHelpTyped)
		{
			Uploads.push_back(HelpImage);
		}
		const FBattlingTutorial& Tutorial = Fight.Tutorial;
		if (Tutorial.bMenuDrawn)
		{
			MenuImage.Type(Fight.Text->Font, Tutorial.Menu, MenuPlace(), Uploads);
		}
		TextImage.Type(Fight.Text->Font, Tutorial.Text, TextPlace(Tutorial), Uploads);
	}
}
