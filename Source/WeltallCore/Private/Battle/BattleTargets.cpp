#include "Battle/BattleParty.h"
#include "Battle/BattlePartySteps.h"
#include "Graphics/GteMath.h"

#include <algorithm>

namespace Weltall
{
	using namespace BattleParty;

	namespace
	{
		constexpr uint8 None = FBattleTargeting::None;
		/**
		 * The x the game compares with when a slot has no default target: its slot record's table read as if the
		 * target were slot FFh lands in an ordering table, where a link's high half is below 20h; as the tables
		 * are cleared, 0Ch.
		 */
		constexpr uint16 NoTargetX = 0x000C;
		constexpr uint16 KnockedOutOrGone = 0xC001;
		constexpr uint16 KnockedOut = 0x8000;
		/** The direction search reads the list's first eleven entries. */
		constexpr std::size_t SearchedEntries = 11;

		enum ESelectPc : uint8
		{
			SelectStart,
			SelectFrame,
		};

		/** The slot's status word the targeting reads: its gear's in a gear. */
		uint16 TargetStatus(const FBattleContext& Context, uint8 Slot)
		{
			const FBattleCombatant& Combatant = Context.Combatants[Slot];
			return Context.ApRecords[Slot].InGear == 0 ? Combatant.Status : Combatant.GearPart.Status;
		}

		bool IsValidTarget(const FBattleContext& Context, uint8 Slot, bool bKnockedOut)
		{
			if (!Context.InBattle[Slot] || Context.Slots[Slot].Presence != 0)
			{
				return false;
			}
			return bKnockedOut || (TargetStatus(Context, Slot) & KnockedOutOrGone) == 0;
		}

		void ClearList(FBattleTargeting& Targeting, std::size_t Entries)
		{
			for (std::size_t Entry = 0; Entry < Entries; ++Entry)
			{
				Targeting.List[Entry] = None;
			}
		}

		void Add(FBattleContext& Context, uint8 Slot, uint8& Next)
		{
			FBattleTargeting& Targeting = Context.Targeting;
			Targeting.List[Next] = Slot;
			++Next;
			++Targeting.Count;
			Targeting.Candidates = static_cast<uint16>(Targeting.Candidates | Bit(Context, Slot));
		}

		/** The valid combatants of one side or both in slot order (Mode 0 the enemies, 1 the party, 2 both, the party first when told). */
		uint8 ListTargets(FBattleContext& Context, uint8 Mode, bool bKnockedOut, bool bPartyFirst)
		{
			FBattleTargeting& Targeting = Context.Targeting;
			ClearList(Targeting, Targeting.List.size());
			uint8 First = 0;
			uint8 FirstCount = 0;
			uint8 Second = 0;
			uint8 SecondCount = 0;
			if (Mode == 1)
			{
				FirstCount = FBattleContext::PartySlotCount;
			}
			else if (Mode == 0)
			{
				First = FBattleContext::PartySlotCount;
				FirstCount = FBattleContext::EnemySlotCount;
			}
			else if (!bPartyFirst)
			{
				First = FBattleContext::PartySlotCount;
				FirstCount = FBattleContext::EnemySlotCount;
				SecondCount = FBattleContext::PartySlotCount;
			}
			else
			{
				FirstCount = FBattleContext::PartySlotCount;
				Second = FBattleContext::PartySlotCount;
				SecondCount = FBattleContext::EnemySlotCount;
			}
			uint8 Next = 0;
			Targeting.Count = 0;
			Targeting.Candidates = 0;
			for (uint8 Slot = First; Slot < First + FirstCount; ++Slot)
			{
				if (IsValidTarget(Context, Slot, bKnockedOut))
				{
					Add(Context, Slot, Next);
				}
			}
			for (uint8 Slot = Second; Slot < Second + SecondCount; ++Slot)
			{
				if (IsValidTarget(Context, Slot, bKnockedOut))
				{
					Add(Context, Slot, Next);
				}
			}
			return Targeting.List[0];
		}

		uint8 ListReachableTargets(FBattleContext& Context, uint8 Actor)
		{
			FBattleTargeting& Targeting = Context.Targeting;
			ClearList(Targeting, Targeting.List.size());
			uint8 Next = 0;
			Targeting.Count = 0;
			Targeting.Candidates = 0;
			for (uint8 Slot = FBattleContext::PartySlotCount; Slot < FBattleContext::SlotCount; ++Slot)
			{
				if (CanReachTarget(Context, Actor, Slot))
				{
					Add(Context, Slot, Next);
				}
			}
			return Targeting.List[0];
		}

		/** The candidates in the cursor's group. */
		uint16 TargetGroupMask(FBattleContext& Context)
		{
			const FBattleTargeting& Targeting = Context.Targeting;
			uint16 Mask = 0;
			for (uint8 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
			{
				if ((Targeting.Candidates & Bit(Context, Slot)) != 0 && Context.Slots[Targeting.Target].Group == Context.Slots[Slot].Group)
				{
					Mask = static_cast<uint16>(Mask | Bit(Context, Slot));
				}
			}
			return Mask;
		}

		/**
		 * A targeting word's candidates and first target: bits 0-3 the mode (0 one, 1 all, 2 a group), 1000h the
		 * enemies' side, 2000h both sides, 4000h the actor alone, 8000h the knocked out only.
		 */
		void BuildTargetList(FBattleContext& Context, uint16 Word, uint8 Actor, bool bReachable)
		{
			FBattleTargeting& Targeting = Context.Targeting;
			const bool bPartySide = (Word & 0x1000) == 0;
			uint16 SideMask = bPartySide ? 0x0007 : 0xFFF8;
			uint8 Mode = bPartySide ? 1 : 0;
			if ((Word & 0x2000) != 0)
			{
				Mode = 2;
				SideMask = 0xFFFF;
			}
			Targeting.Target = bReachable ? ListReachableTargets(Context, Actor) : ListTargets(Context, Mode, (Word & KnockedOut) != 0, bPartySide);
			Targeting.Candidates = static_cast<uint16>(Targeting.Candidates & SideMask);
			if ((Word & KnockedOut) == 0)
			{
				if ((Word & 0x4000) != 0)
				{
					Targeting.Target = Actor;
					Targeting.Candidates = Bit(Context, Actor);
					ClearList(Targeting, SearchedEntries);
				}
				return;
			}
			uint8 Next = 0;
			Targeting.Target = None;
			for (uint8 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
			{
				if ((Targeting.Candidates & Bit(Context, Slot)) == 0)
				{
					continue;
				}
				if ((TargetStatus(Context, Slot) & KnockedOut) == 0)
				{
					Targeting.Candidates = static_cast<uint16>(Targeting.Candidates & ~Bit(Context, Slot));
				}
				else
				{
					Targeting.Target = Slot;
					Targeting.List[Next] = Slot;
					++Next;
				}
			}
			for (; Next < SearchedEntries; ++Next)
			{
				Targeting.List[Next] = None;
			}
		}

		/** Whether an angle (4096 a turn) lies in a direction's quarter: 0 right, 1 and 3 across, 2 the rest. */
		bool InQuarter(int32 Angle, uint8 Direction)
		{
			const uint32 Turned = static_cast<uint32>(Angle);
			switch (Direction)
			{
			case 0: return ((Turned + 0x200) & 0xFFFF) < 0x400;
			case 1: return ((Turned + 0x600) & 0xFFFF) < 0x400;
			case 2: return ((Turned + 0x800) & 0xFFFF) < 0x200 || ((Turned - 0x600) & 0xFFFF) < 0x201;
			case 3: return ((Turned - 0x200) & 0xFFFF) < 0x400;
			default: return false;
			}
		}
	}

	namespace BattleParty
	{
		bool CanReachTarget(FBattleContext& Context, uint8 Actor, uint8 Target)
		{
			if (!Context.InBattle[Target] || Context.Slots[Target].Presence != 0)
			{
				return false;
			}
			const FBattleCombatant& Combatant = Context.Combatants[Target];
			if (Context.ApRecords[Target].InGear != 0)
			{
				return (Combatant.GearPart.Status & KnockedOutOrGone) == 0;
			}
			return Context.Stage.Route(Context.Slots[Actor].Group, Context.Slots[Target].Group, 0) == 0 && (Combatant.Status & KnockedOutOrGone) == 0;
		}

		uint8 FindTargetInDirection(FBattleContext& Context, uint8 From, uint8 Direction)
		{
			uint8 Found = From;
			int32 Nearest = 0xFFFFFF;
			for (std::size_t Entry = 0; Entry < SearchedEntries; ++Entry)
			{
				const uint8 Slot = Context.Targeting.List[Entry];
				if (Slot == None || Slot == From)
				{
					continue;
				}
				if (From >= FBattleContext::SlotCount || Slot >= FBattleContext::SlotCount)
				{
					Context.Fault = EBattleFault::SlotOutsideBattle;
					return From;
				}
				const FBattleStagePoint At = Context.Slots[From].Position;
				const FBattleStagePoint To = Context.Slots[Slot].Position;
				const int32 Across = static_cast<int32>(To.Z) - static_cast<int32>(At.Z);
				const int32 Along = static_cast<int32>(To.X) - static_cast<int32>(At.X);
				if (!InQuarter(GteRatan2(Across, Along, *Context.Arctangents), Direction))
				{
					continue;
				}
				const int32 Distance = static_cast<int32>(static_cast<uint32>(Across) * static_cast<uint32>(Across)
					+ static_cast<uint32>(Along) * static_cast<uint32>(Along));
				if (Distance < Nearest)
				{
					Found = Slot;
					Nearest = Distance;
				}
			}
			return Found;
		}

		EFlow CallSelectTarget(FBattleContext& Context, uint16 Word, uint8 Actor, bool bReachable)
		{
			FBattleTargetSelect& Select = Context.Party.Select;
			Select.Word = Word;
			Select.Actor = Actor;
			Select.bReachable = bReachable;
			return Call(Context, EBattlePartyRoutine::SelectTarget);
		}

		EFlow RunSelectTarget(FBattleContext& Context, FBattlePartyFrame& Frame)
		{
			FBattleTargetSelect& Select = Context.Party.Select;
			FBattleTargeting& Targeting = Context.Targeting;
			if (Frame.Pc == SelectStart)
			{
				BuildTargetList(Context, Select.Word, Select.Actor, Select.bReachable);
				Select.Result = Targeting.Target == None ? 1 : 0;
				Select.Confirmed = Targeting.Candidates;
				Select.ArrowsEvent = 0xFE;
				if (Select.Result != 0)
				{
					return ReturnValue(Context, 0);
				}
				Frame.Pc = SelectFrame;
				return EFlow::Yield;
			}
			switch (Select.Word & 0xF)
			{
			case 0:
				SetMarkers(Context, Bit(Context, Targeting.Target));
				FrameCamera(Context, Bit(Context, Targeting.Target));
				break;
			case 1:
				SetMarkers(Context, Targeting.Candidates);
				FrameCamera(Context, Targeting.Candidates);
				break;
			case 2:
				Select.Confirmed = TargetGroupMask(Context);
				SetMarkers(Context, Select.Confirmed);
				FrameCamera(Context, Select.Confirmed);
				break;
			default:
				break;
			}
			const uint8 Event = Context.Party.Event;
			if (Event <= BattleMenuEvents::Up)
			{
				const uint8 Moved = FindTargetInDirection(Context, Targeting.Target, Event);
				if (Moved < 16 && (Targeting.Candidates & (1u << Moved)) != 0)
				{
					Targeting.Target = Moved;
				}
			}
			else if (Event == BattleMenuEvents::Confirm)
			{
				Select.Result = 2;
				Targeting.Candidates = Select.Confirmed;
			}
			else if (Event == BattleMenuEvents::Cancel)
			{
				Select.Result = 1;
			}
			if (Event != Select.ArrowsEvent)
			{
				uint8 Ways = 0;
				for (uint8 Way = 0; Way < 4; ++Way)
				{
					if (FindTargetInDirection(Context, Targeting.Target, Way) != Targeting.Target)
					{
						Ways = static_cast<uint8>(Ways | 1u << Way);
					}
				}
				PlayMenuCue(Context, EBattleMenuCue::ArrowTargets, Select.Actor, Ways);
				Select.ArrowsEvent = Event;
			}
			if (Select.Result == 0)
			{
				return EFlow::Yield;
			}
			return ReturnValue(Context, static_cast<uint8>(Select.Result - 1));
		}
	}

	uint8 PickBattleDefaultTarget(FBattleContext& Context, uint8 Slot)
	{
		FBattleTargeting& Targeting = Context.Targeting;
		std::array<uint8, 16> Reachable{};
		std::array<bool, 24> SameGroup{};
		Reachable.fill(None);
		ClearList(Targeting, Targeting.List.size());
		Targeting.Count = 0;
		uint8 Found = 0;
		const bool bParty = Slot < FBattleContext::PartySlotCount;
		const uint8 First = bParty ? FBattleContext::PartySlotCount : 0;
		const uint8 Last = bParty ? FBattleContext::SlotCount : FBattleContext::PartySlotCount;
		for (uint8 Target = First; Target < Last; ++Target)
		{
			if (CanReachTarget(Context, Slot, Target))
			{
				Reachable[Found] = Target;
				++Found;
				++Targeting.Count;
			}
		}
		// At most eight are reachable (the enemies), so the list's twelve hold them all.
		const std::size_t Listable = std::min<std::size_t>(Found, Targeting.List.size());
		const uint8 Group = Context.Slots[Slot].Group;
		std::size_t Listed = 0;
		for (std::size_t Index = 0; Index < Listable; ++Index)
		{
			if (Context.Slots[Reachable[Index]].Group == Group)
			{
				Targeting.List[Listed] = Reachable[Index];
				Reachable[Index] = None;
				SameGroup[Listed] = true;
				++Listed;
			}
		}
		for (std::size_t Index = 0; Index < Listable && Listed < Listable; ++Index)
		{
			if (Reachable[Index] != None)
			{
				Targeting.List[Listed] = Reachable[Index];
				++Listed;
			}
		}
		// The lowest HP comes to the front by swaps, among the same group's when any shares it.
		for (std::size_t Index = 1; Index < Listable; ++Index)
		{
			if (SameGroup[0] && !SameGroup[Index])
			{
				continue;
			}
			const uint8 Candidate = Targeting.List[Index];
			const uint8 Front = Targeting.List[0];
			if (Context.Combatants[Candidate].Hp < Context.Combatants[Front].Hp)
			{
				Targeting.List[Index] = Front;
				Targeting.List[0] = Candidate;
			}
		}
		return Targeting.List[0];
	}

	void AimBattleSlot(FBattleContext& Context, uint8 Slot)
	{
		const uint8 Target = PickBattleDefaultTarget(Context, Slot);
		Context.SlotTargets[Slot] = Target;
		const uint16 TargetX = Target < FBattleContext::SlotCount ? Context.Slots[Target].Position.X : NoTargetX;
		Context.Slots[Slot].FacesLeft = TargetX < Context.Slots[Slot].Position.X ? 1 : 0;
	}
}
