#include "EventOps.h"

#include "Field/FieldFollowers.h"

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint16 PlayerLocked = 0xFFFF;

		void ControlRunningActor(FEventContext& Context)
		{
			if (!ControlByPad(Context, Self(Context)))
			{
				Fail(Context, EEventFault::EncountersNeverScheduled, static_cast<uint32>(Context.Encounters.PerPeriod));
			}
		}

		/** 0C: the pad's control, every tick, forever: the thread stays on the instruction and yields. */
		void PlayerControl(FEventContext& Context)
		{
			ControlRunningActor(Context);
			Context.Run.bYield = true;
		}

		/** A7: one tick of the pad's control, then on. */
		void PlayerControlStep(FEventContext& Context)
		{
			ControlRunningActor(Context);
			Advance(Context, 1);
		}

		/** 14: the player controls the party again. */
		void EnableControls(FEventContext& Context)
		{
			Context.PlayerLock = 0;
			Advance(Context, 1);
		}

		/** 15: takes the party from the player, once no battle or world map is on its way. */
		void DisableControls(FEventContext& Context)
		{
			if (Context.bBattleRequested || Context.Requests.bWorldMap)
			{
				Context.Run.bYield = true;
				return;
			}
			Context.PlayerLock = PlayerLocked;
			Advance(Context, 1);
		}

		/** D5: which of the pad's buttons the field reads. */
		void SetPadMask(FEventContext& Context)
		{
			Context.Controls.ScriptMask = ReadU16(Context, 1);
			Advance(Context, 3);
		}

		/** FE AE: the action mode, the jump's animation and the cooldown between jumps. */
		void SetActionMode(FEventContext& Context)
		{
			FFieldMovement& Movement = Context.Movement;
			Movement.ActionMode = static_cast<int16>(ReadValue(Context, 1));
			Movement.JumpAnimation = static_cast<int16>(ReadValue(Context, 3));
			Movement.JumpInterval = static_cast<int16>(ReadValue(Context, 5));
			Movement.JumpCooldown = 0;
			Advance(Context, 7);
		}

		/** FE D9: whether the d-pad walks the opposite way. */
		void SetReversedControls(FEventContext& Context)
		{
			Context.Controls.bReversed = CodeByte(Context, 1) != 0;
			Advance(Context, 2);
		}

		/** FE 43: the followers keep to the leader's newest step. */
		void FollowClose(FEventContext& Context)
		{
			Context.Controls.bFollowClose = true;
			Advance(Context, 1);
		}

		/** FE 44: the party follows the leader again from where it stands. */
		void RestartFollowing(FEventContext& Context)
		{
			Context.Controls.bFollowClose = false;
			Context.bPartyWalking = false;
			Context.Controls.bPartyPlaced = false;
			if (!RestartFollowTrail(Context))
			{
				Fail(Context, EEventFault::NoActorModels, Context.LeaderActor);
				return;
			}
			Advance(Context, 1);
		}
	}

	void RegisterPlayerControl(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0x0C] = &PlayerControl;
		Base[0xA7] = &PlayerControlStep;
		Base[0x14] = &EnableControls;
		Base[0x15] = &DisableControls;
		Base[0xD5] = &SetPadMask;
		Extended[0x43] = &FollowClose;
		Extended[0x44] = &RestartFollowing;
		Extended[0xAE] = &SetActionMode;
		Extended[0xD9] = &SetReversedControls;
	}
}
