#include "Field/FieldScene.h"

#include "Field/FieldCamera.h"
#include "Field/FieldSceneJoins.h"

#include <algorithm>
#include <string>
#include <utility>

namespace Weltall
{
	namespace
	{
		/** The light mode every lit model of the maps uses; the kernel stores its light data only in it. */
		constexpr int32 StoredLightMode = 2;
		constexpr uint16 AlternateDrawModeFlag = 0x4000;
		constexpr uint16 BackgroundFlag = 0x8000;

		FGteVector Shifted(const FGteVector& Value, int32 Shift)
		{
			return {static_cast<int16>(Value[0] * (1 << Shift)), static_cast<int16>(Value[1] * (1 << Shift)), static_cast<int16>(Value[2] * (1 << Shift))};
		}

		TResult<EFieldDrawMode> DrawModeOf(const FFieldActorDefinition& Actor)
		{
			if ((Actor.Flags & AlternateDrawModeFlag) != 0)
			{
				return MakeError("the kernel's alternate draw mode");
			}
			switch (Actor.ModelDrawMode(false))
			{
			case 1:
				return EFieldDrawMode::Relit;
			case 2:
				return EFieldDrawMode::Farthest;
			default:
				return EFieldDrawMode::Average;
			}
		}

		TResult<FFieldSceneObject> PlaceObject(const FFieldActorDefinition& Actor, const FFieldModelObject& Object)
		{
			if (Actor.MatrixMode() > 1)
			{
				return MakeError("matrix mode " + std::to_string(Actor.MatrixMode()));
			}
			if (Actor.LightMode() != 0 && Actor.LightMode() != StoredLightMode)
			{
				return MakeError("light mode " + std::to_string(Actor.LightMode()));
			}
			TResult<EFieldDrawMode> DrawMode = DrawModeOf(Actor);
			if (!DrawMode)
			{
				return DrawMode.Error();
			}
			FFieldSceneObject Placed;
			Placed.Actor = Object.Actor;
			Placed.Model = Actor.Model;
			Placed.Placement = RotationXyz(Actor.Rotation);
			Placed.Placement.T = {Actor.Position[0], Actor.Position[1], Actor.Position[2]};
			Placed.Built = Placed.Placement;
			Placed.Bounds = Object.Bounds;
			Placed.DrawMode = *DrawMode;
			Placed.MatrixMode = Actor.MatrixMode() == 1 ? EFieldMatrixMode::FacingScreen : EFieldMatrixMode::Placed;
			Placed.bHidden = Actor.IsHidden();
			Placed.bBackground = (Actor.Flags & BackgroundFlag) != 0;
			return Placed;
		}

		/** A relit model reads its flat and untextured polygons' light data back from the part's buffer. */
		bool StoresWhatItRelights(const FMdlPart& Part, const std::vector<FMdlPacket>& Packets)
		{
			for (const FMdlPacket& Packet : Packets)
			{
				if (RelightReadsLightBuffer(Packet.Relight) && Part.LightBufferBytes == 0)
				{
					return false;
				}
			}
			return true;
		}
	}

	FMdlLights FieldMapLights(const FFieldLights& Lights)
	{
		FMdlLights Result;
		for (int32 Light = 0; Light < 3; ++Light)
		{
			const FFieldLight& Source = Lights.Directional[static_cast<std::size_t>(Light == 2 ? 0 : Light)];
			Result.SetDirectional(Light, {Source.Direction[0], Source.Direction[1], Source.Direction[2]}, Shifted(Source.Color, 3));
		}
		Result.SetAmbient(Shifted(Lights.Ambient, 4));
		return Result;
	}

	FGte FieldLightingAtLoad(const FMdlLights& Lights)
	{
		const FGteMatrix ResetView = MulMatrix(FieldLookAt({}, {}), RotationXyz({0, 0, 0}));
		FGte Gte;
		Lights.Load(Gte, ResetView);
		return Gte;
	}

	TResult<FFieldScene> BuildFieldScene(const FFieldMap& Map)
	{
		FFieldScene Scene;
		Scene.Models = Map.Models;
		Scene.Lights = FieldMapLights(Map.Lights);
		const FGte Lighting = FieldLightingAtLoad(Scene.Lights);
		for (const FFieldModelObject& Object : Map.ModelObjects)
		{
			const FFieldActorDefinition& Actor = Map.Actors[Object.Actor];
			const std::string Where = "actor " + std::to_string(Object.Actor) + ": ";
			TResult<FFieldSceneObject> Placed = PlaceObject(Actor, Object);
			if (!Placed)
			{
				return MakeError(Where + Placed.Error().Message);
			}
			const FMdlPart& Part = Scene.Models[Actor.Model].Parts.front();
			TResult<std::vector<FMdlPacket>> Packets = BuildMdlPackets(Part, Actor.LightMode(), Lighting);
			if (!Packets)
			{
				return MakeError(Where + Packets.Error().Message);
			}
			if (Placed->DrawMode == EFieldDrawMode::Relit && !StoresWhatItRelights(Part, *Packets))
			{
				return MakeError(Where + "relit without room for its light data");
			}
			Placed->FirstPolygon = static_cast<uint32>(Scene.Polygons.size());
			Placed->PolygonCount = static_cast<uint32>(Packets->size());
			const uint16 ObjectNumber = static_cast<uint16>(Scene.Objects.size());
			for (std::size_t Index = 0; Index < Packets->size(); ++Index)
			{
				const FMdlPrimitive& Primitive = Part.Primitives[Index];
				FFieldScenePolygon Polygon;
				Polygon.Object = ObjectNumber;
				Polygon.Primitive = static_cast<uint16>(Index);
				Polygon.Packet = (*Packets)[Index];
				if (Primitive.Traits().bTextured)
				{
					const FMdlTextureState Texture = Primitive.Texture(Object.TextureAtBuild);
					Polygon.Page = FTexturePage::FromAttribute(Texture.TexturePage);
					Polygon.Clut = FClutPosition::FromAttribute(Texture.Clut);
				}
				Scene.Polygons.push_back(Polygon);
			}
			Scene.Objects.push_back(*Placed);
		}
		Scene.Joins = BuildFieldSceneJoins(Scene);
		return Scene;
	}

	bool FFieldSceneObject::IsAsBuilt() const
	{
		const auto LowHalves = [](const std::array<int32, 3>& Translation)
		{
			return std::array<int16, 3>{static_cast<int16>(Translation[0]), static_cast<int16>(Translation[1]), static_cast<int16>(Translation[2])};
		};
		return MatrixMode == EFieldMatrixMode::Placed && !bWithoutWorldRotation && MovedVertices.empty() && Placement.M == Built.M
			&& LowHalves(Placement.T) == LowHalves(Built.T);
	}

	const FFieldSceneJoin* FFieldScene::JoinOf(uint32 Object, uint32 Vertex) const
	{
		using FKey = std::pair<uint32, uint32>;
		const auto Found = std::lower_bound(Joins.begin(), Joins.end(), FKey(Object, Vertex),
			[](const FFieldSceneJoin& Join, const FKey& Key) { return FKey(Join.Object, Join.Vertex) < Key; });
		return Found != Joins.end() && Found->Object == Object && Found->Vertex == Vertex ? &*Found : nullptr;
	}
}
