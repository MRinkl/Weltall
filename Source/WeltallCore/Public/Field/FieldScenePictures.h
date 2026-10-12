#pragma once

#include "Field/FieldScene.h"
#include "Graphics/MdlModel.h"

namespace Weltall
{
	/**
	 * Whether a model's first part is a scene's picture as the developers made them: textured quads that tile a 4:3
	 * rectangle lying flat in the model's xy plane, one beside another, each a rectangle of its own.
	 */
	WELTALLCORE_API bool IsScenePicture(const FMdlPart& Part);

	/**
	 * The scenes the maps stage before a picture: a flat 4:3 model behind the scene's people and gears, the screen's
	 * whole picture for the original's camera (the prologue's in map 4, the excavation site of map 39, the desert of
	 * map 73 where Fei and Bart stand in their Gears, Maison's three stills in map 81, Solaris in map 514). A screen
	 * wider than the original's shows the picture's edges with nothing beyond, and what walks or flies in from there.
	 *
	 * Marks each such picture of the map's scene (FFieldSceneObject::bPicture), for the drawing to stretch it across a
	 * wide view; a map whose model is not such a picture is left as it is.
	 */
	WELTALLCORE_API void MarkScenePictures(int32 Map, FFieldScene& Scene);
}
