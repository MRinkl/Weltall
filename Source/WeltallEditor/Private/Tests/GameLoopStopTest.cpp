#include "Game/GameSession.h"
#include "Misc/AutomationTest.h"
#include "Shell/GameNotice.h"
#include "Shell/GameVersion.h"
#include "Tests/GameLoopTestGame.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Map 15: a forest. */
	constexpr uint16 StopTestMap = 15;
	/** Past the first disc's last map. */
	constexpr uint16 StopTestMissingMap = 1023;
	/** The Normal Menu. */
	constexpr uint8 StopTestMenuMode = 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameLoopStopTest, "Weltall.Screens.AGameThatStopsOrCannotStartSaysWhy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameLoopStopTest::RunTest(const FString&)
{
	Weltall::FGameLibrary Library;
	const Weltall::FIso9660* Disc = FindTestDisc(Library);
	if (Disc == nullptr)
	{
		AddInfo(TEXT("Skipped: the first disc is not in the Games folder"));
		return true;
	}

	// A map the disc does not have: the game cannot start, and names it.
	FGameLoopTestGame Missing;
	const std::optional<Weltall::FError> NotStarted = Missing.Start(*Disc, StopTestMissingMap, nullptr);
	if (TestTrue(TEXT("The game cannot start"), NotStarted.has_value()))
	{
		TestTrue(FString::Printf(TEXT("Why it cannot start (%hs)"), NotStarted->Message.c_str()), NotStarted->Message.find("field map 1023") != std::string::npos);
	}

	// A game without the Menu program stops as the field opens a menu, and tells its host why, once.
	FGameLoopTestGame Game;
	if (!TestFalse(TEXT("The game enters the map"), Game.Start(*Disc, StopTestMap, [](Weltall::FGameSession& Staged) { Staged.SetMenus(nullptr); }).has_value()))
	{
		return false;
	}
	AddExpectedError(TEXT("The game stopped: the Menu program"), EAutomationExpectedErrorFlags::Contains, 1);
	TArray<FString> Told;
	Game.Screen->SetStopped([&Told](const Weltall::FError& Error) { Told.Add(UTF8_TO_TCHAR(Error.Message.c_str())); });
	Game.Run(1.0);
	TestFalse(TEXT("Stopped before the menu"), Game.Screen->HasStopped());
	TestTrue(TEXT("The field opens a menu"), Game.Screen->RequestMenu(StopTestMenuMode, 0));
	Game.Run(2.0);
	TestTrue(TEXT("Stopped at the menu"), Game.Screen->HasStopped());
	Game.Run(1.0);
	if (TestEqual(TEXT("Told once"), Told.Num(), 1))
	{
		TestTrue(TEXT("Told why"), Told[0].Contains(TEXT("is not run yet")));
	}

	// The screen ends on the version the project's settings name, for a report to say which game stopped.
	TestFalse(TEXT("The project names its version"), GameVersion().IsEmpty());
	const FString Shown = GameStoppedText(Weltall::FError{"the Menu program is not run yet"}, true).ToString();
	TestTrue(FString::Printf(TEXT("The stop names the version (%s)"), *Shown), Shown.EndsWith(TEXT("\n\nWeltall ") + GameVersion()));
	return true;
}

#endif
