#pragma once

#include "Graphics/Sprite.h"
#include "Graphics/SpriteSystem.h"

// The kernel's arithmetic and bookkeeping that the sprite routines share.
namespace Weltall::SpriteRules
{
	/** A value as a 32-bit register holds it: the low 32 bits, two's complement. */
	inline int32 Wrap(int64 Value)
	{
		return static_cast<int32>(static_cast<uint32>(static_cast<uint64>(Value)));
	}

	/** A product as the CPU's 32-bit multiply leaves it. */
	inline int32 Multiply(int64 A, int64 B)
	{
		return Wrap(A * B);
	}

	/** Division by 2^Bits rounded toward zero, as the compiler's shift for a signed division does. */
	inline int32 DivideByPower(int64 Value, int32 Bits)
	{
		const int32 Truncated = Wrap(Value);
		return (Truncated < 0 ? static_cast<int32>(static_cast<int64>(Truncated) + (int64{1} << Bits) - 1) : Truncated) >> Bits;
	}

	/** An instruction's length in the kernel's table: SetFrame is in it at two bytes, though it has three. */
	inline uint32 TableLength(uint8 Opcode)
	{
		return Opcode < 0xA0 ? 1 : Opcode < 0xC8 ? 2 : Opcode < 0xF1 ? 3 : 4;
	}

	/** One more step run, stuck at 63. */
	inline void CountStep(FSprite& Sprite)
	{
		uint32 Steps = (Sprite.StepsRun() + 1) & 0x3F;
		if (Steps == 0)
		{
			Steps = 0x3F;
		}
		Sprite.AnimationFlags = (Sprite.AnimationFlags & ~SpriteAnimationFlags::StepsMask) | Steps << SpriteAnimationFlags::StepsShift;
	}

	/** Scales a motion by the sprite's size factor, when it has one. */
	inline int32 ScaleBySize(const FSprite& Sprite, int32 Value)
	{
		return Sprite.SizeFactor != 0 ? DivideByPower(int64{Value} * Sprite.SizeFactor, 10) : Value;
	}
}
