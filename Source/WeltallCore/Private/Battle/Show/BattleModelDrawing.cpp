#include "Battle/Show/BattleModelDrawing.h"

#include "BattleShowDrawing.h"
#include "Graphics/GteVectors.h"
#include "Graphics/SpriteFile.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		using namespace BattleShowDrawing;

		constexpr uint32 ScreenSpace = 0x1000000;
		constexpr uint32 FarEntry = 0x2000000;
		constexpr uint32 ScreenCentred = 0x80000000;
		/** A model in the far entry: its depths shifted away and this bias alone. */
		constexpr int32 FarShift = 16;
		constexpr int32 FarBias = 0xFEC;
		constexpr int32 NearestEntry = 5;
		/** The shards' drawers' blend keys: a set's number, marked. */
		constexpr uint32 ShardKeys = 0x78000000;
		/** The middle of the screen, which a sprite can ask its model to be projected about. */
		constexpr int32 ScreenCentreX = 0xA0;
		constexpr int32 ScreenCentreY = 0x70;
		/** A model sprite's own light: the ambient level, and the one light's row of directions before its turn. */
		constexpr int32 OwnAmbient = 0x20 * 16;
		constexpr int16 LightAxis = 0x1000;
		/** A triangle is dropped when a corner's x left the GTE's screen range, a quad when the depth cue's sum overflowed below. */
		constexpr uint32 TriangleDropFlag = GteFlag::ScreenXSaturated;
		constexpr uint32 QuadDropFlag = 0x8000;
	}

	bool FBattleModelDrawing::Draw(const FSprite& Sprite, const FGteMatrix& Own, uint32 Key, bool bOwnRotation)
	{
		if (!Model.Read(*Sprite.ModelFile, Sprite.Model))
		{
			return false;
		}
		FGteMatrix Matrix = Own;
		const bool bOwnLight = !bOwnRotation && (Sprite.BuildFlags & SpriteBuildFlags::OwnLight) != 0;
		const FGteMatrix LightBefore = Gte->LightMatrix;
		const FGteMatrix ColorsBefore = Gte->ColorMatrix;
		const std::array<int32, 3> BackBefore = Gte->BackColor;
		if (StageLight && !bOwnLight)
		{
			Gte->LightMatrix = MulMatrix(StageLight->Direction, Own);
			Gte->ColorMatrix = StageLight->Colours;
			Gte->BackColor = StageLight->Ambient;
		}
		const int32 OffsetX = Gte->OffsetX;
		const int32 OffsetY = Gte->OffsetY;
		if (bOwnRotation)
		{
			// The sprite's own rotation at its place in the view.
			const FGteLongVector At = ApplyMatrix(*View, Place(Sprite));
			Matrix.T = {View->T[0] + At[0], View->T[1] + At[1], View->T[2] + At[2]};
		}
		else
		{
			if (bOwnLight)
			{
				// One light of the sprite's colour, shining along all three axes turned by its angles and the model's
				// rotation; the GTE keeps it for whatever is lit next.
				FGteMatrix Axes{};
				Axes.M[0] = {LightAxis, LightAxis, LightAxis};
				Gte->LightMatrix = MulMatrix(Axes, MulMatrix(RotationXyz(Sprite.LightAngles), Matrix));
				Gte->ColorMatrix = FGteMatrix{};
				for (std::size_t Row = 0; Row < 3; ++Row)
				{
					Gte->ColorMatrix.M[Row][0] = Sprite.LightColor[Row];
				}
				Gte->BackColor = {OwnAmbient, OwnAmbient, OwnAmbient};
			}
			if ((Sprite.DrawFlags & ScreenSpace) == 0)
			{
				Matrix = CompMatrix(*View, Matrix);
			}
			if ((Sprite.DrawFlags & ScreenCentred) != 0)
			{
				Gte->OffsetX = ScreenCentreX << 16;
				Gte->OffsetY = ScreenCentreY << 16;
			}
		}
		Gte->Transform = Matrix;
		const bool bFar = (Sprite.DrawFlags & FarEntry) != 0;
		const bool bDrawn = Link(Sprite, Key, bFar ? FarShift : EntryShift, bFar ? FarBias : int32{Sprite.OtBias});
		Gte->OffsetX = OffsetX;
		Gte->OffsetY = OffsetY;
		if (StageLight && !bOwnLight)
		{
			Gte->LightMatrix = LightBefore;
			Gte->ColorMatrix = ColorsBefore;
			Gte->BackColor = BackBefore;
		}
		return bDrawn;
	}

	bool FBattleModelDrawing::Link(const FSprite& Sprite, uint32 Key, int32 Shift, int32 Bias)
	{
		const uint8 Blend = static_cast<uint8>(Sprite.DrawFlags >> SpriteDrawFlags::BlendShift & 7);
		const uint16 Number = static_cast<uint16>(Frame->Models.size());
		Frame->Models.push_back({Gte->Transform, Gte->OffsetX >> 16, Gte->OffsetY >> 16, static_cast<uint16>(Frame->Drawers.size())});
		Frame->Drawers.push_back({Key, EBattleShowDrawer::Model, Place(Sprite)});
		// The lit kinds take their colours from the lights whether they are linked or not: the buffer's packets keep them.
		// The lights are the GTE's as they stand: a model without its own takes what was lit last, before the battle even.
		if (std::any_of(Model.Primitives.begin(), Model.Primitives.end(), [](const FBattleTmdPrimitive& Primitive) { return Primitive.LitColors != 0; }))
		{
			FBattleModelLights* Kept = LightsOf(Sprite.ModelPackets.Build);
			if (Kept == nullptr)
			{
				Kept = &Lit.emplace_back();
				Kept->Build = Sprite.ModelPackets.Build;
			}
			Kept->Buffers[static_cast<std::size_t>(Buffer & 1)] = {true, false, Gte->LightMatrix, Gte->ColorMatrix, Gte->BackColor};
		}
		const std::size_t FirstPolygon = Frame->ModelPolygons.size();
		for (std::size_t Index = 0; Index < Model.Primitives.size(); ++Index)
		{
			const FBattleTmdPrimitive& Primitive = Model.Primitives[Index];
			// The corners through the GTE: the first three together, a quad's fourth only when the three face the eye.
			std::array<FGteProjection, 4> Corners;
			uint32 Flag = 0;
			for (std::size_t Corner = 0; Corner < 3; ++Corner)
			{
				Corners[Corner] = Gte->Rtps(Model.Vertices[Primitive.Vertices[Corner]]);
				Flag |= Corners[Corner].Flag;
			}
			if (Nclip(Corners[0].ScreenX, Corners[0].ScreenY, Corners[1].ScreenX, Corners[1].ScreenY, Corners[2].ScreenX, Corners[2].ScreenY) < 1)
			{
				continue;
			}
			int32 Depth = 0;
			if (Primitive.CornerCount == 4)
			{
				Corners[3] = Gte->Rtps(Model.Vertices[Primitive.Vertices[3]]);
				Flag |= Corners[3].Flag;
				if ((Flag & QuadDropFlag) != 0)
				{
					continue;
				}
				Depth = Gte->AverageDepth4(Corners[0].ScreenZ, Corners[1].ScreenZ, Corners[2].ScreenZ, Corners[3].ScreenZ);
			}
			else
			{
				if ((Flag & TriangleDropFlag) != 0)
				{
					continue;
				}
				Depth = Gte->AverageDepth3(Corners[0].ScreenZ, Corners[1].ScreenZ, Corners[2].ScreenZ);
			}
			const int32 Entry = std::max((Depth >> Shift) + Bias, NearestEntry);
			if (Entry > static_cast<int32>(LastEntry))
			{
				continue;
			}
			FBattleShowModelPolygon& Polygon = Frame->ModelPolygons.emplace_back();
			Polygon.Model = Number;
			Polygon.Primitive = static_cast<uint16>(Index);
			Polygon.CornerCount = Primitive.CornerCount;
			Polygon.bTextured = Primitive.bTextured;
			Polygon.bGouraud = Primitive.bGouraud;
			for (std::size_t Corner = 0; Corner < Primitive.CornerCount; ++Corner)
			{
				Polygon.Corners[Corner] = Model.Vertices[Primitive.Vertices[Corner]];
				Polygon.ColorCodes[Corner] = BattleTmdColorCode(Primitive, Corner, Sprite.ModelPackets);
				if (Corner < Primitive.LitColors)
				{
					// The lights' colour for the corner's normal in place of the packet's, under its code.
					const FGteColor Shade = Gte->NormalColor(Model.Normals[Primitive.Normals[Corner]]);
					Polygon.ColorCodes[Corner] = (Polygon.ColorCodes[Corner] & 0xFF000000u) | Shade.R | static_cast<uint32>(Shade.G) << 8 | static_cast<uint32>(Shade.B) << 16;
				}
			}
			Polygon.Uvs = Primitive.Uvs;
			Polygon.Clut = Primitive.Clut;
			Polygon.TexturePage = Primitive.TexturePage;
			// An untextured polygon of a sprite that blends is linked before a drawing mode of the sprite's blend mode.
			Polygon.Blend = !Primitive.bTextured && Blend != 0 ? static_cast<int8>((Blend - 1) & 3) : int8{-1};
			Links->push_back({EBattleShowLink::ModelPolygon, static_cast<uint32>(Frame->ModelPolygons.size() - 1), Entry});
		}
		Drawn->push_back({&Sprite, Sprite.ModelPackets.Build, FirstPolygon, Frame->ModelPolygons.size() - FirstPolygon});
		return true;
	}

	bool FBattleModelDrawing::Retint(const FSprite& Sprite)
	{
		// The tint writes both buffers' packets, the ones the frame linked before the sprite's step among them: the GPU
		// draws them tinted, a lit textured polygon's light replaced by the tint.
		if (FBattleModelLights* Kept = LightsOf(Sprite.ModelPackets.Build))
		{
			Kept->Buffers[0].bTinted = Kept->Buffers[1].bTinted = true;
		}
		for (const FBattleDrawnModel& Linked : *Drawn)
		{
			if (Linked.Sprite != &Sprite || Linked.Build != Sprite.ModelPackets.Build || Sprite.ModelFile == nullptr)
			{
				continue;
			}
			if (!Model.Read(*Sprite.ModelFile, Sprite.Model))
			{
				return false;
			}
			for (std::size_t Index = Linked.FirstPolygon; Index < Linked.FirstPolygon + Linked.PolygonCount; ++Index)
			{
				FBattleShowModelPolygon& Polygon = Frame->ModelPolygons[Index];
				if (Polygon.Primitive >= Model.Primitives.size())
				{
					return false;
				}
				const FBattleTmdPrimitive& Primitive = Model.Primitives[Polygon.Primitive];
				if (!Primitive.bTinted)
				{
					continue;
				}
				for (std::size_t Corner = 0; Corner < Primitive.CornerCount; ++Corner)
				{
					Polygon.ColorCodes[Corner] = BattleTmdColorCode(Primitive, Corner, Sprite.ModelPackets);
				}
			}
		}
		return true;
	}

	uint32 FBattleModelDrawing::PacketColor(const FSprite& Sprite, const FBattleTmdPrimitive& Primitive, const std::vector<FGteVector>& Normals, std::size_t Corner,
		int32 InBuffer) const
	{
		const uint32 Built = BattleTmdColorCode(Primitive, Corner, Sprite.ModelPackets);
		if (Corner >= Primitive.LitColors)
		{
			return Built;
		}
		for (const FBattleModelLights& Kept : Lit)
		{
			const FBattleModelLights::FBuffer& State = Kept.Buffers[static_cast<std::size_t>(InBuffer & 1)];
			if (Kept.Build != Sprite.ModelPackets.Build || !State.bDrawn || (State.bTinted && Primitive.bTinted))
			{
				continue;
			}
			FGte Lights;
			Lights.LightMatrix = State.Light;
			Lights.ColorMatrix = State.Colors;
			Lights.BackColor = State.Back;
			const FGteColor Color = Lights.NormalColor(Normals[Primitive.Normals[Corner]]);
			return (Built & 0xFF000000u) | Color.R | static_cast<uint32>(Color.G) << 8 | static_cast<uint32>(Color.B) << 16;
		}
		return Built;
	}

	void FBattleModelDrawing::TakePackets(const FSprite& Sprite, const FBattleTmdModel& From, std::size_t Number, FBattleShard& Shard) const
	{
		const FBattleTmdPrimitive& Packet = From.Primitives[Number];
		Shard.CornerCount = Packet.CornerCount;
		Shard.bTextured = Packet.bTextured;
		Shard.bGouraud = Packet.bGouraud;
		Shard.Uvs = Packet.Uvs;
		Shard.Clut = Packet.Clut;
		Shard.TexturePage = Packet.TexturePage;
		for (std::size_t InBuffer = 0; InBuffer < Shard.ColorCodes.size(); ++InBuffer)
		{
			for (std::size_t Corner = 0; Corner < Packet.CornerCount; ++Corner)
			{
				Shard.ColorCodes[InBuffer][Corner] = PacketColor(Sprite, Packet, From.Normals, Corner, static_cast<int32>(InBuffer));
			}
		}
	}

	void FBattleModelDrawing::DrawShards(const FBattleShards& Set)
	{
		const FGteMatrix Base = CompMatrix(*View, Set.Matrix);
		const uint16 Drawer = static_cast<uint16>(Frame->Drawers.size());
		Frame->Drawers.push_back({ShardKeys | Set.Serial, EBattleShowDrawer::Model, {}});
		const std::size_t InBuffer = static_cast<std::size_t>(Buffer & 1);
		for (std::size_t Number = 0; Number < Set.Shards.size(); ++Number)
		{
			const FBattleShard& Shard = Set.Shards[Number];
			FGteMatrix Local = RotationXyz(Shard.Rotation);
			Local.T = {static_cast<int16>(Shard.Place[0] >> 16), static_cast<int16>(Shard.Place[1] >> 16), static_cast<int16>(Shard.Place[2] >> 16)};
			FBattleShowModelPolygon& Polygon = Frame->ModelPolygons.emplace_back();
			Polygon.Model = static_cast<uint16>(Frame->Models.size());
			Polygon.Primitive = static_cast<uint16>(Number);
			Polygon.CornerCount = Shard.CornerCount;
			Polygon.bTextured = Shard.bTextured;
			Polygon.bGouraud = Shard.bGouraud;
			Polygon.Corners = Shard.Corners;
			Polygon.ColorCodes = Shard.ColorCodes[InBuffer];
			Polygon.Uvs = Shard.Uvs;
			Polygon.Clut = Shard.Clut;
			Polygon.TexturePage = Shard.TexturePage;
			Frame->Models.push_back({CompMatrix(Base, Local), Gte->OffsetX >> 16, Gte->OffsetY >> 16, Drawer});
			Links->push_back({EBattleShowLink::ModelPolygon, static_cast<uint32>(Frame->ModelPolygons.size() - 1), 0});
		}
	}

	void FBattleModelDrawing::Reset()
	{
		Buffer = 0;
		Lit.clear();
	}

	FBattleModelLights* FBattleModelDrawing::LightsOf(uint32 Build)
	{
		for (FBattleModelLights& Kept : Lit)
		{
			if (Kept.Build == Build)
			{
				return &Kept;
			}
		}
		return nullptr;
	}
}
