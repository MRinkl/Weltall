#pragma once

#include "Battling/BattlingArenaData.h"
#include "Battling/BattlingEffects.h"
#include "Battling/BattlingHud.h"
#include "Graphics/Gte.h"
#include "Graphics/MdlLights.h"
#include "Graphics/MdlPackets.h"
#include "Menu/MenuMesWindow.h"
#include "Foundation/Result.h"
#include "Presentation/DrawList.h"
#include "WorldMap/WorldMapDrawing.h"
#include "WorldMap/WorldMapFrame.h"

#include <array>
#include <optional>
#include <vector>

namespace Weltall
{
	class FBattlingFight;
	struct FBattlingMenus;
	class FBattlingTerrain;
	struct FGteArctangents;

	/** Which rules decide what a frame of the arena draws. */
	enum class EBattlingCulling : uint8
	{
		/**
		 * The original's, made for its 320 x 218 screen: the ground's cells in the wedge it rasterizes from the eye, the
		 * triangles that face the eye on its screen, the wall's sector about the eye's bearing.
		 */
		Original,
		/** Everything a view of any width shows within the original's reach: every cell and every piece of the wall that near, facing the eye. */
		InRange,
	};

	/**
	 * What the renderer reads of one pass of a bout, as the pass drew it: the scene's polygons in the world map's frame form
	 * (the same renderer draws both), in the order the GPU draws them, and the screen's 2D layers under and over them.
	 */
	struct FBattlingFrame
	{
		static constexpr int32 ScreenWidth = 320;
		static constexpr int32 ScreenHeight = 218;

		/** The main loop's pass the frame was drawn in. */
		uint32 Pass = 0;
		/**
		 * The screen's columns: ScreenWidth, or twice as many on the winner screen, across the same width (its pixels half as
		 * wide). The scene's view and the 2D layer's corners count them.
		 */
		int32 Columns = ScreenWidth;
		/**
		 * The scene: the sky behind everything (its polygon keeps the screen's corners the original draws it between; its view
		 * corners reach as far across as the widest view), then the arena and the gears.
		 */
		FWorldMapFrame Scene;
		/** The colour the screen is cleared to before the scene: the lighting's far colour, which the ground fades into. */
		FDrawColor Clear;
		/**
		 * A paused pass shows the picture the pause captured under its 2D layer (and no scene), blurred this many times, each
		 * pixel of the original's screen the mean of its 2 x 2 with the pixels right of and below it; 0 none.
		 */
		int32 PictureBlurs = 0;
		/**
		 * The demonstration's: the screen is never cleared, and the frame drawn before this one lies under its scene, each of
		 * its pixels' 5-bit channels at 70h/80h of their value (black pixels left out, the screen as the frame before last left
		 * it showing there).
		 */
		bool bTrails = false;
		/**
		 * The 2D layer over the scene, on the 320 x 218 screen, in the GPU's order: the labels, the HUD, then the scripted
		 * ending's tint, the tutorial's lesson menu and pointer, and the message window.
		 */
		std::vector<FMenuPrimitive> Screen;
		/** The buttons Screen's lines of text show or name (the tutorial's windows, the menus' help lines), as cells of its primitives. */
		std::vector<FTextButtonCell> TextButtons;
	};

	/** The world anchors of a frame's polygons; a gear's shadow is anchored by its side, a gear's part by its side and joint. */
	namespace BattlingAnchors
	{
		inline constexpr uint32 Wall = 0;
		inline constexpr uint32 Ground = 1;
		/** The effects: the sprites, trails and bands, the dust, splashes, lines and hit sparks. */
		inline constexpr uint32 Effects = 2;
		inline constexpr uint32 Sky = 0;

		constexpr uint32 GearJoint(uint32 Side, uint32 Joint) { return Side << 16 | Joint; }
	}

	/** A gear's part as its joint's object keeps it: the packets built as the gear was loaded, and each polygon's page and CLUT. */
	struct FBattlingGearPart
	{
		std::vector<FMdlPacket> Packets;
		std::vector<FMdlTextureState> Textures;
	};

	/**
	 * Draws a bout's frames as the program does: the HUD and the labels, the gears, the effects, the ground, the wall, the gears'
	 * shadows and the sky. Kept from frame to frame, it allocates only as frames grow.
	 */
	class FBattlingDrawing
	{
	public:
		/** Everything must outlive the drawing. */
		WELTALLCORE_API FBattlingDrawing(const FBattlingArenaTables& InTables, const FBattlingArenaTextures& InTextures, const FBattlingTerrain& InTerrain,
			const FGteArctangents& InArctangents);

		/**
		 * Builds the two gears' packets as loading them does: every joint's part lit once under Lighting (the GTE's lights as
		 * the load finds them, which only the kinds the frames never light again keep), its textures on the side's own page
		 * and palette row. An error names a part the builder cannot build.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> LoadGears(const FBattlingFight& Fight, const FGte& Lighting);

		/** The frame of the pass the bout just ran. */
		WELTALLCORE_API void Draw(const FBattlingFight& Fight, FBattlingFrame& Frame);

		/**
		 * The frame of a pass of the menus' demonstration, as the program draws it: the two gears lit as the bout's are, the hit
		 * sparks and the lines, no arena, over the frame before dimmed (FBattlingFrame::bTrails).
		 */
		WELTALLCORE_API void DrawDemonstration(const FBattlingFight& Fight, FBattlingFrame& Frame);

		/**
		 * The frame of a pass of the winner screen, as the program draws it on its display of 640 x 218 over black: the winning
		 * gear turning alone, lit as the last frame of the bout left the lights, its shadow cast on the ground beneath it, and its
		 * text.
		 */
		WELTALLCORE_API void DrawWinner(const FBattlingFight& Fight, FBattlingFrame& Frame);

		/** Appends the 2D layer of the pass the bout just ran (FBattlingFrame::Screen) to Out, the buttons its text shows to TextButtons when given. */
		WELTALLCORE_API void DrawScreen(const FBattlingFight& Fight, std::vector<FMenuPrimitive>& Out, std::vector<FTextButtonCell>* TextButtons = nullptr) const;

		/**
		 * The versus screen of a story bout, as a pass of it draws it: the fire at C0h (darkening the screen under it in the
		 * load's passes, bLoading, not while the gears are read), VS and the gears' names, both portraits at rest.
		 */
		WELTALLCORE_API void DrawVersus(const FBattlingFight& Fight, const std::array<int32, 2>& Opponents, bool bLoading, std::vector<FMenuPrimitive>& Out) const;

		/**
		 * The 2D layer of a paused pass, as the program links it: over the paused picture, the open menu's box darkening
		 * it, the help lines and the menu's letters. True when the picture is drawn under it: while a menu is open, and on the
		 * pass that resumes the bout.
		 */
		WELTALLCORE_API bool DrawPause(const FBattlingFight& Fight, std::vector<FMenuPrimitive>& Out, std::vector<FTextButtonCell>* TextButtons = nullptr) const;

		/**
		 * A pass of the arena's menus, as the program links it: the fire at C0h, the help lines and the letters over it, and
		 * over them the gear choice's portraits, the last the carousels showed first. A step of the chosen gears' load
		 * (bLoading) draws the same, the fire darkening the screen under it.
		 */
		WELTALLCORE_API void DrawMenus(const FBattlingFight& Fight, bool bLoading, std::vector<FMenuPrimitive>& Out,
			std::vector<FTextButtonCell>* TextButtons = nullptr) const;

		/**
		 * The lines the pass's message windows typed, drawn into their images, and the pause's help lines the pass changed
		 * (both in one image, a line in each plane of its texels), and the images' uploads into Uploads (cleared
		 * first), which VRAM takes before the frame is drawn. After every pass, whether it draws a frame or not: the message
		 * window types in every pass.
		 */
		WELTALLCORE_API void TypeWindows(const FBattlingFight& Fight, std::vector<FVramImage>& Uploads);

		/** Whether the gears are drawn (once loaded). */
		bool bGears = true;

		/** The GTE's light matrix as the last gear part drawn left it: what the program after Battling finds there. */
		const FGteMatrix& LightLeft() const { return LightMatrixLeft; }

		EBattlingCulling Culling = EBattlingCulling::Original;

	private:
		const FBattlingArenaTables& Tables;
		const FBattlingArenaTextures& Textures;
		const FBattlingTerrain& Terrain;
		const FGteArctangents& Arctangents;
		FWorldMapFrameBuilder Builder;
		FBattlingHud Hud;
		std::array<FBattlingSpriteSet, 5> SpriteSets;
		/** Each side's parts by joint; empty for a joint without one. */
		std::array<std::vector<FBattlingGearPart>, 2> Gears;
		std::vector<FGteProjection> Projected;
		/** The lights the last gear drawn left in the GTE, which the winner screen lights its gear with. */
		FMdlLights LightsLeft;
		/** A part's corners cast on the ground, for the winner screen's shadow. */
		std::vector<FGteVector> CastCorners;
		/**
		 * The light every surface of a gear receives: the lighting's back colour as the frame before set it (the program sets
		 * it after it has drawn the gears), none before the first frame; and whether the hit lights are halved, which the
		 * program reads of the lighting the frame before set too (before the first frame through a null pointer, at the
		 * memory's 10h, which the port takes as clear). The demonstration, which sets neither, keeps what the last bout left.
		 */
		FDrawColor GearAmbient{0, 0, 0};
		bool bHitLightsHalved = false;
		FGteMatrix LightMatrixLeft;

		/**
		 * Where a trail's segment was drawn last, as its slot keeps it: its two corners on the screen and in that frame's
		 * view, and its point on the screen. The next segment of its trail is drawn on to them.
		 */
		struct FTrailMark
		{
			std::array<std::array<int16, 2>, 2> Corners{};
			std::array<std::array<float, 3>, 2> View{};
			std::array<int16, 2> Point{};
		};
		std::array<FTrailMark, FBattlingEffects::SlotCount> TrailMarks{};

		/** The text images of the message window and the lesson menu, as the kernel keeps them. */
		FMesWindowImage TextImage;
		FMesWindowImage MenuImage;
		/** The pause's help lines: their image, the one-line window 3Fh units wide they are typed through, and each line's width as typed last. */
		FVramImage HelpImage;
		FMesWindow HelpTyping{0x3F, 1};
		std::array<int32, 2> HelpWidths{};
		/** The buttons each help line shows, as typed last: cells of the line from its left end. */
		std::array<std::vector<FTextButtonCell>, 2> HelpButtons;
		/** Finds the buttons in the windows' rows and the help lines. */
		mutable FMesButtonCells ButtonCells;

		/** How a gear is drawn: its root hung from the GTE's matrix at RootPlace, under Lights, on a screen ClipColumns wide. */
		struct FGearView
		{
			FGte Gte;
			FGteLongVector RootPlace{};
			FMdlLights Lights;
			uint16 ClipColumns = FBattlingFrame::ScreenWidth;
			FMdlEnvironmentMapping Environment;
		};

		/** The lights the bout's drawing sets for a gear: the scene's, the hit's from the opponent's side, and one from the eye that gives none. */
		FMdlLights GearLights(const FBattlingFight& Fight, uint32 Side) const;
		void DrawGear(const FBattlingFight& Fight, uint32 Side, const FGearView& View);
		/** The winner screen's shadow of the gear on Side (ModelShadow_*), its cast corners drawn through Gte. */
		void DrawWinnerShadow(const FBattlingFight& Fight, uint32 Side, const FGte& Gte);
		/** The fire at a fade, darkening the screen under it when asked and the fade is above 80h. */
		static void AppendFire(int32 Fade, bool bDarkens, std::vector<FMenuPrimitive>& Out);
		/**
		 * A gear's portrait at its side's place moved Offset pixels right, as it is at Brightness 40h, else
		 * darkened and shrunk about its middle.
		 */
		static void AppendPortrait(int32 Gear, int32 Side, int32 Offset, int32 Brightness, std::vector<FMenuPrimitive>& Out);
		/** The help lines' page and the lines showing, the second first. */
		void AppendHelpLines(const FBattlingMenus& Menus, std::vector<FMenuPrimitive>& Out, std::vector<FTextButtonCell>* TextButtons) const;
		/** The effects, then the sparks, with the view the scene is drawn through. */
		void DrawEffects(const FBattlingFight& Fight, const FGte& View, bool bSparksAndLinesAlone = false);
		/** The frame's scene begun as the camera's view sees it, which it returns. */
		FGte BeginScene(const FBattlingFight& Fight, FBattlingFrame& Frame);
		/** Both gears as the bout and the demonstration draw them. */
		void DrawGears(const FBattlingFight& Fight, const FGte& View);
	};
}
