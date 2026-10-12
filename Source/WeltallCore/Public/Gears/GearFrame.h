#pragma once

#include "Foundation/Types.h"
#include "Graphics/AffineBlend.h"
#include "Graphics/Gte.h"
#include "Graphics/MdlModel.h"

#include <array>
#include <optional>
#include <vector>

namespace Weltall
{
	struct FGearBonePackets;
	struct FGearCloth;

	/**
	 * A bone with a part, whether or not it is shown: the part's vertices go through the model's root
	 * local matrix times World, and it is lit through the root's world matrix times World.
	 */
	struct FGearFramePart
	{
		uint16 Bone = 0;
		uint16 Part = 0;
		bool bShown = false;
		FGteMatrix World;
		/** 0 none; 1 turned about the up axis toward the camera; more: facing it (the original's billboard modes). */
		int16 Billboard = 0;
	};

	/**
	 * The floor shadow: the sprites' shadow texture on a quad of half-sizes HalfWidth by HalfDepth,
	 * turned by Yaw about the up axis at Position and scaled by Scale (4.12), lying on the floor under
	 * bone 1 and shrinking as the root rises.
	 */
	struct FGearFrameShadow
	{
		bool bShown = false;
		std::array<int32, 3> Position{};
		int16 Yaw = 0;
		int16 Scale = 0;
		int16 HalfWidth = 0;
		int16 HalfDepth = 0;
	};

	/** What a loaded model draws this tick. Parts is sized at the model's load. */
	struct FGearFrameModel
	{
		bool bPresent = false;
		bool bShown = false;
		/** The model's scale (4.12), which billboarded parts take for their size. */
		int16 Scale = 0;
		/** The root's local matrix (its rotation times the scale, its position) and its world matrix (the rotation alone). */
		FGteMatrix RootLocal;
		FGteMatrix RootWorld;
		/** FGearModel::Placement: where the model's own space, which its matrices and cloths are in, lies in the world. */
		std::optional<FAffine> Placement;
		/** Every bone with a part, in bone order. */
		std::vector<FGearFramePart> Parts;
		FGearFrameShadow Shadow;
		/** The MDL model the parts come from, and each bone's part as the load built it (Parts' order). */
		const FMdlModel* Model = nullptr;
		const std::vector<FGearBonePackets>* Packets = nullptr;
		/** The model's cloths as this tick left them (their points and normals; triangles, UVs and colours fixed). */
		const std::vector<FGearCloth>* Cloths = nullptr;
	};

	/**
	 * What the gears' models leave for the drawing each tick, by slot: the drawing pairs a slot's
	 * parts with the previous tick's to interpolate (the root and every bone's world matrix are
	 * continuous; shown bones, parts and the shadow's being there are not). Rebuilt each tick without
	 * allocating.
	 */
	struct FGearFrame
	{
		std::array<FGearFrameModel, 10> Models;
	};
}
