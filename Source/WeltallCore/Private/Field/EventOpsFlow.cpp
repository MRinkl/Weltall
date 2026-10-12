#include "EventOps.h"

#include "Field/FieldMap.h"
#include "Field/FieldWalkmesh.h"
#include "Graphics/Gte.h"

#include <optional>

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint32 AccumulatorAddress = 0x000;

		/** 00: the thread ends; the actor's other threads and its call depth stay as they are. */
		void End(FEventContext& Context)
		{
			EndThread(Context);
			Context.Run.bUnlimited = false;
			Context.Run.bYield = true;
		}

		void Jump(FEventContext& Context)
		{
			Self(Context).Pc = ReadU16(Context, 1);
		}

		/**
		 * 02: compares two operands as 32-bit signed numbers. A constant compared with a variable is
		 * widened by the variable's signedness, and so is a second variable, by the first one's; any
		 * other high nibble in the flags byte compares 0 with 0.
		 */
		void If(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 5);
			int32 A = 0;
			int32 B = 0;
			switch (Flags & 0xF0)
			{
			case 0x00:
				A = GetVar(Context, ReadU16(Context, 1));
				B = GetVar(Context, ReadU16(Context, 3));
				B = IsVarUnsigned(Context, ReadU16(Context, 1)) ? (B & 0xFFFF) : static_cast<int16>(B);
				break;
			case 0x40:
				A = GetVar(Context, ReadU16(Context, 1));
				B = ReadS16(Context, 3);
				if (IsVarUnsigned(Context, ReadU16(Context, 1)))
				{
					B &= 0xFFFF;
				}
				break;
			case 0x80:
				A = ReadS16(Context, 1);
				B = GetVar(Context, ReadU16(Context, 3));
				if (IsVarUnsigned(Context, ReadU16(Context, 3)))
				{
					A &= 0xFFFF;
				}
				break;
			case 0xC0:
				A = ReadS16(Context, 1);
				B = ReadS16(Context, 3);
				break;
			default:
				break;
			}
			bool bHolds = false;
			switch (Flags & 0x0F)
			{
			case 0:
				bHolds = A == B;
				break;
			case 1:
			case 7:
				bHolds = A != B;
				break;
			case 2:
				bHolds = A > B;
				break;
			case 3:
				bHolds = A < B;
				break;
			case 4:
				bHolds = A >= B;
				break;
			case 5:
				bHolds = A <= B;
				break;
			case 6:
			case 9:
				bHolds = (A & B) != 0;
				break;
			case 8:
				bHolds = (A | B) != 0;
				break;
			case 10:
				bHolds = (~A & B) != 0;
				break;
			default:
				break;
			}
			Branch(Context, bHolds, 8, 6);
		}

		template <int32 Length>
		void CallRoutine(FEventContext& Context)
		{
			if (!Call(Context, Length, 1))
			{
				// A full stack: the original prints an error and the call waits for ever.
				Context.Run.bYield = true;
			}
		}

		/** 0D: returns from a call; with no call to return from, the thread ends. */
		void Return(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			const uint32 Depth = State.CallDepth();
			if (Depth == 0)
			{
				End(Context);
				return;
			}
			State.SetCallDepth(Depth - 1);
			State.Pc = State.CallStack[Depth - 1];
		}

		/** 26: waits the operand's low byte of ticks (0 none), counting on the thread's wait counter. */
		void Wait(FEventContext& Context)
		{
			FEventThread& Thread = Self(Context).Running();
			if (Thread.WaitCount == 0)
			{
				Thread.WaitCount = static_cast<uint8>(ReadValue(Context, 1));
			}
			else
			{
				--Thread.WaitCount;
			}
			if (Thread.WaitCount == 0)
			{
				Advance(Context, 3);
			}
			Context.Run.bYield = true;
		}

		/** A6: jumps into the table of 3-byte instructions that follows, by the operand's index. */
		void JumpTable(FEventContext& Context)
		{
			const int32 Index = static_cast<int16>(ReadValue(Context, 1));
			Advance(Context, Index * 3 + 3);
		}

		void Yield(FEventContext& Context)
		{
			Context.Run.bYield = true;
			Advance(Context, 1);
		}

		void ExtendBudget(FEventContext& Context)
		{
			Context.Run.Budget += 0x20;
			Advance(Context, 1);
		}

		void IfAccumulatorLess(FEventContext& Context)
		{
			const int32 Value = ReadValue(Context, 1);
			Branch(Context, GetVar(Context, AccumulatorAddress) < Value, 5, 3);
		}

		void IfAccumulatorGreater(FEventContext& Context)
		{
			const int32 Value = ReadValue(Context, 1);
			Branch(Context, Value < GetVar(Context, AccumulatorAddress), 5, 3);
		}

		void IfAccumulatorEqual(FEventContext& Context)
		{
			const int32 Value = ReadValue(Context, 1);
			Branch(Context, GetVar(Context, AccumulatorAddress) == Value, 5, 3);
		}

		void IfButtonHeld(FEventContext& Context)
		{
			Branch(Context, (ReadU16(Context, 1) & ScriptHeldButtons(Context)) != 0, 5, 3);
		}

		void IfButtonLatched(FEventContext& Context)
		{
			Branch(Context, (ReadU16(Context, 1) & Context.PadLatched) != 0, 5, 3);
		}

		void IfButtonsEqual(FEventContext& Context)
		{
			Branch(Context, ReadU16(Context, 1) == Context.PadHeld, 5, 3);
		}

		void IfLatchedButtonsEqual(FEventContext& Context)
		{
			Branch(Context, ReadU16(Context, 1) == Context.PadLatched, 5, 3);
		}

		void ClearButtonLatch(FEventContext& Context)
		{
			Context.PadLatched = 0;
			Advance(Context, 1);
		}

		/**
		 * A ground point as the GTE's screen registers take it: z in the high half, x in the low. The
		 * halves are added as words, so a negative x borrows 1 from z.
		 */
		std::array<int16, 2> PackGroundPoint(int32 X, int32 Z)
		{
			const uint32 Packed = (static_cast<uint32>(Z) << 16) + static_cast<uint32>(X);
			return {static_cast<int16>(Packed), static_cast<int16>(Packed >> 16)};
		}

		/**
		 * Whether the leader stands in the operand's zone, seen from above (and, WithHeight, with the
		 * zone's first corner between its feet and its head); nullopt after a fault.
		 */
		std::optional<bool> LeaderInZone(FEventContext& Context, bool bWithHeight)
		{
			const FEventActorState* Leader = StateOf(Context, Context.LeaderActor);
			const uint8 ZoneIndex = CodeByte(Context, 1);
			if (Leader == nullptr)
			{
				return std::nullopt;
			}
			if (ZoneIndex >= Context.Zones.size())
			{
				Fail(Context, EEventFault::ZoneOutOfRange, ZoneIndex);
				return std::nullopt;
			}
			const FFieldZone& Zone = Context.Zones[ZoneIndex];
			if (bWithHeight)
			{
				const int32 Feet = static_cast<int16>(Leader->Position[1] >> 16);
				const int32 CornerHeight = Zone.Corners[0][1];
				if (!(CornerHeight < Feet && Feet - int32{Leader->Height} < CornerHeight))
				{
					return false;
				}
			}
			const std::array<int16, 2> Point = PackGroundPoint(static_cast<int16>(Leader->Position[0] >> 16), static_cast<int16>(Leader->Position[2] >> 16));
			std::array<std::array<int16, 2>, 4> Corners{};
			for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
			{
				Corners[Corner] = PackGroundPoint(Zone.Corners[Corner][0], Zone.Corners[Corner][2]);
			}
			for (std::size_t Edge = 0; Edge < Corners.size(); ++Edge)
			{
				const std::array<int16, 2>& From = Corners[Edge];
				const std::array<int16, 2>& To = Corners[(Edge + 1) % Corners.size()];
				if (Nclip(From[0], From[1], To[0], To[1], Point[0], Point[1]) < 0)
				{
					return false;
				}
			}
			return true;
		}

		/** 0A and CC: call the routine when the leader is in the zone and the call stack has room. */
		template <bool bWithHeight>
		void CallIfLeaderInZone(FEventContext& Context)
		{
			const std::optional<bool> bInside = LeaderInZone(Context, bWithHeight);
			if (!bInside)
			{
				return;
			}
			if (!*bInside || !Call(Context, 4, 2))
			{
				++Context.Run.Budget;
				Advance(Context, 4);
			}
		}

		/** C9 and CB: go on when the leader is in the zone, else jump; the jump costs no budget. */
		template <bool bWithHeight>
		void IfLeaderInZone(FEventContext& Context)
		{
			const std::optional<bool> bInside = LeaderInZone(Context, bWithHeight);
			if (!bInside)
			{
				return;
			}
			if (!*bInside)
			{
				++Context.Run.Budget;
			}
			Branch(Context, *bInside, 4, 2);
		}

		/**
		 * FE 00 and FE 78-7E: do nothing and leave the pc on their second byte, which then runs as a base
		 * opcode (FE 00 as End). D1 and E4 do nothing at all: the thread runs them again and again.
		 */
		void Nothing(FEventContext&)
		{
		}

		template <int32 Length>
		void Skip(FEventContext& Context)
		{
			Advance(Context, Length);
		}

		/** FE 05: goes on when the actor stands on the operand's walkmesh layer, else jumps. */
		void IfActorOnLayer(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				const FEventActorState* Target = StateOf(Context, Actor);
				if (Target == nullptr)
				{
					return;
				}
				if (ReadValue(Context, 2) == static_cast<int16>(Target->Layer))
				{
					Advance(Context, 6);
					return;
				}
			}
			Self(Context).Pc = ReadU16(Context, 4);
		}

		/** FE 06: goes on when the attribute byte of the actor's floor triangle is the operand, else jumps. */
		void IfActorFloorAttribute(FEventContext& Context)
		{
			const uint32 Actor = ReadActor(Context, 1);
			if (Actor != FEventContext::NoActor)
			{
				const FEventActorState* Target = StateOf(Context, Actor);
				if (Target == nullptr)
				{
					return;
				}
				const int32 Layer = static_cast<int16>(Target->Layer);
				if (Context.Walkmesh == nullptr || Layer < 0 || static_cast<std::size_t>(Layer) >= Target->FloorTriangles.size()
					|| static_cast<std::size_t>(Layer) >= Context.Walkmesh->Layers.size())
				{
					Fail(Context, EEventFault::NoFloorTriangle, 0xFFFF);
					return;
				}
				const int32 Triangle = Target->FloorTriangles[static_cast<std::size_t>(Layer)];
				const std::vector<FWalkmeshTriangle>& Triangles = Context.Walkmesh->Layers[static_cast<std::size_t>(Layer)].Triangles;
				if (Triangle < 0 || static_cast<std::size_t>(Triangle) >= Triangles.size())
				{
					Fail(Context, EEventFault::NoFloorTriangle, static_cast<uint16>(Triangle));
					return;
				}
				const uint8 Attribute = Triangles[static_cast<std::size_t>(Triangle)].Attribute;
				if (static_cast<uint32>(ReadValue(Context, 2)) == Attribute)
				{
					Advance(Context, 6);
					return;
				}
			}
			Self(Context).Pc = ReadU16(Context, 4);
		}
	}

	bool Call(FEventContext& Context, int32 Length, int32 TargetOffset)
	{
		FEventActorState& State = Self(Context);
		const uint32 Depth = State.CallDepth();
		if (Depth >= FEventActorState::CallStackSize)
		{
			return false;
		}
		State.CallStack[Depth] = static_cast<uint16>(State.Pc + Length);
		State.Pc = ReadU16(Context, TargetOffset);
		State.SetCallDepth(Depth + 1);
		return true;
	}

	void RegisterFlow(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0x00] = &End;
		Base[0x01] = &Jump;
		Base[0x02] = &If;
		Base[0x05] = &CallRoutine<3>;
		Base[0x06] = &CallRoutine<5>;
		Base[0x0A] = &CallIfLeaderInZone<false>;
		Base[0x0D] = &Return;
		Base[0x26] = &Wait;
		Base[0x31] = &IfButtonHeld;
		Base[0x32] = &IfButtonLatched;
		Base[0x33] = &ClearButtonLatch;
		Base[0x84] = &IfAccumulatorLess;
		Base[0x85] = &IfAccumulatorGreater;
		Base[0x86] = &IfAccumulatorEqual;
		Base[0xA6] = &JumpTable;
		Base[0xC3] = &Yield;
		Base[0xC6] = &ExtendBudget;
		Base[0xC9] = &IfLeaderInZone<false>;
		Base[0xCB] = &IfLeaderInZone<true>;
		Base[0xCC] = &CallIfLeaderInZone<true>;
		Base[0xE2] = &IfButtonsEqual;
		Base[0xE3] = &IfLatchedButtonsEqual;
		Base[0x0E] = &Skip<1>;
		Base[0x0F] = &Skip<1>;
		Base[0x13] = &Skip<1>;
		Base[0xD1] = &Nothing;
		Base[0xE4] = &Nothing;
		Base[0xFD] = &Skip<1>;
		Base[0xFF] = &Skip<1>;
		Extended[0x00] = &Nothing;
		// FE 74 prints a variable in development builds; FE D2 skips a word.
		Extended[0x74] = &Skip<3>;
		Extended[0xD2] = &Skip<3>;
		for (uint8 Opcode = 0x78; Opcode <= 0x7E; ++Opcode)
		{
			Extended[Opcode] = &Nothing;
		}
		Extended[0x05] = &IfActorOnLayer;
		Extended[0x06] = &IfActorFloorAttribute;
	}
}
