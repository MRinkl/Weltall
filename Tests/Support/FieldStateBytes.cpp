#include "Support/FieldStateBytes.h"

#include "Field/FieldWalkmesh.h"

namespace Weltall::Testing
{
	namespace
	{
		void Add(std::vector<uint8>& Bytes, uint32 Value, int32 Size)
		{
			for (int32 Byte = 0; Byte < Size; ++Byte)
			{
				Bytes.push_back(static_cast<uint8>(Value >> (8 * Byte)));
			}
		}

		void AddPoint(std::vector<uint8>& Bytes, const FFieldPoint& Point)
		{
			for (const int32 Axis : Point)
			{
				Add(Bytes, static_cast<uint32>(Axis), 4);
			}
		}

		/** The fog, the far bucket, the drift, the backdrop and the gears' lights, as FIELD_STATE ends. */
		void AddScenery(std::vector<uint8>& Bytes, const FFieldRenderState& Render)
		{
			const FFieldFog& Fog = Render.Fog;
			Add(Bytes, Fog.bOn ? 1 : 0, 2);
			for (const uint8 Channel : Fog.Color)
			{
				Add(Bytes, Channel, 1);
			}
			for (const uint8 Channel : Fog.FarColor)
			{
				Add(Bytes, Channel, 1);
			}
			Add(Bytes, static_cast<uint16>(Fog.Near), 2);
			Add(Bytes, static_cast<uint16>(Fog.Far), 2);
			Add(Bytes, Render.FarBucket, 2);
			Add(Bytes, Render.SceneryDrift, 1);

			const FFieldPanorama& Panorama = Render.Panorama;
			for (const int16 Value : {Panorama.VramX, Panorama.VramY, Panorama.Width, Panorama.Height, Panorama.ClutX, Panorama.ClutY,
					 Panorama.TexturePage, Panorama.Scroll})
			{
				Add(Bytes, static_cast<uint16>(Value), 2);
			}
			for (const int32 Axis : Panorama.Position)
			{
				Add(Bytes, static_cast<uint32>(Axis), 4);
			}
			for (const std::array<uint8, 3>& Color : Panorama.Colors)
			{
				for (const uint8 Channel : Color)
				{
					Add(Bytes, Channel, 1);
				}
			}
			for (const int16 Value : {Panorama.BandScale, Panorama.DepthDivisor, Panorama.DepthOffset})
			{
				Add(Bytes, static_cast<uint16>(Value), 2);
			}
			Add(Bytes, Panorama.bEnabled ? 1 : 0, 2);

			const FFieldGearLights& Lights = Render.GearLights;
			for (const int16 Word : Lights.Words)
			{
				Add(Bytes, static_cast<uint16>(Word), 2);
			}
			for (const uint8 Channel : Lights.Ambient)
			{
				Add(Bytes, Channel, 1);
			}
		}
	}

	std::vector<uint8> FieldStateBytes(const FEventContext& Context, const FFieldSongsModel& Songs)
	{
		std::vector<uint8> Bytes;
		const FFieldCamera& Camera = Context.Camera;
		Add(Bytes, static_cast<uint16>(Camera.Pitch), 2);
		Add(Bytes, static_cast<uint16>(Camera.DistanceScale), 2);
		Add(Bytes, static_cast<uint32>(Camera.ProjectionDistance), 4);
		Add(Bytes, static_cast<uint16>(Camera.Rotation[1]), 2);
		Add(Bytes, static_cast<uint32>(Camera.TurningYaw), 4);
		Add(Bytes, static_cast<uint32>(Camera.YawGoal), 4);
		Add(Bytes, Camera.Flags, 4);
		Add(Bytes, static_cast<uint32>(Camera.SnapTicks), 4);
		for (const FFieldCameraRamp* Ramp : {&Camera.ZoomRamp, &Camera.PitchRamp, &Camera.ProjectionRamp})
		{
			Add(Bytes, static_cast<uint16>(Ramp->Ticks), 2);
			Add(Bytes, static_cast<uint32>(Ramp->Value), 4);
			Add(Bytes, static_cast<uint32>(Ramp->Step), 4);
		}
		Add(Bytes, Camera.ForbiddenDirections, 1);
		Add(Bytes, static_cast<uint16>(Camera.Mode), 2);
		Add(Bytes, static_cast<uint32>(Camera.TargetDivisor), 4);
		Add(Bytes, static_cast<uint32>(Camera.EyeDivisor), 4);
		Add(Bytes, static_cast<uint16>(Camera.ScriptYaw), 2);
		Add(Bytes, static_cast<uint16>(Camera.ScriptPitch), 2);
		Add(Bytes, static_cast<uint32>(Camera.ScriptDistance), 4);
		Add(Bytes, static_cast<uint16>(Camera.ScriptScale), 2);
		for (const int16 Side : Camera.Bounds)
		{
			Add(Bytes, static_cast<uint16>(Side), 2);
		}
		Add(Bytes, Camera.FollowHeight, 1);
		for (const FFieldPoint* Point : {&Camera.Eye, &Camera.Target, &Camera.EyeGoal, &Camera.TargetGoal, &Camera.TargetFrom, &Camera.TargetTo,
				 &Camera.EyeFrom, &Camera.EyeTo})
		{
			AddPoint(Bytes, *Point);
		}
		Add(Bytes, Camera.PathMoves, 2);
		for (const FFieldCameraMove* Move : {&Camera.TargetMove, &Camera.EyeMove})
		{
			Add(Bytes, static_cast<uint16>(Move->Ticks), 2);
			AddPoint(Bytes, Move->At);
			AddPoint(Bytes, Move->Step);
		}

		const FFieldEncounters& Encounters = Context.Encounters;
		Add(Bytes, static_cast<uint32>(Encounters.Period), 4);
		Add(Bytes, static_cast<uint32>(Encounters.PerPeriod), 4);
		Add(Bytes, static_cast<uint32>(Encounters.Countdown), 4);
		for (const uint16 Tick : Encounters.Pending)
		{
			Add(Bytes, Tick, 2);
		}
		const FFieldRenderState& Render = Context.Render;
		Add(Bytes, Render.BackgroundColor.R, 1);
		Add(Bytes, Render.BackgroundColor.G, 1);
		Add(Bytes, Render.BackgroundColor.B, 1);
		Add(Bytes, static_cast<uint32>(Render.CullMarginX), 4);
		Add(Bytes, static_cast<uint32>(Render.CullMarginY), 4);
		Add(Bytes, Context.PlayerLock, 2);
		Add(Bytes, Context.bMenuLocked ? 1 : 0, 1);
		Add(Bytes, Context.bCompassHidden ? 1 : 0, 1);
		Add(Bytes, Context.EncounterBattleKind, 1);
		Add(Bytes, Context.ScriptedBattleKind, 1);
		Add(Bytes, Context.GearBoardingOverride, 2);
		const FFieldFade& Fade = Render.Fades[1];
		for (const int32 Channel : Fade.Color)
		{
			Add(Bytes, static_cast<uint32>(Channel), 4);
		}
		for (const int32 Step : Fade.Step)
		{
			Add(Bytes, static_cast<uint32>(Step), 4);
		}
		Add(Bytes, Fade.BlendMode, 2);
		Add(Bytes, Fade.bActive ? 1 : 0, 2);
		Add(Bytes, static_cast<uint16>(Fade.Ticks), 2);
		AddScenery(Bytes, Render);
		Add(Bytes, Context.GearTurnSpeed, 2);
		Add(Bytes, Context.MenuParam, 1);

		Add(Bytes, static_cast<uint32>(Songs.Song), 4);
		Add(Bytes, Songs.bLoading ? 0xFFFFFFFFu : 0u, 4);
		Add(Bytes, static_cast<uint32>(Songs.StartVolume), 4);
		Add(Bytes, static_cast<uint32>(Songs.Stops), 4);
		if (Context.Walkmesh != nullptr)
		{
			for (const uint32 Word : Context.Walkmesh->Attributes)
			{
				Add(Bytes, Word, 4);
			}
		}
		return Bytes;
	}
}
