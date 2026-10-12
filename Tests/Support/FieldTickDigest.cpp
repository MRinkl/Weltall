#include "Support/FieldTickDigest.h"

#include "Text/MesWindow.h"

namespace Weltall::Testing
{
	namespace
	{
		void AddPoint(FOracleDigest& Digest, const FFieldPoint& Point)
		{
			Digest.I32(Point[0], Point[1], Point[2]);
		}

		void AddMatrix(FOracleDigest& Digest, const FGteMatrix& Matrix)
		{
			for (const auto& Row : Matrix.M)
			{
				Digest.I16(Row[0], Row[1], Row[2]);
			}
			Digest.I32(Matrix.T[0], Matrix.T[1], Matrix.T[2]);
		}

		class FLittleBytes
		{
		public:
			void U8(uint32 Value) { Bytes.push_back(static_cast<uint8>(Value)); }
			void U16(uint32 Value)
			{
				U8(Value);
				U8(Value >> 8);
			}
			void U32(uint32 Value)
			{
				U16(Value);
				U16(Value >> 16);
			}

			std::vector<uint8> Bytes;
		};

		void AddWindow(FLittleBytes& Out, const FFieldMessageWindow& Window)
		{
			Out.U16(Window.bOpen ? 0 : 0xFFFF);
			Out.U16(static_cast<uint16>(Window.Age));
			Out.U16(Window.bFocus ? 1 : 0);
			Out.U16(Window.bCloseRequested ? 0 : 0xFFFF);
			Out.U16(static_cast<uint16>(Window.Owner));
			Out.U16(static_cast<uint16>(Window.Speaker));
			Out.U16(static_cast<uint16>(Window.OpenTicks));
			Out.U16(Window.Flags);
			for (const int16 Value : {Window.Rect.X, Window.Rect.Y, Window.Rect.Width, Window.Rect.Height})
			{
				Out.U16(static_cast<uint16>(Value));
			}
			for (const int32 Value : {Window.OpenOffset[0], Window.OpenOffset[1], Window.OpenStep[0], Window.OpenStep[1]})
			{
				Out.U32(static_cast<uint32>(Value));
			}
			Out.U16(Window.bChoice ? 0 : 0xFFFF);
			for (const int16 Value : {Window.ChoiceFirst, Window.ChoiceCount, Window.ChoiceCursor})
			{
				Out.U16(static_cast<uint16>(Value));
			}
			Out.U16(Window.bTextWaits ? 0 : 0xFFFF);
			Out.U16(static_cast<uint16>(Window.MoreCursorDelay));
			Out.U8(Window.bPortrait ? 1 : 0);
			Out.U8(Window.Portrait);
			if (!Window.bOpen)
			{
				return;
			}
			const FFieldMessageFrame& Frame = Window.Frame;
			Out.U16(static_cast<uint16>(Window.TextX));
			Out.U16(static_cast<uint16>(Window.TextY));
			Out.U16(static_cast<uint16>(Frame.Rect.X));
			Out.U16(static_cast<uint16>(Frame.Rect.Y + 1));
			Out.U16(static_cast<uint16>(Frame.Rect.Width));
			Out.U16(static_cast<uint16>(Frame.Rect.Height - 2));
			for (const int16 Value : {Frame.PortraitX0, Frame.PortraitX1, Frame.PortraitY0, Frame.PortraitY1})
			{
				Out.U16(static_cast<uint16>(Value));
			}
			if (Window.bPortrait)
			{
				Out.U8(Window.PortraitCell);
			}
			const auto Cursor = [&Out](bool bShown, int16 X, int16 Y)
			{
				Out.U8(bShown ? 1 : 0);
				if (bShown)
				{
					Out.U16(static_cast<uint16>(X));
					Out.U16(static_cast<uint16>(Y));
				}
			};
			Cursor(Frame.bMoreCursor, Frame.MoreCursorX, Frame.MoreCursorY);
			Cursor(Frame.bChoiceCursor, Frame.ChoiceCursorX, Frame.ChoiceCursorY);
			if (Window.OpenTicks != 0)
			{
				return;
			}
			const FMesWindow& Text = *Window.Text;
			const FMesWindow::FState State = Text.State();
			Out.U16(static_cast<uint16>(State.X));
			Out.U16(static_cast<uint16>(State.Line));
			Out.U16(State.Flags);
			Out.U16(static_cast<uint16>(State.TopLine));
			Out.U16(static_cast<uint16>(State.LinesTyped));
			Out.U8(State.BaseSpeed);
			Out.U8(State.Speed);
			Out.U8(State.SavedSpeed);
			Out.U8(State.Reason);
			Out.U8(State.bClosing ? 1 : 0);
			Out.U16(static_cast<uint16>(State.Queued));
			Out.U16(static_cast<uint16>(State.WaitTimer));
			Out.U16(static_cast<uint16>(State.DelayCounter));
			Out.U16(static_cast<uint16>(State.DelayReload));
			Out.U8(static_cast<uint32>(Text.HighlightedRow()));
			for (const int16 Width : State.Widths)
			{
				Out.U16(static_cast<uint16>(Width));
			}
			Out.U8(State.Next);
			for (const uint8 Channel : Text.HighlightColor())
			{
				Out.U8(Channel);
			}
		}

	}

	void DigestFieldCamera(FOracleDigest& Digest, const FFieldCamera& Camera)
	{
		for (const FFieldPoint* Point : {&Camera.Eye, &Camera.Target, &Camera.EyeGoal, &Camera.TargetGoal, &Camera.Shake})
		{
			AddPoint(Digest, *Point);
		}
		Digest.I16(Camera.ViewYaw, Camera.GoalViewYaw);
		Digest.I32(Camera.ViewPitch);
		Digest.I16(Camera.Rotation[1]);
		Digest.I32(Camera.TurningYaw, Camera.YawGoal, Camera.TurnStep);
		Digest.I16(Camera.TurnTicks);
		Digest.I32(Camera.Flags);
		Digest.I16(Camera.Pitch, Camera.DistanceScale);
		Digest.I32(Camera.ProjectionDistance);
		for (const FFieldCameraRamp* Ramp : {&Camera.ZoomRamp, &Camera.PitchRamp, &Camera.ProjectionRamp})
		{
			Digest.I16(Ramp->Ticks);
			Digest.I32(Ramp->Value);
		}
		Digest.I32(Camera.SnapTicks);
		Digest.I16(Camera.Mode);
		Digest.I32(Camera.TargetDivisor, Camera.EyeDivisor, Camera.FollowTicks, Camera.ReturnTicks, Camera.bHeightKept ? 1 : 0);
		Digest.I16(Camera.PathMoves);
		for (const FFieldCameraMove* Move : {&Camera.TargetMove, &Camera.EyeMove})
		{
			Digest.I16(Move->Ticks);
			AddPoint(Digest, Move->At);
		}
		Digest.I16(Camera.bShaking ? 1 : 0, Camera.ShakeTicks, Camera.bShakeStops ? 1 : 0);
		AddPoint(Digest, Camera.ShakeAmplitude);
		AddMatrix(Digest, Camera.LookAt);
		AddMatrix(Digest, Camera.View.View);
	}

	std::vector<uint8> MessageWindowBytes(const FEventContext& Context)
	{
		const FFieldMessages& Messages = Context.Messages;
		FLittleBytes Out;
		for (const FFieldMessageWindow& Window : Messages.Windows)
		{
			AddWindow(Out, Window);
		}
		Out.U16(Messages.OpenMask);
		Out.U16(static_cast<uint16>(Messages.OpenTicks));
		Out.U32(static_cast<uint32>(Context.FrameDelay));
		Out.U32(static_cast<uint32>(Messages.CursorTicks));
		Out.U32(static_cast<uint32>(Messages.CursorFrame));
		for (const FFieldPortraitSlot& Slot : Messages.Portraits)
		{
			Out.U16(static_cast<uint16>(Slot.Character));
			Out.U16(static_cast<uint16>(Slot.Load));
		}
		Out.U32(static_cast<uint32>(Messages.LastPortraitSlot));
		return Out.Bytes;
	}
}
