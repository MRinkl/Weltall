#pragma once

#include "Field/FieldMap.h"
#include "Field/FieldRenderState.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/MdlLights.h"
#include "Graphics/MdlPackets.h"
#include "Graphics/Texture.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	/** How the kernel draws a model's polygons: the field's draw modes without fog, as it sets them when the map loads, and Battle's stage's. */
	enum class EFieldDrawMode : uint8
	{
		/** Sorted by the average depth of each polygon's corners. */
		Average,
		/** Lit again with the lights before every frame, then sorted as Average. */
		Relit,
		/** Sorted by the farthest corner (the large floor and wall pieces). */
		Farthest,
		/** Sorted by the nearest corner (the kernel's draw mode 3, which no map asks for; some battle stages' pieces). */
		Nearest,
	};

	/** How a placed model is turned before it is drawn. */
	enum class EFieldMatrixMode : uint8
	{
		/** By its own rotation, under the camera. */
		Placed,
		/** Kept facing the screen: the camera's pitch alone, then its own rotation. */
		FacingScreen,
	};

	/** A placed model of the map: an actor with a model, as the field builds and draws it. */
	struct FFieldSceneObject
	{
		uint16 Actor = 0;
		uint16 Model = 0;
		/** The actor's rotation (Rx Ry Rz), with the low 16 bits of its position as translation. */
		FGteMatrix Placement;
		/** Its placement as the map's data gives it, which the scene's joins were made for. */
		FGteMatrix Built;
		FFieldModelBounds Bounds;
		EFieldDrawMode DrawMode = EFieldDrawMode::Average;
		EFieldMatrixMode MatrixMode = EFieldMatrixMode::Placed;
		/** Neither drawn nor animated. */
		bool bHidden = false;
		/** Drawn into the background ordering table: behind everything else (sky domes). */
		bool bBackground = false;
		/** A scene's painted picture (FieldScenePictures.h): a wide view stretches it across, the rest of the scene moving sideways with it. */
		bool bPicture = false;
		/**
		 * A model attached to another's or riding a gear's joint: Placement is what carries it (the parent's world
		 * block, the joint), under the view without the world's rotation, and Attached its own matrix under that.
		 */
		bool bWithoutWorldRotation = false;
		FGteMatrix Attached;
		/** Drawn whether or not its bounds are on the screen (the scenery's drift can ask for it). */
		bool bNeverCulled = false;
		/**
		 * In fog (the kernel's draw modes 4 and 5): its FT3 and FT4 polygons are depth-cued toward the
		 * fog's far colour and sorted by DrawMode, every other kind sorted by its average depth.
		 */
		bool bFogged = false;
		/** The vertices it is drawn with when a MIMe animation moves them; empty for its model's own. */
		std::span<const FGteVector> MovedVertices;
		/** Its polygons in Polygons, in the order the kernel builds and draws them. */
		uint32 FirstPolygon = 0;
		uint32 PolygonCount = 0;

		/**
		 * Whether it stands as the map's data places it, its vertices its model's own: the scene's joins hold for it.
		 * Its translation counts by its low halves, which are what the drawing reads of it.
		 */
		WELTALLCORE_API bool IsAsBuilt() const;
	};

	/** A polygon of a placed model: its primitive and its packet as built for the model's actor. */
	struct FFieldScenePolygon
	{
		uint16 Object = 0;
		/** The primitive's number in its model's part. */
		uint16 Primitive = 0;
		FMdlPacket Packet;
		/** The texture page and CLUT it samples, its inherited ones resolved; for textured kinds only. */
		FTexturePage Page;
		FClutPosition Clut;
	};

	/**
	 * A vertex of a placed model that is drawn at a point of another model instead of its own place, so that two
	 * pieces of the scenery whose edges the map's data leaves a hair apart meet: by the other model's matrix, at one
	 * of its vertices or on one of its edges.
	 */
	struct FFieldSceneJoin
	{
		uint16 Object = 0;
		uint16 Vertex = 0;
		/** The model it is drawn by, and the point in that model's space. */
		uint16 Anchor = 0;
		std::array<float, 3> Point{};
	};

	/**
	 * A map's scenery as the field builds it when the map loads: every placed model with its
	 * polygons' packets (lit where their kinds and the actor's light mode say), and the lights the
	 * relit ones are lit with again every frame. MIMe models keep their rest pose.
	 */
	struct FFieldScene
	{
		/** The map's models, as loaded. */
		std::vector<FMdlModel> Models;
		std::vector<FFieldSceneObject> Objects;
		std::vector<FFieldScenePolygon> Polygons;
		/** Where the pieces of the scenery are drawn to meet (Field/FieldSceneJoins.h), by object and vertex. */
		std::vector<FFieldSceneJoin> Joins;
		FMdlLights Lights;
		FFieldRenderState State;

		const FMdlPart& PartOf(const FFieldSceneObject& Object) const { return Models[Object.Model].Parts.front(); }
		const FMdlPrimitive& PrimitiveOf(const FFieldScenePolygon& Polygon) const
		{
			return PartOf(Objects[Polygon.Object]).Primitives[Polygon.Primitive];
		}
		/** The join of an object's vertex, if it has one. */
		WELTALLCORE_API const FFieldSceneJoin* JoinOf(uint32 Object, uint32 Vertex) const;
	};

	/** The map's lights as the field sets them: the colors times 8, the third light a copy of the first. */
	WELTALLCORE_API FMdlLights FieldMapLights(const FFieldLights& Lights);

	/**
	 * The GTE's lighting as a map's load leaves it and builds under: the light matrix follows the view of the
	 * camera the load has just reset, which looks from the origin to the origin and is all zeros.
	 */
	WELTALLCORE_API FGte FieldLightingAtLoad(const FMdlLights& Lights);

	/**
	 * Builds the scene of a map. The field builds without ever setting the GTE's ambient light, so
	 * its lit polygons that are never lit again keep whatever light the previous program left; the
	 * scene gives them the map's own. Fails for what no map holds: light modes 1 and 3, matrix modes
	 * 2 and 3, the kernel's alternate draw mode, and a model relit from light data it never stored.
	 */
	WELTALLCORE_API TResult<FFieldScene> BuildFieldScene(const FFieldMap& Map);
}
