#include "WorldMap/WorldMapFrameBlend.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace Weltall
{
	namespace
	{
		/** The camera cut rather than moved when its target jumped this far, or its view turned further than this cosine's angle. */
		constexpr int32 CutDistance = 1024;
		constexpr float CutCosine = 0.866f;
		/** An anchor jumped rather than moved this far along an axis between ticks. */
		constexpr float JumpDistance = 512.0f;
		/** A turn of the yaw, and the yaw a texel of the horizon's band scrolls by. */
		constexpr int32 YawTurn = 0x1000;
		constexpr float YawPerTexel = 4.0f;
		constexpr float YawPerDegree = static_cast<float>(YawTurn) / 360.0f;
		constexpr float HalfTurnDegrees = 180.0f;

		/** A turn in degrees taken the shorter way round. */
		float ShorterTurn(float Degrees)
		{
			const float Turn = std::remainder(Degrees, 2.0f * HalfTurnDegrees);
			return Turn <= -HalfTurnDegrees ? Turn + 2.0f * HalfTurnDegrees : Turn;
		}

		FAffine AffineOf(const FViewMove& Move)
		{
			FAffine Affine;
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				Affine.M[Row] = {Move[Row][0], Move[Row][1], Move[Row][2]};
				Affine.T[Row] = Move[Row][3];
			}
			return Affine;
		}

		float Lerp(float From, float To, float Alpha)
		{
			return From + (To - From) * Alpha;
		}

		/** A difference along a world that wraps every Size units, the shorter way round. */
		int32 WrapDelta(int32 Delta, int32 Size)
		{
			if (Size <= 0)
			{
				return Delta;
			}
			Delta %= Size;
			if (Delta > Size / 2)
			{
				Delta -= Size;
			}
			else if (Delta < -Size / 2)
			{
				Delta += Size;
			}
			return Delta;
		}

		/** How far the target moved, in the view's world axes (x east, z south). */
		std::array<float, 3> TargetMove(const FWorldMapFrame& Latest, const FWorldMapFrame& Previous)
		{
			return {static_cast<float>(WrapDelta(Latest.TargetX - Previous.TargetX, Latest.WorldUnitsX)), 0.0f,
				static_cast<float>(-WrapDelta(Latest.TargetZ - Previous.TargetZ, Latest.WorldUnitsZ))};
		}

		bool AreContinuous(const FWorldMapFrame& Latest, const FWorldMapFrame& Previous)
		{
			if (Latest.Tick != Previous.Tick + 1 || Latest.Anchors.empty())
			{
				return false;
			}
			const std::array<float, 3> Moved = TargetMove(Latest, Previous);
			if (std::fabs(Moved[0]) > CutDistance || std::fabs(Moved[2]) > CutDistance)
			{
				return false;
			}
			const auto Forward = [](const FGteMatrix& Camera)
			{
				const std::array<int16, 3>& Row = Camera.M[2];
				const float Length = std::sqrt(static_cast<float>(Row[0] * Row[0] + Row[1] * Row[1] + Row[2] * Row[2]));
				return std::array<float, 3>{Row[0] / Length, Row[1] / Length, Row[2] / Length};
			};
			if (Latest.Camera.M[2] == std::array<int16, 3>{} || Previous.Camera.M[2] == std::array<int16, 3>{})
			{
				return false;
			}
			const std::array<float, 3> Now = Forward(Latest.Camera);
			const std::array<float, 3> Then = Forward(Previous.Camera);
			return Now[0] * Then[0] + Now[1] * Then[1] + Now[2] * Then[2] >= CutCosine;
		}
	}

	void MatchWorldMapAnchors(const FWorldMapFrame& Earlier, const FWorldMapFrame& Later, std::vector<int32>& Matches,
		std::vector<std::pair<uint32, int32>>& Scratch)
	{
		Scratch.clear();
		for (std::size_t Index = 0; Index < Earlier.Anchors.size(); ++Index)
		{
			Scratch.emplace_back(Earlier.Anchors[Index].Key, static_cast<int32>(Index));
		}
		std::sort(Scratch.begin(), Scratch.end());
		Matches.assign(Later.Anchors.size(), -1);
		for (std::size_t Index = 0; Index < Later.Anchors.size(); ++Index)
		{
			const uint32 Key = Later.Anchors[Index].Key;
			const auto Found = std::lower_bound(Scratch.begin(), Scratch.end(), std::pair<uint32, int32>(Key, -1));
			if (Found != Scratch.end() && Found->first == Key)
			{
				Matches[Index] = Found->second;
			}
		}
	}

	void BlendWorldMapFrames(const FWorldMapFrame& Latest, const FWorldMapFrame* Previous, std::span<const int32> Matches, float Alpha,
		FWorldMapFrameBlend& Blend)
	{
		Blend.Moves.assign(Latest.Anchors.size(), UnmovedView);
		Blend.Paired.assign(Latest.Anchors.size(), 0);
		Blend.ProjectionDistance = static_cast<float>(Latest.ProjectionDistance);
		Blend.ScreenCenterY = static_cast<float>(Latest.ScreenCenterY);
		Blend.HorizonScroll = 0.0f;
		Blend.bBlended = false;
		if (Previous == nullptr || Matches.size() != Latest.Anchors.size() || !AreContinuous(Latest, *Previous))
		{
			return;
		}
		FAffine LatestInverse;
		FAffine PreviousInverse;
		if (!FAffine::From(Latest.Camera).Invert(LatestInverse) || !FAffine::From(Previous->Camera).Invert(PreviousInverse))
		{
			return;
		}
		Blend.bBlended = true;
		Blend.ProjectionDistance = Lerp(static_cast<float>(Previous->ProjectionDistance), Blend.ProjectionDistance, Alpha);
		Blend.ScreenCenterY = Lerp(static_cast<float>(Previous->ScreenCenterY), Blend.ScreenCenterY, Alpha);
		const int32 Turned = ((Latest.Yaw - Previous->Yaw + YawTurn / 2) % YawTurn + YawTurn) % YawTurn - YawTurn / 2;
		Blend.HorizonScroll = (Alpha - 1.0f) * static_cast<float>(Turned) / YawPerTexel;
		const std::array<float, 3> TargetMoved = TargetMove(Latest, *Previous);
		for (std::size_t Index = 0; Index < Latest.Anchors.size(); ++Index)
		{
			const int32 Match = Matches[Index];
			if (Match < 0 || static_cast<std::size_t>(Match) >= Previous->Anchors.size())
			{
				continue;
			}
			const FWorldMapAnchor& Now = Latest.Anchors[Index];
			const FWorldMapAnchor& Earlier = Previous->Anchors[static_cast<std::size_t>(Match)];
			// An anchor placed finer than the GTE's numbers blends as it was placed.
			const bool bFine = Now.bFine && Earlier.bFine;
			FAffine To = bFine ? Now.Fine : FAffine::From(Now.Matrix);
			FAffine From = bFine ? Earlier.Fine : FAffine::From(Earlier.Matrix);
			for (std::size_t Axis = 0; Axis < 3 && !bFine; ++Axis)
			{
				To.T[Axis] += Now.Fraction[Axis];
				From.T[Axis] += Earlier.Fraction[Axis];
			}
			switch (Now.Kind())
			{
			case EWorldMapAnchorKind::World:
			{
				// The world is drawn about the target: the tick before's about where the target was then.
				const std::array<float, 3> Moved = From.Turn(TargetMoved);
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					From.T[Axis] += Moved[Axis];
				}
				break;
			}
			case EWorldMapAnchorKind::Sky:
				break;
			default:
			{
				// Where its space starts in the world each tick, about the latest tick's target.
				const std::array<float, 3> Here = LatestInverse.Turn(To.T);
				const std::array<float, 3> There = PreviousInverse.Turn(From.T);
				bool bJumped = false;
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					const float Then = There[Axis] + PreviousInverse.T[Axis] - TargetMoved[Axis];
					bJumped = bJumped || std::fabs(Here[Axis] + LatestInverse.T[Axis] - Then) > JumpDistance;
				}
				if (bJumped)
				{
					continue;
				}
				break;
			}
			}
			Blend.Moves[Index] = BlendedViewMove(From, To, Alpha);
			Blend.Paired[Index] = 1;
		}
	}

	void UnroundWorldMapBlend(const FWorldMapFrame& Latest, const FWorldMapFrame* Previous, float Alpha, FWorldMapFrameBlend& Blend)
	{
		// What takes a frame's view space under its drawn camera to the view space of the same camera unrounded.
		const auto Unrounding = [](const FWorldMapFrame& Frame) -> std::optional<FAffine>
		{
			FAffine Back;
			if (!Frame.bPlayerView || !FAffine::From(Frame.Camera).Invert(Back))
			{
				return std::nullopt;
			}
			return WorldMapPlayerViewAffine(Frame.CameraBasis, Frame.ShownOffsets).After(Back);
		};
		const std::optional<FAffine> Now = Unrounding(Latest);
		if (!Now || Blend.Moves.size() != Latest.Anchors.size())
		{
			return;
		}
		FAffine Between = *Now;
		if (Blend.bBlended && Previous != nullptr)
		{
			if (const std::optional<FAffine> Before = Unrounding(*Previous); !Before || !BlendAffine(*Before, *Now, Alpha, Between))
			{
				Between = *Now;
			}
		}
		for (std::size_t Index = 0; Index < Latest.Anchors.size(); ++Index)
		{
			if (Latest.Anchors[Index].Kind() == EWorldMapAnchorKind::Sky)
			{
				continue;
			}
			const FAffine& Move = Index < Blend.Paired.size() && Blend.Paired[Index] != 0 ? Between : *Now;
			Blend.Moves[Index] = Move.After(AffineOf(Blend.Moves[Index])).ToMove();
		}
	}

	void MoveWorldMapBlendToView(const FWorldMapFrame& Latest, const FWorldMapFrame* Previous, float Alpha, const FWorldMapCameraOffsets& Live,
		FWorldMapFrameBlend& Blend)
	{
		if (!Latest.bPlayerView || Blend.Moves.size() != Latest.Anchors.size())
		{
			return;
		}
		FWorldMapCameraOffsets Shown = Latest.ShownOffsets;
		if (Blend.bBlended && Previous != nullptr && Previous->bPlayerView)
		{
			const FWorldMapCameraOffsets& Before = Previous->ShownOffsets;
			Shown.Zoom = Lerp(Before.Zoom, Shown.Zoom, Alpha);
			Shown.Tilt = Lerp(Before.Tilt, Shown.Tilt, Alpha);
			Shown.Turn = Before.Turn + ShorterTurn(Shown.Turn - Before.Turn) * Alpha;
		}
		if (Shown == Live && Latest.ShownOffsets == Live)
		{
			return;
		}
		// From the offsets a blended anchor shows, or for one drawn as the latest tick has it (new, or jumped) from the latest's.
		struct FToLive
		{
			FAffine World;
			FAffine Sky;
		};
		const auto ToLive = [&Latest, &Live](const FWorldMapCameraOffsets& From) -> std::optional<FToLive>
		{
			FAffine WorldBack;
			FAffine SkyBack;
			if (!WorldMapPlayerViewAffine(Latest.CameraBasis, From).Invert(WorldBack) || !WorldMapPlayerSkyAffine(Latest.CameraBasis, From).Invert(SkyBack))
			{
				return std::nullopt;
			}
			return FToLive{WorldMapPlayerViewAffine(Latest.CameraBasis, Live).After(WorldBack), WorldMapPlayerSkyAffine(Latest.CameraBasis, Live).After(SkyBack)};
		};
		const std::optional<FToLive> FromShown = ToLive(Shown);
		const std::optional<FToLive> FromLatest = ToLive(Latest.ShownOffsets);
		if (!FromShown || !FromLatest)
		{
			return;
		}
		for (std::size_t Index = 0; Index < Latest.Anchors.size(); ++Index)
		{
			const FToLive& Moves = Index < Blend.Paired.size() && Blend.Paired[Index] != 0 ? *FromShown : *FromLatest;
			const FAffine& Move = Latest.Anchors[Index].Kind() == EWorldMapAnchorKind::Sky ? Moves.Sky : Moves.World;
			Blend.Moves[Index] = Move.After(AffineOf(Blend.Moves[Index])).ToMove();
		}
		Blend.HorizonScroll += ShorterTurn(Live.Turn - Shown.Turn) * YawPerDegree / YawPerTexel;
	}
}
