#include "Field/FieldPresenter.h"

#include "Field/EventContext.h"
#include "Field/FieldParticles.h"
#include "Graphics/GteVectors.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace Weltall
{
	namespace
	{
		/** The effects' texture page (4-bit, at (960, 256)) and the particles' CLUT (256, 247), as the GPU's attributes. */
		constexpr uint16 ParticlePage = 0x3C0 >> 6 | 1 << 4;
		constexpr uint16 ParticleClut = 247 << 6 | 256 >> 4;
		constexpr uint16 PageBlendShift = 5;
		constexpr uint16 FlagBlendShift = 8;
		/** Where a particle sorts: in front of everything, 16 entries nearer than its depth, at it, or 16 farther. */
		constexpr int32 FrontEntry = 1;
		constexpr int32 EntryStep = 16;
		constexpr int32 LastEntry = 0xFFF;
		constexpr int32 FractionBits = 12;
		/** A particle's blend key: its effect, its emitter, its place in the emitter's particles. */
		constexpr uint32 KeyEffectShift = 20;
		constexpr uint32 KeyEmitterShift = 17;
	}

	std::optional<std::array<int32, 3>> FFieldPresenter::ThrusterShift(const FFieldParticleEffect& Running, const FFieldView& Shown, const FFieldFrame& Frame) const
	{
		const auto Owned = std::find_if(Running.Emitters.begin(), Running.Emitters.end(), [](const FFieldParticleEmitter& Emitter) { return Emitter.Count != 0; });
		if (!GearModels || Owned == Running.Emitters.end() || !GearModels->Thrusts(Owned->Owner))
		{
			return std::nullopt;
		}
		const std::optional<int32> Slot = GearModels->LayoutSlotOf(Owned->Owner);
		if (!Slot)
		{
			return std::nullopt;
		}
		// Where the effect starts its particles: each emitter's newest, in view space.
		std::array<float, 3> Origin{};
		int32 Newest = 0;
		for (std::size_t Number = 0; Number < Running.Emitters.size(); ++Number)
		{
			const FFieldParticleEmitter& Emitter = Running.Emitters[Number];
			const std::vector<FFieldParticle>& Particles = Running.Particles[Number];
			int16 Youngest = std::numeric_limits<int16>::min();
			for (const FFieldParticle& Particle : Particles)
			{
				Youngest = Particle.bDrawn ? std::max(Youngest, Particle.Life) : Youngest;
			}
			const bool bScaled = Emitter.Space() == EFieldParticleSpace::ScaledOwner;
			const FGteMatrix Space = bScaled ? ScaleColumns(Shown.View, {Emitter.Scale, Emitter.Scale, Emitter.Scale}) : Shown.View;
			for (const FFieldParticle& Particle : Particles)
			{
				if (!Particle.bDrawn || Particle.Life != Youngest)
				{
					continue;
				}
				FGteMatrix Own;
				Own.T = {Particle.Position[0] >> FractionBits, Particle.Position[1] >> FractionBits, Particle.Position[2] >> FractionBits};
				const FGteMatrix Placed = CompMatrix(Space, Own);
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Origin[Axis] += static_cast<float>(Placed.T[Axis]);
				}
				++Newest;
			}
		}
		if (Newest == 0)
		{
			return std::nullopt;
		}
		for (float& Axis : Origin)
		{
			Axis /= static_cast<float>(Newest);
		}
		// The nearest place on the model's surface.
		const FFieldGearsFrame& Gears = Frame.Gears;
		std::optional<std::array<float, 3>> Nearest;
		float Least = 0.0f;
		for (std::size_t Item = 0; Item < Gears.Items.size(); ++Item)
		{
			const FFieldGearLayoutItem& Laid = Gears.Items[Item];
			const FFieldGearObject& Object = Gears.Objects[Item];
			if (Laid.Slot != *Slot || Laid.Part == nullptr || !Object.bDrawn || !Object.bFine)
			{
				continue;
			}
			for (const FGteVector& Vertex : Laid.Part->Vertices)
			{
				const std::array<float, 3> Turned = Object.Fine.Turn({static_cast<float>(Vertex[0]), static_cast<float>(Vertex[1]), static_cast<float>(Vertex[2])});
				const std::array<float, 3> At{Turned[0] + Object.Fine.T[0], Turned[1] + Object.Fine.T[1], Turned[2] + Object.Fine.T[2]};
				const float Distance = (At[0] - Origin[0]) * (At[0] - Origin[0]) + (At[1] - Origin[1]) * (At[1] - Origin[1]) + (At[2] - Origin[2]) * (At[2] - Origin[2]);
				if (!Nearest || Distance < Least)
				{
					Nearest = At;
					Least = Distance;
				}
			}
		}
		if (!Nearest)
		{
			return std::nullopt;
		}
		return std::array<int32, 3>{static_cast<int32>(std::lround((*Nearest)[0] - Origin[0])), static_cast<int32>(std::lround((*Nearest)[1] - Origin[1])),
			static_cast<int32>(std::lround((*Nearest)[2] - Origin[2]))};
	}

	std::optional<FError> FFieldPresenter::CaptureParticles(const FEventContext& Context, const FFieldView& Shown, FFieldFrame& Frame)
	{
		Frame.Particles.clear();
		const FGteMatrix& View = Shown.View;
		FGte Gte = FieldDrawingGte(Shown);
		for (std::size_t Effect = 0; Effect < Context.Particles.Effects.size(); ++Effect)
		{
			const FFieldParticleEffect& Running = Context.Particles.Effects[Effect];
			EEffectLook& Look = EffectLooks[Effect];
			std::array<int32, 3>& Shift = EffectShifts[Effect];
			if (!Running.bActive)
			{
				Look = EEffectLook::Ended;
				Shift = {};
				continue;
			}
			if (const std::optional<std::array<int32, 3>> Moved = ThrusterShift(Running, Shown, Frame))
			{
				Shift = *Moved;
			}
			if (Look == EEffectLook::Ended)
			{
				// A gear's model that runs on foot shows none of its dash's thrusters, the smoke left behind as it stops
				// included: an effect begun on it as it runs is left out until it ends.
				const bool bOnFoot = GearModels && std::any_of(Running.Emitters.begin(), Running.Emitters.end(),
					[this](const FFieldParticleEmitter& Emitter) { return Emitter.Count != 0 && GearModels->RunsOnFoot(Emitter.Owner); });
				Look = bOnFoot ? EEffectLook::LeftOut : EEffectLook::Drawn;
			}
			if (Look == EEffectLook::LeftOut)
			{
				continue;
			}
			for (std::size_t Slot = 0; Slot < Running.Emitters.size(); ++Slot)
			{
				const FFieldParticleEmitter& Emitter = Running.Emitters[Slot];
				const std::vector<FFieldParticle>& Particles = Running.Particles[Slot];
				if (Emitter.Count == 0 || Particles.empty())
				{
					continue;
				}
				if (Emitter.Sprite < 0 || Emitter.Sprite >= FFieldDrawingTables::ParticleSpriteCount)
				{
					return MakeError("particle effect " + std::to_string(Effect) + " draws picture " + std::to_string(Emitter.Sprite)
						+ ", past the table the field reads it from");
				}
				const FFieldParticleSprite& Sprite = DrawingTables.ParticleSprites[static_cast<std::size_t>(Emitter.Sprite)];
				const uint16 Blend = static_cast<uint16>(((Emitter.Flags >> FlagBlendShift) + 1) & 3);
				const int32 Sorting = Emitter.Flags >> FieldParticleFlags::DepthShift & 3;
				const bool bScaled = Emitter.Space() == EFieldParticleSpace::ScaledOwner;
				const std::array<int32, 3> Scale{Emitter.Scale, Emitter.Scale, Emitter.Scale};
				for (std::size_t Number = 0; Number < Particles.size(); ++Number)
				{
					const FFieldParticle& Particle = Particles[Number];
					if (!Particle.bDrawn)
					{
						continue;
					}
					// The quad faces the screen: only its place goes through the view; its roll and size turn and scale it there.
					FGteMatrix Own = RotationXyz({0, 0, Particle.Roll});
					Own.T = {Particle.Position[0] >> FractionBits, Particle.Position[1] >> FractionBits, Particle.Position[2] >> FractionBits};
					FGteMatrix Matrix = CompMatrix(bScaled ? ScaleColumns(View, Scale) : View, Own);
					Matrix.M = Own.M;
					for (std::size_t Axis = 0; Axis < 3; ++Axis)
					{
						Matrix.T[Axis] += Shift[Axis];
					}
					Matrix = ScaleColumns(Matrix, {Particle.Size[0], Particle.Size[1], Particle.Size[2]});
					if (bScaled)
					{
						Matrix = ScaleColumns(Matrix, Scale);
					}
					Gte.Transform = Matrix;
					std::array<FGteVector, 4> Corners{};
					std::array<uint16, 4> Depths{};
					for (std::size_t Corner = 0; Corner < 4; ++Corner)
					{
						Corners[Corner] = {Sprite.Corners[Corner][0], Sprite.Corners[Corner][1], 0};
						Depths[Corner] = Gte.Rtps(Corners[Corner]).ScreenZ;
					}
					const int32 Bucket = Gte.AverageDepth4(Depths[0], Depths[1], Depths[2], Depths[3]) >> 2;
					const int32 Entry = Sorting == 0 ? FrontEntry : Bucket + (Sorting - 2) * EntryStep;
					if (Entry < FrontEntry || Entry > LastEntry)
					{
						continue;
					}
					FFieldParticleQuad& Quad = Frame.Particles.emplace_back();
					Quad.BlendKey = static_cast<uint32>(Effect) << KeyEffectShift | static_cast<uint32>(Slot) << KeyEmitterShift | static_cast<uint32>(Number);
					Quad.Life = Particle.Life;
					Quad.Matrix = Matrix;
					Quad.Corners = Corners;
					Quad.Texels = Sprite.Texels;
					Quad.TexturePage = static_cast<uint16>(ParticlePage | Blend << PageBlendShift);
					Quad.Clut = ParticleClut;
					Quad.Color = Particle.Color;
					Quad.OtIndex = Entry;
					Quad.World = Own.T;
				}
			}
		}
		return std::nullopt;
	}
}
