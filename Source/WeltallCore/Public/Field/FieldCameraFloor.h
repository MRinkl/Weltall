#pragma once

#include "Field/FieldCamera.h"
#include "Field/FieldWalkmesh.h"
#include "Foundation/Types.h"
#include "Graphics/GteVectors.h"

namespace Weltall
{
	/** The walkmesh's floor under a point, as the field finds it. */
	struct FFieldFloor
	{
		/** The first triangle of the layer that holds the point, or 0 when none does (the field cannot tell them apart). */
		int32 Triangle = 0;
		/** The height of that triangle's plane under the point (y grows downward); 0 when no triangle holds it. */
		int16 Height = 0;
		/** The plane's normal, 4.12, as the height was computed with; zero when no triangle holds the point. */
		FGteLongVector Normal{};
	};

	/**
	 * The floor of a layer under (X, Z): the first triangle whose three edges have the point on
	 * their inner side, tested with the GTE's NCLIP on coordinates packed as its screen points,
	 * and the height of its plane there.
	 */
	WELTALLCORE_API FFieldFloor FindFieldFloor(const FWalkmeshLayer& Layer, int32 X, int32 Z);

	/**
	 * Keeps the eye at or above the camera layer, as the follow camera does unless a script lets it
	 * go below: an eye under the layer's plane is raised to it (and an eye over no triangle of the
	 * layer is kept at or above height 0).
	 */
	WELTALLCORE_API FFieldPoint KeepEyeAboveCameraLayer(const FFieldWalkmesh& Walkmesh, const FFieldPoint& Eye);

	/** Where the follow camera's target may go for a point on the ground, in whole units. */
	struct FFieldCameraAreaPoint
	{
		/** The point lies inside the camera's area; else X and Z are where its way back into the area crosses the area's edge. */
		bool bInside = true;
		int16 X = 0;
		int16 Z = 0;
	};

	/**
	 * Finds whether a point lies in the camera's area: the camera layer's triangles open to the camera
	 * that the way from the nearest point of the camera's bounds rectangle reaches, stepping from
	 * triangle to triangle. A point the way cannot reach is taken back along that way to the edge it
	 * stopped at. The camera layer must have triangles.
	 */
	WELTALLCORE_API FFieldCameraAreaPoint FindFieldCameraAreaPoint(const FFieldWalkmesh& Walkmesh, const std::array<int16, 4>& Bounds, int16 X, int16 Z);
}
