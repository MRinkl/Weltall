#pragma once

#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMatrix.h"

#include <array>

namespace Weltall
{
	struct FGteArctangents;

	/** What the camera's framing sees of a combatant slot: its sprite's place and height, and the slot's flags. */
	struct FBattleCameraSubject
	{
		/** A sprite stands for the slot (the combatant's own, or a 3D combatant's proxy). */
		bool bPresent = false;
		/** The slot is hidden or gone (its record's presence byte). */
		bool bHidden = false;
		bool bInGear = false;
		/** 16.16, y down. */
		std::array<int32, 3> Position{};
		/** The sprite's frame height (+0x36), which the framing adds above a gear. */
		int16 Height = 0;
	};

	/** How the camera moves each step. */
	namespace BattleCameraModes
	{
		/** The eye and the look-at stay where the easing takes them. */
		inline constexpr int32 Still = 0;
		/** Frames the slots the last framing named again every step. */
		inline constexpr int32 Framing = 1;
		/** Heads for the camera controller sprites' places. */
		inline constexpr int32 Controllers = 2;
		/** A model sequence's camera tracks move the eye and the look-at themselves; no easing. */
		inline constexpr int32 Model = 4;
	}

	/** The camera controllers' side: their tasks live with the sprites, and the camera ends them as it leaves their mode. */
	class IBattleCameraControllers
	{
	public:
		virtual ~IBattleCameraControllers() = default;

		/** Ends the eye's controller, then the look-at's, each through its own end. */
		virtual void EndControllers() = 0;
	};

	/**
	 * The battle's camera: an eye and a look-at easing toward where they head, the framing of a mask
	 * of combatants, the view matrix the frame is drawn with, and the distance, pitch and yaw between
	 * the eye and the look-at, which effects read.
	 */
	struct FBattleCamera
	{
		static constexpr FGteVector Up{0, 4096, 0};
		/** The default easing: an eighth of the way each step. */
		static constexpr int32 DefaultEaseRate = 0x200;
		static constexpr int16 DefaultPitch = 0xC0;

		FGteVector Eye{};
		FGteVector Target{};
		FGteVector DesiredEye{};
		FGteVector DesiredTarget{};
		int32 Mode = BattleCameraModes::Still;
		/** 1 eases toward the desired points; the model camera's 5 does not. */
		int32 Easing = 1;
		/** 4.12 of the remaining way per step. */
		int32 EaseRate = DefaultEaseRate;
		/** The slots the framing mode frames each step. */
		uint32 FramedSlots = 0xFFFFFFFF;
		/** The mode the camera goes back to when its last controller ends: framing as the program starts, still during a playback. */
		int32 ModeAfter = BattleCameraModes::Framing;
		/** The framing's rotation about x. */
		int16 Pitch = 0;
		/** Framing ignores gears' heights (system command 66). */
		bool bIgnoresHeights = false;
		/** Held by the battle event: framing, the model camera and releases change nothing. */
		bool bLocked = false;
		/** How far the last framing put the eye, which scales effects by the camera's distance. */
		int32 FramingDistance = 0;
		/** The framing distance's low half, which each framing call leaves for the sprites that scale by it. */
		uint16 SpriteDistance = 0;
		/** From the look-at to the eye, measured each step. */
		int32 Distance = 0;
		int16 ViewPitch = 0;
		int16 ViewYaw = 0;
		/** The camera controllers' places, 16.16: the kernel's fixed points the eye and the look-at follow in mode 2. */
		std::array<int32, 3> EyePoint{};
		std::array<int32, 3> TargetPoint{};
		/** The view the frame is drawn with: the look-at from the eye, the shake added to its translation. */
		FGteMatrix View;
		/** The controllers a mode other than theirs or the model's ends; none where no sprites run them. */
		IBattleCameraControllers* Controllers = nullptr;

		/** The default easing, every slot framed, easing on, the still mode. */
		WELTALLCORE_API void Init();
		/** A mode but the controllers' and the model's ends the controllers (after the mode is set: the last one's end may set another). */
		WELTALLCORE_API void SetMode(int32 NewMode);
		/**
		 * Unless locked, the framing mode on the slots of the mask; the sprites' camera
		 * distance follows the framing distance either way.
		 */
		WELTALLCORE_API void Frame(uint32 Slots, const std::array<FBattleCameraSubject, 11>& Subjects, const FGte& Gte);
		/** Where the eye and the look-at head to frame the slots of the mask. */
		WELTALLCORE_API void FrameSlots(uint32 Slots, const std::array<FBattleCameraSubject, 11>& Subjects, const FGte& Gte);
		/** The model camera takes the camera, or gives it back to the framing, unless locked. */
		WELTALLCORE_API void TakeForModel();
		WELTALLCORE_API void Release();
		/** The mode's desired points, the easing, then the distance, pitch and yaw. */
		WELTALLCORE_API void Step(const std::array<FBattleCameraSubject, 11>& Subjects, const FGte& Gte, const FGteArctangents& Arctangents);
		/** The view from the eye to the look-at, offset by the shake. */
		WELTALLCORE_API void BuildView(const FGteVector& ShakeOffset);
	};

	/** The view from Eye toward Target with Up's roll, its translation the eye's. */
	WELTALLCORE_API FGteMatrix BattleLookAt(const FGteVector& Eye, const FGteVector& Target, const FGteVector& Up);
}
