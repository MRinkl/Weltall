#include "Battle/BattleContext.h"
#include "Battle/Show/BattleShow.h"
#include "Battle/Show/BattleShowPlayback.h"
#include "Graphics/SpriteFile.h"

#include <algorithm>
#include <cstdlib>

namespace Weltall
{
	namespace
	{
		/** The animations RefreshPoses lets give way to the refresh when the state no longer asks for them. */
		constexpr std::array<int32, 5> HeldAnimations{5, 7, 0xE, 0xF, FBattlePlayback::CollapseAnimation};
		/** How far a combatant may stand from home before it walks back. */
		constexpr int32 HomeReach = 9;
		/** The status marks' bits that show a sprite over the combatant (Poison, Confusion, Sleep) and the common effects' animations they play. */
		constexpr int32 FirstMarkBit = 13;
		constexpr int32 FirstMarkAnimation = 9;
		/** The results the drains' numbers show as: HP damage, EP damage, fuel. */
		constexpr std::array<uint8, 3> DrainResults{0, 1, 10};
		/** The escape's run: its frames, over which the screen fades out (subtracting). */
		constexpr int32 EscapeFrames = 0x28;
		constexpr int32 EscapeFadeMode = 2;

		uint16 SlotBit(int32 Slot)
		{
			return static_cast<uint16>(1u << (static_cast<uint32>(Slot) & 0x1F));
		}

		int32 High(int32 Value)
		{
			return static_cast<int16>(Value >> 16);
		}

		bool IsHeld(int32 Animation)
		{
			return std::find(HeldAnimations.begin(), HeldAnimations.end(), Animation) != HeldAnimations.end();
		}
	}

	void FBattlePlayback::PrepareNext(int32 Slot)
	{
		CallFromRules(EPlayRoutine::PrepareNext, Slot);
	}

	void FBattlePlayback::ShowTurnDamage(int32 Slot, const std::array<uint32, 3>& Amounts)
	{
		Drains = Amounts;
		CallFromRules(EPlayRoutine::TurnDamage, Slot);
	}

	void FBattlePlayback::RefreshPoses()
	{
		CallFromRules(EPlayRoutine::RefreshPoses);
	}

	void FBattlePlayback::FinishLoads()
	{
		CallFromRules(EPlayRoutine::WaitLoads);
	}

	void FBattlePlayback::BoardGear(int32 Slot)
	{
		CallFromRules(EPlayRoutine::BoardGear, Slot);
	}

	void FBattlePlayback::PlayEndPose(int32 Pose)
	{
		CallFromRules(EPlayRoutine::EndPose, Pose);
	}

	void FBattlePlayback::CallFromRules(EPlayRoutine Kind, int32 Value, int32 Other)
	{
		if (!Rules.Push(static_cast<uint8>(Kind), Value, Other))
		{
			Fail(static_cast<uint8>(EBattleShowFault::Playback), "the show's routines nest past their room");
			return;
		}
		Run(Rules);
	}

	EShowFlow FBattlePlayback::RunTurnDamage(FShowRoutine& Routine)
	{
		FSprite* Sprite = SlotSprite(Routine.Value);
		switch (Routine.Resume)
		{
		case 0:
		{
			Show.Sprites.CountedTasks = 0;
			Show.Sprites.bCountingTasks = true;
			Show.Numbers.bWithoutTotal = true;
			if (Sprite == nullptr)
			{
				break;
			}
			Routine.Other = Sprite->Animation();
			Show.Sprites.SetAnimation(*Sprite, FlinchAnimation);
			// The numbers pop only while a playback runs: the drains' act as one.
			const bool bWasPlaying = Show.Numbers.bPlaying;
			Show.Numbers.bPlaying = true;
			for (std::size_t Kind = 0; Kind < Drains.size(); ++Kind)
			{
				if (Drains[Kind] != 0)
				{
					Show.Numbers.Pop(*Sprite, Drains[Kind], DrainResults[Kind]);
				}
			}
			Show.Numbers.bPlaying = bWasPlaying;
			Routine.Resume = 1;
			[[fallthrough]];
		}
		case 1:
			if (Show.Numbers.Pending() != 0)
			{
				return EShowFlow::Frame;
			}
			Routine.Resume = 2;
			[[fallthrough]];
		case 2:
			if (Sprite->Animation() == FlinchAnimation)
			{
				return EShowFlow::Frame;
			}
			Show.Sprites.SetAnimation(*Sprite, Routine.Other);
			break;
		default:
			break;
		}
		Show.Numbers.bWithoutTotal = false;
		Show.Numbers.CloseTotal();
		Show.Sprites.CountedTasks = 0;
		Show.Sprites.bCountingTasks = false;
		return EShowFlow::Done;
	}

	EShowFlow FBattlePlayback::RunRefreshPoses(FShowRoutine& Routine)
	{
		switch (Routine.Resume)
		{
		case 0:
			// An animation the state no longer asks for gives way to the refresh; a collapse given way may come again.
			for (int32 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
			{
				FSprite* Sprite = SlotSprite(Slot);
				if (IsModel(Slot) || Sprite == nullptr || Pose(Slot) == OutPose)
				{
					continue;
				}
				const int32 Playing = Sprite->Animation();
				if (IsHeld(Playing) && Playing != PoseAnimations[static_cast<std::size_t>(Pose(Slot))])
				{
					Show.Sprites.SetAnimation(*Sprite, RefreshAnimation);
					CollapsedSlots = static_cast<uint16>(CollapsedSlots & ~SlotBit(FBattleSprites::SlotOf(*Sprite)));
				}
			}
			return Call(Routine, 1, EPlayRoutine::WaitPoses);
		case 1:
			// Those away from home jump back to it, twice as fast.
			for (int32 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
			{
				FSprite* Sprite = SlotSprite(Slot);
				if (IsModel(Slot) || Sprite == nullptr || Pose(Slot) == OutPose || Sprite->Animation() == CollapseAnimation)
				{
					continue;
				}
				const FBattleStagePoint& Home = Show.Context->Slots[static_cast<std::size_t>(Slot)].Position;
				if (std::abs(High(Sprite->Position[0]) - static_cast<int32>(Home.X)) < HomeReach
					&& std::abs(High(Sprite->Position[2]) - static_cast<int32>(Home.Z)) < HomeReach)
				{
					continue;
				}
				Sprite->TimeFlags |= SpriteTimeFlags::UpdateTwice;
				Sprite->SavedPoint = {static_cast<int16>(Home.X), 0, static_cast<int16>(Home.Z)};
				Show.Sprites.SetAnimation(*Sprite, JumpAnimation);
			}
			return Call(Routine, 2, EPlayRoutine::WaitPoses);
		case 2:
			for (FSprite* Sprite : Show.Battle.SlotSprites)
			{
				if (Sprite != nullptr)
				{
					Sprite->TimeFlags &= ~SpriteTimeFlags::UpdateTwice;
				}
			}
			Routine.Value = 0;
			Routine.Resume = 3;
			[[fallthrough]];
		case 3:
			// Each in turn faces home, the knocked-out collapse, then its pose and marks.
			for (; Routine.Value < FBattleContext::SlotCount; ++Routine.Value)
			{
				if (!IsModel(Routine.Value) && SlotSprite(Routine.Value) != nullptr)
				{
					FaceHome(Routine.Value);
					return Call(Routine, 4, EPlayRoutine::Collapse);
				}
			}
			return EShowFlow::Done;
		case 4:
			SettlePose(Routine.Value);
			++Routine.Value;
			Routine.Resume = 3;
			return EShowFlow::Again;
		default:
			return EShowFlow::Done;
		}
	}

	EShowFlow FBattlePlayback::RunBoardGear(FShowRoutine& Routine)
	{
		const int32 Slot = Routine.Value;
		switch (Routine.Resume)
		{
		case 0:
			Show.FrameSlots(SlotBit(Slot));
			Show.FrameSlots(0);
			// The tasks there are now: the animation's own are waited for.
			Routine.Other = static_cast<int32>(Show.Sprites.Tasks().size());
			return Call(Routine, 1, EPlayRoutine::WaitLoads);
		case 1:
			if (FSprite* Sprite = SlotSprite(Slot))
			{
				Show.Sprites.SetAnimation(*Sprite, BoardAnimation);
			}
			Routine.Resume = 2;
			[[fallthrough]];
		case 2:
		{
			const FSprite* Sprite = SlotSprite(Slot);
			if (Sprite == nullptr)
			{
				// The slot has no sprite, or its task ended under the animation: the original reads through the pointer it kept.
				Fail(static_cast<uint8>(EBattleShowFault::EndedTask));
				return EShowFlow::Done;
			}
			if (Sprite->Wait != 0 && Sprite->Animation() == BoardAnimation)
			{
				return EShowFlow::Frame;
			}
			Routine.Resume = 3;
			[[fallthrough]];
		}
		case 3:
			if (static_cast<int32>(Show.Sprites.Tasks().size()) != Routine.Other)
			{
				return EShowFlow::Frame;
			}
			Show.Battle.RemoveCombatant(Slot);
			Routine.Resume = 4;
			return EShowFlow::Frame;
		case 4:
			Routine.Resume = 5;
			return EShowFlow::Frame;
		case 5:
			Show.Objects.QueueGearModel(Slot);
			Routine.Resume = 6;
			[[fallthrough]];
		case 6:
			return Show.Objects.GearModelsPending() ? EShowFlow::Frame : EShowFlow::Done;
		default:
			return EShowFlow::Done;
		}
	}

	EShowFlow FBattlePlayback::RunEndPose(FShowRoutine& Routine)
	{
		switch (Routine.Resume)
		{
		case 0:
			if (Routine.Value == static_cast<int32>(EBattleEndPose::Victory))
			{
				Show.FrameSlots(0xFFFFFFFFu);
				for (int32 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
				{
					FSprite* Sprite = SlotSprite(Slot);
					if (Sprite != nullptr && Sprite->Animation() != CollapseAnimation)
					{
						Show.Sprites.EndChildren(*Sprite, Show.Battle.CommonEffects, std::nullopt);
						Show.Sprites.SetAnimation(*Sprite, VictoryAnimation);
					}
				}
			}
			else if (Routine.Value == static_cast<int32>(EBattleEndPose::Escape))
			{
				Show.FrameSlots(0);
				Show.Fade.Start(EscapeFrames, EscapeFadeMode, 0xFF, 0xFF, 0xFF);
				for (int32 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
				{
					FSprite* Sprite = SlotSprite(Slot);
					if (Sprite != nullptr && Sprite->Animation() != CollapseAnimation)
					{
						Show.Sprites.SetAnimation(*Sprite, EscapeAnimation);
					}
				}
				Routine.Other = EscapeFrames;
				Routine.Resume = 1;
				return EShowFlow::Frame;
			}
			Routine.Resume = 2;
			return EShowFlow::Again;
		case 1:
			if (--Routine.Other > 0)
			{
				return EShowFlow::Frame;
			}
			[[fallthrough]];
		case 2:
			// The enemies released, the battle's own bank back in sound memory, the battle over.
			for (int32 Slot = FBattleContext::PartySlotCount; Slot < FBattleContext::SlotCount; ++Slot)
			{
				Show.Objects.Free(Slot);
				Show.Battle.RemoveCombatant(Slot);
			}
			Show.ReloadSystemSoundBank();
			Show.bStarted = false;
			return EShowFlow::Done;
		default:
			return EShowFlow::Done;
		}
	}

	void FBattlePlayback::SettlePose(int32 Slot)
	{
		FSprite* Sprite = SlotSprite(Slot);
		if (Sprite == nullptr)
		{
			return;
		}
		const int32 PoseNumber = Pose(Slot);
		if (PoseNumber != OutPose)
		{
			int32 Wanted = PoseAnimations[static_cast<std::size_t>(PoseNumber)];
			// A knocked-out combatant takes its collapse here only when a record set it aside.
			if (Wanted != CollapseAnimation || (Stayed() >> FBattleSprites::SlotOf(*Sprite) & 1) != 0)
			{
				if (Wanted == 1)
				{
					Wanted = Sprite->NextAnimation();
				}
				if (!IsHidden(Slot) && Sprite->Animation() != Wanted)
				{
					Show.Sprites.SetAnimation(*Sprite, Wanted);
				}
			}
		}
		RefreshMarks(*Sprite, Slot);
	}

	uint16 FBattlePlayback::StatusMarks(int32 Slot) const
	{
		const FBattleCombatant& Combatant = Show.Context->Combatants[static_cast<std::size_t>(Slot)];
		if ((Combatant.Status & 0x8000) != 0)
		{
			return 0;
		}
		uint16 Marks = 0;
		if ((Combatant.Mental & 0x1000) != 0)
		{
			Marks = 0x8000;
		}
		if ((Combatant.Mental & 0x2000) != 0)
		{
			Marks |= 0x4000;
		}
		if ((Combatant.Status & 0x800) != 0)
		{
			Marks |= 0x2000;
		}
		const uint16 Elements = static_cast<uint16>(Combatant.Elements | Combatant.PermanentElements);
		return static_cast<uint16>(Marks | (Elements & 0xF00) >> 3 | (Elements & 0xF000) >> 3);
	}

	void FBattlePlayback::RefreshMarks(FSprite& Sprite, int32 Slot)
	{
		const uint16 Marks = StatusMarks(Slot);
		const uint16 Shown = Sprite.Extra.StatusMarks;
		Sprite.Extra.StatusMarks = Marks;
		const uint16 Appeared = static_cast<uint16>(Marks & ~Shown);
		const uint16 Gone = static_cast<uint16>(Shown & ~Marks);
		FSpriteResource& Common = Show.Battle.CommonEffects;
		for (int32 Bit = FirstMarkBit; Bit < 16; ++Bit)
		{
			const int32 Mark = FirstMarkAnimation + Bit - FirstMarkBit;
			// The mark's effect child: once per sprite, not among the tasks the playback counts.
			if ((Appeared >> Bit & 1) == 0 || Sprite.PrimaryFile == nullptr || Show.Sprites.FindChild(Sprite, Common, Mark) != nullptr)
			{
				continue;
			}
			if (Common.File == nullptr)
			{
				Fail(static_cast<uint8>(EBattleShowFault::File));
				return;
			}
			const bool bWasCounting = Show.Sprites.bCountingTasks;
			Show.Sprites.bCountingTasks = false;
			FSprite* Child = Show.Sprites.SpawnChild(Sprite, *Common.File, Common.File->AnimationHeader(Common.Tables, Mark), &Common);
			Show.Sprites.bCountingTasks = bWasCounting;
			if (Child == nullptr)
			{
				Fail(static_cast<uint8>(EBattleShowFault::Sprites));
				return;
			}
			Child->TimeFlags = (Child->TimeFlags & ~(0xFFu << SpriteTimeFlags::AnimationShift)) | static_cast<uint32>(Mark) << SpriteTimeFlags::AnimationShift;
			Child->Target = &Sprite;
			Child->NextWord |= SpriteNextFlags::KeepTaskMarks;
		}
		for (int32 Bit = FirstMarkBit; Bit < 16; ++Bit)
		{
			if ((Gone >> Bit & 1) != 0)
			{
				Show.Sprites.EndChildren(Sprite, Common, FirstMarkAnimation + Bit - FirstMarkBit);
			}
		}
	}
}
