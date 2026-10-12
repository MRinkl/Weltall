#pragma once

#include "Foundation/Result.h"
#include "Graphics/Vram.h"
#include "Presentation/DrawList.h"

namespace Weltall
{
	class FPsxExecutable;

	/**
	 * The publisher's notice the game shows before its first movie: a 256 x 48 picture from the
	 * executable, brightened in 16 vertical blanks to 120, held 110, then from 128 back to black in 17.
	 */
	class FBootNotice
	{
	public:
		static constexpr FScreenSize ScreenSize{320, 240};
		static constexpr int32 Vblanks = 16 + 110 + 17;

		/** Takes the picture from the executable, as the game unpacks and uploads it. */
		WELTALLCORE_API static TResult<FBootNotice> Load(const FPsxExecutable& Executable);

		/** Runs one vertical blank; returns whether the notice still shows. */
		WELTALLCORE_API bool Tick();

		/** Whether the notice has run its vertical blanks. */
		bool IsOver() const { return Vblank >= Vblanks; }

		const FVram& Vram() const { return Screen; }

		/** The texels the notice's sprite samples, from Vram(). */
		WELTALLCORE_API static FDrawTexture Texture();

		/** What the GPU draws now: nothing before the first blank. */
		WELTALLCORE_API FDrawList DrawList() const;
		/** The same into Into, its memory reused. */
		WELTALLCORE_API void DrawList(FDrawList& Into) const;

	private:
		FVram Screen;
		int32 Vblank = 0;
	};
}
