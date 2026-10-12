#pragma once

#include "CoreMinimal.h"
#include "Field/FieldFrame.h"
#include "Rendering/FieldSceneRendering.h"

/** Whether the frame's gears fit the mesh's: the same models' revision and layout. */
bool DoGearsFitMesh(const Weltall::FFieldFrame& Latest, const FFieldSceneMesh& Mesh);

/** Every gear polygon's colours this frame, after the scenery's: drawn or not, as the mesh numbers them. */
void AddGearColors(const Weltall::FFieldFrame& Latest, const FFieldSceneMesh& Mesh, FFieldSceneDraw& Draw);

/**
 * The gears' object rows after the identity, each Alpha of the way from the tick before's (Before,
 * when the frames follow each other) unless it was not drawn then or its model jumped; then the row
 * the cloths are drawn by: the view.
 */
void AddGearObjects(const Weltall::FFieldFrame& Latest, const Weltall::FFieldFrame* Before, float Alpha, FFieldSceneDraw& Draw);

/** The cloths' triangles as dynamic corners after the characters' quads, each point Alpha of the way from where the tick before had it. */
void AddGearCloths(const Weltall::FFieldFrame& Latest, const Weltall::FFieldFrame* Before, float Alpha, const FFieldSceneMesh& Mesh, FFieldSceneDraw& Draw);
