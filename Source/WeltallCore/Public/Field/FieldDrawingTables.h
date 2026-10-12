#pragma once

#include "Field/FieldCompass.h"
#include "Field/FieldScreenOverlays.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Vram.h"
#include "Media/DiscIdentification.h"

#include <array>
#include <optional>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FPsxExecutable;

	/** Where the field puts a TIM: its pixels at (X, Y), its CLUT at (ClutX, ClutY), ClutWidth x ClutHeight of it when not 0. */
	struct FFieldImagePlace
	{
		int16 X = 0;
		int16 Y = 0;
		int16 ClutX = 0;
		int16 ClutY = 0;
		int16 ClutWidth = 0;
		int16 ClutHeight = 0;
	};

	/** A texture window: the texels (in a texture page) a primitive's texture repeats. */
	struct FFieldTexelWindow
	{
		int16 U = 0;
		int16 V = 0;
		int16 Width = 0;
		int16 Height = 0;
	};

	/**
	 * A picture the field's particles can take: their quad's corners (top left, top right, bottom
	 * left, bottom right) in view units before a particle's size scales them, and each corner's
	 * texel in the effects' texture page, edge to edge.
	 */
	struct FFieldParticleSprite
	{
		std::array<std::array<int16, 2>, 4> Corners{};
		std::array<std::array<int16, 2>, 4> Texels{};
	};

	/**
	 * The tables the field draws its screen with, from the Field program and the kernel: where its
	 * system images (the windows' frame, the cursors, the shadow) and the characters' faces go in
	 * VRAM, where each message window's text is typed, the frame's pieces and the cursors' frames,
	 * the faces' files, the message font's colours, the cockpit's instruments, the particles'
	 * pictures and the compass's pieces.
	 */
	struct FFieldDrawingTables
	{
		static constexpr int32 SystemImageCount = 8;
		static constexpr int32 ParticleSpriteCount = 21;
		static constexpr int32 CursorFrames = 5;
		static constexpr int32 FaceSlots = 3;
		/** The faces' files are this file of directory 4 onward. */
		static constexpr int32 FirstFaceFile = 0x46;

		/** Where each image of the system images' file goes. */
		std::array<FFieldImagePlace, SystemImageCount> SystemImages{};
		/** Where each of the four message windows types its text lines: a 4-bit page's x and y. */
		std::array<std::array<int16, 2>, 4> MessageText{};
		/** The frame's corners and edges: top left, right edge, top right, left edge, bottom left, top edge, bottom right, bottom edge. */
		std::array<FFieldTexelWindow, 8> FramePieces{};
		/** The choice cursor's and the "more" cursor's animation frames. */
		std::array<FFieldTexelWindow, CursorFrames> ChoiceCursor{};
		std::array<FFieldTexelWindow, CursorFrames> MoreCursor{};
		/** Each portrait cell's first texel (u, v) in the faces' page: a slot's second face (drawn mirrored) and its first. */
		std::array<std::array<uint8, 2>, 2 * FaceSlots + 2> PortraitCells{};
		/** Where each face slot's first and second face go, their CLUTs 256 colours in one row. */
		std::array<FFieldImagePlace, 2 * FaceSlots> FacePlaces{};
		/** For each portrait, the faces' two files (from FirstFaceFile). */
		std::vector<uint8> PortraitFiles;
		/** The message font's two 16-colour CLUTs, for its even and odd lines. */
		std::array<uint16, 32> TextClut{};
		FFieldCockpitHudLayout CockpitHud;
		std::array<FFieldParticleSprite, ParticleSpriteCount> ParticleSprites{};
		FFieldCompassLayout Compass;
		/**
		 * The pause's picture (the kernel's own image): its 15-bit pixels, Rect's place its offset from where
		 * the pause loads it, the buffer on display's (PauseX, PauseY).
		 */
		FVramImage PauseImage;
		static constexpr int16 PauseX = 0x88;
		static constexpr int16 PauseY = 100;

		/** The faces' files of a portrait, none past the table. */
		WELTALLCORE_API std::optional<std::array<int32, 2>> FaceFiles(int32 Portrait) const;

		/** Reads them from a disc of the given release, its kernel the executable. */
		WELTALLCORE_API static TResult<FFieldDrawingTables> Read(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Executable,
			EDiscRelease Release);
	};
}
