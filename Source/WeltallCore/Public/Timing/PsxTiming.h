#pragma once

namespace Weltall::PsxTiming
{
	/** The GPU's clock on an NTSC console, which paces the picture. */
	inline constexpr double GpuClockHz = 53693175.0;

	/** One non-interlaced NTSC frame: 263 lines of 3413 GPU clocks, about 59.817 per second. */
	inline constexpr double FrameSeconds = 263.0 * 3413.0 / GpuClockHz;
}
