#include "WorldMap/WorldMapParticles.h"

#include "Foundation/LittleEndian.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		constexpr uint8 Emitting = 0x80;
		constexpr uint8 DelayedStart = 0x10;
		constexpr uint8 FlatSpawn = 0x20;
		constexpr uint8 FlatSpread = 0x40;
		constexpr uint8 FixedDistance = 0x04;
		constexpr uint8 FixedSpread = 0x08;
		/** A particle's texture page: 8-bit at (340h, 100h), its blend the emitter's two low flags. */
		constexpr uint16 ParticlePage = 0x9D;

		int32 Multiply(int32 A, int32 B)
		{
			return static_cast<int32>(static_cast<uint32>(A) * static_cast<uint32>(B));
		}

		int32 Add(int32 A, int32 B)
		{
			return static_cast<int32>(static_cast<uint32>(A) + static_cast<uint32>(B));
		}

		int32 Subtract(int32 A, int32 B)
		{
			return static_cast<int32>(static_cast<uint32>(A) - static_cast<uint32>(B));
		}

		FGteVector ReadVector(std::span<const uint8> Bytes, std::size_t At)
		{
			return {ReadS16(Bytes, At), ReadS16(Bytes, At + 2), ReadS16(Bytes, At + 4)};
		}

		/** A direction drawn at random (the vertical kept flat when asked), normalized. */
		FGteLongVector RandomDirection(FPsxRandom& Random, bool bFlat)
		{
			FGteLongVector Vector{};
			Vector[1] = bFlat ? 0 : (Random.Next() & 0xFFF) - 0x800;
			Vector[0] = (Random.Next() & 0xFFF) - 0x800;
			Vector[2] = (Random.Next() & 0xFFF) - 0x800;
			return VectorNormal(Vector);
		}

		/** The distance a spawn goes: the setup's, or a draw below it. */
		int32 DrawDistance(FPsxRandom& Random, bool bFixed, uint16 Limit)
		{
			if (bFixed)
			{
				return Limit;
			}
			return Random.Next() % static_cast<int32>(Limit);
		}

		uint8 ClampChannel(int32 Value)
		{
			return static_cast<uint8>(std::clamp(Value, 0, 0xFF));
		}
	}

	TResult<FWorldMapParticles> FWorldMapParticles::FromSection(std::span<const uint8> Section)
	{
		if (Section.size() % EmitterBytes != 0)
		{
			return MakeError("the world map's particle effects are not whole emitters");
		}
		FWorldMapParticles Particles;
		for (std::size_t At = 0; At < Section.size(); At += EmitterBytes)
		{
			FWorldMapEmitter Emitter;
			Emitter.StartDelay = ReadU32(Section, At);
			Emitter.MaxParticles = ReadS16(Section, At + 0x08);
			Emitter.Life = ReadS16(Section, At + 0x0C);
			Emitter.Shape = ReadS16(Section, At + 0x0E);
			if (Emitter.Shape < 0 || Emitter.Shape >= ShapeCount)
			{
				return MakeError("a world map particle emitter takes shape " + std::to_string(Emitter.Shape) + ", past the program's ten");
			}
			Emitter.Interval = ReadS16(Section, At + 0x10);
			Emitter.PlaceTail = ReadS16(Section, At + 0x1A);
			Emitter.AnglesPad = ReadS16(Section, At + 0x22);
			Emitter.Offset = ReadVector(Section, At + 0x24);
			Emitter.OffsetPad = ReadS16(Section, At + 0x2A);
			Emitter.Axis = ReadVector(Section, At + 0x2C);
			Emitter.AxisPad = ReadS16(Section, At + 0x32);
			Emitter.Speed = static_cast<int32>(ReadU32(Section, At + 0x34));
			Emitter.Acceleration = {ReadS16(Section, At + 0x38), ReadS16(Section, At + 0x3A), ReadS16(Section, At + 0x3C)};
			Emitter.AccelerationPad = ReadS16(Section, At + 0x3E);
			Emitter.Distance = ReadU16(Section, At + 0x40);
			Emitter.Spread = ReadU16(Section, At + 0x42);
			Emitter.Size = ReadU32(Section, At + 0x44);
			Emitter.Growth = ReadU32(Section, At + 0x48);
			Emitter.Colour = ReadU32(Section, At + 0x4C);
			Emitter.ColourStep = ReadU32(Section, At + 0x50);
			// The particles' start clears the running counters, the place and the angles; the rest is the file's.
			Particles.EmitterList.push_back(Emitter);
		}
		return Particles;
	}

	void FWorldMapParticles::Start(int32 Effect, const std::optional<std::array<int16, 4>>& Place, const std::optional<FGteVector>& Rotation)
	{
		const std::size_t First = static_cast<std::size_t>(Effect) * EmittersPerEffect;
		// The program tests its first emitter eight times over rather than each in turn.
		const bool bRunning = (EmitterList[First].Flags() & Emitting) != 0;
		for (std::size_t Index = First; Index < First + EmittersPerEffect; ++Index)
		{
			FWorldMapEmitter& Emitter = EmitterList[Index];
			if (!bRunning)
			{
				Emitter.SetFlags(static_cast<uint8>(Emitter.Flags() | Emitting));
				Emitter.Alive = 0;
				Emitter.Countdown = Emitter.Interval;
				Emitter.Delay = Emitter.StartDelay;
			}
			if (Place)
			{
				Emitter.Place = {(*Place)[0], (*Place)[1], (*Place)[2]};
				Emitter.PlaceTail = (*Place)[3];
			}
			else
			{
				Emitter.Place = {0, 0, 0};
			}
			if (Rotation)
			{
				Emitter.Angles = {static_cast<int16>(-(*Rotation)[0]), static_cast<int16>(-(*Rotation)[1]), static_cast<int16>(-(*Rotation)[2])};
			}
			else
			{
				Emitter.Angles = {0, 0, 0};
			}
		}
	}

	void FWorldMapParticles::Stop(int32 Effect)
	{
		const std::size_t First = static_cast<std::size_t>(Effect) * EmittersPerEffect;
		for (std::size_t Index = First; Index < First + EmittersPerEffect; ++Index)
		{
			EmitterList[Index].SetFlags(static_cast<uint8>(EmitterList[Index].Flags() & ~Emitting));
		}
	}

	void FWorldMapParticles::StopAll()
	{
		for (FWorldMapEmitter& Emitter : EmitterList)
		{
			Emitter.SetFlags(static_cast<uint8>(Emitter.Flags() & ~Emitting));
		}
	}

	void FWorldMapParticles::StopParticles(int32 Effect)
	{
		const int32 First = Effect * EmittersPerEffect;
		for (FWorldMapParticle& Particle : ParticleList)
		{
			if (Particle.Emitter >= First && Particle.Emitter < First + EmittersPerEffect && Particle.Shape != 0)
			{
				Particle.Life = 0;
			}
		}
	}

	void FWorldMapParticles::Tick(FPsxRandom& Random, const FGteArctangents& Arctangents)
	{
		for (std::size_t Index = 0; Index < EmitterList.size(); ++Index)
		{
			FWorldMapEmitter& Emitter = EmitterList[Index];
			const uint8 Flags = Emitter.Flags();
			if ((Flags & Emitting) == 0)
			{
				continue;
			}
			if ((Flags & DelayedStart) != 0)
			{
				// The low half delays the first spawn; the high half counts the spawns left, one a tick.
				int32 Spawns = static_cast<int32>(Emitter.Delay) >> 16;
				const int32 Wait = static_cast<int16>(Emitter.Delay);
				if (Wait != 0)
				{
					// The program ORs the whole decremented word in, sign and all.
					Emitter.Delay = static_cast<uint32>(Spawns) << 16 | static_cast<uint32>(Wait - 1);
					continue;
				}
				if (Spawns == 0)
				{
					Emitter.SetFlags(static_cast<uint8>(Flags ^ Emitting));
					Emitter.Delay = 0;
					continue;
				}
				--Spawns;
				Emitter.Delay = static_cast<uint32>(Spawns) << 16;
			}
			if (Emitter.Countdown != 0)
			{
				--Emitter.Countdown;
				continue;
			}
			Emitter.Countdown = Emitter.Interval;
			if (Emitter.Shape != 0 && Emitter.MaxParticles > 0 && Emitter.Alive < Emitter.MaxParticles)
			{
				Spawn(static_cast<int32>(Index), Flags, Random, Arctangents);
			}
		}
		Move();
	}

	void FWorldMapParticles::Spawn(int32 Index, uint8 Flags, FPsxRandom& Random, const FGteArctangents& Arctangents)
	{
		FWorldMapEmitter& Emitter = EmitterList[static_cast<std::size_t>(Index)];
		for (FWorldMapParticle& Particle : ParticleList)
		{
			if (Particle.Shape != 0)
			{
				continue;
			}
			Particle.Emitter = static_cast<int16>(Index);
			Particle.Life = Emitter.Life;
			Particle.Shape = Emitter.Shape;
			const FGteMatrix Turn = RotationYxz(Emitter.Angles);
			const FGteLongVector Offset = ApplyMatrix(Turn, Emitter.Offset);
			const FGteLongVector Out = RandomDirection(Random, (Flags & FlatSpawn) != 0);
			const int32 Distance = DrawDistance(Random, (Flags & FixedDistance) != 0, Emitter.Distance);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Particle.Position[Axis] = Add(Add(Multiply(Offset[Axis], 0x1000), Multiply(Out[Axis], Distance)), Multiply(Emitter.Place[Axis], 0x1000));
			}
			const FGteLongVector Towards = ApplyMatrix(Turn, Emitter.Axis);
			const FGteLongVector Spread = RandomDirection(Random, (Flags & FlatSpread) != 0);
			const int32 SpreadDistance = DrawDistance(Random, (Flags & FixedSpread) != 0, Emitter.Spread);
			FGteLongVector Heading{};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				const int32 Goal = Add(Add(Multiply(Towards[Axis], 0x1000), Multiply(Spread[Axis], SpreadDistance)), Multiply(Emitter.Place[Axis], 0x1000));
				Heading[Axis] = Subtract(Goal, Particle.Position[Axis]) >> 12;
			}
			const FGteLongVector Unit = VectorNormal(Heading);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Particle.Velocity[Axis] = Multiply(Unit[Axis], Emitter.Speed) >> 12;
				Particle.Acceleration[Axis] = Emitter.Acceleration[Axis];
			}
			Particle.Angle = static_cast<int16>(GteRatan2(Unit[1], Unit[0], Arctangents));
			Particle.Colour = Emitter.Colour;
			Particle.ColourStep = Emitter.ColourStep;
			Particle.Size = {static_cast<int16>(Emitter.Size), static_cast<int16>(Emitter.Size >> 16)};
			Particle.Growth = {static_cast<int16>(Emitter.Growth), static_cast<int16>(Emitter.Growth >> 16)};
			Particle.TexturePage = static_cast<uint16>((Flags & 3) << 5 | ParticlePage);
			++Emitter.Alive;
			return;
		}
	}

	void FWorldMapParticles::Move()
	{
		for (FWorldMapParticle& Particle : ParticleList)
		{
			if (Particle.Shape == 0)
			{
				continue;
			}
			if (Particle.Life < 1)
			{
				--EmitterList[static_cast<std::size_t>(Particle.Emitter)].Alive;
				Particle.Emitter = 0;
				Particle.Life = 0;
				Particle.Shape = 0;
				continue;
			}
			--Particle.Life;
			const uint32 Colour = Particle.Colour;
			const uint32 Step = Particle.ColourStep;
			const uint8 Red = ClampChannel(static_cast<int32>(Colour & 0xFF) + static_cast<int8>(Step & 0xFF));
			const uint8 Green = ClampChannel(static_cast<int32>(Colour >> 8 & 0xFF) + static_cast<int8>(Step >> 8 & 0xFF));
			const uint8 Blue = ClampChannel(static_cast<int32>(Colour >> 16 & 0xFF) + static_cast<int8>(Step >> 16 & 0xFF));
			Particle.Colour = (Colour & 0xFF000000u) | static_cast<uint32>(Blue) << 16 | static_cast<uint32>(Green) << 8 | Red;
			Particle.Size[0] = static_cast<int16>(Particle.Size[0] + Particle.Growth[0]);
			Particle.Size[1] = static_cast<int16>(Particle.Size[1] + Particle.Growth[1]);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Particle.Position[Axis] = Add(Particle.Position[Axis], Particle.Velocity[Axis]);
				Particle.Velocity[Axis] = Add(Particle.Velocity[Axis], Particle.Acceleration[Axis]);
			}
		}
	}
}
