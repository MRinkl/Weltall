#pragma once

#include "CoreMinimal.h"
#include "Field/FieldCamera.h"
#include "Field/FieldMap.h"
#include "Field/FieldScene.h"
#include "Field/FieldSceneFrame.h"
#include "Graphics/Vram.h"
#include "Rendering/FieldSceneRendering.h"
#include "Screens/GameScreen.h"

#include <optional>

class FFieldScreenFrame;
class FVramTexture;

namespace Weltall
{
	class FIso9660;
}

/**
 * Where a field scenery screen's camera looks: the follow camera at rest on a point of the map, with
 * the settings a map's scripts give it (the field's start values unless set), or a script's camera.
 */
struct FFieldViewpoint
{
	/**
	 * An entrance of the map: where the party arrives by it gives the point and its layer, and the
	 * camera's direction unless the entrance keeps the previous map's (then YawStep's).
	 */
	std::optional<int32> Entrance;
	int32 X = 0;
	int32 Z = 0;
	/** The walkmesh layer whose floor under (X, Z) the point stands on. */
	int32 Layer = 0;
	/** The point's height given outright, instead of the layer's floor. */
	std::optional<int32> Y;
	/** The camera's yaw in 45-degree steps, 0 to 7; maps start at 4. */
	int32 YawStep = 4;
	/** Degrees the eye sits above the target. */
	int16 Pitch = 30;
	/** H, the projection distance. */
	int32 ProjectionDistance = 512;
	/** The eye's distance, 4096 = 1.0. */
	int16 DistanceScale = 0x1000;
	/**
	 * A camera a script places: the eye outright, in whole units, looking at the point as given
	 * (without the follow's lift); the yaw, pitch and zoom then play no part.
	 */
	std::optional<Weltall::FFieldPoint> Eye;
};

/**
 * A field map's scenery alone, seen by the follow camera at rest on a point, as the field draws it:
 * the map stands still and its scripts do not run.
 */
class WELTALL_API FFieldSceneryScreen final : public IGameScreen
{
public:
	/**
	 * Map Number (0 to 729) of a disc, with the render state its scripts set (the field's start
	 * state unless given); null, with the reason logged, when it cannot be shown.
	 */
	static TSharedPtr<FFieldSceneryScreen> Open(const Weltall::FIso9660& Disc, int32 Number, const FFieldViewpoint& Viewpoint, const FFieldRenderOptions& Options,
		const Weltall::FFieldRenderState& State = {});

	FFieldSceneryScreen(Weltall::FFieldMap InMap, Weltall::FFieldScene InScene, const Weltall::FFieldCamera& InCamera, const FFieldRenderOptions& InOptions);
	virtual ~FFieldSceneryScreen() override;

	virtual void Update(double DeltaSeconds, Weltall::FPadState Held) override;
	virtual TSharedRef<const IScreenFrame> CaptureFrame() const override;

	const Weltall::FFieldCamera& Camera() const { return FieldCamera; }

private:
	Weltall::FFieldMap Map;
	Weltall::FFieldScene Scene;
	Weltall::FFieldCamera FieldCamera;
	FFieldRenderOptions Options;
	Weltall::FVram Vram;
	TSharedRef<FVramTexture> VramUnits;
	TSharedRef<const FFieldSceneMesh> Mesh;
	mutable Weltall::FFieldSceneFrame Frame;
	/** Frames handed out; one the rendering thread no longer holds is filled again. */
	mutable TArray<TSharedRef<FFieldScreenFrame>> Frames;
};
