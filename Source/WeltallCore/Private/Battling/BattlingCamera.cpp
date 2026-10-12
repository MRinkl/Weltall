#include "BattlingFightOps.h"
#include "BattlingMath.h"

namespace Weltall::BattlingFightOps
{
	namespace
	{
		using namespace BattlingMath;

		constexpr int32 EyeReach = 0x3A00;
		constexpr int32 EyeAboveGround = 0x100;
		constexpr uint8 EasedOut = 100;
	}

	void StartCamera(FBattlingFight& Fight)
	{
		const int32 Radius = static_cast<int32>(static_cast<uint32>(Fight.Random.Next()) & 0x1FFF) + 0x800;
		const int32 Angle = Fight.Random.Next() % 0x600 + 0x500;
		Fight.Eye[0] = (GteSin(Angle) * Radius >> 12) + 0x4000;
		Fight.Eye[2] = (GteCos(Angle) * Radius >> 12) + 0x4000;
		Fight.Eye[1] = -((Fight.Random.Next() & 0x7FF) + 0x400);
		Fight.CameraRadius = 0x100;
		Fight.CameraLift = 0x40;
		Fight.CameraEase = 0x40;
		Fight.CameraSideAngle = 0x400;
	}

	void FollowFight(FBattlingFight& Fight)
	{
		if (Fight.bEndingActive)
		{
			return;
		}
		const FBattlingPoint& One = Fight.Fighters[0].Position;
		const FBattlingPoint& Two = Fight.Fighters[1].Position;
		const int32 Angle = GteRatan2(One[0] - Two[0], One[2] - Two[2], Fight.Tables.Arctangents);
		Fight.DistanceXZ = DistanceXZ(One, Two);
		Fight.FightAngle = Angle;
		const int32 Apart = Distance(One, Two);
		Fight.Target = {(One[0] + Two[0]) / 2, (One[1] + Two[1]) / 2 - 0xA0, (One[2] + Two[2]) / 2};
		const int32 Radius = (Apart << 1) / 3 + 0xC0;

		// The eye goes to whichever side of the fighters' line is nearer it.
		const auto Candidate = [&](int32 Turn) {
			return FBattlingPoint{Fight.Target[0] + (GteSin(Angle + Turn) * Radius >> 12) - Fight.Eye[0], 0,
				Fight.Target[2] + (GteCos(Angle + Turn) * Radius >> 12) - Fight.Eye[2]};
		};
		const FBattlingPoint Left = Candidate(-0x400);
		const FBattlingPoint Right = Candidate(0x400);
		Fight.CameraSide = Length2(Left[0], Left[2]) <= Length2(Right[0], Right[2]) ? 1 : 0;
		Fight.CameraSideAngle = Fight.CameraSide != 0 ? -0x400 : 0x400;

		int32 Quarter = (Apart < 0 ? Apart + 3 : Apart) >> 2;
		int32 Rise = 0x300 < Quarter ? 0x340 : Quarter + 0x40;
		Rise = Fight.Target[1] - Rise;
		FBattlingPoint Step{(Fight.Target[0] + (GteSin(Angle + Fight.CameraSideAngle) * Radius >> 12) - Fight.Eye[0]) / Fight.CameraEase, 0,
			(Fight.Target[2] + (GteCos(Angle + Fight.CameraSideAngle) * Radius >> 12) - Fight.Eye[2]) / Fight.CameraEase};
		// The clamp normalizes a vector whose height it never sets; as the camera follows, the stack there holds whatever the
		// fight tick left, so the port takes the flat vector the clamp means.
		ClampToArena(Fight.Eye, Step, EyeReach, 0);
		Fight.Eye[0] += Step[0];
		Fight.Eye[2] += Step[2];
		const int32 Ground = TerrainHeight(Fight, Fight.Eye, false);
		if (Ground - EyeAboveGround < Rise)
		{
			Rise = Ground - EyeAboveGround;
		}
		const int32 Ease = Fight.CameraEase;
		Fight.CameraEase = EasedOut;
		Fight.Eye[1] += (Rise - Fight.Eye[1]) / Ease;
	}
}
