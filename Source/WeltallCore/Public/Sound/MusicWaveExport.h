#pragma once

#include "Formats/Wave.h"

#include <memory>

namespace Weltall
{
	class FGameSoundSession;

	/** One field song rendered in bounded steps. The supplied sound session is dedicated to exporting. */
	class FMusicWaveExport
	{
	public:
		WELTALLCORE_API static TResult<std::unique_ptr<FMusicWaveExport>> Start(FGameSoundSession& Session, int32 Song);
		/** Renders at most MaxFrames; true when the song has ended or every live track has looped. */
		WELTALLCORE_API TResult<bool> Step(uint32 MaxFrames = 4096);
		const FWave& Wave() const { return Output; }

	private:
		explicit FMusicWaveExport(FGameSoundSession& InSession) : Session(InSession) {}
		FGameSoundSession& Session;
		FWave Output;
		bool bDone = false;
	};
}
