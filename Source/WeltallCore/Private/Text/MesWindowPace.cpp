#include "Text/MesWindow.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** Past this many ticks a string is not timed further: it types on for minutes, or never ends. */
		constexpr int32 PaceLimit = 0x2000;
	}

	void FMesWindow::MeasurePace(const FMesContext& Context)
	{
		PaceTicks.clear();
		ScrollTicks.clear();
		PaceStretch = 0;
		StretchTicks = 0;
		OwedWait = 0;
		Scrolled = 0;
		if (Context.Speed == EMesTextSpeed::Original)
		{
			return;
		}
		// The same string in a window of the same shape at the game's own speed, the button pressed at every stop.
		FMesContext Own = Context;
		Own.Speed = EMesTextSpeed::Original;
		FMesWindow Paced(Width, LineCount);
		Paced.BaseSpeed = BaseSpeed;
		Paced.Numbers = Numbers;
		Paced.Selector = Selector;
		Paced.Queue(Current);
		ScrollTicks.emplace_back();
		bool bTimedEnd = false;
		int32 Ticks = 0;
		for (int32 Tick = 0; Tick < PaceLimit; ++Tick)
		{
			const int16 LinesBefore = Paced.LinesTyped;
			Paced.Update(Own);
			++Ticks;
			if (Paced.LinesTyped > LinesBefore && (Paced.Flags & FlagScrolling) != 0)
			{
				ScrollTicks.back().push_back(Paced.StretchTicks);
			}
			if (Paced.bClosing && !Paced.IsStopped())
			{
				// A timed end: the one the wait is lengthened for, unless the button answers its wait.
				bTimedEnd = (Paced.Flags & FlagAutoAdvance) == 0;
				if (bTimedEnd)
				{
					PaceTicks.push_back(Ticks);
				}
				break;
			}
			if (!Paced.HasText() || (Paced.IsStopped() && Paced.StopReason() == EMesStop::End))
			{
				break;
			}
			if (Paced.IsStopped())
			{
				PaceTicks.push_back(Ticks);
				Ticks = 0;
				Paced.Continue();
				ScrollTicks.emplace_back();
			}
		}
		if (!bTimedEnd)
		{
			PaceTicks.clear();
		}
	}

	bool FMesWindow::HoldsScroll() const
	{
		// The next step starts a line, and the line scrolls the window: the top line goes.
		if (X <= Width || ((Flags & FlagScrolling) == 0 && Line + 1 < LineCount) || PaceStretch >= ScrollTicks.size())
		{
			return false;
		}
		const std::vector<int32>& Scrolls = ScrollTicks[PaceStretch];
		return Scrolled < Scrolls.size() && StretchTicks < Scrolls[Scrolled];
	}

	void FMesWindow::DropOwedWait()
	{
		WaitTimer = static_cast<int16>(std::max(WaitTimer - OwedWait, 0));
		OwedWait = 0;
	}
}
