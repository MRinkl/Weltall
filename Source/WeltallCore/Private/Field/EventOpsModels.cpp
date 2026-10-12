#include "EventOps.h"
#include "EventSprites.h"

#include "Graphics/GteMatrix.h"
#include "Graphics/GteVectors.h"
#include "Graphics/Sprite.h"
#include "Graphics/SpriteSystem.h"

#include <algorithm>

namespace Weltall::EventOps
{
	void BuildModelMatrix(FEventContext& Context, uint32 Actor)
	{
		FFieldActor& Record = Context.Actors[Actor];
		const FEventActorState& State = Context.States[Actor];
		Record.Matrix = ScaleColumns(RotationXyz(Record.Rotation), {State.Scale[0], State.Scale[1], State.Scale[2]}).M;
	}

	namespace
	{
		constexpr int16 SpriteScale = 0xC00;
		/** The highest weight DB gives a MIMe key. */
		constexpr int32 MaxPartValue = 0xFFF;
		/** The sprite's drawing settings' draw mode: whole, below the line, above it, or both apart. */
		constexpr uint32 DrawModeShift = 5;
		constexpr uint32 DrawModeMask = 0x3u << DrawModeShift;

		void BuildOwnMatrix(FEventContext& Context)
		{
			BuildModelMatrix(Context, static_cast<uint32>(Context.Run.Actor));
		}

		/** 58: one of the actor's angles (0 x, 1 y, 2 z; another sets none), and its matrix. */
		void SetModelRotation(FEventContext& Context)
		{
			const uint8 Axis = CodeByte(Context, 3);
			if (Axis < 3)
			{
				Context.Actors[static_cast<std::size_t>(Context.Run.Actor)].Rotation[Axis] = static_cast<int16>(ReadValue(Context, 1));
			}
			Advance(Context, 4);
			BuildOwnMatrix(Context);
		}

		/** BD to C2: adds to or takes from one of the actor's angles, wrapping, and builds its matrix. */
		template <std::size_t Axis, int32 Sign>
		void TurnModel(FEventContext& Context)
		{
			int16& Angle = Context.Actors[static_cast<std::size_t>(Context.Run.Actor)].Rotation[Axis];
			Angle = static_cast<int16>(Angle + Sign * ReadValue(Context, 1));
			Advance(Context, 3);
			BuildOwnMatrix(Context);
		}

		/** FA: BD to C2 for any actor: the operation (0 to 5) turns x, y or z forward or back; another turns nothing. */
		void RotateActor(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 2);
			if (Actor != FEventContext::NoActor)
			{
				if (StateOf(Context, Actor) == nullptr)
				{
					return;
				}
				const uint8 Operation = CodeByte(Context, 1);
				if (Operation < 6)
				{
					int16& Angle = Context.Actors[Actor].Rotation[Operation / 2];
					const int32 Turn = ReadValue(Context, 3);
					Angle = static_cast<int16>(Operation % 2 == 0 ? Angle + Turn : Angle - Turn);
				}
				BuildModelMatrix(Context, Actor);
			}
			Advance(Context, 5);
		}

		/** FE 03: scales the actor alike on every axis, and its sprite by three quarters of that. */
		void SetScale(FEventContext& Context)
		{
			const int32 Scale = ReadValue(Context, 1);
			if (FSprite* Sprite = ActorSprite(Context, Context.Run.Actor))
			{
				Sprite->Scale = static_cast<int16>(static_cast<uint32>(Scale * 3) >> 2);
			}
			Self(Context).Scale.fill(static_cast<int16>(Scale));
			BuildOwnMatrix(Context);
			Advance(Context, 3);
		}

		/** FE 08: scales the actor by axis (x, y, z); its sprite goes back to the field's size. */
		void SetScaleXyz(FEventContext& Context)
		{
			const std::array<int32, 3> Scale = {ReadValue(Context, 1), ReadValue(Context, 3), ReadValue(Context, 5)};
			if (FSprite* Sprite = ActorSprite(Context, Context.Run.Actor))
			{
				Sprite->Scale = SpriteScale;
			}
			std::array<int16, 3>& Own = Self(Context).Scale;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Own[Axis] = static_cast<int16>(Scale[Axis]);
			}
			BuildOwnMatrix(Context);
			Advance(Context, 7);
		}

		/** FE 04: the actor's sprite's weight, which scales its animation's moves (0x1000 as made). */
		void SetSpriteWeight(FEventContext& Context)
		{
			const int32 Weight = ReadValue(Context, 1);
			if (FSprite* Sprite = ActorSprite(Context, Context.Run.Actor))
			{
				Sprite->Weight = static_cast<int16>(Weight << 1);
			}
			Advance(Context, 3);
		}

		/** FE 5E: how the actor's sprite blends with what is behind it (0 opaque). */
		void SetSpriteBlend(FEventContext& Context)
		{
			const int32 Mode = ReadValue(Context, 1);
			if (FSprite* Sprite = ActorSprite(Context, Context.Run.Actor))
			{
				if (FSpriteSystem* System = Context.Models->SpriteSystem())
				{
					System->SetBlend(*Sprite, Mode);
				}
			}
			Advance(Context, 3);
		}

		/** An actor's sprite colour (mask bit 1) and its second colour (bit 2), each from the three values from Offset on. */
		void SetTints(FEventContext& Context, FEventActorState& State, uint8 Mask, int32 Offset)
		{
			for (std::size_t Colour = 0; Colour < 2; ++Colour)
			{
				if ((Mask & (1u << Colour)) != 0)
				{
					for (std::size_t Channel = 0; Channel < 3; ++Channel)
					{
						State.Tints[3 * Colour + Channel] = static_cast<uint8>(ReadValue(Context, Offset + 2 * static_cast<int32>(Channel)));
					}
				}
			}
		}

		/** FE 5F: the running actor's tints. */
		void SetTint(FEventContext& Context)
		{
			SetTints(Context, Self(Context), CodeByte(Context, 1), 2);
			Advance(Context, 8);
		}

		/** FE 9A: another actor's tints, none for an empty party slot. */
		void SetActorTint(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 2);
			if (Actor != FEventContext::NoActor)
			{
				FEventActorState* Target = StateOf(Context, Actor);
				if (Target == nullptr)
				{
					return;
				}
				SetTints(Context, *Target, CodeByte(Context, 1), 3);
			}
			Advance(Context, 9);
		}

		/** FE C3: the actor's sprite is no longer drawn (it keeps moving), and not aligned to the floor. */
		void HideOwnSprite(FEventContext& Context)
		{
			Self(Context).Flags1 |= EventActorFlags1::SpriteHidden | EventActorFlags1::NotFloorAligned;
			Advance(Context, 1);
		}

		/** FE C4: FE C3 for any actor. */
		void HideSprite(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				FEventActorState* Target = StateOf(Context, Actor);
				if (Target == nullptr)
				{
					return;
				}
				Target->Flags1 |= EventActorFlags1::SpriteHidden | EventActorFlags1::NotFloorAligned;
			}
			Advance(Context, 2);
		}

		/**
		 * DB: the weight of one key of the actor's model's MIMe animation, at most 0xFFF; nothing when its
		 * model has none. The original does not check the key: one outside the weights is a fault.
		 */
		void SetModelPartValue(FEventContext& Context)
		{
			const int32 Key = ReadValue(Context, 1);
			const int32 Value = std::min(ReadValue(Context, 3), MaxPartValue);
			if ((Context.Actors[static_cast<std::size_t>(Context.Run.Actor)].Flags & FieldActorFlag::VertexAnimation) != 0)
			{
				if (Key < 0 || Key >= FEventActorState::PartValueCount)
				{
					Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Key));
					return;
				}
				Self(Context).PartValues[static_cast<std::size_t>(Key)] = Value;
			}
			Advance(Context, 5);
		}

		/** How a sprite is drawn (the mode's low two bits) and the height of the line its modes split it at. */
		void SetSpriteDrawMode(FEventContext& Context, FEventActorState& State, int32 ModeAt, int32 LineAt, uint8 Flags)
		{
			State.SpriteDraw = (State.SpriteDraw & ~DrawModeMask) | (static_cast<uint32>(ReadOperand(Context, ModeAt, Flags, 0x80)) & 3) << DrawModeShift;
			State.DrawSize = static_cast<int16>(ReadOperand(Context, LineAt, Flags, 0x40));
		}

		/** DD: the actor's sprite drawn whole (0), below a line (1), above it (2) or both apart in their tints (3). */
		void SetDrawMode(FEventContext& Context)
		{
			SetSpriteDrawMode(Context, Self(Context), 1, 3, CodeByte(Context, 5));
			Advance(Context, 6);
		}

		/** E0: DD for any actor. */
		void SetActorDrawMode(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				FEventActorState* Target = StateOf(Context, Actor);
				if (Target == nullptr)
				{
					return;
				}
				SetSpriteDrawMode(Context, *Target, 2, 4, CodeByte(Context, 6));
			}
			Advance(Context, 7);
		}

		/** FE 46: 0: the actor's gear model turns the actor; another value: the actor turns its gear model. */
		void SetModelYawMode(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.Flags1 = CodeByte(Context, 1) == 0 ? State.Flags1 | EventActorFlags1::GearMovesIt : State.Flags1 & ~EventActorFlags1::GearMovesIt;
			Advance(Context, 2);
		}

		/** FE 5B: how far the actor's sprite turns toward its facing each tick (4096 a turn). */
		void SetTurnSpeed(FEventContext& Context)
		{
			Self(Context).TurnSpeed = static_cast<uint16>(ReadValue(Context, 1));
			Advance(Context, 3);
		}

		/** FE 47: how fast the gears' models turn to their direction. */
		void SetGearTurnSpeed(FEventContext& Context)
		{
			Context.GearTurnSpeed = static_cast<uint16>(ReadValue(Context, 1));
			Advance(Context, 3);
		}
	}

	void RegisterModels(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0x58] = &SetModelRotation;
		Base[0xBD] = &TurnModel<0, 1>;
		Base[0xBE] = &TurnModel<0, -1>;
		Base[0xBF] = &TurnModel<1, 1>;
		Base[0xC0] = &TurnModel<1, -1>;
		Base[0xC1] = &TurnModel<2, 1>;
		Base[0xC2] = &TurnModel<2, -1>;
		Base[0xDB] = &SetModelPartValue;
		Base[0xDD] = &SetDrawMode;
		Base[0xE0] = &SetActorDrawMode;
		Base[0xFA] = &RotateActor;
		Extended[0x03] = &SetScale;
		Extended[0x04] = &SetSpriteWeight;
		Extended[0x08] = &SetScaleXyz;
		Extended[0x46] = &SetModelYawMode;
		Extended[0x47] = &SetGearTurnSpeed;
		Extended[0x5B] = &SetTurnSpeed;
		Extended[0x5E] = &SetSpriteBlend;
		Extended[0x5F] = &SetTint;
		Extended[0x9A] = &SetActorTint;
		Extended[0xC3] = &HideOwnSprite;
		Extended[0xC4] = &HideSprite;
	}
}
