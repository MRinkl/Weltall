#include "Battle/BattleContext.h"
#include "Battle/BattleInputStep.h"
#include "Battle/BattleProgramShow.h"
#include "Game/GameState.h"
#include "Input/KernelPad.h"

#include <catch_amalgamated.hpp>

#include <memory>
#include <vector>

using namespace Weltall;

namespace
{
	/** A show that starts the battle when told and notes the pause's picture. */
	class FInputShow final : public IBattleProgramShow
	{
	public:
		bool bStarted = true;
		std::vector<bool> Pauses;
		int32 HudTicks = 0;

		void BeginBattle(FBattleContext&) override {}
		void FrameCamera(uint16) override {}
		void SetMessageBox(uint8, uint8) override {}
		void ShowNameBox(uint8) override {}
		void BeginPlayback(bool, uint8, uint8, uint8) override {}
		void CancelPlayback() override {}
		void SetTargetMarkers(uint16) override {}
		void PlaySound(uint8) override {}
		void BoardGear(uint8) override {}
		void BeginEvent() override {}
		void LoadBattleBank() override {}
		bool IsHolding() const override { return false; }
		void HoldBlank() override {}
		int32 CatchUpSteps() const override { return 0; }
		bool DrewFirstBuffer() const override { return false; }
		void ShowTurnDamage(uint8, const std::array<uint32, 3>&) override {}
		void PlayEventCue(const FBattleEventCue&) override {}
		bool IsBlocking() const override { return false; }
		void PlayTurnCue(EBattleTurnCue, uint8) override {}
		void PlayMenuCue(EBattleMenuCue, uint8, uint8) override {}
		void PlayEndCue(EBattleEndCue, uint8) override {}
		void PlayOpeningCue(EBattleOpeningCue, uint8) override {}
		void BeginFrame() override {}
		bool FinishFrame() override { return true; }
		bool ResumeRules() override { return true; }
		bool HasStopped() const override { return false; }
		bool HasStarted() const override { return bStarted; }
		void TickHud(uint8) override { ++HudTicks; }
		void ShowPause(bool bPaused) override { Pauses.push_back(bPaused); }
	};

	/** A battle's input: its context in an input mode, the kernel's pad, the play time counted per vertical blank. */
	struct FInputBattle
	{
		std::unique_ptr<FBattleContext> Context = std::make_unique<FBattleContext>();
		FGameState Game;
		FKernelPad Pad;
		FInputShow Show;
		FBattleInputStep Step;

		explicit FInputBattle(uint8 Mode)
		{
			Context->GameState = &Game;
			Context->Party.InputMode = Mode;
			Game.SetPlayTime(100);
		}

		FBattleInputServices Services()
		{
			FBattleInputServices Input;
			Input.Context = Context.get();
			Input.Pad = &Pad;
			Input.Show = &Show;
			Input.Game = &Game;
			return Input;
		}

		/** A vertical blank with Buttons (FPadState's) held. */
		void Vblank(uint16 Buttons = 0)
		{
			Game.SetPlayTime(Game.PlayTime() + 1);
			Pad.Vblank(FPadState{Buttons}, Game.PlayTime());
		}

		bool Run() { return Step.Run(Services()); }
		bool Resume() { return Step.Resume(Services()); }
	};

	constexpr uint16 Cross = FPadState::Bit(EPadButton::Cross);
	constexpr uint16 Circle = FPadState::Bit(EPadButton::Circle);
	constexpr uint16 Square = FPadState::Bit(EPadButton::Square);
	constexpr uint16 Start = FPadState::Bit(EPadButton::Start);
	constexpr uint16 Select = FPadState::Bit(EPadButton::Select);
	constexpr uint16 R2 = FPadState::Bit(EPadButton::R2);
	constexpr uint16 Left = FPadState::Bit(EPadButton::Left);
	constexpr uint8 StartEvent = 0x0E;
}

TEST_CASE("The battle's input takes one event a frame, from the first queued entry with a button the menus know", "[Battle]")
{
	FInputBattle Battle(BattleInputModes::Commands);
	Battle.Vblank();
	Battle.Vblank(Square);
	Battle.Vblank();
	Battle.Vblank(Cross);
	REQUIRE(Battle.Run());
	// Physical Square is the logical Triangle: the Strong attack.
	CHECK(Battle.Context->Party.Event == BattleMenuEvents::Strong);
	CHECK(Battle.Show.HudTicks == 1);
	REQUIRE(Battle.Run());
	// Physical Cross is the logical Circle: the confirm.
	CHECK(Battle.Context->Party.Event == BattleMenuEvents::Confirm);
	REQUIRE(Battle.Run());
	CHECK(Battle.Context->Party.Event == BattleMenuEvents::Nothing);

	Battle.Vblank(Left);
	REQUIRE(Battle.Run());
	CHECK(Battle.Context->Party.Event == BattleMenuEvents::Left);
	CHECK(Battle.Context->Party.LastDirection == BattleMenuEvents::Left);
	Battle.Vblank(Select);
	REQUIRE(Battle.Run());
	CHECK(Battle.Context->Party.Event == BattleMenuEvents::Select);
	// R2 ends the reading with nothing on the retail discs, the entries after it left for the next frame.
	Battle.Vblank(R2);
	Battle.Vblank(R2 | Cross);
	REQUIRE(Battle.Run());
	CHECK(Battle.Context->Party.Event == BattleMenuEvents::Nothing);
	REQUIRE(Battle.Run());
	CHECK(Battle.Context->Party.Event == BattleMenuEvents::Confirm);
}

TEST_CASE("The battle's input gives Over for any entry once the battle has ended or the actions have played", "[Battle]")
{
	FInputBattle Battle(BattleInputModes::Commands);
	Battle.Context->bActionsPlayed = true;
	REQUIRE(Battle.Run());
	CHECK(Battle.Context->Party.Event == BattleMenuEvents::Nothing);
	Battle.Vblank();
	REQUIRE(Battle.Run());
	CHECK(Battle.Context->Party.Event == BattleMenuEvents::Over);
	Battle.Context->bActionsPlayed = false;
	Battle.Context->End = BattleEnd::Won;
	Battle.Vblank(Cross);
	REQUIRE(Battle.Run());
	CHECK(Battle.Context->Party.Event == BattleMenuEvents::Over);
}

TEST_CASE("The battle's input empties an overflowed queue and reads nothing that frame", "[Battle]")
{
	FInputBattle Battle(BattleInputModes::Commands);
	for (int32 Vblank = 0; Vblank <= FKernelPad::QueueCapacity; ++Vblank)
	{
		Battle.Vblank(Cross);
	}
	REQUIRE(Battle.Pad.Overflowed());
	REQUIRE(Battle.Run());
	CHECK(Battle.Context->Party.Event == BattleMenuEvents::Nothing);
	CHECK_FALSE(Battle.Pad.Overflowed());
	CHECK_FALSE(Battle.Pad.Pop());
}

TEST_CASE("Start pauses a battle that has started and holds the frame until Start again, without play time", "[Battle]")
{
	FInputBattle Battle(BattleInputModes::Commands);
	Battle.Vblank(Start);
	CHECK_FALSE(Battle.Run());
	CHECK(Battle.Step.IsPaused());
	CHECK(Battle.Show.Pauses == std::vector<bool>{true});
	CHECK(Battle.Show.HudTicks == 0);
	const uint32 PlayTime = Battle.Game.PlayTime();
	// Paused, every press but Start is taken off the queue and ignored.
	Battle.Vblank();
	CHECK_FALSE(Battle.Resume());
	Battle.Vblank(Cross);
	CHECK_FALSE(Battle.Resume());
	Battle.Vblank();
	Battle.Vblank(Start);
	CHECK(Battle.Resume());
	CHECK_FALSE(Battle.Step.IsPaused());
	CHECK(Battle.Show.Pauses == std::vector<bool>{true, false});
	CHECK(Battle.Context->Party.Event == StartEvent);
	CHECK(Battle.Game.PlayTime() == PlayTime);
	CHECK(Battle.Show.HudTicks == 1);
	CHECK_FALSE(Battle.Pad.Pop());
}

TEST_CASE("Start before the battle has started gives its event without pausing", "[Battle]")
{
	FInputBattle Battle(BattleInputModes::Commands);
	Battle.Show.bStarted = false;
	Battle.Vblank(Start);
	REQUIRE(Battle.Run());
	CHECK(Battle.Context->Party.Event == StartEvent);
	CHECK(Battle.Show.Pauses.empty());
}

TEST_CASE("With no controller the battle's input holds the frame until one is back, without play time", "[Battle]")
{
	FInputBattle Battle(BattleInputModes::Commands);
	Battle.Pad.SetConnected(false);
	Battle.Vblank(Cross);
	CHECK_FALSE(Battle.Run());
	CHECK(Battle.Step.IsWaitingForPad());
	const uint32 PlayTime = Battle.Game.PlayTime();
	Battle.Vblank();
	CHECK_FALSE(Battle.Resume());
	Battle.Pad.SetConnected(true);
	Battle.Vblank();
	CHECK(Battle.Resume());
	CHECK(Battle.Show.Pauses == std::vector<bool>{true, false});
	CHECK(Battle.Game.PlayTime() == PlayTime);
	// The queue kept the press made before the controller went.
	CHECK(Battle.Context->Party.Event == BattleMenuEvents::Confirm);
}

TEST_CASE("A catch-up step of the battle's input steps the HUD's counters without reading the pad; a message's reads it again", "[Battle]")
{
	FInputBattle Battle(BattleInputModes::Commands);
	Battle.Vblank(Cross);
	Battle.Vblank(Square);
	REQUIRE(Battle.Run());
	CHECK(Battle.Context->Party.Event == BattleMenuEvents::Confirm);
	CHECK(Battle.Show.HudTicks == 1);
	// The entry still queued stays for the next frame's own step, and the frame's event stays what it was.
	REQUIRE(Battle.Step.RunCatchUp(Battle.Services()));
	REQUIRE(Battle.Step.RunCatchUp(Battle.Services()));
	CHECK(Battle.Context->Party.Event == BattleMenuEvents::Confirm);
	CHECK(Battle.Show.HudTicks == 3);
	REQUIRE(Battle.Run());
	CHECK(Battle.Context->Party.Event == BattleMenuEvents::Strong);

	// A message's step runs whole again: a confirm not yet taken is overwritten by the step after it.
	FInputBattle Message(BattleInputModes::Messages);
	Message.Vblank(Cross);
	REQUIRE(Message.Run());
	CHECK(Message.Context->Party.Event == BattleMenuEvents::Confirm);
	REQUIRE(Message.Step.RunCatchUp(Message.Services()));
	CHECK(Message.Context->Party.Event == 0xFF);
	CHECK(Message.Show.HudTicks == 2);
}

TEST_CASE("A message's input confirms with Circle unless paused; the result screen's even paused", "[Battle]")
{
	FInputBattle Message(BattleInputModes::Messages);
	Message.Vblank(Circle);
	REQUIRE(Message.Run());
	CHECK(Message.Context->Party.Event == 0xFF);
	Message.Vblank(Cross);
	REQUIRE(Message.Run());
	CHECK(Message.Context->Party.Event == BattleMenuEvents::Confirm);
	Message.Vblank(Start);
	Message.Vblank(Cross);
	CHECK_FALSE(Message.Run());
	Message.Vblank(Start);
	CHECK(Message.Resume());
	CHECK(Message.Context->Party.Event == 0xFF);

	FInputBattle Result(BattleInputModes::Result);
	Result.Show.bStarted = false;
	Result.Vblank(Start);
	Result.Vblank(Cross);
	CHECK_FALSE(Result.Run());
	CHECK(Result.Context->Party.Event == BattleMenuEvents::Confirm);
	Result.Vblank(Start);
	CHECK(Result.Resume());
	CHECK(Result.Context->Party.Event == BattleMenuEvents::Confirm);
}
