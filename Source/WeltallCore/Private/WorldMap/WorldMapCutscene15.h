#pragma once

#include "WorldMap/WorldMapCutsceneDirector.h"

namespace Weltall::WorldMapOps::Scene15
{
	inline constexpr uint16 FieldMap = 0x1A1;
	/** The tasks' slots, as the start adds them: the shot's rings ride in the slots of their models' numbers. */
	inline constexpr int32 CameraSlot = 2;
	inline constexpr int32 ShotSlot = 3;
	inline constexpr int32 FirstRingSlot = 4;
	inline constexpr int32 LastRingSlot = 8;
	inline constexpr int32 LightRingSlot = 9;
	/** The models: the shot's body and the two that hang from it, its five rings (4 to 8), the light on the ground. */
	inline constexpr int32 ShotBody = 1;
	inline constexpr int32 ShotParts[] = {3, 2, 1};
	inline constexpr int32 LightRing = 9;
	inline constexpr int32 ModelCount = 11;
	/** The particle effects: the hit, the graze's second burst, the shot's trail, the rings' two, the three on the ground. */
	inline constexpr int32 HitEffect = 0x20;
	inline constexpr int32 GrazeEffect = 0x21;
	inline constexpr int32 TrailEffect = 0x22;
	inline constexpr int32 RingEffects[] = {0x23, 0x24};
	inline constexpr int32 GroundEffects[] = {0x25, 0x26, 0x27};
	inline constexpr int32 LaunchSounds[] = {0x19, 0x1A, 0x1B};
	inline constexpr int32 FlightSounds[] = {0x1C, 0x1D, 0x1E};

	/** The way the shot flies (4.12): south-west, level; then, after the hit, down to the ground. */
	inline constexpr FWorldMapVector Course{-0x85A, 0, 0xDA6, 0};
	inline constexpr FWorldMapVector Dive{-0x988, 0x227, -0xCAB, 0};
	/** 128 units a tick. */
	inline constexpr int32 FlightStep = 0x80;
	/** Where the shot meets the structure, where the graze's flight ends, where the pass's does, and where the dive strikes the ground (20.12). */
	inline constexpr int32 HitX = 0x1F9E000;
	inline constexpr int32 HitZ = 0x5998000;
	inline constexpr int32 GrazeEndX = 0x199D000;
	inline constexpr int32 GrazeEndZ = 0x6367000;
	inline constexpr int32 PassEndX = 0x1EB5000;
	inline constexpr int32 PassEndZ = 0x5EE6000;
	inline constexpr int32 GroundX = 0x1498000;
	inline constexpr int32 GroundZ = 0x4AF2000;

	inline FWorldMapTask& TaskAt(FWorldMapContext& Context, int32 Slot)
	{
		return Context.Tasks[static_cast<std::size_t>(Slot)];
	}

	inline FWorldMapObject& ObjectAt(FWorldMapContext& Context, int32 Number)
	{
		return Context.Objects[static_cast<std::size_t>(Number)];
	}

	/** The entrance's tables; an entrance past them is a fault (the original reads the tables after them). */
	inline const FWorldMapShotEntrance* Entrance(FWorldMapContext& Context)
	{
		const auto& Entrances = Context.Tables->Cutscenes.Directors.Scene15;
		if (Context.Entrance < 0 || static_cast<std::size_t>(Context.Entrance) >= Entrances.size())
		{
			Context.Fault = MakeError("the world map's scene 15 was entered by entrance " + std::to_string(Context.Entrance) + ", past its three");
			return nullptr;
		}
		return &Entrances[static_cast<std::size_t>(Context.Entrance)];
	}

	inline void SendRings(FWorldMapContext& Context, int16 Message)
	{
		for (int32 Slot = FirstRingSlot; Slot <= LastRingSlot; ++Slot)
		{
			SendMessage(Context, Slot, Message);
		}
	}

	/** The camera's messages. */
	namespace CameraMessage
	{
		inline constexpr int16 FirstView = 1;
		inline constexpr int16 SecondView = 2;
		inline constexpr int16 ThirdView = 3;
		inline constexpr int16 LastView = 4;
		inline constexpr int16 Chase = 5;
		inline constexpr int16 FollowDive = 6;
		inline constexpr int16 Quake = 7;
		inline constexpr int16 GrazeChase = 0x10;
		inline constexpr int16 GrazeView = 0x11;
		inline constexpr int16 PassChase = 0x18;
	}

	/** The shot's messages. */
	namespace ShotMessage
	{
		inline constexpr int16 FirstFlight = 1;
		inline constexpr int16 SecondFlight = 2;
		inline constexpr int16 ThirdFlight = 3;
		inline constexpr int16 LastFlight = 4;
		inline constexpr int16 Dive = 5;
		inline constexpr int16 GrazeFlight = 0x10;
		inline constexpr int16 PassFlight = 0x18;
	}

	/** The rings' messages. */
	namespace RingMessage
	{
		inline constexpr int16 Wake = 1;
		inline constexpr int16 Hold = 2;
		inline constexpr int16 Ride = 3;
		inline constexpr int16 Shrink = 4;
		inline constexpr int16 Linger = 5;
	}

	/** The shot (models 1 to 3), its five rings and the light it leaves on the ground. */
	FResult InitShot(FWorldMapContext& Context, int32 Slot);
	FResult UpdateShot(FWorldMapContext& Context, int32 Slot);
	FResult InitRing(FWorldMapContext& Context, int32 Slot);
	FResult UpdateRing(FWorldMapContext& Context, int32 Slot);
	FResult InitLightRing(FWorldMapContext& Context, int32 Slot);
	FResult UpdateLightRing(FWorldMapContext& Context, int32 Slot);
}
