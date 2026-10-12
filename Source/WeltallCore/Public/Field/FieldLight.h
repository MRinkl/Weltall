#pragma once

#include "Field/FieldScene.h"
#include "Graphics/GteMatrix.h"

#include <optional>

namespace Weltall
{
	class FGearModels;
	struct FEventContext;
	struct FFieldGearLights;

	/**
	 * How a tick's drawing places a record's model: its matrix mode and its placement, a script's extra turn
	 * composed, a model attached to another placed by its world block. bDriftDrawsAll: the scenery's drift keeps
	 * the models that are neither turned nor attached on screen.
	 */
	WELTALLCORE_API void PlaceFieldSceneObject(const FEventContext& Context, FFieldSceneObject& Object, bool bDriftDrawsAll);

	/**
	 * The light matrix a tick's drawing of the models loads last: the map's lights turned by the matrix of the
	 * last record whose model is relit, drawn or not. None when no record's is.
	 */
	WELTALLCORE_API std::optional<FGteMatrix> FieldModelsLight(const FEventContext& Context, const FMdlLights& Lights);

	/**
	 * The light matrix a tick's drawing of the gears loads last: the gears' lights turned by the root of the last
	 * shown model and by its last shown part. None when no part is drawn.
	 */
	WELTALLCORE_API std::optional<FGteMatrix> FieldGearsLight(const FGearModels& Models, const FFieldGearLights& Lights);

	/** The colour matrix and the back colour a tick's drawing of the gears loads, whatever it shows: the gears' lights' colours and ambient. */
	WELTALLCORE_API void LoadFieldGearColors(const FFieldGearLights& Lights, FGte& Gte);
}
