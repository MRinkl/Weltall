#include "Field/FieldRuntime.h"
#include "Field/FieldTick.h"
#include "Foundation/ProcessorDivide.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 MainLoopStrayWait = 0xFF;
		/** The dissolves the scripts ask for (FE 9B-9D). */
		constexpr uint8 FromStillRedrawn = 1;
		constexpr uint8 ToStill = 3;
		/** The picture's brightness runs between none and full (16.16) over the dissolve's ticks. */
		constexpr int32 FullCoverBrightness = 0x800000;
	}

	bool FFieldRuntime::BeginDissolve()
	{
		FEventContext& Context = FieldContext;
		Context.Messages.ResetPortraits();
		Context.Render.SaveEffectTextures();
		// Redrawn before the picture is taken, the first dissolve's frame swaps the draw buffers.
		if (Context.Requests.Dissolve == FromStillRedrawn)
		{
			Context.DrawBuffer ^= 1;
		}
		FFieldCover& Cover = Context.Render.Cover;
		if (Context.Requests.Dissolve == ToStill)
		{
			// To a still: the picture area as the scripts left it, the screen not copied.
			Cover.Take(true, false);
			Context.Render.StartFadeOut(Context.Requests.DissolveTicks);
			Cover.Brightness[(Context.DrawBuffer + 1) & 1] = 0;
			CoverBrightness = 0;
			FramesLeft = 0;
			Phase = Context.Requests.DissolveTicks > 0 ? EPhase::DissolveToStill : EPhase::DissolveHold;
			return true;
		}
		return BeginDissolveFromStill(Context.Requests.Dissolve == FromStillRedrawn);
	}

	bool FFieldRuntime::BeginDissolveFromStill(bool bScreenCopied)
	{
		FEventContext& Context = FieldContext;
		FFieldCover& Cover = Context.Render.Cover;
		Cover.Take(true, bScreenCopied);
		Context.Render.bScreenFadedOut = true;
		Context.Render.StartFadeIn(Context.Requests.DissolveTicks);
		CoverBrightness = FullCoverBrightness;
		FramesLeft = 0;
		if (Context.Requests.DissolveTicks > 0)
		{
			Phase = EPhase::DissolveFromStill;
			return true;
		}
		EndDissolve();
		return false;
	}

	void FFieldRuntime::EndDissolve()
	{
		FEventContext& Context = FieldContext;
		Context.Requests.Dissolve = 0;
		Context.Render.RestoreEffectTextures();
		Phase = EPhase::MainLoop;
	}

	std::optional<FError> FFieldRuntime::TickDissolve()
	{
		FieldContext.Render.Cover.bShown = true;
		if (auto Error = RunFrame(MainLoopStrayWait, true))
		{
			return Error;
		}
		FEventContext& Context = FieldContext;
		FFieldCover& Cover = Context.Render.Cover;
		const int32 Ticks = Context.Requests.DissolveTicks;
		switch (Phase)
		{
		case EPhase::DissolveToStill:
			Cover.Brightness[(Context.DrawBuffer + 1) & 1] = static_cast<uint8>(CoverBrightness >> 16);
			CoverBrightness += ProcessorDivide(FullCoverBrightness, Ticks);
			if (++FramesLeft < Context.Requests.DissolveTicks)
			{
				return std::nullopt;
			}
			if (Context.Requests.Dissolve == ToStill)
			{
				Phase = EPhase::DissolveHold;
				return std::nullopt;
			}
			break;
		case EPhase::DissolveHold:
			if (Context.Requests.Dissolve == ToStill)
			{
				return std::nullopt;
			}
			break;
		case EPhase::DissolveFromStill:
			Cover.Brightness[(Context.DrawBuffer + 1) & 1] = static_cast<uint8>(CoverBrightness >> 16);
			CoverBrightness = std::max(CoverBrightness - ProcessorDivide(FullCoverBrightness, Ticks), 0);
			if (++FramesLeft < Context.Requests.DissolveTicks)
			{
				return std::nullopt;
			}
			EndDissolve();
			return RunIteration(EStep::AfterDissolve);
		default:
			return std::nullopt;
		}
		// Out of the still: a dissolve from the picture area as it stands, unless the scripts asked for none.
		if (Context.Requests.Dissolve > 0 && Context.Requests.Dissolve < 4)
		{
			if (BeginDissolveFromStill(false))
			{
				return std::nullopt;
			}
		}
		else
		{
			EndDissolve();
		}
		return RunIteration(EStep::AfterDissolve);
	}
}
