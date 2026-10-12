#include "EventMoves.h"
#include "MovementRules.h"

#include "Field/FieldCameraFloor.h"
#include "Field/FieldWalkmesh.h"
#include "Graphics/GteVectors.h"
#include "Graphics/Sprite.h"

// 57: the running actor jumps to a point, in a parabola its sprite's gravity bends, the script moving it
// every tick until it lands.
namespace Weltall::EventOps
{
	namespace
	{
		using namespace Moves;
		using MovementRules::Add;
		using MovementRules::Multiply;
		using MovementRules::WholePart;
		using MovementRules::Wrap;

		constexpr int32 SetupLength = 11;
		/** The setup and the flight after it. */
		constexpr int32 JumpLength = SetupLength + 2;
		constexpr uint8 ModeMask = 0x3;
		/** The duration given in ticks, a horizontal speed, or the height to rise above the start. */
		constexpr uint8 ByTicks = 0;
		constexpr uint8 BySpeed = 1;
		constexpr uint8 ByHeight = 2;
		constexpr uint8 Flight = 3;
		/** The y operand names a walkmesh layer, whose floor under the point is the landing's height. */
		constexpr uint8 LandsOnLayer = 0x80;
		/** The actor takes the triangles under it on the walking layers once it has landed. */
		constexpr uint8 SnapToFloor = 0x0F;

		/** The distance between a 16.16 coordinate and a whole one, in whole units, as the processor computes it. */
		int32 WholeDelta(int32 Target, int32 Position)
		{
			return Wrap(int64{static_cast<int32>(static_cast<uint32>(Target) << 16)} - Position) >> 16;
		}

		/** The same difference kept 16.16. */
		int32 FixedDelta(int32 Target, int32 Position)
		{
			return Wrap(int64{static_cast<int32>(static_cast<uint32>(Target) << 16)} - Position);
		}

		/** A walkmesh layer's floor under a point: its triangle (0 without one) and height. */
		std::optional<FFieldFloor> FloorOn(FEventContext& Context, int32 Layer, int32 X, int32 Z)
		{
			if (Context.Walkmesh == nullptr || Layer < 0 || static_cast<std::size_t>(Layer) >= Context.Walkmesh->Layers.size())
			{
				Fail(Context, EEventFault::NoFloorTriangle, static_cast<uint16>(Layer));
				return std::nullopt;
			}
			return FindFieldFloor(Context.Walkmesh->Layers[static_cast<std::size_t>(Layer)], X, Z);
		}

		/** The landing's height: the y operand, or the floor under the point of the layer it names. */
		std::optional<int32> LandingHeight(FEventContext& Context, uint8 Mode, uint8 Flags, int32 X, int32 Z, bool bKeepTriangle)
		{
			const int32 Y = ReadOperand(Context, 6, Flags, 0x20);
			if ((Mode & LandsOnLayer) == 0)
			{
				return Y;
			}
			const std::optional<FFieldFloor> Floor = FloorOn(Context, Y, X, Z);
			if (!Floor)
			{
				return std::nullopt;
			}
			FEventActorState& State = Self(Context);
			if (bKeepTriangle)
			{
				State.FloorTriangles[static_cast<std::size_t>(Y)] = static_cast<int16>(Floor->Triangle);
			}
			else
			{
				State.Layer = static_cast<uint16>(Y);
			}
			return Floor->Height;
		}

		/**
		 * The jump's length in ticks as its mode gives it: at once, from the ground distance over a speed,
		 * or from the root of the height it rises (by the y operand read as a height even when it names a
		 * layer); never 0.
		 */
		std::optional<int32> JumpTicks(FEventContext& Context, uint8 Mode, uint8 Flags)
		{
			const FEventActorState& State = Self(Context);
			int32 Ticks = 0;
			switch (Mode & ModeMask)
			{
				case ByTicks:
					Ticks = ReadOperand(Context, 8, Flags, 0x10);
					break;
				case BySpeed:
				{
					const int32 DeltaX = WholeDelta(ReadOperand(Context, 2, Flags, 0x80), State.Position[0]);
					const int32 DeltaZ = WholeDelta(ReadOperand(Context, 4, Flags, 0x40), State.Position[2]);
					const int32 Speed = ReadOperand(Context, 8, Flags, 0x10);
					int32 Distance = 0;
					if (!Length(Context, DeltaX, DeltaZ, 0, Distance))
					{
						return std::nullopt;
					}
					Ticks = ProcessorDivide(Distance, Speed);
					break;
				}
				case ByHeight:
				{
					ReadOperand(Context, 2, Flags, 0x80);
					ReadOperand(Context, 4, Flags, 0x40);
					const int32 Y = ReadOperand(Context, 6, Flags, 0x20);
					const int32 Height = ReadOperand(Context, 8, Flags, 0x10);
					const int32 Rise = Wrap(int64{Wrap(-int64{Height})} - WholeDelta(Y, State.Position[1]));
					const int32 Size = Rise >= 0 ? Rise : Wrap(-int64{Rise});
					if (Size < 0)
					{
						Fail(Context, EEventFault::SquareRootOutsideTable, static_cast<uint32>(Size));
						return std::nullopt;
					}
					Ticks = SquareRoot0(Size);
					break;
				}
			}
			return Ticks == 0 ? 1 : Ticks;
		}

		/**
		 * The setup: the jump's ticks, the sprite's vertical speed for a parabola that lands on the
		 * target's height at its gravity, and the horizontal step, which covers the distance in one tick
		 * more than the jump lasts (the landing puts the actor on the point).
		 */
		void StartJump(FEventContext& Context, FSprite& Sprite, uint8 Mode)
		{
			const uint8 Flags = CodeByte(Context, 10);
			const std::optional<int32> Ticks = JumpTicks(Context, Mode, Flags);
			if (!Ticks)
			{
				return;
			}
			const int32 X = ReadOperand(Context, 2, Flags, 0x80);
			const int32 Z = ReadOperand(Context, 4, Flags, 0x40);
			const std::optional<int32> Y = LandingHeight(Context, Mode, Flags, X, Z, false);
			if (!Y)
			{
				return;
			}
			FEventActorState& State = Self(Context);
			const int32 Half = Multiply(Sprite.Gravity, *Ticks) / 2;
			Sprite.Velocity[1] = Add(-int64{Half}, ProcessorDivide(FixedDelta(*Y, State.Position[1]), *Ticks));
			const int32 Steps = Wrap(int64{*Ticks} + 1);
			State.MoveWork = {ProcessorDivide(FixedDelta(X, State.Position[0]), Steps), 0, ProcessorDivide(FixedDelta(Z, State.Position[2]), Steps)};
			State.JumpTicks = static_cast<int16>(*Ticks);
			State.Counter = 0;
			Advance(Context, SetupLength);
		}

		/** One tick in the air, or the landing on the target (read from the setup before the instruction). */
		void Fly(FEventContext& Context, FSprite& Sprite)
		{
			FEventActorState& State = Self(Context);
			if (State.Counter < State.JumpTicks)
			{
				State.Position[0] = Add(State.Position[0], State.MoveWork[0]);
				State.Position[2] = Add(State.Position[2], State.MoveWork[2]);
				State.Position[1] = Add(State.Position[1], Sprite.Velocity[1]);
				Sprite.Velocity[1] = Add(Sprite.Velocity[1], Sprite.Gravity);
				if ((State.MoveWork[0] != 0 || State.MoveWork[2] != 0) && (State.Flags0 & EventActorFlags0::FacingLocked) == 0)
				{
					const uint16 Heading = DirectionOf(Context, State.MoveWork[0], State.MoveWork[2]) | DirectionApplied;
					State.MoveDirection = Heading;
					State.Facing = Heading;
				}
				return;
			}
			if (State.Pc < SetupLength)
			{
				Fail(Context, EEventFault::ReadOutsideCode, State.Pc);
				return;
			}
			Advance(Context, -SetupLength);
			const uint8 Mode = CodeByte(Context, 1);
			const uint8 Flags = CodeByte(Context, 10);
			const int32 X = ReadOperand(Context, 2, Flags, 0x80);
			const int32 Z = ReadOperand(Context, 4, Flags, 0x40);
			const std::optional<int32> Y = LandingHeight(Context, Mode, Flags, X, Z, true);
			if (!Y)
			{
				return;
			}
			Sprite.Velocity[1] = 0;
			State.Position[0] = static_cast<int32>(static_cast<uint32>(X) << 16);
			State.Position[1] = static_cast<int32>(static_cast<uint32>(*Y) << 16);
			State.Position[2] = static_cast<int32>(static_cast<uint32>(Z) << 16);
			State.Flags0 &= ~EventActorFlags0::MovedByScript;
			Advance(Context, JumpLength);
			State.Flags1 &= ~EventActorFlags1::ExplicitPosition;
		}

		/** The flight's and the landing's end: the record and the sprite follow the actor, and the tick is counted. */
		void FollowJump(FEventContext& Context, FSprite& Sprite)
		{
			FEventActorState& State = Self(Context);
			FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Context.Run.Actor)];
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Record.Translation[Axis] = WholePart(State.Position[Axis]);
			}
			Sprite.Position = State.Position;
			State.Counter = static_cast<int16>(State.Counter + 1);
		}

		/** The actor takes the floor triangles under it on the walking layers, out of the script's hands. */
		void SnapTriangles(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			const int32 WalkingLayers = Context.Walkmesh != nullptr ? static_cast<int32>(Context.Walkmesh->Layers.size()) - 1 : 0;
			for (int32 Layer = 0; Layer < WalkingLayers && Layer < static_cast<int32>(State.FloorTriangles.size()); ++Layer)
			{
				const FFieldFloor Floor = FindFieldFloor(Context.Walkmesh->Layers[static_cast<std::size_t>(Layer)], WholePart(State.Position[0]), WholePart(State.Position[2]));
				State.FloorTriangles[static_cast<std::size_t>(Layer)] = static_cast<int16>(Floor.Triangle);
			}
			State.Flags0 &= ~EventActorFlags0::MovedByScript;
			State.Flags1 &= ~EventActorFlags1::ExplicitPosition;
			Advance(Context, 2);
		}

		/**
		 * 57: the setup (its mode in the low bits of operand 1: the jump's ticks, a horizontal speed or a
		 * height; bit 7: the y operand names the layer to land on), the flight that must follow it (mode
		 * 3), or the triangles taken after a landing (0F). The script moves the actor meanwhile; the
		 * actor's turn ends every time.
		 */
		void JumpTo(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.Flags0 |= EventActorFlags0::MovedByScript;
			Context.Run.bYield = true;
			const uint8 Mode = CodeByte(Context, 1);
			if (Mode == SnapToFloor)
			{
				SnapTriangles(Context);
				return;
			}
			FSprite* Sprite = MovedSprite(Context);
			if (Sprite == nullptr)
			{
				return;
			}
			if ((Mode & ModeMask) != Flight)
			{
				StartJump(Context, *Sprite, Mode);
				return;
			}
			Fly(Context, *Sprite);
			if (Context.Run.Fault == EEventFault::None)
			{
				FollowJump(Context, *Sprite);
			}
		}
	}

	void RegisterJumps(FBaseTable& Base, FExtendedTable&)
	{
		Base[0x57] = &JumpTo;
	}
}
