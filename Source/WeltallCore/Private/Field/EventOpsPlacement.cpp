#include "EventOps.h"
#include "EventSprites.h"

#include "Field/EventPlacement.h"
#include "Graphics/Sprite.h"

namespace Weltall::EventOps
{
	namespace
	{
		/** Status bit: the actor keeps within its movement bounds. */
		constexpr uint32 MoveBoundsSet = 0x1000;
		constexpr int32 MoveBoundsWords = 8;

		/** Holds the running actor at a height, off the floor's. */
		void HoldAt(FEventContext& Context, int32 Height)
		{
			FEventActorState& State = Self(Context);
			State.Position[1] = static_cast<int32>(static_cast<uint32>(static_cast<uint16>(Height)) << 16);
			State.TargetHeight = static_cast<int16>(Height);
			State.FloorHeight = static_cast<int16>(Height);
			State.Flags0 |= EventActorFlags0::FixedHeight;
		}

		/** 19: moves the actor onto the floor at a ground point. */
		void SetPosition(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 5);
			const int32 X = ReadOperand(Context, 1, Flags, 0x80);
			const int32 Z = ReadOperand(Context, 3, Flags, 0x40);
			if (!PlaceOnFloor(Context, X, Z))
			{
				return;
			}
			FEventActorState& State = Self(Context);
			State.Flags1 &= ~EventActorFlags1::ExplicitPosition;
			State.Flags0 &= ~EventActorFlags0::MovedByScript;
			Advance(Context, 6);
		}

		/** 1A: moves the actor to another walkmesh layer, onto its floor where it stands. */
		void SetWalkmeshLayer(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.Layer = CodeByte(Context, 1);
			if (PlaceOnFloor(Context, static_cast<int16>(State.Position[0] >> 16), static_cast<int16>(State.Position[2] >> 16)))
			{
				Advance(Context, 2);
			}
		}

		/** 1B: moves the actor to a layer and onto its floor at a ground point. */
		void SetPositionLayer(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.Layer = CodeByte(Context, 5);
			const uint8 Flags = CodeByte(Context, 6);
			const int32 X = ReadOperand(Context, 1, Flags, 0x80);
			const int32 Z = ReadOperand(Context, 3, Flags, 0x40);
			if (!PlaceOnFloor(Context, X, Z))
			{
				return;
			}
			State.Flags1 &= ~EventActorFlags1::ExplicitPosition;
			State.Flags0 &= ~EventActorFlags0::MovedByScript;
			Advance(Context, 7);
		}

		void SetHeight(FEventContext& Context)
		{
			HoldAt(Context, ReadOperand(Context, 1, CodeByte(Context, 3), 0x80));
			Advance(Context, 4);
		}

		/** 1D: moves the actor onto the floor at a ground point, then holds it at a height. */
		void SetPosition3D(FEventContext& Context)
		{
			if (!PlaceOnFloor(Context, ReadS16(Context, 1), ReadS16(Context, 3)))
			{
				return;
			}
			HoldAt(Context, ReadS16(Context, 5));
			Advance(Context, 7);
		}

		/** 1E: the actor follows the floor again, from its height now. */
		void FollowFloor(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.TargetHeight = 0;
			State.Flags0 = (State.Flags0 & ~EventActorFlags0::FixedHeight) | EventActorFlags0::FollowsFloor;
			State.FloorHeight = static_cast<int16>(State.Position[1] >> 16);
			Advance(Context, 1);
		}

		/** FE 1C: puts the actor at a point in space, off the walkmesh; its record's position keeps the old. */
		void SetPositionXYZ(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.Flags0 |= EventActorFlags0::MovedByScript;
			State.Flags1 |= EventActorFlags1::ExplicitPosition;
			const uint8 Flags = CodeByte(Context, 7);
			State.Position[0] = static_cast<int32>(static_cast<uint32>(ReadOperand(Context, 1, Flags, 0x80)) << 16);
			State.Position[2] = static_cast<int32>(static_cast<uint32>(ReadOperand(Context, 3, Flags, 0x40)) << 16);
			State.Position[1] = static_cast<int32>(static_cast<uint32>(ReadOperand(Context, 5, Flags, 0x20)) << 16);
			FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Context.Run.Actor)];
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Record.Translation[Axis] = static_cast<int16>(State.Position[Axis] >> 16);
			}
			if (FSprite* Sprite = ActorSprite(Context, Context.Run.Actor))
			{
				Sprite->Position = State.Position;
			}
			Advance(Context, 8);
		}

		/**
		 * 17: the actor keeps within a quad (four corners' x and z, each by its bit of the flags at 17,
		 * from 80h down): the actors' movement refuses its moves out of it.
		 */
		void SetMoveBounds(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.Status |= MoveBoundsSet;
			const uint8 Flags = CodeByte(Context, 17);
			for (int32 Word = 0; Word < MoveBoundsWords; ++Word)
			{
				State.MoveBounds[static_cast<std::size_t>(Word)] = static_cast<int16>(ReadOperand(Context, 1 + 2 * Word, Flags, static_cast<uint8>(0x80 >> Word)));
			}
			Advance(Context, 18);
		}

		/** FE 16: the actor moves out of its bounds freely again. */
		void FreeMoveBounds(FEventContext& Context)
		{
			Self(Context).Status &= ~MoveBoundsSet;
			Advance(Context, 1);
		}
	}

	void RegisterPlacement(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0x17] = &SetMoveBounds;
		Extended[0x16] = &FreeMoveBounds;
		Base[0x19] = &SetPosition;
		Base[0x1A] = &SetWalkmeshLayer;
		Base[0x1B] = &SetPositionLayer;
		Base[0x1C] = &SetHeight;
		Base[0x1D] = &SetPosition3D;
		Base[0x1E] = &FollowFloor;
		Extended[0x1C] = &SetPositionXYZ;
	}
}
