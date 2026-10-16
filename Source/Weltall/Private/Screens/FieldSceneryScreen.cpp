#include "Screens/FieldSceneryScreen.h"

#include "Field/EventScripts.h"
#include "Field/FieldCameraFloor.h"
#include "FieldScreenFrame.h"
#include "Media/GameFileIndex.h"
#include "Media/Iso9660.h"
#include "Media/PsxExecutable.h"
#include "Rendering/SharedRenderResource.h"
#include "Rendering/VramTexture.h"
#include "Rendering/TextureModSources.h"
#include "WeltallLog.h"

namespace
{
	Weltall::FVram MapVram(const Weltall::FFieldMap& Map)
	{
		Weltall::FVram Vram;
		Weltall::UploadFieldTextures(Map, Vram, false);
		return Vram;
	}
}

TSharedPtr<FFieldSceneryScreen> FFieldSceneryScreen::Open(const Weltall::FIso9660& Disc, int32 Number, const FFieldViewpoint& Viewpoint, const FFieldRenderOptions& Options,
	const Weltall::FFieldRenderState& State)
{
	const auto Executable = Weltall::ReadBootExecutable(Disc);
	const auto Index = Executable ? Weltall::FGameFileIndex::Read(*Executable) : Weltall::TResult<Weltall::FGameFileIndex>(Executable.Error());
	if (!Index)
	{
		UE_LOG(LogWeltall, Error, TEXT("The disc's file index cannot be read: %hs"), Index.Error().Message.c_str());
		return nullptr;
	}
	auto Map = Weltall::LoadFieldMap(Disc.Image(), *Index, Number);
	if (!Map)
	{
		UE_LOG(LogWeltall, Error, TEXT("The field map cannot be loaded: %hs"), Map.Error().Message.c_str());
		return nullptr;
	}
	auto Scene = Weltall::BuildFieldScene(*Map);
	if (!Scene)
	{
		UE_LOG(LogWeltall, Error, TEXT("The field map's scene cannot be built: %hs"), Scene.Error().Message.c_str());
		return nullptr;
	}
	FFieldViewpoint Point = Viewpoint;
	if (Viewpoint.Entrance)
	{
		const auto Scripts = Weltall::FEventScripts::Parse(Map->EventScripts);
		const std::optional<Weltall::FEventEntrance> Entrance = Scripts ? Scripts->Entrance(*Viewpoint.Entrance) : std::nullopt;
		if (!Entrance)
		{
			UE_LOG(LogWeltall, Error, TEXT("Map %d has no entrance %d"), Number, *Viewpoint.Entrance);
			return nullptr;
		}
		Point.X = Entrance->X;
		Point.Z = Entrance->Z;
		Point.Layer = Entrance->Layer;
		Point.Y.reset();
		if (Entrance->CameraDirection != Weltall::FEventEntrance::KeepDirection)
		{
			Point.YawStep = Weltall::FEventEntrance::CameraYaw(Entrance->CameraDirection) / Weltall::FFieldCamera::YawStep;
		}
	}
	if (Point.Layer < 0 || Point.Layer >= static_cast<int32>(Map->Walkmesh.Layers.size()))
	{
		UE_LOG(LogWeltall, Error, TEXT("Map %d has no walkmesh layer %d"), Number, Point.Layer);
		return nullptr;
	}
	const int32 Floor = Point.Y.value_or(Weltall::FindFieldFloor(Map->Walkmesh.Layers[static_cast<std::size_t>(Point.Layer)], Point.X, Point.Z).Height);
	Scene->State = State;
	Weltall::FFieldCamera Camera;
	Camera.Rotation[1] = static_cast<int16>((Point.YawStep & 7) * Weltall::FFieldCamera::YawStep);
	Camera.Pitch = Point.Pitch;
	Camera.ProjectionDistance = Point.ProjectionDistance;
	Camera.DistanceScale = Point.DistanceScale;
	const Weltall::FFieldPoint Target = {Point.X * 0x10000, Floor * 0x10000, Point.Z * 0x10000};
	if (Point.Eye)
	{
		Camera.Target = Target;
		Camera.Eye = {(*Point.Eye)[0] * 0x10000, (*Point.Eye)[1] * 0x10000, (*Point.Eye)[2] * 0x10000};
	}
	else
	{
		Camera.Target = Weltall::FollowTarget(Target);
		Camera.Eye = Weltall::KeepEyeAboveCameraLayer(Map->Walkmesh, Weltall::FollowEye(Camera, Camera.Target));
	}
	return MakeShared<FFieldSceneryScreen>(std::move(*Map), std::move(*Scene), Camera, Options);
}

FFieldSceneryScreen::FFieldSceneryScreen(Weltall::FFieldMap InMap, Weltall::FFieldScene InScene, const Weltall::FFieldCamera& InCamera,
	const FFieldRenderOptions& InOptions)
	: Map(std::move(InMap)), Scene(std::move(InScene)), FieldCamera(InCamera), Options(InOptions), Vram(MapVram(Map)),
	  VramUnits(ShareRenderResource(new FVramTexture())), Mesh(MakeShared<const FFieldSceneMesh>(Scene))
{
	VramUnits->SetUpscaled(Weltall::UpscalesTextures(Options.Filter, Options.SpriteFilter));
	VramUnits->Follow(Vram);
}

FFieldSceneryScreen::~FFieldSceneryScreen() = default;

void FFieldSceneryScreen::Update(double, Weltall::FPadState)
{
}

TSharedRef<const IScreenFrame> FFieldSceneryScreen::CaptureFrame() const
{
	const Weltall::FFieldView View = Weltall::BuildFieldView(FieldCamera);
	Weltall::EvaluateFieldScene(Scene, View, Options.Culling, Frame);
	TSharedRef<FFieldScreenFrame>* Free = Frames.FindByPredicate([](const TSharedRef<FFieldScreenFrame>& Candidate) { return Candidate.IsUnique(); });
	TSharedRef<FFieldScreenFrame> Next = Free != nullptr ? *Free : Frames.Add_GetRef(MakeShared<FFieldScreenFrame>(Mesh, VramUnits));
	BuildFieldSceneDraw(Scene, Frame, View, *Mesh, Options, Next->Draw);
	FollowTextureModSources(*VramUnits, Vram, &Mesh.Get(), {&Next->Draw}, {});
	return Next;
}
