#include "EventOps.h"

#include <algorithm>

namespace Weltall::EventOps
{
	namespace
	{
		/** Band counts from here on are negative to the original, which then asks its heap for a negative size. */
		constexpr uint16 FirstNegativeBandCount = 0x8000;

		/**
		 * DA: makes a VRAM scroller, unless the map has its 32: a rectangle, its bands, where it is
		 * copied to, and every band's speed (the low byte, signed).
		 */
		void CreateLineScroll(FEventContext& Context)
		{
			FFieldRenderState& Render = Context.Render;
			if (Render.LineScrolls.size() < FFieldRenderState::MaxLineScrolls)
			{
				const uint16 Bands = ReadU16(Context, 9);
				const uint16 Width = ReadU16(Context, 5);
				// No bands divides by zero as the scroller is made; no width, every tick as it is copied.
				if (Bands == 0 || Bands >= FirstNegativeBandCount || Width == 0)
				{
					Fail(Context, EEventFault::UnusableLineScroll, static_cast<uint32>(Bands) << 16 | Width);
					return;
				}
				const auto Operand = [&Context](int32 Offset) { return static_cast<int16>(ReadU16(Context, Offset)); };
				Render.LineScrolls.push_back(FFieldLineScroll::Make(Operand(1), Operand(3), Width, Operand(7), Bands, Operand(11), Operand(13),
					static_cast<int8>(ReadU16(Context, 15))));
				std::size_t Moves = 0;
				for (const FFieldLineScroll& Scroll : Render.LineScrolls)
				{
					Moves += 2 * Scroll.Offsets.size();
				}
				Render.LineScrollMoves.reserve(Moves);
			}
			Advance(Context, 17);
		}

		/**
		 * FE 40: one band's speed (the low byte, signed). A band past the scroller's changes nothing,
		 * and so does a scroller the map has not made, as one never made holds no bands.
		 */
		void SetLineScrollSpeed(FEventContext& Context)
		{
			const int32 Scroller = ReadValue(Context, 1);
			const int32 Band = ReadValue(Context, 3);
			const int8 Speed = static_cast<int8>(ReadU16(Context, 5));
			std::vector<FFieldLineScroll>& Scrolls = Context.Render.LineScrolls;
			if (Scroller < 0 || Scroller >= static_cast<int32>(FFieldRenderState::MaxLineScrolls))
			{
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Scroller));
				return;
			}
			const std::size_t Index = static_cast<std::size_t>(Scroller);
			const int32 Bands = Index < Scrolls.size() ? static_cast<int32>(Scrolls[Index].Speeds.size()) : 0;
			if (Band < 0)
			{
				// Before the speeds, or near address 0 for a scroller never made.
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Band));
				return;
			}
			if (Band < Bands)
			{
				Scrolls[Index].Speeds[static_cast<std::size_t>(Band)] = Speed;
			}
			Advance(Context, 7);
		}

		/** Whether the actor's model holds flat-textured polygons alone, whose packets the scroll's strides fit. */
		bool HasFlatTexturedModel(const FEventContext& Context, const FFieldActor& Record)
		{
			if ((Record.Flags & FieldActorFlag::NoModel) != 0 || Record.Model >= Context.MapModels.size() || Context.MapModels[Record.Model].Parts.empty())
			{
				return false;
			}
			const std::vector<FMdlPrimitive>& Primitives = Context.MapModels[Record.Model].Parts.front().Primitives;
			return std::all_of(Primitives.begin(), Primitives.end(), [](const FMdlPrimitive& Primitive)
				{
					const FMdlPrimitiveTraits& Traits = Primitive.Traits();
					return Traits.bTextured && !Traits.bGouraud && Primitive.Type != EMdlPrimitiveType::EnvironmentMappedTriangle;
				});
		}

		/**
		 * FE 1B: moves the running actor's model's texture coordinates by the low bytes of its two
		 * operands, modulo 256. The original adds them to the model's packets taking every polygon as
		 * flat-textured: a model with other kinds, or an actor without one, is refused.
		 */
		void ScrollModelUv(FEventContext& Context)
		{
			const int32 Actor = Context.Run.Actor;
			if (!HasFlatTexturedModel(Context, Context.Actors[static_cast<std::size_t>(Actor)]))
			{
				Fail(Context, EEventFault::ModelUvScroll, static_cast<uint32>(Actor));
				return;
			}
			const FMdlUv By{static_cast<uint8>(ReadU16(Context, 1)), static_cast<uint8>(ReadU16(Context, 3))};
			Context.Render.ScrollModelUv(static_cast<uint16>(Actor), By);
			Advance(Context, 5);
		}

		/** FE 26: switches the screen wave on and moves its six parameters to new values over a number of ticks. */
		void StartScreenWave(FEventContext& Context)
		{
			FFieldScreenWave& Wave = Context.Render.ScreenWave;
			Wave.bOn = true;
			std::array<int32, FFieldScreenWave::ParameterCount> Targets{};
			for (std::size_t Parameter = 0; Parameter < Targets.size(); ++Parameter)
			{
				Targets[Parameter] = ReadValue(Context, 1 + 2 * static_cast<int32>(Parameter));
			}
			Wave.MoveTo(Targets, ReadValue(Context, 13));
			Wave.bOffWhenDone = false;
			Advance(Context, 15);
		}

		/**
		 * FE 27: 0 fades every parameter to nothing over a number of ticks, the wave switching off at
		 * the end; 1 waits until the wave is off; 2 and 3 switch it off at once. Each ends the actor's
		 * turn. Another operation only ends it, leaving the pc on the instruction's second byte, which
		 * then runs as an instruction of its own.
		 */
		void ControlScreenWave(FEventContext& Context)
		{
			FFieldScreenWave& Wave = Context.Render.ScreenWave;
			Context.Run.bYield = true;
			switch (CodeByte(Context, 1))
			{
			case 0:
				Wave.MoveTo({}, ReadValue(Context, 2));
				Wave.bOffWhenDone = true;
				Advance(Context, 4);
				break;
			case 1:
				Advance(Context, Wave.bOn ? -1 : 2);
				break;
			case 2:
			case 3:
				Wave.bOn = false;
				Advance(Context, 2);
				break;
			default:
				break;
			}
		}
	}

	void RegisterScrolls(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0xDA] = &CreateLineScroll;
		Extended[0x1B] = &ScrollModelUv;
		Extended[0x26] = &StartScreenWave;
		Extended[0x27] = &ControlScreenWave;
		Extended[0x40] = &SetLineScrollSpeed;
	}
}
