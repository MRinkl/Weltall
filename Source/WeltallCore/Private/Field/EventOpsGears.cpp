#include "EventOps.h"

#include "Field/FieldGears.h"
#include "Graphics/GteVectors.h"

#include <algorithm>
#include <string>

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint8 GearOff = 0;
		constexpr uint8 GearFree = 1;
		constexpr uint8 GearLoad = 2;
		/** A swapped gear's first scale, the actor's as a fraction (the header's usual 140h at full scale). */
		constexpr int32 SwappedScaleShift = 6;
		constexpr int32 SwappedScaleFactor = 5;
		constexpr uint32 JointSlotShift = 12;
		constexpr uint8 PointSlotConstant = 0x80;
		constexpr uint8 PointJointConstant = 0x40;
		constexpr uint8 PointXConstant = 0x20;
		constexpr uint8 PointYConstant = 0x10;
		constexpr uint8 PointZConstant = 0x08;

		FGearModels* Module(FEventContext& Context)
		{
			if (!Context.Gears.Models)
			{
				Fail(Context, EEventFault::GearModule, static_cast<uint32>(Context.Run.Actor));
				return nullptr;
			}
			return Context.Gears.Models.get();
		}

		bool HoldsGear(FEventContext& Context, int32 Slot)
		{
			if (Slot < 0 || Slot >= FFieldGears::Capacity)
			{
				Fail(Context, EEventFault::GearArraysFull, static_cast<uint32>(Slot));
				return false;
			}
			return true;
		}

		/** A slot's model hidden; an empty slot's the original clears a byte of low memory for. */
		void HideGear(FEventContext& Context, int32 Slot)
		{
			if (Module(Context) == nullptr)
			{
				return;
			}
			if (FGearModel* Model = FieldGearModel(Context, Slot))
			{
				Model->bShown = 0;
			}
		}

		/** FE 3C: once the init is over, the gear in a slot plays an animation, which the field keeps. */
		void GearModelAnim(FEventContext& Context)
		{
			if (Context.bInitDone)
			{
				const int32 Slot = ReadValue(Context, 1) & 0xFFFF;
				const int32 Animation = ReadValue(Context, 3);
				FGearModels* Models = Module(Context);
				if (Models == nullptr)
				{
					return;
				}
				if (Models->PlayAnimation(Slot, 0, Animation))
				{
					Fail(Context, EEventFault::GearModule, static_cast<uint32>(Context.Run.Actor));
					return;
				}
				if (!HoldsGear(Context, Slot))
				{
					return;
				}
				Context.Gears.Animations[static_cast<std::size_t>(Slot)] = static_cast<int16>(Animation);
			}
			Advance(Context, 5);
		}

		/**
		 * FE 5C: the running gear actor's model hidden (0), freed with the next instruction's model's files
		 * read (1), or loaded from them (2), as the scripts swap a gear with FE 5C 00, FE 5C 01, FE 5C 02 n.
		 * The port reads files at once: it never waits for the disc. Another sub-op ends the turn on it.
		 */
		void GearModel(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.Flags1 &= ~EventActorFlags1::Gear;
			const int32 Slot = FieldGearSlot(State);
			FFieldGears& Gears = Context.Gears;
			Context.Run.bYield = true;
			switch (CodeByte(Context, 1))
			{
			case GearOff:
				HideGear(Context, Slot);
				Advance(Context, 2);
				break;
			case GearFree:
			{
				FGearModels* Models = Module(Context);
				if (Models == nullptr)
				{
					return;
				}
				Models->Free(Slot);
				const int32 Model = ReadValue(Context, 5);
				if (!HoldsGear(Context, Slot))
				{
					return;
				}
				Gears.FileNumbers[static_cast<std::size_t>(Slot)] = static_cast<uint16>(Model * 2);
				auto Files = Gears.Files != nullptr ? Gears.Files->Read(Gears.FileNumbers[static_cast<std::size_t>(Slot)] / 2) : MakeError("no disc to read gears from");
				if (!Files)
				{
					Models->Fail("gear model " + std::to_string(Model) + "'s files cannot be read: " + Files.Error().Message);
					Fail(Context, EEventFault::GearModule, static_cast<uint32>(Context.Run.Actor));
					return;
				}
				Gears.Pending[static_cast<std::size_t>(Slot)] = std::move(*Files);
				Advance(Context, 2);
				break;
			}
			case GearLoad:
			{
				ReadValue(Context, 2);
				FGearModels* Models = Module(Context);
				if (Models == nullptr || !HoldsGear(Context, Slot))
				{
					return;
				}
				std::optional<FGearFiles>& Pending = Gears.Pending[static_cast<std::size_t>(Slot)];
				if (!Pending)
				{
					Models->Fail("gear slot " + std::to_string(Slot) + " is loaded with no files read for it, where the original loads the last ones it freed");
					Fail(Context, EEventFault::GearModule, static_cast<uint32>(Context.Run.Actor));
					return;
				}
				Models->BuildLighting = Gears.DrawnLighting;
				if (Models->Load(Slot, std::move(Pending->Model), std::move(Pending->Motion), FieldGearTexturePlace(Gears, Slot), {0, 0, 0}))
				{
					Fail(Context, EEventFault::GearModule, static_cast<uint32>(Context.Run.Actor));
					return;
				}
				Pending.reset();
				FGearModel* Loaded = Models->Model(Slot);
				Gears.LoadScales[static_cast<std::size_t>(Slot)] = Loaded->Scale;
				Advance(Context, 4);
				State.Flags1 |= EventActorFlags1::Gear;
				Loaded->Scale = static_cast<int16>((State.Scale[0] * SwappedScaleFactor) >> SwappedScaleShift);
				Loaded->FloorY = static_cast<int16>(State.Position[1] >> 16);
				Loaded->Bones[0].Position[0] = static_cast<int16>(State.Position[0] >> 16);
				Loaded->Bones[0].Position[2] = static_cast<int16>(State.Position[2] >> 16);
				break;
			}
			default:
				break;
			}
		}

		/** FE CA: the running gear actor's model hidden (0), or freed and the field's gear count lowered (1). */
		void GearRelease(FEventContext& Context)
		{
			FEventActorState& State = Self(Context);
			State.Flags1 &= ~EventActorFlags1::Gear;
			const int32 Slot = FieldGearSlot(State);
			Context.Run.bYield = true;
			switch (CodeByte(Context, 1))
			{
			case GearOff:
				HideGear(Context, Slot);
				Advance(Context, 2);
				break;
			case GearFree:
				if (FGearModels* Models = Module(Context))
				{
					Models->Free(Slot);
					--Context.Gears.Count;
					Advance(Context, 2);
				}
				break;
			default:
				break;
			}
		}

		/** FE C5: the running actor's model rides a gear's joint: the slot in the top four bits, the joint below. */
		void AttachToGearJoint(FEventContext& Context)
		{
			const int32 Slot = ReadValue(Context, 1);
			const int32 Joint = ReadValue(Context, 3);
			Self(Context).GearJoint = static_cast<uint16>(Slot << JointSlotShift | Joint);
			Advance(Context, 5);
		}

		/**
		 * FE AF: a point in a gear's joint's frame, placed in the world, into three variables (16 bits
		 * each, saturated). A slot without a model, a joint its skeleton does not have, or a map that
		 * loaded no gears places the point by the identity at the origin: the original's matrix there is
		 * its origin with a rotation of stale stack (and without gears it calls into whatever memory holds
		 * the gears' module), so the point is exact where it is the joint's origin.
		 */
		void GetGearJointPoint(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 0xB);
			const int32 Slot = ReadOperand(Context, 1, Flags, PointSlotConstant);
			const int32 Joint = ReadOperand(Context, 3, Flags, PointJointConstant);
			if (Slot < 0 || Slot >= FGearModels::Slots)
			{
				Fail(Context, EEventFault::GearJointUnset, static_cast<uint32>(Slot));
				return;
			}
			FGteMatrix Matrix = FGteMatrix::Identity();
			if (Context.Gears.Models)
			{
				const std::optional<FGteMatrix> Found = FieldGearJoint(Context, Slot, Joint);
				if (Context.Gears.Models->Fault)
				{
					Fail(Context, EEventFault::GearModule, static_cast<uint32>(Context.Run.Actor));
					return;
				}
				if (Found)
				{
					Matrix = *Found;
				}
			}
			const FGteVector Point = {static_cast<int16>(ReadOperand(Context, 5, Flags, PointXConstant)), static_cast<int16>(ReadOperand(Context, 7, Flags, PointYConstant)),
				static_cast<int16>(ReadOperand(Context, 9, Flags, PointZConstant))};
			const FGteLongVector Placed = RotTrans(Matrix, Point);
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				SetVar(Context, ReadU16(Context, 0xC + 2 * Axis), std::clamp(Placed[static_cast<std::size_t>(Axis)], -0x8000, 0x7FFF));
			}
			Advance(Context, 0x12);
		}

		/** The point light a script names; the original's arrays go on past the third into their neighbours. */
		FFieldGearLights::FPointLight* PointLight(FEventContext& Context, int32 Light)
		{
			if (Light < 0 || Light >= FFieldGearLights::PointLights)
			{
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Light));
				return nullptr;
			}
			return &Context.Render.GearLights.Points[static_cast<std::size_t>(Light)];
		}

		/** FE 88: a point light's near and far colours and its radius. */
		void SetPointLightColor(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 0x11);
			const int32 Light = ReadOperand(Context, 1, Flags, 0x80);
			std::array<int32, 7> Values{};
			uint8 Bit = 0x40;
			for (std::size_t Operand = 0; Operand < Values.size(); ++Operand, Bit >>= 1)
			{
				Values[Operand] = ReadOperand(Context, 3 + 2 * static_cast<int32>(Operand), Flags, Bit);
			}
			FFieldGearLights::FPointLight* Point = PointLight(Context, Light);
			if (Point == nullptr)
			{
				return;
			}
			for (std::size_t Channel = 0; Channel < 3; ++Channel)
			{
				Point->Near[Channel] = static_cast<int16>(Values[Channel]);
				Point->Far[Channel] = static_cast<int16>(Values[3 + Channel]);
			}
			Point->Radius = Values[6];
			Advance(Context, 0x12);
		}

		/** FE 89: a point light's place, and the actor it follows (the first party slot's when empty: none). */
		void SetPointLightPosition(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 9);
			const int32 Light = ReadOperand(Context, 1, Flags, 0x80);
			const int32 X = ReadOperand(Context, 3, Flags, 0x40);
			const int32 Z = ReadOperand(Context, 5, Flags, 0x20);
			const int32 Y = ReadOperand(Context, 7, Flags, 0x10);
			const uint32 Actor = ReadActor(Context, 10);
			FFieldGearLights::FPointLight* Point = PointLight(Context, Light);
			if (Point == nullptr)
			{
				return;
			}
			Point->Position = {static_cast<int16>(X), static_cast<int16>(Y), static_cast<int16>(Z)};
			Point->Actor = Actor == FEventContext::NoActor ? FFieldGearLights::FPointLight::Off : static_cast<int16>(Actor);
			Advance(Context, 0xB);
		}

		/** FE CE: the channel pool the map's load makes the gears' module with. */
		void SetGearPoolSize(FEventContext& Context)
		{
			Context.Gears.PoolSize = static_cast<int16>(ReadValue(Context, 1));
			Advance(Context, 3);
		}

		/** FE DC: a gear slot's textures shifted left by columns, from its next load. */
		void SetGearTextureSlot(FEventContext& Context)
		{
			const int32 Slot = ReadValue(Context, 1);
			const int32 Shift = ReadValue(Context, 3);
			if (!HoldsGear(Context, Slot))
			{
				return;
			}
			Context.Gears.TextureShifts[static_cast<std::size_t>(Slot)] = static_cast<uint8>(Shift);
			Advance(Context, 5);
		}
	}

	void RegisterGears(FBaseTable&, FExtendedTable& Extended)
	{
		Extended[0x3C] = &GearModelAnim;
		Extended[0x5C] = &GearModel;
		Extended[0x88] = &SetPointLightColor;
		Extended[0x89] = &SetPointLightPosition;
		Extended[0xAF] = &GetGearJointPoint;
		Extended[0xC5] = &AttachToGearJoint;
		Extended[0xCA] = &GearRelease;
		Extended[0xCE] = &SetGearPoolSize;
		Extended[0xDC] = &SetGearTextureSlot;
	}
}
