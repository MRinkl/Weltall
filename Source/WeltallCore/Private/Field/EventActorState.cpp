#include "Field/EventActorState.h"

namespace Weltall
{
	namespace
	{
		/** Status bits a reset clears: the rotation axis, portrait slot, door, call depth, facing sector, bounds flag and texture fields. */
		constexpr uint32 StatusResetKeep = 0xF0000000u | 0xE000u;
		constexpr uint32 SpriteTextureResetKeep = 0xF0000000u;
		constexpr uint32 SpriteDrawResetClear = 0xE0u;
		constexpr uint32 ThreadWordResetKeep = 0xFE000000u;
	}

	void FEventActorState::Reset(int32 RandomNumber)
	{
		Flags0 = EventActorFlags0::Initial;
		Flags1 = EventActorFlags1::Initial;
		HalfWidth = 0x10;
		HalfDepth = 0x10;
		Height = 0x60;
		LinkedActor = 0xFF;
		Parent = 0xFF;
		Velocity = {};
		Step = {};
		InteractX = 0;
		Word62 = 0;
		InteractZ = 0;
		MoveWork = {};
		IdleAnimation = 0;
		AnimationRequest = 0xFF;
		DoorStep = 0;
		Pc = 0;
		StillTicks = 0;
		TurnSpeed = 0x200;
		Radius = HalfWidth;
		Tints.fill(0x80);
		GearJoint = 0xFFFF;
		Status &= StatusResetKeep;
		SpriteTexture &= SpriteTextureResetKeep;
		for (FEventThread& Thread : Threads)
		{
			Thread.WaitCount = 0;
			Thread.Pc = 0xFFFF;
			Thread.Script = FEventThread::NoScript;
			Thread.Word = (Thread.Word & ThreadWordResetKeep) | FEventThread::PriorityMask | FEventThread::MoveCounterMask;
		}
		Character = 0xFF;
		SpeedDivisor = 0x100;
		WindowLines = 0;
		WindowWidth = 0;
		WindowY = 0;
		WindowX = 0;
		WindowStyle = 0;
		RequestSlot = 0;
		Slot = 0;
		Animation = 0;
		Layer = 0;
		TargetHeight = 0;
		SpriteDraw &= ~SpriteDrawResetClear;
		Counter = static_cast<int16>(RandomNumber);
		Scale = {0x1000, 0x1000, 0x1000};
		SoundMode = 0xFF;
		Portrait = 0xFF;
		Facing = 0x8000;
		MoveDirection = 0x8000;
		Yaw = 0x8000;
		ExternalAnimationFile = -1;
		PushCount = 0;
		FloorTriangles = {};
	}
}
