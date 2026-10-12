#pragma once

#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	class FPsxRandom;

	/**
	 * The gear cockpit's targeting reticle: it wanders between the points of a 5 x 5 grid, sliding toward
	 * one at 1 to 8 steps of its major axis a frame; once there, each frame a 1-in-32 roll picks the next
	 * point; while it moves, 20 random digits flicker beside it. Every frame of a gear member's command
	 * input draws from the game's random numbers here.
	 */
	struct FBattleCockpitReticle
	{
		static constexpr std::array<int32, 5> GridX{0xA0, 0xBE, 0x82, 0x64, 0xDC};
		static constexpr std::array<int32, 5> GridY{0x64, 0x6E, 0x5A, 0x50, 0x78};
		static constexpr int32 DigitCount = 20;

		int16 X = 0xA0;
		int16 Y = 0x64;
		int32 StartX = 0;
		int32 StartY = 0;
		int32 TargetX = 0;
		int32 TargetY = 0;
		/** Each step's move along each axis in 256ths: 256 along the major axis. */
		int32 StepX = 0;
		int32 StepY = 0;
		bool bLeft = false;
		bool bUp = false;
		/** How far it has come, in 256ths. */
		int32 MovedX = 0;
		int32 MovedY = 0;
		/** Steps a frame. */
		int32 Speed = 0;
		/** It has passed its target and waits for the roll that sends it on (the program starts so). */
		bool bArrived = true;
		/** The digits the last frame drew beside it (0-9), when it moved. */
		std::array<uint8, DigitCount> Digits{};

		/** The cockpit is opened for a gear member's turn: the reticle back at the screen's middle. */
		void Open()
		{
			X = 0xA0;
			Y = 0x64;
		}
		/** Heads from (FromX, FromY) to (ToX, ToY) unless either coordinate is the same. */
		WELTALLCORE_API void Aim(int32 FromX, int32 FromY, int32 ToX, int32 ToY, FPsxRandom& Random);
		/** The frame's steps, and whether it passed its target. */
		WELTALLCORE_API void Move();
		/** The reticle's logic, each frame the cockpit shows it; whether it drew digits. */
		WELTALLCORE_API bool Frame(FPsxRandom& Random);
	};
}
