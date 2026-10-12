#pragma once

#include "CoreMinimal.h"
#include "Rendering/FieldSceneRendering.h"
#include "ScenePolygonOrder.h"

namespace Weltall
{
	struct FBattleFrameBlend;
	struct FBattleObjectLink;
	struct FBattleShowFrame;
}

/**
 * What a link of the stage's and the 3D objects' drawing draws, as the field scene's passes take it: a part's polygon,
 * an object's floor shadow, a cloth's triangle or a trail's quad, its corners in view space as the GTE drew them (a
 * trail's on the screen at the projection's distance) and moved as Blend draws them between frames (none: as they are),
 * textured from VRAM as its packet is, into Draw's dynamic polygons and Items. Under a depth sorting the 3D objects are
 * sorted as models and the stage is drawn over them in the order, sorted by depth beside them with bSceneryByDepth.
 */
void AddBattleObjectLink(const Weltall::FBattleShowFrame& Frame, const Weltall::FBattleFrameBlend* Blend, const Weltall::FBattleObjectLink& Link, bool bSceneryByDepth,
	FFieldSceneDraw& Draw, TArray<FScenePolygonItem>& Items);
