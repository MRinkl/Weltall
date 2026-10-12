#include "EventOps.h"
#include "EventSprites.h"

#include "Field/EventPlacement.h"
#include "Field/FieldFollowers.h"
#include "Field/FieldPartySprites.h"
#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Graphics/Sprite.h"

namespace Weltall::EventOps
{
	namespace
	{
		constexpr int32 PartySlots = FPartySpriteFiles::Slots;
		constexpr int32 GearModeBits = 0xC000;
		constexpr uint32 KindFieldMask = 0xF07F;
		constexpr uint16 SpriteActorKind = 0x200;
		constexpr uint8 SetPartyCharacterOpcode = 0x16;
		constexpr int32 InitScript = 0;

		/** The instruction waits, running again next tick. */
		void Wait(FEventContext& Context)
		{
			Context.Run.bYield = true;
			Advance(Context, -1);
		}

		FPartySpriteFiles* PartySpritesOf(FEventContext& Context)
		{
			if (Context.PartySprites == nullptr)
			{
				Fail(Context, EEventFault::NoPartySprites, 0);
			}
			return Context.PartySprites;
		}

		uint8& Member(FEventContext& Context, int32 Slot)
		{
			return Context.PartyCharacters[static_cast<std::size_t>(Slot)];
		}

		/** The first empty party slot for a character, or -1 when the party is full or the character is in it before one. */
		int32 FreeSlotFor(const FEventContext& Context, int32 Character)
		{
			for (int32 Slot = 0; Slot < PartySlots; ++Slot)
			{
				const uint8 InSlot = Context.PartyCharacters[static_cast<std::size_t>(Slot)];
				if (InSlot == Character)
				{
					return -1;
				}
				if (InSlot == FEventContext::NoActor)
				{
					return Slot;
				}
			}
			return -1;
		}

		void MarkJoined(FGameState& Game, int32 Character)
		{
			Game.SetJoinedCharacters(static_cast<uint16>(Game.JoinedCharacters() | 1u << (Character & 31)));
		}

		/**
		 * FE 18 and FE C6: a character joins the party's first empty slot, not riding a gear, and its
		 * sprite file (its own, or in gears its gear's) starts loading for FE 1A, which the instruction
		 * falls through to. A character already in the party, or a full party, only marks the character
		 * as joined, and the instruction skips the FE 1A.
		 */
		void JoinParty(FEventContext& Context, int32 Character, int32 Length)
		{
			FGameState* Game = GameStateOf(Context);
			FPartySpriteFiles* Files = PartySpritesOf(Context);
			if (Game == nullptr || Files == nullptr)
			{
				return;
			}
			const int32 Slot = FreeSlotFor(Context, Character);
			if (Slot < 0)
			{
				MarkJoined(*Game, Character);
				Advance(Context, Length + 2);
				return;
			}
			const std::optional<int32> File = FPartySpriteFiles::FileFor(*Game, Character, (Context.MapNumber & GearModeBits) != 0);
			if (!File)
			{
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Character));
				return;
			}
			Game->SetPartySlotInGear(Slot, 0);
			Member(Context, Slot) = static_cast<uint8>(Character);
			Files->SetFile(Slot, *File);
			Context.MemberLoad = {true, static_cast<uint8>(Character), static_cast<uint8>(Slot), *File};
			Advance(Context, Length);
		}

		/** FE 18: a character joins the party (JoinParty); waits while another member's file loads. */
		void PartyAdd(FEventContext& Context)
		{
			if (Context.MemberLoad.bPending)
			{
				Wait(Context);
				return;
			}
			JoinParty(Context, CodeByte(Context, 1), 2);
		}

		/** FE C6: FE 18 with the character a value; FF joins nobody and skips the FE 1A. */
		void JoinPartyValue(FEventContext& Context)
		{
			if (Context.MemberLoad.bPending)
			{
				Wait(Context);
				return;
			}
			const int32 Character = ReadValue(Context, 1);
			if (Character == 0xFF)
			{
				Advance(Context, 5);
				return;
			}
			if (Character < 0 || Character > 0xFF)
			{
				// The party's list keeps a byte per member; the original would load a file that is no sprite's.
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Character));
				return;
			}
			JoinParty(Context, Character, 3);
		}

		/** An actor's script state reset where its record stands, as the field resets one: its defaults, then the floor under it. */
		bool ResetActor(FEventContext& Context, int32 Actor)
		{
			FEventActorState* State = StateOf(Context, static_cast<uint32>(Actor));
			if (State == nullptr)
			{
				return false;
			}
			State->Reset(Context.Random->Next());
			if (Context.Walkmesh == nullptr || !PlaceActorOnLoad(*State, Context.Actors[static_cast<std::size_t>(Actor)], *Context.Walkmesh))
			{
				Fail(Context, EEventFault::NoFloorTriangle, static_cast<uint16>(State->Layer));
				return false;
			}
			return true;
		}

		/** An actor put where the leader stands: its floors, layer, position, heights and floor attributes, its sprite's and record's place. */
		bool PutOnLeader(FEventContext& Context, int32 Actor)
		{
			FEventActorState* State = StateOf(Context, static_cast<uint32>(Actor));
			const FEventActorState* Leader = StateOf(Context, Context.LeaderActor);
			if (State == nullptr || Leader == nullptr)
			{
				return false;
			}
			FSprite* Sprite = ActorSprite(Context, Actor);
			const FSprite* LeaderSprite = ActorSprite(Context, Context.LeaderActor);
			if (Sprite == nullptr || LeaderSprite == nullptr)
			{
				Fail(Context, EEventFault::ActorWithoutSprite, static_cast<uint32>(Sprite == nullptr ? Actor : Context.LeaderActor));
				return false;
			}
			State->FloorTriangles = Leader->FloorTriangles;
			State->Layer = Leader->Layer;
			State->FloorNormal = Leader->FloorNormal;
			State->Position = Leader->Position;
			State->TargetHeight = Leader->TargetHeight;
			State->FloorHeight = Leader->FloorHeight;
			State->FloorAttributes = Leader->FloorAttributes;
			Sprite->FloorY = LeaderSprite->FloorY;
			Sprite->Position = LeaderSprite->Position;
			Context.Actors[static_cast<std::size_t>(Actor)].Translation = Context.Actors[Context.LeaderActor].Translation;
			return true;
		}

		/** An actor's init script run at once from its start; false on a fault. */
		bool RunInitAgain(FEventContext& Context, int32 Actor)
		{
			Context.States[static_cast<std::size_t>(Actor)].Pc = Context.Scripts->EntryPoint(Actor, InitScript);
			RunScriptNow(Context, Actor);
			return Context.Run.Fault == EEventFault::None;
		}

		/** The actor that stands for a character: the first whose init script starts with 16 and the character's byte. */
		int32 MemberActorOf(const FEventContext& Context, uint8 Character)
		{
			const uint8* Code = Context.Scripts->Code();
			for (int32 Actor = 0; Actor < Context.Scripts->ActorCount(); ++Actor)
			{
				const uint16 Entry = Context.Scripts->EntryPoint(Actor, InitScript);
				if (Entry < Context.Scripts->CodeSize() && Code[Entry] == SetPartyCharacterOpcode && Code[Entry + 1] == Character)
				{
					return Actor;
				}
			}
			return -1;
		}

		/**
		 * The joining member's actor once the map runs: reset, put on the leader and its init script run
		 * at once, which makes it the party's; the party is then set on the leader as the map's load sets
		 * it, its layer steps on a platform unless the member's actor is actor 0 (the original passes a
		 * word it never set, which here holds that actor's number). In gears the slot's gear actor is
		 * reset and its init script run too, and the member's pc put back on its init's start. The running
		 * actor and its registers are kept, but for whether its run is unlimited, which the scripts run
		 * last leave as they end.
		 */
		void SetUpJoiningMember(FEventContext& Context, FGameState& Game, const FPartyMemberLoad& Load)
		{
			const int32 Actor = MemberActorOf(Context, Load.Character);
			if (Actor < 0)
			{
				return;
			}
			const FEventRun Caller = Context.Run;
			const uint16 CallerPc = Caller.State->Pc;
			bool bSetUp = ResetActor(Context, Actor) && PutOnLeader(Context, Actor) && RunInitAgain(Context, Actor);
			if (bSetUp)
			{
				if (const std::optional<int32> Unplaced = PlacePartyOnLeader(Context, Actor != 0))
				{
					const FEventActorState* State = StateOf(Context, static_cast<uint32>(*Unplaced));
					if (State != nullptr && ActorSprite(Context, *Unplaced) == nullptr)
					{
						Fail(Context, EEventFault::ActorWithoutSprite, static_cast<uint32>(*Unplaced));
					}
					else if (State != nullptr)
					{
						Fail(Context, EEventFault::NoFloorTriangle, State->Layer);
					}
					bSetUp = false;
				}
			}
			if (bSetUp)
			{
				MarkJoined(Game, Load.Character);
				if ((Context.MapNumber & GearModeBits) != 0)
				{
					const uint8 Gear = Context.GearActors[Load.Slot];
					if (ResetActor(Context, Gear))
					{
						Context.States[static_cast<std::size_t>(Actor)].Pc = Context.Scripts->EntryPoint(Actor, InitScript);
						RunInitAgain(Context, Gear);
					}
				}
			}
			const FEventRun Nested = Context.Run;
			Context.Run = Caller;
			Context.Run.bUnlimited = Nested.bUnlimited;
			Context.Run.Fault = Nested.Fault;
			Context.Run.FaultValue = Nested.FaultValue;
			Caller.State->Pc = CallerPc;
		}

		/**
		 * FE 1A: the joining member's sprite file is unpacked into its slot's buffer and the character
		 * marked as joined; once the map's init is over its actor is set up on the leader too. The actor's
		 * turn ends. It waits until a file is on its way.
		 */
		void PartyAddWait(FEventContext& Context)
		{
			FPartyMemberLoad& Load = Context.MemberLoad;
			if (!Load.bPending)
			{
				Wait(Context);
				return;
			}
			FGameState* Game = GameStateOf(Context);
			FPartySpriteFiles* Files = PartySpritesOf(Context);
			if (Game == nullptr || Files == nullptr)
			{
				return;
			}
			if (Files->Unpack(Load.Slot, Load.File))
			{
				Fail(Context, EEventFault::PartySpriteFileUnreadable, static_cast<uint32>(Load.File + FPartySpriteFiles::DiscFileOffset));
				return;
			}
			MarkJoined(*Game, Load.Character);
			if (Context.bInitDone)
			{
				SetUpJoiningMember(Context, *Game, Load);
				if (Context.Run.Fault != EEventFault::None)
				{
					return;
				}
			}
			Load.bPending = false;
			Context.Run.bYield = true;
			Advance(Context, 1);
		}

		/**
		 * FE 1E: the party goes into gears for the map: the map's gear bits are set, the party's
		 * sprite files become the gears' (unless they are already), and the operand becomes the base
		 * of the pilots' map sprites.
		 */
		void GearMode(FEventContext& Context)
		{
			FGameState* Game = GameStateOf(Context);
			FPartySpriteFiles* Files = PartySpritesOf(Context);
			if (Game == nullptr || Files == nullptr)
			{
				return;
			}
			Context.MapNumber |= GearModeBits;
			if (Files->LoadForMap(true, *Game, Context.PartyCharacters))
			{
				Fail(Context, EEventFault::PartySpriteFileUnreadable, 0);
				return;
			}
			Context.PilotSpriteBase = CodeByte(Context, 1);
			Advance(Context, 2);
		}

		/** Moves the member of slot From down to slot To during the map's init: its buffer, file, character and gear byte. */
		void MoveMemberDown(FEventContext& Context, FPartySpriteFiles& Files, FGameState& Game, int32 From)
		{
			const int32 To = From - 1;
			Files.Copy(From, To);
			Member(Context, To) = Member(Context, From);
			Member(Context, From) = FEventContext::NoActor;
			Files.SetFile(From, FPartySpriteFiles::NoFile);
			Game.SetPartySlotInGear(To, Game.PartySlotInGear(From));
		}

		/** Empties a slot during the map's init. */
		void EmptySlot(FEventContext& Context, FPartySpriteFiles& Files, FGameState& Game, int32 Slot)
		{
			Member(Context, Slot) = FEventContext::NoActor;
			Files.SetFile(Slot, FPartySpriteFiles::NoFile);
			Game.SetPartySlotInGear(Slot, 0);
		}

		/**
		 * FE 19 during the map's init: the party's lists close the gap the character leaves, the later
		 * members' buffers copied down over the earlier ones. A member moved down from slot 1 to 0 with
		 * nobody after it leaves slot 1's gear byte as it was, and one moved from 2 leaves slot 2's.
		 */
		void CloseGap(FEventContext& Context, FPartySpriteFiles& Files, FGameState& Game, int32 Slot)
		{
			if (Slot == 2)
			{
				EmptySlot(Context, Files, Game, 2);
				return;
			}
			if (Member(Context, Slot + 1) == FEventContext::NoActor)
			{
				EmptySlot(Context, Files, Game, Slot);
				return;
			}
			MoveMemberDown(Context, Files, Game, Slot + 1);
			if (Slot == 1)
			{
				Game.SetPartySlotInGear(2, 0);
			}
			else if (Member(Context, 2) != FEventContext::NoActor)
			{
				MoveMemberDown(Context, Files, Game, 2);
			}
		}

		/**
		 * The actor of a slot a member leaves once the map runs: reset where it stands, given slot 0's
		 * sprite, frozen and taken out of the map; the slot is emptied.
		 */
		bool ClearSlot(FEventContext& Context, FPartySpriteFiles& Files, int32 Slot)
		{
			uint8& Actor = Context.PartyActors[static_cast<std::size_t>(Slot)];
			if (Actor != FEventContext::NoActor)
			{
				FEventActorState* State = StateOf(Context, Actor);
				if (State == nullptr || Context.Random == nullptr)
				{
					return false;
				}
				FFieldActor& Record = Context.Actors[Actor];
				// The running thread's pc outlives the reset in the reset actor's state: the running
				// actor's own when it clears itself.
				const uint16 RunningPc = Self(Context).Pc;
				State->Reset(Context.Random->Next());
				State->Pc = RunningPc;
				if (Context.Walkmesh == nullptr || !PlaceActorOnLoad(*State, Record, *Context.Walkmesh))
				{
					Fail(Context, EEventFault::NoFloorTriangle, static_cast<uint16>(State->Layer));
					return false;
				}
				Record.Flags = static_cast<uint16>((Record.Flags & KindFieldMask) | SpriteActorKind);
				if (!LoadActorSprite(Context, Actor, EFieldSpriteKind::Party, 0, 0, 0, true))
				{
					return false;
				}
				State->Flags0 |= EventActorFlags0::Frozen | EventActorFlags0::NoTalk;
				State->Flags1 |= EventActorFlags1::Removed | EventActorFlags1::KeepsRunningOffScreen;
				Context.Run.bYield = false;
				Actor = FEventContext::NoActor;
			}
			Member(Context, Slot) = FEventContext::NoActor;
			Files.SetFile(Slot, FPartySpriteFiles::NoFile);
			return true;
		}

		/**
		 * The member after slot To moves down into it once the map runs: its buffer, file, character
		 * and actor, whose sprite is made again from its new slot (or from its map sprite when it has
		 * one).
		 */
		bool ShiftDown(FEventContext& Context, FPartySpriteFiles& Files, int32 To)
		{
			const int32 From = To + 1;
			const uint8 Actor = Context.PartyActors[static_cast<std::size_t>(From)];
			if (Actor != FEventContext::NoActor)
			{
				Files.Copy(From, To);
			}
			else
			{
				Files.SetFile(To, Files.File(From));
			}
			Member(Context, To) = Member(Context, From);
			Context.PartyActors[static_cast<std::size_t>(To)] = Actor;
			if (Actor != FEventContext::NoActor)
			{
				FEventActorState* State = StateOf(Context, Actor);
				if (State == nullptr)
				{
					return false;
				}
				const bool bMapSprite = (State->ModelId & 0x80) != 0;
				const bool bLoaded = bMapSprite
					? LoadActorSprite(Context, Actor, EFieldSpriteKind::Map, State->ModelIndex, static_cast<int32>(State->SpriteDraw & 0xF), State->ModelId, (State->SpriteDraw >> 4 & 1) != 0)
					: LoadActorSprite(Context, Actor, EFieldSpriteKind::Party, To, 0, static_cast<uint8>(To), true);
				if (!bLoaded)
				{
					return false;
				}
			}
			Member(Context, From) = FEventContext::NoActor;
			Files.SetFile(From, FPartySpriteFiles::NoFile);
			Context.PartyActors[static_cast<std::size_t>(From)] = FEventContext::NoActor;
			return true;
		}

		/** FE 19 once the map runs: the slot's actor leaves, the later members move down, and slot 0's actor leads. */
		bool LeaveParty(FEventContext& Context, FPartySpriteFiles& Files, FGameState& Game, int32 Slot)
		{
			for (int32 Later = Slot; Later < PartySlots - 1; ++Later)
			{
				Game.SetPartySlotInGear(Later, Game.PartySlotInGear(Later + 1));
			}
			Game.SetPartySlotInGear(PartySlots - 1, 0);
			if (!ClearSlot(Context, Files, Slot))
			{
				return false;
			}
			for (int32 To = Slot; To < PartySlots - 1; ++To)
			{
				if (!ShiftDown(Context, Files, To))
				{
					return false;
				}
			}
			const uint8 Leader = Context.PartyActors[0];
			if (Member(Context, 0) == FEventContext::NoActor || Leader == FEventContext::NoActor)
			{
				Context.LeaderActor = 0;
				return true;
			}
			FEventActorState* State = StateOf(Context, Leader);
			if (State == nullptr)
			{
				return false;
			}
			Context.LeaderActor = Leader;
			State->Flags0 = (State->Flags0 & ~EventActorFlags0::PassesThroughActors) | EventActorFlags0::PadControlled | EventActorFlags0::PartyMember;
			return true;
		}

		/** FE 19: a character leaves the party; waits while a joining member's file loads. */
		void PartyRemove(FEventContext& Context)
		{
			if (Context.MemberLoad.bPending)
			{
				Wait(Context);
				return;
			}
			const int32 Slot = PartySlotOf(Context, ResolveCharacter(Context, CodeByte(Context, 1)));
			if (Slot >= 0)
			{
				FGameState* Game = GameStateOf(Context);
				FPartySpriteFiles* Files = PartySpritesOf(Context);
				if (Game == nullptr || Files == nullptr)
				{
					return;
				}
				if (!Context.bInitDone)
				{
					CloseGap(Context, *Files, *Game, Slot);
				}
				else if (!LeaveParty(Context, *Files, *Game, Slot))
				{
					return;
				}
			}
			Advance(Context, 2);
		}
	}

	void RegisterParty(FBaseTable&, FExtendedTable& Extended)
	{
		Extended[0x18] = &PartyAdd;
		Extended[0x19] = &PartyRemove;
		Extended[0x1A] = &PartyAddWait;
		Extended[0x1E] = &GearMode;
		Extended[0xC6] = &JoinPartyValue;
	}
}
