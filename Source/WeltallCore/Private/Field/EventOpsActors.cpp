#include "EventOps.h"

#include "Graphics/Gte.h"

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint32 Hidden = 0x20;
		constexpr uint32 ExtraAxisMask = 0x3;
		/** The movement chooses the actor's animations. */
		constexpr uint16 NoAnimationRequest = 0xFF;

		/** An actor's record, or a fault when the map has no such actor. */
		/** 22: shows the running actor and its sprite, and resets its current animation. */
		void Show(FEventContext& Context)
		{
			Context.Actors[static_cast<std::size_t>(Context.Run.Actor)].Flags &= ~Hidden;
			FEventActorState& State = Self(Context);
			State.Animation = 0xFF;
			State.Flags1 &= ~EventActorFlags1::SpriteHidden;
			Advance(Context, 1);
		}

		void Hide(FEventContext& Context)
		{
			Context.Actors[static_cast<std::size_t>(Context.Run.Actor)].Flags |= Hidden;
			Advance(Context, 1);
		}

		/** 24: shows another actor, unless it was removed. */
		void ShowActor(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				FFieldActor* Record = RecordOf(Context, Actor);
				FEventActorState* Target = StateOf(Context, Actor);
				if (Record == nullptr || Target == nullptr)
				{
					return;
				}
				if ((Target->Flags1 & EventActorFlags1::Removed) == 0)
				{
					Record->Flags &= ~Hidden;
					Target->Flags1 &= ~EventActorFlags1::SpriteHidden;
				}
			}
			Advance(Context, 2);
		}

		/** 25: hides any actor of the map, script actor or not. */
		void HideActor(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				FFieldActor* Record = RecordOf(Context, Actor);
				if (Record == nullptr)
				{
					return;
				}
				Record->Flags |= Hidden;
			}
			Advance(Context, 2);
		}

		void Unfreeze(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				FEventActorState* Target = StateOf(Context, Actor);
				if (Target == nullptr)
				{
					return;
				}
				Target->Flags0 &= ~EventActorFlags0::Frozen;
			}
			Advance(Context, 2);
		}

		template <bool bAllow>
		void SetTalk(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.Flags0 = bAllow ? State.Flags0 & ~EventActorFlags0::NoTalk : State.Flags0 | EventActorFlags0::NoTalk;
			Advance(Context, 1);
		}

		/** 18: the body's half width and depth, height and radius, in units of 2; 0 keeps a size. */
		void SetCollisionSize(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			uint16* Sizes[] = {&State.HalfWidth, &State.HalfDepth, &State.Height, &State.Radius};
			for (int32 Operand = 0; Operand < 4; ++Operand)
			{
				const uint8 Size = CodeByte(Context, 1 + Operand);
				if (Size != 0)
				{
					*Sizes[Operand] = static_cast<uint16>(Size << 1);
				}
			}
			Advance(Context, 5);
		}

		/** 1F: which walkmesh layers (bits 0-2) and which layers' attributes (bits 4-6) the actor ignores. */
		void SetWalkmeshMask(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			const uint8 Mask = CodeByte(Context, 1);
			State.Flags1 = (State.Flags1 & ~0x3Fu) | (Mask & 7u) | (Mask >> 1 & 0x38u);
			Advance(Context, 2);
		}

		/** 20: the operand's bits in the order of the actor's collision flags. */
		void SetCollisionFlags(FEventContext& Context)
		{
			const uint32 Value = static_cast<uint32>(ReadValue(Context, 1));
			uint32 Flags = (Value & 1) << 7;
			constexpr std::array<std::pair<uint32, uint32>, 5> Bits = {{{0x4, 0x20}, {0x8, 0x10}, {0x10, 0x8}, {0x20, 0x4}, {0x40, 0x8000000}}};
			for (const auto& [From, To] : Bits)
			{
				if ((Value & From) != 0)
				{
					Flags |= To;
				}
			}
			FEventActorState& State = Self(Context);
			State.Flags0 = (State.Flags0 & 0xF7FFFF43u) | Flags;
			Advance(Context, 3);
		}

		/** 2C: the animation the actor's movement plays (FF: chosen by the movement), animations on. */
		void SetMoveAnimation(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.Flags1 &= ~EventActorFlags1::NoAnimations;
			State.AnimationRequest = CodeByte(Context, 1);
			Advance(Context, 2);
		}

		/** 5D: 2C, the animation's end not yet seen: 5E then waits for it. */
		void PlayAnimation(FEventContext& Context)
		{
			SetMoveAnimation(Context);
			Self(Context).Flags1 &= ~EventActorFlags1::AnimationFinished;
		}

		/**
		 * 5E: once the actor's animation has ended, the movement chooses its animations again. Until
		 * then it runs again at once, neither moving on nor ending the turn: the run's budget ends it.
		 */
		void WaitAnimation(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			if ((State.Flags1 & EventActorFlags1::AnimationFinished) != 0)
			{
				State.AnimationRequest = NoAnimationRequest;
				Advance(Context, 1);
			}
		}

		/** FE 45: the animation the actor returns to whenever it stands still, from its next stop. */
		void SetIdleAnimation(FEventContext& Context)
		{
			Self(Context).IdleAnimation = CodeByte(Context, 1);
			Advance(Context, 2);
		}

		/** 2D: any actor's integer position into three variables: x, z, then y. */
		void GetActorPosition(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				const FFieldActor* Record = RecordOf(Context, Actor);
				if (Record == nullptr)
				{
					return;
				}
				SetVar(Context, ReadU16(Context, 2), Record->Position[0]);
				SetVar(Context, ReadU16(Context, 4), Record->Position[2]);
				SetVar(Context, ReadU16(Context, 6), Record->Position[1]);
			}
			Advance(Context, 8);
		}

		void GetCharacter(FEventContext& Context)
		{
			SetVar(Context, ReadU16(Context, 1), static_cast<int16>(Self(Context).Character));
			Advance(Context, 3);
		}

		void GetLeaderCharacter(FEventContext& Context)
		{
			const FEventActorState* Leader = StateOf(Context, Context.LeaderActor);
			if (Leader == nullptr)
			{
				return;
			}
			SetVar(Context, ReadU16(Context, 1), static_cast<int16>(Leader->Character));
			Advance(Context, 3);
		}

		/** 46: the point others interact with, 36 units along the actor's model as its yaw turns it (the middle of a door, whose model starts at its hinge), and its model left as placed. */
		void SetInteractOffsetFront(FEventContext& Context)
		{
			const int32 Yaw = Context.Actors[static_cast<std::size_t>(Context.Run.Actor)].Rotation[1];
			FEventActorState& State = Self(Context);
			State.InteractX = static_cast<int16>(static_cast<uint32>(GteCos(Yaw) * 9) >> 10);
			State.InteractZ = static_cast<int16>(GteSin(Yaw) * -0x24 >> 12);
			State.Flags1 |= EventActorFlags1::NotFloorAligned;
			Advance(Context, 1);
		}

		template <bool bSet>
		void SetFlag23(FEventContext& Context)
		{
			constexpr uint32 Bit = 0x800000;
			FEventActorState& State = Self(Context);
			State.Flags0 = bSet ? State.Flags0 | Bit : State.Flags0 & ~Bit;
			Advance(Context, 1);
		}

		/** F8: sets (modes 0-3) or clears (4-7) the operand's bits in a half of one of the actor's flag words. */
		void SetStateFlags(FEventContext& Context)
		{
			const uint32 Bits = ReadU16(Context, 2);
			FEventActorState& State = Self(Context);
			const uint8 Mode = CodeByte(Context, 1);
			if (Mode < 8)
			{
				uint32& Word = (Mode & 2) != 0 ? State.Flags1 : State.Flags0;
				const uint32 Shifted = (Mode & 1) != 0 ? Bits << 16 : Bits;
				Word = Mode < 4 ? Word | Shifted : Word & ~Shifted;
			}
			Advance(Context, 4);
		}

		/** D7-D9: an extra rotation of the model about one axis. */
		template <uint32 Axis>
		void SetExtraRotation(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.Status = (State.Status & ~ExtraAxisMask) | Axis;
			State.ExtraAngle = static_cast<int16>(ReadValue(Context, 1));
			Advance(Context, 3);
		}

		/** F9: attaches the actor to another's model. */
		void AttachToActor(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				Self(Context).Parent = static_cast<uint8>(Actor);
			}
			Advance(Context, 2);
		}

		/** FE 49: detaches the actor from the model F9 attached it to. */
		void DetachFromActor(FEventContext& Context)
		{
			Self(Context).Parent = FEventContext::NoActor;
			Advance(Context, 1);
		}

		/** FE 07: mode 0 or 1 clears or sets the flag that keeps a hidden sprite drawn and updated. */
		void SetShowFlag(FEventContext& Context)
		{
			constexpr uint32 Bit = 0x400;
			FEventActorState& State = Self(Context);
			const uint8 Mode = CodeByte(Context, 1);
			if (Mode == 0)
			{
				State.Flags1 &= ~Bit;
			}
			else if (Mode == 1)
			{
				State.Flags1 |= Bit;
			}
			Advance(Context, 2);
		}

		void SetNoFloorAlign(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.Flags1 = ReadValue(Context, 1) == 0 ? State.Flags1 & ~EventActorFlags1::NotFloorAligned : State.Flags1 | EventActorFlags1::NotFloorAligned;
			Advance(Context, 3);
		}

		/** FE 0D: the portrait beside the actor's messages; FF, FE and FD name the party's slots 2, 1 and 0, FC none. */
		void SetMessageFace(FEventContext& Context)
		{
			Self(Context).Portrait = static_cast<uint8>(ResolveCharacter(Context, ReadValue(Context, 1)));
			Advance(Context, 3);
		}
	}

	void RegisterActors(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0x18] = &SetCollisionSize;
		Base[0x1F] = &SetWalkmeshMask;
		Base[0x20] = &SetCollisionFlags;
		Base[0x22] = &Show;
		Base[0x23] = &Hide;
		Base[0x24] = &ShowActor;
		Base[0x25] = &HideActor;
		Base[0x28] = &Unfreeze;
		Base[0x2A] = &SetTalk<false>;
		Base[0x2B] = &SetTalk<true>;
		Base[0x2C] = &SetMoveAnimation;
		Base[0x5D] = &PlayAnimation;
		Base[0x5E] = &WaitAnimation;
		Base[0x2D] = &GetActorPosition;
		Base[0x2F] = &GetCharacter;
		Base[0x30] = &GetLeaderCharacter;
		Base[0x46] = &SetInteractOffsetFront;
		Base[0xCD] = &SetFlag23<true>;
		Base[0xCE] = &SetFlag23<false>;
		Base[0xD7] = &SetExtraRotation<1>;
		Base[0xD8] = &SetExtraRotation<2>;
		Base[0xD9] = &SetExtraRotation<3>;
		Base[0xF8] = &SetStateFlags;
		Base[0xF9] = &AttachToActor;
		Extended[0x07] = &SetShowFlag;
		Extended[0x09] = &SetNoFloorAlign;
		Extended[0x0D] = &SetMessageFace;
		Extended[0x45] = &SetIdleAnimation;
		Extended[0x49] = &DetachFromActor;
	}
}
