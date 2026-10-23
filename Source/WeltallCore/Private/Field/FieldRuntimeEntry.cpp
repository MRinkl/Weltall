#include "Field/FieldFrameVariables.h"
#include "Field/FieldGears.h"
#include "Field/FieldReturn.h"
#include "Field/FieldRuntime.h"
#include "Gears/GearModels.h"
#include "Input/KernelPad.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		// What the entry's loops hold in the register the sprites' stray steps wait by: the fade-in's loop its
		// brightness step (in 16.16); the loop after a lost battle sets none, and holds the 0xFF of the field's
		// own loop, which the battle it left for keeps.
		constexpr int32 EntryStrayWait = -0x40000;
		constexpr int32 DefeatEntryStrayWait = 0xFF;
		/** The battle's music fades out over this many vertical blanks after a defeat; the game over takes no button meanwhile. */
		constexpr int32 DefeatHoldLength = 0xF0;
		/** After a lost battle the map runs eight ticks under the picture of the screen before the main loop. */
		constexpr int32 DefeatEntryTicks = 8;
		/** After a lost battle the draw buffer at the top left, which shows the picture of the screen, is copied here. */
		constexpr int16 DefeatPictureX = 0x200;
		constexpr int16 DefeatPictureY = 0;
		/** The entry's picture of the screen fades by 4 a tick (16.16) and zooms by 0x20 a tick up to 0x22C0. */
		constexpr int32 EntryCoverStep = 0x40000;
		constexpr int32 CoverZoomStep = 0x20;
		constexpr int32 CoverZoomLimit = 0x22C0;
	}

	std::optional<FError> FFieldRuntime::BeginEntry()
	{
		if (bEntryReturning)
		{
			if (auto Error = RestoreFieldScene(*Services.Game, FieldContext))
			{
				return Error;
			}
		}
		FieldContext.bReturning = false;
		// The pad's queue emptied, the map's song, the frame variables once, the fade-in.
		Services.Pad->Reset();
		Services.Songs->StartMapSong(Entry.bAfterDefeat);
		if (auto Error = UpdateFieldFrameVariables(FieldContext, *Services.Clock))
		{
			return Error;
		}
		if (!Entry.bAfterDefeat)
		{
			FieldContext.Render.StartFadeIn(EntryTicks);
		}
		else
		{
			DefeatHoldVblanks = Entry.bHoldsGameOver ? DefeatHoldLength : 0;
			bDefeatHeldUntilReleased = Entry.bHoldsGameOver;
			// The draw buffer at the top left holds the picture as its last frame drew it, at that buffer's
			// brightness: map 490's panorama reads it where it goes.
			FFieldRenderState& Render = FieldContext.Render;
			FFieldVramOperation Copy;
			Copy.Kind = EFieldVramChange::Copy;
			Copy.Rect = FFieldCover::PictureArea;
			Copy.ToX = DefeatPictureX;
			Copy.ToY = DefeatPictureY;
			Copy.Brightness = Render.Cover.Brightness[0];
			Render.PictureMoves.push_back(Copy);
			Render.WroteVram({DefeatPictureX, DefeatPictureY, Copy.Rect.Width, Copy.Rect.Height});
			Render.bDefeatPicture = true;
		}
		FieldContext.Messages.bMapEntering = true;
		if (Entry.bBattleEventFade && !Entry.bAfterDefeat)
		{
			// The map fades in over the main loop's first ticks.
			EndEntry();
			return std::nullopt;
		}
		FramesLeft = Entry.bAfterDefeat ? DefeatEntryTicks : EntryTicks;
		Phase = EPhase::Entry;
		return std::nullopt;
	}

	std::optional<FError> FFieldRuntime::TickEntry()
	{
		FFieldCover& Cover = FieldContext.Render.Cover;
		Cover.bShown = true;
		if (auto Error = RunFrame(Entry.bAfterDefeat ? DefeatEntryStrayWait : EntryStrayWait, true))
		{
			return Error;
		}
		// The picture fades into the next draw buffer's and zooms; after a lost battle it stays as it is.
		if (!Entry.bAfterDefeat)
		{
			Cover.Brightness[(FieldContext.DrawBuffer + 1) & 1] = static_cast<uint8>(CoverBrightness >> 16);
			CoverBrightness = std::max(CoverBrightness - EntryCoverStep, 0);
			if (Cover.Scale < CoverZoomLimit)
			{
				Cover.Scale += CoverZoomStep;
			}
		}
		if (--FramesLeft == 0)
		{
			EndEntry();
		}
		return std::nullopt;
	}

	std::optional<FError> FFieldRuntime::TickEntryFade()
	{
		// The picture alone on black: each frame gives the next draw buffer's picture its brightness, from
		// full down by 4 until the step has passed 0.
		LastFrameKind = EFieldFrame::Cover;
		FFieldCover& Cover = FieldContext.Render.Cover;
		Cover.bShown = true;
		FieldContext.DrawBuffer ^= 1;
		Cover.Brightness[(FieldContext.DrawBuffer + 1) & 1] = static_cast<uint8>(CoverBrightness >> 16);
		CoverBrightness -= EntryCoverStep;
		if (CoverBrightness >= 0)
		{
			return std::nullopt;
		}
		return BeginEntry();
	}

	void FFieldRuntime::EndEntry()
	{
		FieldContext.Render.RestoreEffectTextures();
		FieldContext.Messages.bMapEntering = false;
		FieldContext.bFieldRunning = true;
		// The entry's blends to a pose snap; the main loop's blend.
		if (FieldContext.Gears.Count != 0 && FieldContext.Gears.Models)
		{
			FieldContext.Gears.Models->SetSnapPoses(false);
		}
		Phase = EPhase::MainLoop;
	}
}
