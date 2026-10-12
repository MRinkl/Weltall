#include "Support/GameDiscs.h"
#include "Support/MenuOracleRun.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using namespace Weltall::Testing;
using namespace Weltall::Testing::MenuPad;

// The disc change (the main menu module's mode 6) against the original module run whole under the oracle harness,
// frame by frame. The port never asks for a disc: the harness puts disc 2
// in the drive.

TEST_CASE("The disc change goes on to disc 2 when saving is declined, as the original does", "[Content][Menu]")
{
	// "Save?" answered on its left option, then "Are you sure you don't need to save?" with Yes.
	FMenuScenario Scenario{"disc_change_skip", EMenuMode::DiscChange, 0,
		{{0, 50}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 30}, {Right, 2}, {0, 4}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 60}},
		200};
	Scenario.Disc = 2;
	CheckMenuScenario(Scenario, "f7ec6103",
		"ac8ab4a0 09a4e140*33 a355032a 116d7d6e 2f1dcc45 eaf6f244 3659b4fa f20f6c1d 0c9ca275 8eb6f094 87ff3b70*9 194113ad 86dd6edf*7 "
		"a3aa973a 09a4e140 a355032a 116d7d6e 2f1dcc45 eaf6f244 3659b4fa f20f6c1d 42285dd0 02988583 b4fcda2b*22 92253c39 b4fcda2b*5 92253c39 "
		"8b6191ba*7 5bcb3f26 09a4e140 f5dc5faf 09a4e140*35 24ddaa35*3");
}

TEST_CASE("The disc change's Save? cancelled asks whether to go on without saving, as the original does", "[Content][Menu]")
{
	// Cancel on "Save?" asks the same question, answered Yes.
	FMenuScenario Scenario{"disc_change_cancel", EMenuMode::DiscChange, 0,
		{{0, 50}, {Circle, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 60}},
		160};
	Scenario.Disc = 2;
	CheckMenuScenario(Scenario, "4d514947",
		"ac8ab4a0 09a4e140*33 a355032a 116d7d6e 2f1dcc45 eaf6f244 3659b4fa f20f6c1d 0c9ca275 8eb6f094 87ff3b70*9 8b6b2b07 09a4e140 a355032a "
		"116d7d6e 2f1dcc45 eaf6f244 3659b4fa f20f6c1d 42285dd0 02988583 b4fcda2b*22 92253c39 8b6191ba*7 5bcb3f26 09a4e140 f5dc5faf "
		"09a4e140*35 24ddaa35*3");
}
