#include "Battling/BattlingDrawing.h"

#include "Battling/BattlingFight.h"
#include "Graphics/GteVectors.h"

namespace Weltall
{
	namespace
	{
		/** The winner screen's display: twice the bout's columns, the same lines. */
		constexpr int32 WinnerColumns = 2 * FBattlingFrame::ScreenWidth;
		constexpr uint16 WinnerClipRows = FBattlingFrame::ScreenHeight - 1;
		/** Its environment mapping: the chrome gears' reflections move less than in the bout. */
		constexpr FMdlEnvironmentMapping WinnerEnvironment{5, 4, 0x40, 0x40};

		/** The point the shadow is cast from, in the gear's own space: (-8, -8, -16) normalized, then 16 times as far. */
		constexpr FGteLongVector ShadowLightDirection{-8, -8, -16};
		constexpr int32 ShadowLightShift = 4;
		constexpr FDrawColor ShadowColour{0x30, 0x30, 0x40};
		/** The shadow's scene has an ordering table of its own, drawn before the gear's whole table: its polygons go into its entry 1. */
		constexpr uint16 ShadowTableStart = 0x1000;
		constexpr uint16 ShadowBucket = ShadowTableStart + 1;

		/** A corner cast from Light onto the ground (y = 0) of the gear's space, divided as the shadow's own transform divides. */
		bool Cast(const FGteLongVector& Point, const FGteLongVector& Light, FGteVector& Out)
		{
			const int32 Down = Light[1] - Point[1];
			if (Down == 0)
			{
				return false;
			}
			const auto Across = [&](std::size_t Axis) {
				const int32 Scaled = static_cast<int32>(static_cast<int64>(Point[Axis]) * Light[1] - static_cast<int64>(Light[Axis]) * Point[1]);
				return static_cast<int16>(Scaled / Down);
			};
			Out = {Across(0), 0, Across(2)};
			return true;
		}
	}

	void FBattlingDrawing::DrawWinner(const FBattlingFight& Fight, FBattlingFrame& Frame)
	{
		const uint32 Side = Fight.WinnerSide == 0 ? 0 : 1;
		Frame.Pass = Fight.Pass;
		Frame.Columns = WinnerColumns;
		Frame.PictureBlurs = 0;
		Frame.bTrails = false;
		Frame.Clear = {0, 0, 0};
		// The text is linked after both scenes, so drawn over them, the last letter printed first.
		Frame.Screen.clear();
		Frame.TextButtons.clear();
		for (auto Letter = Fight.FrameLabels.rbegin(); Letter != Fight.FrameLabels.rend(); ++Letter)
		{
			Frame.Screen.push_back(MakeLabelPrimitive(*Letter, Textures.FontPage, Textures.FontClut));
		}

		// The gear's turn: its pitch and the spin's yaw under the display's aspect, at the screen's camera height and distance.
		FGteMatrix Turn = MulMatrix(Fight.DisplayAspect(), RotationXyz({static_cast<int16>(Fight.WinnerPitch), Fight.WinnerYaw, 0}));
		Turn.T = {0, Fight.WinnerCameraHeight, Fight.WinnerCameraDistance};
		FGte Gte;
		Gte.Transform = Turn;
		Gte.OffsetX = (WinnerColumns / 2) << 16;
		Gte.OffsetY = (FBattlingFrame::ScreenHeight / 2) << 16;
		Gte.ProjectionDistance = Fight.ProjectionDistance;

		FWorldMapFrame& Scene = Frame.Scene;
		Scene.Tick = Fight.Pass;
		Scene.ScreenCenterX = WinnerColumns / 2;
		Scene.ScreenCenterY = FBattlingFrame::ScreenHeight / 2;
		Scene.ProjectionDistance = Gte.ProjectionDistance;
		Scene.Camera = Turn;
		Scene.bGearsUnderGround = true;
		Scene.TargetX = 0;
		Scene.TargetZ = 0;
		Scene.Yaw = 0;
		Scene.WorldUnitsX = 0;
		Scene.WorldUnitsZ = 0;
		Builder.Begin(Scene);
		DrawWinnerShadow(Fight, Side, Gte);
		// Nothing sets the lights on this screen: the gear is lit as the bout's last frame left them, its root at the turn's origin.
		FGearView Gear;
		Gear.Gte = Gte;
		Gear.Lights = LightsLeft;
		Gear.ClipColumns = WinnerColumns;
		Gear.Environment = WinnerEnvironment;
		DrawGear(Fight, Side, Gear);
		Builder.Finish();
	}

	void FBattlingDrawing::DrawWinnerShadow(const FBattlingFight& Fight, uint32 Side, const FGte& Gte)
	{
		const FBattlingSkeleton& Skeleton = Fight.Fighters[Side].Skeleton;
		FGteLongVector Light = VectorNormal(ShadowLightDirection);
		for (int32& Axis : Light)
		{
			Axis <<= ShadowLightShift;
		}
		Builder.Anchor(EWorldMapAnchorKind::Shadow, Side, Gte.Transform);

		// The shadow's tree is built mirrored: a joint before its children, the children in the reverse of the gear's order.
		const std::size_t Count = Skeleton.Joints.size();
		std::vector<std::size_t> Stack;
		const auto PushChildren = [&](int32 Parent) {
			for (std::size_t Joint = 0; Joint < Count; ++Joint)
			{
				if (Skeleton.Joints[Joint].Parent == Parent || (Parent < 0 && Skeleton.Joints[Joint].Parent < 0))
				{
					Stack.push_back(Joint);
				}
			}
		};
		PushChildren(-1);
		while (!Stack.empty())
		{
			const std::size_t Joint = Stack.back();
			Stack.pop_back();
			PushChildren(static_cast<int32>(Joint));
			const FBattlingJoint& Posed = Skeleton.Joints[Joint];
			if (Posed.Part < 0 || Posed.bHidden || static_cast<std::size_t>(Posed.Part) >= Skeleton.Model.Parts.size())
			{
				continue;
			}
			// Each corner where the part's matrix in the gear puts it, cast from the light; one the cast cannot place (level with the
			// light, far above any gear) stays at the ground's origin.
			const FMdlPart& Part = Skeleton.Model.Parts[static_cast<std::size_t>(Posed.Part)];
			CastCorners.assign(Part.Vertices.size(), FGteVector{});
			Projected.resize(Part.Vertices.size());
			for (std::size_t Vertex = 0; Vertex < Part.Vertices.size(); ++Vertex)
			{
				Cast(RotTrans(Posed.World, Part.Vertices[Vertex]), Light, CastCorners[Vertex]);
				Projected[Vertex] = Gte.Rtps(CastCorners[Vertex]);
			}
			// Every polygon of the part as a flat one, kept when it faces the eye and has a corner across the screen and one down it;
			// a view of any width keeps those beyond the original's screen too.
			for (const FMdlPrimitive& Primitive : Part.Primitives)
			{
				const std::size_t Corners = Primitive.Traits().Corners;
				const FGteProjection& A = Projected[Primitive.Vertices[0]];
				const FGteProjection& B = Projected[Primitive.Vertices[1]];
				const FGteProjection& C = Projected[Primitive.Vertices[2]];
				if (Nclip(A.ScreenX, A.ScreenY, B.ScreenX, B.ScreenY, C.ScreenX, C.ScreenY) <= 0)
				{
					continue;
				}
				bool bAcross = false;
				bool bDown = false;
				for (std::size_t Corner = 0; Corner < Corners; ++Corner)
				{
					const FGteProjection& At = Projected[Primitive.Vertices[Corner]];
					bAcross = bAcross || static_cast<uint16>(At.ScreenX) < WinnerColumns;
					bDown = bDown || static_cast<uint16>(At.ScreenY) < WinnerClipRows;
				}
				if (Culling == EBattlingCulling::Original && (!bAcross || !bDown))
				{
					continue;
				}
				FWorldMapPolygon& Polygon = Builder.Add(ShadowBucket);
				Polygon.Layer = EWorldMapLayer::Shadow;
				Polygon.CornerCount = static_cast<uint8>(Corners);
				Polygon.Colors.fill(ShadowColour);
				for (std::size_t Corner = 0; Corner < Corners; ++Corner)
				{
					const std::size_t Vertex = Primitive.Vertices[Corner];
					Polygon.View[Corner] = ViewPoint(Gte.Transform, CastCorners[Vertex]);
					Polygon.ScreenX[Corner] = Projected[Vertex].ScreenX;
					Polygon.ScreenY[Corner] = Projected[Vertex].ScreenY;
				}
			}
		}
	}
}
