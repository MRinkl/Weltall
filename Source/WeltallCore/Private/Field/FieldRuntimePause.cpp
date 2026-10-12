#include "Field/FieldRuntime.h"
#include "Field/FieldTick.h"
#include "Game/GameState.h"
#include "Input/KernelPad.h"

namespace Weltall
{
	namespace
	{
		constexpr uint16 StartButton = 0x0800;
	}

	std::optional<FError> FFieldRuntime::TickPause()
	{
		ReadFieldInput(FieldContext, TakeInput().Pad);
		if (IsSoftReset())
		{
			return std::nullopt;
		}
		if ((FieldContext.Controls.Repeated & StartButton) == 0)
		{
			return std::nullopt;
		}
		PauseSound(false);
		Services.Game->SetPlayTime(PauseStartPlayTime);
		Phase = EPhase::MainLoop;
		LastFrameKind = EFieldFrame::Tick;
		return RunIteration(EStep::AfterPause);
	}

	std::optional<FError> FFieldRuntime::TickPadWait()
	{
		ReadFieldInput(FieldContext, TakeInput().Pad);
		if (IsSoftReset() || !Services.Pad->IsConnected())
		{
			return std::nullopt;
		}
		// A controller is back: the pause may follow at once, on the buttons the wait read last.
		PauseSound(false);
		Phase = EPhase::MainLoop;
		LastFrameKind = EFieldFrame::Tick;
		return RunIteration(EStep::AfterPadWait);
	}

	void FFieldRuntime::PauseSound(bool bPaused)
	{
		if (Services.SoundPause == nullptr)
		{
			return;
		}
		if (bPaused)
		{
			Services.SoundPause->PauseSound();
		}
		else
		{
			Services.SoundPause->ResumeSound();
		}
	}
}
