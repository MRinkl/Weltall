#include "Field/FieldCompass.h"

#include "Field/EventContext.h"
#include "Field/FieldActorFacing.h"
#include "Graphics/Gte.h"
#include "Graphics/GteVectors.h"

namespace Weltall
{
	namespace
	{
		/** The needle points a quarter turn on from the facing, turning 0x40 a tick. */
		constexpr int32 NeedleQuarter = 0x400;
		constexpr int32 NeedleSpeed = 0x40;
		constexpr int32 AngleMask = 0xFFF;
		/** The compass's own projection: H, and the screen point its centre lies on. */
		constexpr uint16 CompassProjection = 0x80;
		constexpr int32 CompassCenterX = 266;
		constexpr int32 CompassCenterY = 166;
		/** How far before the eye the compass's ground lies, and the tipped ground's own depth. */
		constexpr int32 CompassNear = 0x80;
		constexpr int32 GroundDepth = 0x1000;
		/** The letters stand up from the ground (a quarter turn about x), each a 16 x 10 sprite at its foot. */
		constexpr int16 Upright = 0x400;
		constexpr int16 LetterHalfWidth = 8;
		constexpr int16 LetterHeight = 10;

		FGteMatrix Translated(const FGteMatrix& Rotation, int32 X, int32 Y, int32 Z)
		{
			FGteMatrix Result = Rotation;
			Result.T = {X, Y, Z};
			return Result;
		}

		/** RotAverage4's screen points of a quad lying on the compass's ground (y 0) under Transform. */
		std::array<std::array<int16, 2>, 4> Project(const FGteMatrix& Transform, const FFieldCompassLayout::FQuad& Quad)
		{
			FGte Gte;
			Gte.Transform = Transform;
			Gte.OffsetX = CompassCenterX << 16;
			Gte.OffsetY = CompassCenterY << 16;
			Gte.ProjectionDistance = CompassProjection;
			std::array<std::array<int16, 2>, 4> Screen{};
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				const FGteProjection Point = Gte.Rtps({Quad.Corners[Corner][0], 0, Quad.Corners[Corner][1]});
				Screen[Corner] = {Point.ScreenX, Point.ScreenY};
			}
			return Screen;
		}

		void Add(FFieldCompassFrame& Frame, const std::array<std::array<int16, 2>, 4>& Corners, const FFieldCompassLayout::FQuad& Quad,
			const FFieldCompassLayout::FTexture& Texture, bool bShadow)
		{
			FFieldCompassQuad& Out = Frame.Quads[static_cast<std::size_t>(Frame.Count++)];
			Out.Corners = Corners;
			Out.Texels = Quad.Texels;
			Out.Texture = Texture;
			Out.bShadow = bShadow;
		}
	}

	void TurnFieldCompass(FEventContext& Context)
	{
		FFieldCompass& Compass = Context.Render.Compass;
		Compass.bDrawn = !Context.bCompassHidden && Context.SettleTicks == 0;
		const std::size_t Followed = Context.Camera.FollowedActor;
		if (Followed >= Context.States.size())
		{
			return;
		}
		const int32 Target = static_cast<int16>(Context.States[Followed].Facing + Context.Camera.ViewYaw + Context.ViewTurn + NeedleQuarter);
		Compass.Needle = static_cast<int16>(Context.SettleTicks != 0 ? Target & AngleMask : StepFieldAngle(Compass.Needle, Target, NeedleSpeed));
	}

	void DrawFieldCompass(const FFieldCompass& Compass, const FFieldCompassLayout& Layout, const FGteMatrix& Rotation, const FGteMatrix& Pitch,
		FFieldCompassFrame& Frame)
	{
		Frame.Count = 0;
		if (!Compass.bDrawn)
		{
			return;
		}
		const FGteMatrix Near = Translated(FGteMatrix::Identity(), 0, 0, CompassNear);
		// The needle on the ground tipped by the camera's pitch alone.
		const FGteMatrix NeedleTurn = MulMatrix(Pitch, RotationXyz({0, Compass.Needle, 0}));
		const FGteMatrix NeedlePlace = CompMatrix(Near, Translated(NeedleTurn, 0, 0, GroundDepth));
		// The dial, the letters and the shadow on the ground turned as the world is.
		const FGteMatrix Ground = CompMatrix(Near, Translated(Rotation, 0, 0, GroundDepth));
		const FGteMatrix Standing = RotationXyz({Upright, 0, 0});
		for (int32 Quad = FFieldCompassLayout::ShadowQuads; Quad-- > 0;)
		{
			const FFieldCompassLayout::FQuad& Shadow = Layout.Shadow[static_cast<std::size_t>(Quad)];
			Add(Frame, Project(Ground, Shadow), Shadow, Layout.ShadowTexture, true);
		}
		for (int32 Quad = FFieldCompassLayout::DialQuads; Quad-- > 0;)
		{
			const FFieldCompassLayout::FQuad& Dial = Layout.Dial[static_cast<std::size_t>(Quad)];
			Add(Frame, Project(Ground, Dial), Dial, Layout.Textures[0], false);
		}
		for (int32 Letter = FFieldCompassLayout::Letters; Letter-- > 0;)
		{
			const std::array<int16, 2>& Place = Layout.LetterPlaces[static_cast<std::size_t>(Letter)];
			FGteMatrix At = CompMatrix(Ground, Translated(FGteMatrix::Identity(), Place[0], 0, Place[1]));
			At.M = Standing.M;
			const FFieldCompassLayout::FQuad& Quad = Layout.LetterQuads[static_cast<std::size_t>(Letter)];
			const std::array<std::array<int16, 2>, 4> Foot = Project(At, Quad);
			// Only the foot's projection places the letter: a sprite at the middle of its bottom edge.
			const int16 Middle = static_cast<int16>((Foot[3][0] + Foot[2][0]) / 2);
			const int16 Bottom = Foot[3][1];
			const int16 Left = static_cast<int16>(Middle - LetterHalfWidth);
			const int16 Right = static_cast<int16>(Middle + LetterHalfWidth);
			const int16 Top = static_cast<int16>(Bottom - LetterHeight);
			Add(Frame, {{{Left, Top}, {Right, Top}, {Left, Bottom}, {Right, Bottom}}}, Quad, Layout.Textures[1], false);
		}
		Add(Frame, Project(NeedlePlace, Layout.Needle), Layout.Needle, Layout.Textures[1], false);
	}

	std::array<uint16, 16 * FFieldCompassLayout::Directions> FieldCompassPalette(const FFieldCompassLayout& Layout, const std::array<uint16, 16>& Colors,
		uint8 ForbiddenDirections)
	{
		std::array<uint16, 16 * FFieldCompassLayout::Directions> Palette{};
		for (std::size_t Direction = 0; Direction < FFieldCompassLayout::Directions; ++Direction)
		{
			if ((ForbiddenDirections & Layout.DirectionBits[Direction]) != 0)
			{
				continue;
			}
			for (std::size_t Color = 0; Color < Colors.size(); ++Color)
			{
				Palette[Direction * 16 + Color] = Colors[Color];
			}
		}
		return Palette;
	}
}
