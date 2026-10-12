#include "Graphics/SpriteSystem.h"

#include "SpriteRules.h"

namespace Weltall
{
	namespace
	{
		constexpr uint32 AnimationBits = 0xFFu << SpriteTimeFlags::AnimationShift;
		constexpr uint32 NextAnimationBits = 0xFF;
		/**
		 * Ticks the restore runs the animation for at the most: far more than any animation's 63 counted
		 * steps take, however long their waits; an animation that stops or starts again short of them
		 * never gets there.
		 */
		constexpr int32 MaxRestoreTicks = 0x100000;
	}

	FSpriteSavedState FSpriteSystem::SaveState(const FSprite& Sprite)
	{
		FSpriteSavedState Saved;
		Saved.Position = Sprite.Position;
		Saved.Facing = Sprite.Facing;
		Saved.ListStep = static_cast<uint16>(Sprite.ListStep());
		Saved.Animation = Sprite.Animation();
		Saved.NextAnimation = Sprite.NextAnimation();
		Saved.StepsRun = static_cast<uint16>(Sprite.StepsRun());
		Saved.PresetVerticalSpeed = Sprite.Extra.PresetVerticalSpeed;
		Saved.PresetGravity = Sprite.Extra.PresetGravity;
		Saved.TransformScale = Sprite.TransformScale;
		Saved.Scale = Sprite.Scale;
		Saved.Weight = Sprite.Weight;
		return Saved;
	}

	void FSpriteSystem::RestoreState(FSprite& Sprite, const FSpriteSavedState& Saved)
	{
		using SpriteRules::Wrap;
		const int32 ExtraFrames = TickExtraFrames;
		TickExtraFrames = 0;
		Sprite.Facing = Saved.Facing;
		Sprite.TimeFlags = (Sprite.TimeFlags & ~AnimationBits) | static_cast<uint32>(static_cast<uint8>(Saved.Animation)) << SpriteTimeFlags::AnimationShift;
		Sprite.NextWord = (Sprite.NextWord & ~NextAnimationBits) | static_cast<uint8>(Saved.NextAnimation);
		Sprite.TransformScale = Saved.TransformScale;
		Sprite.Weight = Saved.Weight;
		Sprite.Scale = Saved.Scale;
		PlayAnimation(Sprite, Sprite.Animation());
		// The animation runs again up to where it was, the sprite moving by its velocity alone as it goes.
		for (int32 Tick = 0; Sprite.StepsRun() != Saved.StepsRun; ++Tick)
		{
			if (Sprite.Wait == 0 || Tick == MaxRestoreTicks)
			{
				Report(ESpriteFault::SavedStepsNeverReached, Saved.StepsRun);
				break;
			}
			TickAnimation(Sprite);
			Sprite.Position[0] = Wrap(int64{Sprite.Position[0]} + Sprite.Velocity[0]);
			Sprite.Position[2] = Wrap(int64{Sprite.Position[2]} + Sprite.Velocity[2]);
			Sprite.Position[1] = Wrap(int64{Sprite.Position[1]} + Sprite.Velocity[1]);
			Sprite.Velocity[1] = Wrap(int64{Sprite.Velocity[1]} + Sprite.Gravity);
		}
		Sprite.Position = Saved.Position;
		Sprite.Extra.PresetVerticalSpeed = Saved.PresetVerticalSpeed;
		Sprite.Extra.PresetGravity = Saved.PresetGravity;
		TickExtraFrames = ExtraFrames;
	}
}
