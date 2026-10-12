#include "EventOps.h"

namespace Weltall::EventOps
{
	namespace
	{
		/**
		 * The emitters past the setup's eight that a script sets: the original writes them over the field's faces'
		 * cache and the compass's first quads while both are unused, and runs only eight. Their instructions set nothing.
		 */
		constexpr int32 StrayEmitterEnd = 11;

		/**
		 * The current emitter in Emitter, null for a stray one; false after a fault, for an index past the strays,
		 * where the original writes wherever the index takes it.
		 */
		bool CurrentEmitter(FEventContext& Context, FFieldParticleEmitter*& Emitter)
		{
			const int32 Index = Context.Particles.Current;
			Emitter = nullptr;
			if (Index >= FFieldParticles::EmitterCount && Index < StrayEmitterEnd)
			{
				return true;
			}
			if (Index < 0 || Index >= FFieldParticles::EmitterCount)
			{
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Index));
				return false;
			}
			Emitter = &Context.Particles.Setup[static_cast<std::size_t>(Index)];
			return true;
		}

		/** The space an effect's points are in, as FE 8F's value names it: 0 to 3 become flag bits, others stay as they are. */
		int32 SpaceBits(int32 Space)
		{
			return Space >= 0 && Space <= 3 ? Space << FieldParticleFlags::SpaceShift : Space;
		}

		/** FE 8F: a new setup of the emitters, placed on an actor (an empty party slot: actor 0) in a space. */
		void BeginParticles(FEventContext& Context)
		{
			uint32 Actor = ReadActor(Context, 1);
			if (Actor == FEventContext::NoActor)
			{
				Actor = 0;
			}
			FFieldParticles& Particles = Context.Particles;
			Particles.Owner = static_cast<int32>(Actor);
			const int32 Space = ReadValue(Context, 2);
			Particles.GearSlot = ReadValue(Context, 4);
			Particles.GearJoint = ReadValue(Context, 6);
			Advance(Context, 8);
			Particles.ResetSetup(static_cast<int16>(Actor));
			Particles.SpaceBits = SpaceBits(Space);
			Context.Run.Budget += 4;
		}

		/** FE C2: FE 8F with the actor a value; the setup is placed on actor 0 for FFh, though the emitters then take FFh. */
		void BeginParticlesOn(FEventContext& Context)
		{
			int32 Actor = ReadValue(Context, 1);
			if (Actor == FEventContext::NoActor)
			{
				Actor = 0;
			}
			FFieldParticles& Particles = Context.Particles;
			Particles.Owner = ReadValue(Context, 1);
			const int32 Space = ReadValue(Context, 3);
			Particles.GearSlot = ReadValue(Context, 5);
			Particles.GearJoint = ReadValue(Context, 7);
			Advance(Context, 9);
			Particles.ResetSetup(static_cast<int16>(Actor));
			Particles.SpaceBits = SpaceBits(Space);
			Context.Run.Budget += 4;
		}

		/** FE 90: chooses the emitter the next instructions set, placed on the setup's actor: its particles, delay and ticks of emitting. */
		void SetEmitter(FEventContext& Context)
		{
			FFieldParticles& Particles = Context.Particles;
			Particles.Current = ReadValue(Context, 1);
			FFieldParticleEmitter* Emitter = nullptr;
			if (!CurrentEmitter(Context, Emitter))
			{
				return;
			}
			const int32 Count = ReadValue(Context, 3);
			const int32 Delay = ReadValue(Context, 5);
			const int32 EmitTicks = ReadValue(Context, 7);
			if (Emitter != nullptr)
			{
				Emitter->SpeedScale = 1;
				Emitter->Owner = static_cast<int16>(Particles.Owner);
				Emitter->Word00 = 0;
				Emitter->Roll = 0;
				Emitter->Count = static_cast<int16>(Count);
				Emitter->Delay = static_cast<int16>(Delay);
				Emitter->EmitTicks = static_cast<int16>(EmitTicks);
				Emitter->FacingOffsets = {};
			}
			Context.Run.Budget += 4;
			Advance(Context, 9);
		}

		/** Six operands at 1, 3 ... 11, each a constant or a variable by the flags byte at 13, as signed 16-bit values. */
		std::array<int16, 6> SixOperands(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 13);
			std::array<int16, 6> Values{};
			for (std::size_t Operand = 0; Operand < Values.size(); ++Operand)
			{
				Values[Operand] = static_cast<int16>(ReadOperand(Context, 1 + 2 * static_cast<int32>(Operand), Flags, static_cast<uint8>(0x80 >> Operand)));
			}
			return Values;
		}

		/** FE 91: the emitter's origin and the target its particles head for. */
		void SetEmitterPoints(FEventContext& Context)
		{
			const std::array<int16, 6> Values = SixOperands(Context);
			FFieldParticleEmitter* Emitter = nullptr;
			if (!CurrentEmitter(Context, Emitter))
			{
				return;
			}
			if (Emitter != nullptr)
			{
				Emitter->Origin = {Values[0], Values[1], Values[2]};
				Emitter->Target = {Values[3], Values[4], Values[5]};
			}
			Context.Run.Budget += 4;
			Advance(Context, 14);
		}

		/** FE 92: the particles' speed and acceleration, the spawn circle's radius and the spread about the target. */
		void SetEmitterMotion(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 13);
			const int32 Speed = ReadOperand(Context, 1, Flags, 0x80);
			const std::array<int16, 6> Values = SixOperands(Context);
			FFieldParticleEmitter* Emitter = nullptr;
			if (!CurrentEmitter(Context, Emitter))
			{
				return;
			}
			if (Emitter != nullptr)
			{
				Emitter->Speed = Speed;
				Emitter->Acceleration = {Values[1], Values[2], Values[3]};
				Emitter->SpawnRadius = Values[4];
				Emitter->TargetSpread = Values[5];
			}
			Context.Run.Budget += 4;
			Advance(Context, 14);
		}

		/** FE 93: the respawn interval, lifetime and picture, the random roll and sorting, with FE 8F's space and gear joint. */
		void SetEmitterLife(FEventContext& Context)
		{
			FFieldParticleEmitter* Emitter = nullptr;
			if (!CurrentEmitter(Context, Emitter))
			{
				return;
			}
			const FFieldParticles& Particles = Context.Particles;
			const int32 Interval = ReadValue(Context, 1);
			const int32 Lifetime = ReadValue(Context, 3);
			const int32 Sprite = ReadValue(Context, 5);
			const int32 Roll = ReadValue(Context, 7);
			const int32 Depth = ReadValue(Context, 9);
			if (Emitter != nullptr)
			{
				Emitter->Interval = static_cast<int16>(Interval);
				Emitter->Lifetime = static_cast<int16>(Lifetime);
				Emitter->Sprite = static_cast<int16>(Sprite);
				Emitter->Flags = static_cast<uint16>(Roll | Depth << FieldParticleFlags::DepthShift | Particles.SpaceBits);
				Emitter->GearSlot = static_cast<int16>(Particles.GearSlot);
				Emitter->GearJoint = static_cast<int16>(Particles.GearJoint);
			}
			Context.Run.Budget += 4;
			Advance(Context, 11);
		}

		/** FE 94: the quads' size across and up, and their change per tick. */
		void SetEmitterSize(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 9);
			std::array<int16, 4> Values{};
			for (std::size_t Operand = 0; Operand < Values.size(); ++Operand)
			{
				Values[Operand] = static_cast<int16>(ReadOperand(Context, 1 + 2 * static_cast<int32>(Operand), Flags, static_cast<uint8>(0x80 >> Operand)));
			}
			FFieldParticleEmitter* Emitter = nullptr;
			if (!CurrentEmitter(Context, Emitter))
			{
				return;
			}
			if (Emitter != nullptr)
			{
				Emitter->Size = {Values[0], Values[1], 0};
				Emitter->SizeStep = {Values[2], Values[3], 0};
			}
			Context.Run.Budget += 4;
			Advance(Context, 10);
		}

		/** FE 95: the particles' colour and its change per tick. */
		void SetEmitterColor(FEventContext& Context)
		{
			const std::array<int16, 6> Values = SixOperands(Context);
			FFieldParticleEmitter* Emitter = nullptr;
			if (!CurrentEmitter(Context, Emitter))
			{
				return;
			}
			if (Emitter != nullptr)
			{
				Emitter->Color = {static_cast<uint8>(Values[0]), static_cast<uint8>(Values[1]), static_cast<uint8>(Values[2])};
				Emitter->ColorStep = {static_cast<int8>(Values[3]), static_cast<int8>(Values[4]), static_cast<int8>(Values[5])};
			}
			Context.Run.Budget += 4;
			Advance(Context, 14);
		}

		/** FE 96: starts an effect from the setup, which the running actor's FE 97 stops; nothing when 64 run, or in the after-menu script. */
		void StartParticles(FEventContext& Context)
		{
			Context.Run.Budget += 4;
			if (!Context.bAfterMenuPass)
			{
				Context.Particles.Start(static_cast<int16>(Context.Run.Actor));
			}
			Advance(Context, 1);
		}

		/** FE 97: the running actor's effects stop respawning (0), or their particles end too. */
		void StopParticles(FEventContext& Context)
		{
			Context.Run.Budget += 4;
			Context.Particles.Stop(static_cast<int16>(Context.Run.Actor), CodeByte(Context, 1) != 0);
			Advance(Context, 2);
		}

		/** FE A5: the speed's multiplier, the quads' blend and their roll. */
		void SetEmitterBlend(FEventContext& Context)
		{
			FFieldParticleEmitter* Emitter = nullptr;
			if (!CurrentEmitter(Context, Emitter))
			{
				return;
			}
			const int32 SpeedScale = ReadValue(Context, 1);
			const int32 Blend = ReadValue(Context, 3);
			const int32 Roll = ReadValue(Context, 5);
			if (Emitter != nullptr)
			{
				Emitter->SpeedScale = static_cast<int16>(SpeedScale);
				Emitter->Flags = static_cast<uint16>(Emitter->Flags | Blend << FieldParticleFlags::BlendShift);
				Emitter->Roll = static_cast<int16>(Roll);
			}
			Context.Run.Budget += 4;
			Advance(Context, 7);
		}

		/** FE BD: particles start on the spawn circle (1) or along x only (2). */
		void SetEmitterShape(FEventContext& Context)
		{
			const int32 Shape = ReadValue(Context, 1);
			FFieldParticleEmitter* Emitter = nullptr;
			if (!CurrentEmitter(Context, Emitter))
			{
				return;
			}
			if (Emitter != nullptr)
			{
				Emitter->Flags |= Shape == 1 ? FieldParticleFlags::OnCircle : Shape == 2 ? FieldParticleFlags::AlongX : 0;
			}
			Context.Run.Budget += 4;
			Advance(Context, 7);
		}

		/** FE C8 and FE C9: the start's offsets for the owner's first or last four eighths of a turn on screen. */
		template <int32 First>
		void SetFacingOffsets(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 17);
			std::array<int16, 8> Values{};
			for (std::size_t Operand = 0; Operand < Values.size(); ++Operand)
			{
				Values[Operand] = static_cast<int16>(ReadOperand(Context, 1 + 2 * static_cast<int32>(Operand), Flags, static_cast<uint8>(0x80 >> Operand)));
			}
			FFieldParticleEmitter* Emitter = nullptr;
			if (!CurrentEmitter(Context, Emitter))
			{
				return;
			}
			if (Emitter != nullptr)
			{
				for (std::size_t Eighth = 0; Eighth < 4; ++Eighth)
				{
					Emitter->FacingOffsets[First + Eighth] = {Values[2 * Eighth], Values[2 * Eighth + 1]};
				}
			}
			Context.Run.Budget += 4;
			Advance(Context, 18);
		}

		/**
		 * 73: sub-operation 0 does nothing; 1 starts a gear's thruster effect on the running actor, on
		 * joint 20h or 22h by its variant. Any other sub-operation leaves the thread on the instruction.
		 */
		void ParticlePreset(FEventContext& Context)
		{
			switch (CodeByte(Context, 1))
			{
				case 0:
					Advance(Context, 2);
					break;
				case 1:
				{
					ReadValue(Context, 2);
					const int32 Variant = ReadValue(Context, 4);
					ReadValue(Context, 6);
					FFieldParticles& Particles = Context.Particles;
					Particles.ResetSetup(static_cast<int16>(Context.Run.Actor));
					FFieldParticleEmitter& Emitter = Particles.Setup[0];
					Emitter.Flags = 0x14;
					Emitter.Word00 = 1;
					Emitter.Count = 0x10;
					Emitter.GearSlot = 0;
					Emitter.GearJoint = Variant == 0x27 ? 0x20 : 0x22;
					Emitter.EmitTicks = 0x1000;
					Particles.Start(static_cast<int16>(Context.Run.Actor));
					Advance(Context, 8);
					break;
				}
				default:
					break;
			}
		}
	}

	void RegisterParticles(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0x73] = &ParticlePreset;
		Extended[0x8F] = &BeginParticles;
		Extended[0x90] = &SetEmitter;
		Extended[0x91] = &SetEmitterPoints;
		Extended[0x92] = &SetEmitterMotion;
		Extended[0x93] = &SetEmitterLife;
		Extended[0x94] = &SetEmitterSize;
		Extended[0x95] = &SetEmitterColor;
		Extended[0x96] = &StartParticles;
		Extended[0x97] = &StopParticles;
		Extended[0xA5] = &SetEmitterBlend;
		Extended[0xBD] = &SetEmitterShape;
		Extended[0xC2] = &BeginParticlesOn;
		Extended[0xC8] = &SetFacingOffsets<0>;
		Extended[0xC9] = &SetFacingOffsets<4>;
	}
}
