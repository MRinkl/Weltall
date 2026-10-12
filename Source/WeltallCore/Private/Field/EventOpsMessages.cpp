#include "EventOps.h"

namespace Weltall::EventOps
{
	namespace
	{
		using EMode = EFieldMessageMode;

		constexpr uint32 OffScreen = 0x200;
		constexpr uint32 DefaultPriority = 7;
		constexpr uint32 LastChoiceAddress = 0x014;
		/** The instruction's length when its actor is none: a byte more than when it opens (the original skips one byte too many). */
		constexpr int32 NoSpeakerLength = 6;
		/** The colour a choice's line is drawn in. */
		constexpr std::array<uint8, 3> ChoiceColor = {0xEF, 0x1E, 0xF0};

		/** 03, D2, D3 and F5: a message of the running actor, placed by the mode. */
		template <EMode Mode>
		void Message(FEventContext& Context)
		{
			OpenMessage(Context, static_cast<uint32>(Context.Run.Actor), Mode);
		}

		/** D4: a message of the running actor at another actor; none skips it. */
		void MessageAtActor(FEventContext& Context)
		{
			const uint32 Speaker = ReadActor(Context, 1);
			if (Speaker == FEventContext::NoActor)
			{
				Advance(Context, NoSpeakerLength);
				return;
			}
			Advance(Context, 1);
			if (!OpenMessage(Context, Speaker, EMode::AtSpeaker))
			{
				Advance(Context, -1);
			}
		}

		/** FC: D4 with the other actor's portrait, which the running actor keeps. */
		void MessageAtActorPortrait(FEventContext& Context)
		{
			const uint32 Speaker = ReadActor(Context, 1);
			if (Speaker == FEventContext::NoActor)
			{
				Advance(Context, NoSpeakerLength);
				return;
			}
			const FEventActorState* SpeakerState = StateOf(Context, Speaker);
			if (SpeakerState == nullptr)
			{
				return;
			}
			Self(Context).Portrait = SpeakerState->Portrait;
			MessageAtActor(Context);
		}

		/**
		 * 9C: waits until the running actor's window has closed, then keeps the player's last choice
		 * in variable 0x014. A window whose speaker is off screen is closed, and the thread ends
		 * unless it is the actor's default one (or the style keeps such windows).
		 */
		void WaitMessageClosed(FEventContext& Context)
		{
			FFieldMessages& Messages = Context.Messages;
			FEventActorState& State = Self(Context);
			const int32 Index = Messages.FindOwnWindow(Context.Run.Actor);
			if (Index < 0)
			{
				Context.Run.Budget += 8;
				SetVar(Context, LastChoiceAddress, State.Choice);
				Advance(Context, 1);
				return;
			}
			FFieldMessageWindow& Window = Messages.Windows[static_cast<std::size_t>(Index)];
			const FEventActorState* Speaker = StateOf(Context, static_cast<uint32>(Window.Speaker));
			if (Speaker == nullptr)
			{
				return;
			}
			if ((Speaker->Flags1 & OffScreen) != 0)
			{
				const uint32 Style = (State.WindowStyle >> 16) != 0 ? State.WindowStyle >> 16 : State.WindowStyle & 0xFFFF;
				if ((Style & FieldMessageStyle::StaysOffScreen) == 0)
				{
					if (State.Running().Priority() != DefaultPriority)
					{
						EndThread(Context);
						Context.Run.bUnlimited = false;
					}
					Window.bCloseRequested = true;
				}
			}
			Context.Run.bYield = true;
		}

		/**
		 * A9: once the running actor's window has typed its text (or waits to end it by itself), its
		 * lines first to last (the operand's nibbles) become a choice, the cursor on the first.
		 */
		void MessageChoice(FEventContext& Context)
		{
			FFieldMessages& Messages = Context.Messages;
			const int32 Index = Messages.FindOwnWindow(Context.Run.Actor);
			if (Index >= 0)
			{
				Context.Run.Budget += 8;
				FFieldMessageWindow& Window = Messages.Windows[static_cast<std::size_t>(Index)];
				FMesWindow& Text = *Window.Text;
				if (Text.StopReason() != EMesStop::End && (Text.WaitTicks() == 0 || !Text.IsClosing()))
				{
					Context.Run.bYield = true;
					return;
				}
				const uint8 Lines = CodeByte(Context, 1);
				// A choice against a timer keeps the game's own time to answer, whatever the text speed.
				Text.DropOwedWait();
				Window.bChoice = true;
				Self(Context).Choice = 0xFF;
				Window.ChoiceFirst = static_cast<int16>(Lines >> 4);
				Window.ChoiceCursor = 0;
				Window.ChoiceCount = static_cast<int16>((Lines & 0xF) - (Lines >> 4) + 1);
				Text.SetColor(ChoiceColor[0], ChoiceColor[1], ChoiceColor[2]);
			}
			Advance(Context, 2);
			Context.Run.bYield = true;
		}

		/** F4: 0 asks the running actor's window to close; anything else forgets the actor's window layout and style. */
		void CloseWindow(FEventContext& Context)
		{
			if (CodeByte(Context, 1) == 0)
			{
				Context.Messages.RequestClose(Context.Run.Actor);
			}
			else
			{
				FEventActorState& State = Self(Context);
				State.WindowWidth = 0;
				State.WindowX = 0;
				State.WindowY = 0;
				State.WindowLines = 0;
				State.WindowStyle = 0;
			}
			Advance(Context, 2);
			Context.Run.bYield = true;
		}

		/** CF: the running actor's window place (x in 2-pixel steps), width (in thirds of what it gives) and lines, 0 each for automatic. */
		void SetWindowLayout(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.WindowX = static_cast<uint16>(CodeByte(Context, 1) << 1);
			State.WindowY = CodeByte(Context, 2);
			State.WindowWidth = static_cast<uint8>(CodeByte(Context, 3) * 3);
			State.WindowLines = CodeByte(Context, 4);
			Advance(Context, 5);
		}

		/** D0: CF from values (x in pixels), and the actor's default message style. */
		void SetWindowLayoutValues(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.WindowX = static_cast<uint16>(ReadValue(Context, 1));
			State.WindowY = static_cast<uint16>(ReadValue(Context, 3));
			State.WindowWidth = static_cast<uint8>(ReadValue(Context, 5) * 3);
			State.WindowLines = static_cast<uint8>(ReadValue(Context, 7));
			State.WindowStyle = static_cast<uint32>(ReadValue(Context, 9));
			Advance(Context, 11);
		}

		/** D6: the field's slow motion, 0-2 vblanks more a tick; windows open in 8, 6 or 4 ticks to match. */
		void SetFrameRate(FEventContext& Context)
		{
			Context.FrameDelay = ReadValue(Context, 1);
			switch (Context.FrameDelay)
			{
			case 0:
				Context.Messages.OpenTicks = 8;
				break;
			case 1:
				Context.Messages.OpenTicks = 6;
				break;
			case 2:
				Context.Messages.OpenTicks = 4;
				break;
			default:
				break;
			}
			Advance(Context, 3);
		}
	}

	void RegisterMessages(FBaseTable& Base, FExtendedTable&)
	{
		Base[0x03] = &Message<EMode::Fixed>;
		Base[0xD2] = &Message<EMode::AtSpeaker>;
		Base[0xD3] = &Message<EMode::FixedFromSpeaker>;
		Base[0xF5] = &Message<EMode::Centered>;
		Base[0xD4] = &MessageAtActor;
		Base[0xFC] = &MessageAtActorPortrait;
		Base[0x9C] = &WaitMessageClosed;
		Base[0xA9] = &MessageChoice;
		Base[0xF4] = &CloseWindow;
		Base[0xCF] = &SetWindowLayout;
		Base[0xD0] = &SetWindowLayoutValues;
		Base[0xD6] = &SetFrameRate;
	}
}
