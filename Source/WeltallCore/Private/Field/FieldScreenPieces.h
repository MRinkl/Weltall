#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	struct FDrawList;
	struct FFieldCockpitHud;
	struct FFieldCockpitHudLayout;
	struct FFieldCompassFrame;
	struct FFieldDrawingTables;
	struct FFieldCover;
	struct FFieldMapPicture;
	struct FFieldScreenMarkers;

	/** The pieces of the field's 2D layer beside the fades and the message windows, each as the field links them. */
	namespace FieldScreenPieces
	{
		/** The cockpit's instruments, in the layer table's entry 4 (before the fades and the windows): the last piece first. */
		void AddCockpitHud(const FFieldCockpitHudLayout& Layout, const FFieldCockpitHud& Hud, FDrawList& Layer);

		/** The compass's quads as the tick links them into the layer table's entry 1, under the scripts' fade layer. */
		void AddCompass(const FFieldCompassFrame& Compass, FDrawList& Layer);

		/** FE DF's still: its five 128-pixel columns across the 640-pixel display (x in its pixels). */
		void AddHiResStill(FDrawList& Layer);

		/** The pause's picture, written into the buffer on display over everything drawn. */
		void AddPausePicture(const FFieldDrawingTables& Tables, int32 DrawBuffer, FDrawList& Layer);

		/** The markers the tick placed, the last placed first: drawn after everything else in the layer table. */
		void AddScreenMarkers(const FFieldScreenMarkers& Markers, FDrawList& Layer);

		/**
		 * The picture of the screen's five columns as VRAM's picture area holds them, at Brightness and
		 * the picture's scale about the screen's centre: added (every texel, as the field marks them),
		 * or opaque.
		 */
		void AddCoverColumns(const FFieldCover& Cover, uint8 Brightness, FDrawList& Layer);

		/** Select's picture: its three columns of the 8-bit picture, then the leader's marker over them, at the picture's brightness. */
		void AddMapPicture(const FFieldMapPicture& Picture, FDrawList& Layer);
	}
}
