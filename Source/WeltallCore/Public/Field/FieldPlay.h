#pragma once

#include "Field/FieldDrawingTables.h"
#include "Field/FieldFrame.h"
#include "Field/FieldPartySprites.h"
#include "Field/FieldPresenter.h"
#include "Field/FieldRuntime.h"
#include "Foundation/Result.h"
#include "Game/GameState.h"
#include "Game/GameStateNames.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteMath.h"
#include "Input/KernelPad.h"
#include "Media/DiscIdentification.h"
#include "Text/MesResident.h"
#include "Timing/PlayClock.h"

#include <memory>
#include <optional>
#include <span>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FPsxExecutable;

	/**
	 * The field as a new game plays it from a disc: the game's state, random numbers, pad and play
	 * clock, the party's sprite files and the text's resources as the kernel keeps them, the field's
	 * runtime and its drawing. Songs and sound effects are the caller's.
	 */
	class FFieldPlay
	{
	public:
		/**
		 * A new game on a disc of the given release (its kernel the executable), the songs and effects
		 * the field's scripts ask for going to Songs and Effects (none: not heard). The disc, its index,
		 * the songs and the effects must outlive the play.
		 */
		WELTALLCORE_API static TResult<std::unique_ptr<FFieldPlay>> NewGame(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Executable,
			EDiscRelease Release, IFieldSongs& Songs, IFieldSoundEffects* Effects);

		FFieldPlay(const FFieldPlay&) = delete;
		FFieldPlay& operator=(const FFieldPlay&) = delete;
		WELTALLCORE_API ~FFieldPlay();

		/** Enters map Number as the field enters a map, and makes its drawing ready. */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> EnterMap(int32 Number);

		/** The vertical blanks the next tick lasts. */
		int32 VblanksPerTick() const { return Runtime->VblanksPerTick(); }

		/** The next tick with the buttons held at each of its vertical blanks, drawn into Frame with the scenery under Culling. */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Tick(std::span<const FPadState> HeldPerVblank, EFieldCulling Culling, FFieldFrame& Frame);

		const FFieldRuntime& Field() const { return *Runtime; }
		const FFieldPresenter& Drawing() const { return *Presenter; }
		FFieldPresenter& Drawing() { return *Presenter; }
		const FGameState& Game() const { return GameState; }
		/** The game's state, for a host that stages it before a map is entered. */
		FGameState& Game() { return GameState; }

	private:
		FFieldPlay(const FCdImage& InImage, const FGameFileIndex& InIndex, FGteArctangents InArctangents, FMesResident InText, FGameState InGame,
			FFieldDrawingTables InTables);

		FGteArctangents Arctangents;
		FMesResident Text;
		FGameState GameState;
		FGameStateNames Names;
		FPsxRandom Random;
		FKernelPad Pad;
		FPlayClock Clock;
		FPartySpriteFiles PartySprites;
		FFieldDrawingTables Tables;
		std::unique_ptr<FFieldRuntime> Runtime;
		std::optional<FFieldPresenter> Presenter;
	};
}
