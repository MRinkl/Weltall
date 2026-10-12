#pragma once

#include "Foundation/Types.h"

#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	/**
	 * The cases no script or data of the game reaches, where the original hangs, corrupts memory or reads
	 * what it does not own. Given one, the systems carry on past such a case with its natural outcome (no
	 * clone made, nothing played, a window not drawn...) and note the case once for the host's log; without
	 * one, as the tests run, or while it stops, they stop there with an error naming it, as the original's
	 * harness ends its case at the same point.
	 */
	class FUnreachedCases
	{
	public:
		/** Stops at every case instead: for development, where a stop points at the script that reached it. */
		bool bStops = false;

		/** Whether play goes on past Case (a sentence naming it), noting it the first time. */
		WELTALLCORE_API bool Recover(std::string_view Case);

		/** The cases noted since the host last took them, each once. */
		WELTALLCORE_API std::vector<std::string> TakeNoted();

	private:
		std::vector<std::string> Seen;
		std::vector<std::string> Noted;
	};

	/** Whether to carry on past Case: never without a record of the cases. */
	inline bool RecoverUnreached(FUnreachedCases* Cases, std::string_view Case)
	{
		return Cases != nullptr && Cases->Recover(Case);
	}
}
