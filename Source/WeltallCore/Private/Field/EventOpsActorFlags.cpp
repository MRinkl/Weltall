#include "EventOps.h"

namespace Weltall::EventOps
{
	namespace
	{
		/** A half of one of an actor's two flag words: word 0 or 1, low or high half. */
		template <int32 Word, bool bHigh>
		uint16 FlagHalf(const FEventActorState& State)
		{
			const uint32 Flags = Word == 0 ? State.Flags0 : State.Flags1;
			return static_cast<uint16>(bHigh ? Flags >> 16 : Flags);
		}

		/** FE 28-2B: copy a half of the running actor's flags into a variable. */
		template <int32 Word, bool bHigh>
		void GetFlags(FEventContext& Context)
		{
			SetVar(Context, ReadU16(Context, 1), FlagHalf<Word, bHigh>(Self(Context)));
			Advance(Context, 3);
		}

		/**
		 * FE 2C-2F: the same for another actor. The actor operand is read from the first byte of the
		 * variable's address, which it overlaps, so the variable's low byte names the actor.
		 */
		template <int32 Word, bool bHigh>
		void GetActorFlags(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				const FEventActorState* Target = StateOf(Context, Actor);
				if (Target == nullptr)
				{
					return;
				}
				SetVar(Context, ReadU16(Context, 1), FlagHalf<Word, bHigh>(*Target));
			}
			Advance(Context, 3);
		}

		/** FE 30-33: go on when any bit of the mask is set in a half of the running actor's flags, else jump. */
		template <int32 Word, bool bHigh>
		void IfFlags(FEventContext& Context)
		{
			Branch(Context, (ReadU16(Context, 1) & FlagHalf<Word, bHigh>(Self(Context))) != 0, 5, 3);
		}

		/** FE 34-37: the same for another actor, which must exist. */
		template <int32 Word, bool bHigh>
		void IfActorFlags(FEventContext& Context)
		{
			const FEventActorState* Target = StateOf(Context, ReadActor(Context, 3));
			if (Target == nullptr)
			{
				return;
			}
			Branch(Context, (ReadU16(Context, 1) & FlagHalf<Word, bHigh>(*Target)) != 0, 6, 4);
		}
	}

	void RegisterActorFlags(FBaseTable&, FExtendedTable& Extended)
	{
		Extended[0x28] = &GetFlags<0, false>;
		Extended[0x29] = &GetFlags<0, true>;
		Extended[0x2A] = &GetFlags<1, false>;
		Extended[0x2B] = &GetFlags<1, true>;
		Extended[0x2C] = &GetActorFlags<0, false>;
		Extended[0x2D] = &GetActorFlags<0, true>;
		Extended[0x2E] = &GetActorFlags<1, false>;
		Extended[0x2F] = &GetActorFlags<1, true>;
		Extended[0x30] = &IfFlags<0, false>;
		Extended[0x31] = &IfFlags<0, true>;
		Extended[0x32] = &IfFlags<1, false>;
		Extended[0x33] = &IfFlags<1, true>;
		Extended[0x34] = &IfActorFlags<0, false>;
		Extended[0x35] = &IfActorFlags<0, true>;
		Extended[0x36] = &IfActorFlags<1, false>;
		Extended[0x37] = &IfActorFlags<1, true>;
	}
}
