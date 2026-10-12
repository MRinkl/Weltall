#include "Battle/BattleInputStep.h"

#include "Battle/BattleContext.h"
#include "Battle/BattleEvent.h"
#include "Battle/BattleParty.h"
#include "Battle/BattleProgramShow.h"
#include "Battle/BattleResult.h"
#include "Battle/BattleWaits.h"
#include "Game/GameState.h"
#include "Input/KernelPad.h"

#include <array>
#include <optional>

namespace Weltall
{
	namespace
	{
		namespace Buttons
		{
			constexpr uint16 L2 = 0x0001;
			constexpr uint16 R2 = 0x0002;
			constexpr uint16 Triangle = 0x0010;
			constexpr uint16 Circle = 0x0020;
			constexpr uint16 Cross = 0x0040;
			constexpr uint16 Square = 0x0080;
			constexpr uint16 Select = 0x0100;
			constexpr uint16 Start = 0x0800;
			constexpr uint16 Up = 0x1000;
			constexpr uint16 Right = 0x2000;
			constexpr uint16 Down = 0x4000;
			constexpr uint16 Left = 0x8000;
		}

		/** The event Start gives the menus. */
		constexpr uint8 StartEvent = 0x0E;
		/** The result screen's and the messages' event when no press counts. */
		constexpr uint8 NoPress = 0xFF;

		struct FButtonEvent
		{
			uint16 Button;
			uint8 Event;
		};

		/** The directions, from the repeat word, in the order the reader tests them. */
		constexpr std::array<FButtonEvent, 4> Directions{{
			{Buttons::Right, BattleMenuEvents::Right},
			{Buttons::Down, BattleMenuEvents::Down},
			{Buttons::Left, BattleMenuEvents::Left},
			{Buttons::Up, BattleMenuEvents::Up},
		}};

		/** The face buttons, from the pressed word, in the order the reader tests them. */
		constexpr std::array<FButtonEvent, 4> FaceButtons{{
			{Buttons::Circle, BattleMenuEvents::Confirm},
			{Buttons::Cross, BattleMenuEvents::Cancel},
			{Buttons::Square, BattleMenuEvents::Weak},
			{Buttons::Triangle, BattleMenuEvents::Strong},
		}};

		/**
		 * The menus' event for a queue entry when it has a button they know, before Start: the first
		 * direction repeating, else the first face button pressed (with its sound, the directions noted),
		 * else L2, R2 (which ends the reading with nothing on the retail discs) or Select.
		 */
		std::optional<uint8> MenuEvent(FBattleContext& Context, const FKernelPadWords& Entry)
		{
			for (const FButtonEvent& Direction : Directions)
			{
				if ((Entry.Repeated & Direction.Button) != 0)
				{
					return TakeBattleMenuPress(Context, Direction.Event);
				}
			}
			for (const FButtonEvent& Face : FaceButtons)
			{
				if ((Entry.Pressed & Face.Button) != 0)
				{
					return TakeBattleMenuPress(Context, Face.Event);
				}
			}
			if ((Entry.Pressed & Buttons::L2) != 0)
			{
				return BattleMenuEvents::L2;
			}
			if ((Entry.Pressed & Buttons::R2) != 0)
			{
				return BattleMenuEvents::Nothing;
			}
			if ((Entry.Pressed & Buttons::Select) != 0)
			{
				return BattleMenuEvents::Select;
			}
			return std::nullopt;
		}
	}

	bool FBattleInputStep::Run(const FBattleInputServices& Services)
	{
		FBattleContext& Context = *Services.Context;
		Mode = Context.Party.InputMode;
		if (Mode != BattleInputModes::Commands && Mode != BattleInputModes::Messages && Mode != BattleInputModes::Result)
		{
			return true;
		}
		if (Mode == BattleInputModes::Commands && Services.Show->HasStarted())
		{
			TickBattleWaits(Context);
		}
		Pending = BattleMenuEvents::Nothing;
		Pc = EPc::PadWait;
		return Go(Services);
	}

	bool FBattleInputStep::RunCatchUp(const FBattleInputServices& Services)
	{
		FBattleContext& Context = *Services.Context;
		if (Context.Party.InputMode != BattleInputModes::Commands)
		{
			return Run(Services);
		}
		if (Services.Show->HasStarted())
		{
			TickBattleWaits(Context);
		}
		Services.Show->TickHud(BattleInputModes::Commands);
		ScrollBattleList(Context);
		return true;
	}

	bool FBattleInputStep::Resume(const FBattleInputServices& Services)
	{
		return Pc == EPc::Idle || Go(Services);
	}

	bool FBattleInputStep::Go(const FBattleInputServices& Services)
	{
		FBattleContext& Context = *Services.Context;
		for (;;)
		{
			switch (Pc)
			{
			case EPc::PadWait:
				if (!Services.Pad->IsConnected())
				{
					if (!bPadWaitShown)
					{
						bPadWaitShown = true;
						Services.Show->ShowPause(true);
						KeptPlayTime = Services.Game->PlayTime();
					}
					return false;
				}
				if (bPadWaitShown)
				{
					bPadWaitShown = false;
					Services.Show->ShowPause(false);
					Services.Game->SetPlayTime(KeptPlayTime);
				}
				if (Mode == BattleInputModes::Messages)
				{
					Services.Show->TickHud(Mode);
					TakeBattleMessageInput(Context, false);
				}
				else if (Mode == BattleInputModes::Result && !Context.Event.bKeyWait)
				{
					Context.Party.Event = NoPress;
				}
				Pc = EPc::Read;
				break;
			case EPc::Read:
				if (!Walk(Services))
				{
					return false;
				}
				Pc = EPc::Idle;
				if (Mode == BattleInputModes::Result)
				{
					// The EXP count's step, and the panels' figures drawn anew while it runs.
					if (StepBattleResultCount(Context))
					{
						Services.Show->PlayMenuCue(EBattleMenuCue::ResultExpCounters, 0, 0);
						Services.Show->PlayMenuCue(EBattleMenuCue::ResultExpGains, 0, 0);
					}
					return true;
				}
				if (Mode != BattleInputModes::Messages)
				{
					Services.Show->TickHud(Mode);
				}
				if (Mode == BattleInputModes::Commands)
				{
					ScrollBattleList(Context);
				}
				return true;
			case EPc::Idle:
				return true;
			}
		}
	}

	bool FBattleInputStep::Walk(const FBattleInputServices& Services)
	{
		FBattleContext& Context = *Services.Context;
		FKernelPad& Pad = *Services.Pad;
		for (;;)
		{
			bool bDrained = false;
			if (Pad.Overflowed())
			{
				Pad.Reset();
			}
			else
			{
				for (;;)
				{
					const std::optional<FKernelPadWords> Entry = Pad.Pop();
					if (!Entry)
					{
						bDrained = true;
						break;
					}
					if (Mode == BattleInputModes::Commands)
					{
						if (Context.End != BattleEnd::Running || Context.bActionsPlayed)
						{
							Pending = BattleMenuEvents::Over;
							break;
						}
						if (!bPaused)
						{
							if (const std::optional<uint8> Event = MenuEvent(Context, *Entry))
							{
								Pending = *Event;
								break;
							}
						}
					}
					else if ((Entry->Pressed & Buttons::Circle) != 0 && (Mode == BattleInputModes::Result || !bPaused))
					{
						Context.Party.Event = BattleMenuEvents::Confirm;
						break;
					}
					if ((Entry->Pressed & Buttons::Start) != 0)
					{
						Pending = StartEvent;
						if (Mode == BattleInputModes::Result || Services.Show->HasStarted())
						{
							TogglePause(Services);
						}
						break;
					}
				}
			}
			if (!bPaused)
			{
				if (Mode == BattleInputModes::Commands)
				{
					Context.Party.Event = Pending;
				}
				return true;
			}
			if (bDrained)
			{
				return false;
			}
		}
	}

	void FBattleInputStep::TogglePause(const FBattleInputServices& Services)
	{
		bPaused = !bPaused;
		Services.Show->ShowPause(bPaused);
		if (bPaused)
		{
			KeptPlayTime = Services.Game->PlayTime();
		}
		else
		{
			Services.Game->SetPlayTime(KeptPlayTime);
		}
	}
}
