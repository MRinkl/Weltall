#include "EventOps.h"
#include "FieldDirections.h"

#include "Field/FieldCamera.h"

#include <type_traits>

namespace Weltall::EventOps
{
	namespace
	{
		/** A change asked to happen in no ticks snaps the camera for two. */
		constexpr int32 SnapTicks = 2;
		/** The camera's flags 76 and B8 keep: all of the lower half but the button lock, or but the eye's freedom. */
		constexpr uint32 KeptWithButtonTurn = 0x7FFF;
		constexpr uint32 KeptWithEyeClamp = 0xBFFF;
		constexpr int32 Directions = 8;

		/** The camera's yaw for one of the scripts' eight directions. */
		int32 DirectionYaw(int32 Direction)
		{
			return static_cast<int32>(static_cast<uint32>(Direction + 4) & 7) << 9;
		}

		/**
		 * Starts turning the camera to a direction over a number of ticks (none snaps it), by the
		 * fewest 45-degree steps from the direction it looks in (four turn up). The steps come from a
		 * table of eight rows by the camera's direction, read at row * 8 + the direction as given, so a
		 * direction past 7 reads on into the next rows; outside the table is a fault.
		 */
		bool StartCameraTurn(FEventContext& Context, int32 Direction, int32 Ticks)
		{
			FFieldCamera& Camera = Context.Camera;
			const int32 Entry = FieldDirections::CameraEighth(Camera) * Directions + Direction;
			if (Entry < 0 || Entry >= Directions * Directions)
			{
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Entry));
				return false;
			}
			if (Ticks == 0)
			{
				Ticks = 1;
				Camera.SnapTicks += SnapTicks;
			}
			Camera.TurnTicks = static_cast<int16>(Ticks);
			Camera.bTurnByButtons = false;
			Camera.YawGoal = DirectionYaw(Direction);
			const int32 Steps = static_cast<int32>(static_cast<uint32>(Entry % Directions - Entry / Directions + 3) & 7) - 3;
			Camera.TurnStep = static_cast<int32>(static_cast<uint32>(Steps) << 25) / Ticks;
			return true;
		}

		/**
		 * B5: turns the camera to one of the eight directions: in the map's init at once (the yaw and
		 * its goal; the turning yaw only with the fix, so in the original the next turn starts from the
		 * old direction), then over a number of ticks, waiting while a turn is under way. Either way it
		 * ends the actor's turn.
		 */
		void CameraRotate(FEventContext& Context)
		{
			const int32 Direction = ReadValue(Context, 1);
			const int32 Ticks = ReadValue(Context, 3);
			FFieldCamera& Camera = Context.Camera;
			Context.Run.bYield = true;
			if (!Context.bInitDone)
			{
				Camera.Rotation[1] = static_cast<int16>(DirectionYaw(Direction));
				Camera.YawGoal = DirectionYaw(Direction);
				if (Context.Options.bInitTurnSetsTurningYaw)
				{
					Camera.TurningYaw = static_cast<int32>(static_cast<uint32>(DirectionYaw(Direction)) << 16);
				}
			}
			else if (Camera.TurnTicks != 0 || !StartCameraTurn(Context, Direction, Ticks))
			{
				return;
			}
			Advance(Context, 5);
		}

		/**
		 * C7 and C8 (the same): turn the camera 45 degrees further over a number of ticks (none snaps
		 * it), waiting while a turn is under way. Either way it ends the actor's turn.
		 */
		void CameraTurnStep(FEventContext& Context)
		{
			FFieldCamera& Camera = Context.Camera;
			if (Camera.TurnTicks == 0)
			{
				int32 Ticks = ReadValue(Context, 1);
				if (Ticks == 0)
				{
					Ticks = 1;
					Camera.SnapTicks += SnapTicks;
				}
				Camera.TurnStep = 0x2000000 / Ticks;
				Camera.YawGoal += FFieldCamera::YawStep;
				Camera.TurnTicks = static_cast<int16>(Ticks);
				Camera.bTurnByButtons = false;
				Advance(Context, 3);
			}
			Context.Run.bYield = true;
		}

		/** A5: the direction the camera looks in, 0 to 7, into a variable. */
		void GetCameraDirection(FEventContext& Context)
		{
			SetVar(Context, ReadU16(Context, 1), FieldDirections::CameraEighth(Context.Camera));
			Advance(Context, 3);
		}

		/** 76 and 77: the player's buttons turn the camera again, or no longer; 76 also clears the flags' upper half. */
		void EnableCameraRotation(FEventContext& Context)
		{
			Context.Camera.Flags &= KeptWithButtonTurn;
			Advance(Context, 1);
		}

		void DisableCameraRotation(FEventContext& Context)
		{
			Context.Camera.Flags |= FieldCameraFlags::NoButtonTurn;
			Advance(Context, 1);
		}

		/** B7 and B8: the eye may go below the camera layer, or is kept above it again (B8 also clears the flags' upper half). */
		void CameraGroundClampOff(FEventContext& Context)
		{
			Context.Camera.Flags |= FieldCameraFlags::EyeMayGoBelowCameraLayer;
			Advance(Context, 1);
		}

		void CameraGroundClampOn(FEventContext& Context)
		{
			Context.Camera.Flags &= KeptWithEyeClamp;
			Advance(Context, 1);
		}

		/** FE 22: the camera's projection distance into a variable. */
		void GetCameraProjection(FEventContext& Context)
		{
			SetVar(Context, ReadU16(Context, 1), Context.Camera.ProjectionDistance);
			Advance(Context, 3);
		}

		/**
		 * AF, B0, B1: with a mode byte of 0, the scripted camera's yaw, pitch or distance into the
		 * variable the word names; otherwise the word itself becomes it (a plain constant). Each grants
		 * the actor another instruction.
		 */
		template <auto Member>
		void CameraRegister(FEventContext& Context)
		{
			auto& Register = Context.Camera.*Member;
			const uint16 Word = ReadU16(Context, 1);
			if (CodeByte(Context, 3) == 0)
			{
				SetVar(Context, Word, Register);
			}
			else
			{
				Register = static_cast<std::remove_reference_t<decltype(Register)>>(Word);
			}
			Context.Run.Budget += 1;
			Advance(Context, 4);
		}

		/**
		 * F2: ramps the shake's amplitudes (x, z, then y, in whole units) to new ones over a number of
		 * ticks (none as one); all three 0 stops the shake once the ramp and two ticks more are over.
		 * The original ramps z from y's amplitude, not its own; the fix ramps it from its own.
		 */
		void CameraShake(FEventContext& Context)
		{
			const int32 X = ReadValue(Context, 1);
			const int32 Z = ReadValue(Context, 3);
			const int32 Y = ReadValue(Context, 5);
			int32 Ticks = ReadValue(Context, 7);
			if (Ticks == 0)
			{
				Ticks = 1;
			}
			Advance(Context, 9);
			FFieldCamera& Camera = Context.Camera;
			Camera.ShakeTicks = static_cast<int16>(Ticks);
			Camera.bShaking = true;
			auto Growth = [Ticks](int32 Whole, int32 From) { return ProcessorDivide(static_cast<int32>(static_cast<uint32>(Whole) * 0x10000u - static_cast<uint32>(From)), Ticks); };
			Camera.ShakeGrowth = {Growth(X, Camera.ShakeAmplitude[0]), Growth(Y, Camera.ShakeAmplitude[1]),
				Growth(Z, Camera.ShakeAmplitude[Context.Options.bShakeDepthFromItsOwn ? 2 : 1])};
			Camera.bShakeStops = X == 0 && Y == 0 && Z == 0;
			if (Camera.bShakeStops)
			{
				Camera.ShakeTicks = static_cast<int16>(Camera.ShakeTicks + SnapTicks);
			}
		}

		/** FE 48 and FE 6F: the screen's or the scenery's rotation, x, z then y (the flags byte makes each a constant). */
		template <FGteVector FFieldCamera::*Rotation>
		void SetCameraAngles(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 7);
			FGteVector& Angles = Context.Camera.*Rotation;
			Angles[0] = static_cast<int16>(ReadOperand(Context, 1, Flags, 0x80));
			Angles[2] = static_cast<int16>(ReadOperand(Context, 3, Flags, 0x40));
			Angles[1] = static_cast<int16>(ReadOperand(Context, 5, Flags, 0x20));
			Advance(Context, 8);
		}
	}

	void RegisterCameraControl(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0x76] = &EnableCameraRotation;
		Base[0x77] = &DisableCameraRotation;
		Base[0xA5] = &GetCameraDirection;
		Base[0xAF] = &CameraRegister<&FFieldCamera::ScriptYaw>;
		Base[0xB0] = &CameraRegister<&FFieldCamera::ScriptPitch>;
		Base[0xB1] = &CameraRegister<&FFieldCamera::ScriptDistance>;
		Base[0xB5] = &CameraRotate;
		Base[0xB7] = &CameraGroundClampOff;
		Base[0xB8] = &CameraGroundClampOn;
		Base[0xF2] = &CameraShake;
		Extended[0x22] = &GetCameraProjection;
		Base[0xC7] = &CameraTurnStep;
		Base[0xC8] = &CameraTurnStep;
		Extended[0x48] = &SetCameraAngles<&FFieldCamera::ScreenAngles>;
		Extended[0x6F] = &SetCameraAngles<&FFieldCamera::WorldAngles>;
	}
}
