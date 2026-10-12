#include "WorldMap/WorldMapOps.h"

#include "Game/GameState.h"
#include "Graphics/GteVectors.h"
#include "WorldMap/WorldMapInput.h"
#include "WorldMap/WorldMapStateLayout.h"

#include <cstdlib>

namespace Weltall::WorldMapOps
{
	namespace
	{
		/** The Yggdrasil's hull, the part sharing its place and the two parts that spin on it. */
		constexpr int32 HullObject = 0;
		constexpr int32 DeckObject = 1;
		constexpr int32 FirstSpinningObject = 2;
		constexpr int32 SecondSpinningObject = 3;
		/** The Yggdrasil's pitch word's start. */
		constexpr int32 LevelHeight = static_cast<int32>(0xFFD80000u);

		namespace VehicleStep
		{
			constexpr int16 Parked = 0;
			constexpr int16 ParkedNear = 1;
			constexpr int16 Flying = 2;
			constexpr int16 Sailing = 3;
			constexpr int16 AtSea = 4;
			constexpr int16 Climbing = 8;
			constexpr int16 Sinking = 0xC;
			constexpr int16 Landing = 0x10;
			constexpr int16 Rising = 0x14;
			/** Under the sand to Nisan (three steps), and back from it (three). */
			constexpr int16 Tunnelling = 0x20;
			constexpr int16 ReturnFromNisan = 0x24;
			/** The scripted voyages of scenes 3, 4 and 5. */
			constexpr int16 VoyageToVillage = 0x28;
			constexpr int16 VoyageToFactory = 0x30;
			constexpr int16 VoyageToNisan = 0x34;
			/** The place entered: nothing more to do. */
			constexpr int16 Entered = 0x40;
		}

		FWorldMapTask& TaskAt(FWorldMapContext& Context, int32 Slot)
		{
			return Context.Tasks[static_cast<std::size_t>(Slot)];
		}

		FWorldMapVector VehiclePlace(const FWorldMapContext& Context)
		{
			FWorldMapVector Place;
			Place.X = static_cast<int32>(static_cast<uint32>(static_cast<int32>(StateWord(Context, WorldMapStateLayout::VehicleX))) << 12);
			Place.Y = static_cast<int32>(static_cast<uint32>(static_cast<int32>(StateWord(Context, WorldMapStateLayout::VehicleY))) << 12);
			Place.Z = static_cast<int32>(static_cast<uint32>(static_cast<int32>(StateWord(Context, WorldMapStateLayout::VehicleZ))) << 12);
			return Place;
		}

		/** The hull and its deck turned by the pitch, heading and the camera's roll. */
		void TurnHull(FWorldMapContext& Context, const FWorldMapTask& Task)
		{
			const FGteVector Angles{static_cast<int16>(-(Task.Word(WorldMapTaskWord::Pitch) >> 12)), Task.Heading, Context.CameraRoll};
			const FGteMatrix Turn = RotationYxz(Angles);
			Context.Objects[HullObject].Matrix.M = Turn.M;
			Context.Objects[DeckObject].Matrix.M = Turn.M;
		}

		void PlaceHull(FWorldMapContext& Context, const FWorldMapTask& Task)
		{
			FWorldMapObject& Hull = Context.Objects[HullObject];
			Hull.X = Task.Position.X;
			Hull.Y = Task.Position.Y;
			Hull.Z = Task.Position.Z;
			Hull.W = Task.Position.W;
			Hull.Hidden = Task.Hidden;
		}

		void HideDeckAndParts(FWorldMapContext& Context, int16 Hidden)
		{
			Context.Objects[SecondSpinningObject].Hidden = Hidden;
			Context.Objects[FirstSpinningObject].Hidden = Hidden;
			Context.Objects[DeckObject].Hidden = Hidden;
		}

		/** The ship's collider while parked, and how deep it rides in sand or water (20.12). */
		constexpr int32 ParkedRadius = 0x40;
		constexpr int32 ParkedHeight = 0x400;
		constexpr int32 ShipRadius = 0x40;
		constexpr int32 ShipHeight = 0x20;
		constexpr int32 ShipDraft = 0x18000;
		/** The height the flying ship climbs to, and how deep it travels under the sand to and from Nisan. */
		constexpr int32 FlightHeight = -0x1E0000;
		constexpr int32 TunnelDepth = 0x30000;
		/** The distance a gear's first step from the ship takes to find where the party lands. */
		constexpr int32 LandingStep = 0x68000;
		/** A dust effect under the flying ship only while it is this near the ground. */
		constexpr int32 FlightDustHeight = 0x60000;
		constexpr int32 TurnPerTick = 0x80;
		constexpr int32 VoyageTurnPerTick = 0x10;
		constexpr int16 WakeTicks = 0x28;
		constexpr int16 VoyageFadeTicks = 0x40;
		constexpr int32 Arrived = 3;
		constexpr int16 VehicleShadow = 2;
		constexpr int16 StoryTunnelKind = 2;

		/** The messages the vehicle takes and sends. */
		constexpr int16 AboardMessage = 4;
		constexpr int16 BehindShipMessage = 0xB;
		constexpr int16 SpinUpMessage = 9;
		constexpr int16 SpinDownMessage = 10;
		constexpr int16 FlyingPresetsMessage = 9;
		constexpr int16 GroundPresetsMessage = 10;
		constexpr int16 FollowHeadingMessage = 9;
		constexpr int16 FollowPadMessage = 10;
		constexpr int16 LowerScreenMessage = 9;
		constexpr int16 RaiseScreenMessage = 10;
		constexpr int16 BackOnFootMessage = 3;
		constexpr int16 FadeOutMessage = 0xD;
		constexpr int16 CloseUpMessage = 0x11;
		/** The camera's fixed yaws for the voyages' second waypoint (messages 15-17). */
		constexpr int16 VillageYawMessage = 0xF;
		constexpr int16 FactoryYawMessage = 0x10;
		constexpr int16 NisanYawMessage = 0x11;

		/** The ship's particle effects. */
		constexpr int32 WakeEffect = 1;
		constexpr int32 DustEffect = 2;
		constexpr int32 SprayEffect = 3;
		constexpr int32 WashEffect = 4;
		constexpr int32 NisanDustEffect = 0x2B;
		constexpr int32 FlightSandEffect = 0x3C;
		constexpr int32 FlightSeaEffect = 0x3F;
		/** The scene's sound effect the voyage to Nisan plays as the ship starts to rise. */
		constexpr int32 NisanRiseSound = 0x38;

		/** Where the ship travels under the sand to Nisan, and where it comes back up. */
		constexpr std::array<int32, 2> NisanTunnelEnd{19264, 7168};
		constexpr std::array<int32, 2> NisanTunnelExit{18743, 8674};

		/** Where the voyages leave for: the map, and the scene the field starts it with. */
		constexpr uint16 VillageMap = 0x50;
		constexpr uint16 VillageScene = 1;
		constexpr uint16 FactoryMap = 0x120;
		constexpr uint16 FactoryScene = 6;
		constexpr uint16 NisanMap = 0x1F0;
		constexpr uint16 NisanScene = 0;

		constexpr int32 Thames = 15;
		constexpr int32 Shevat = 16;

		/** The ship's turn for its effects and its hull: its pitch, heading and the camera's roll. */
		FGteVector ShipAngles(const FWorldMapContext& Context, const FWorldMapTask& Task)
		{
			return {static_cast<int16>(-(Task.Word(WorldMapTaskWord::Pitch) >> 12)), Task.Heading, Context.CameraRoll};
		}

		std::array<int16, 4> ShipPlace(const FWorldMapTask& Task)
		{
			return {static_cast<int16>(Task.Position.X >> 12), static_cast<int16>(Task.Position.Y >> 12), static_cast<int16>(Task.Position.Z >> 12), 0};
		}

		void StartShipEffect(FWorldMapContext& Context, const FWorldMapTask& Task, int32 Effect)
		{
			Context.Particles.Start(Effect, ShipPlace(Task), ShipAngles(Context, Task));
		}

		/** The dust the flying ship raises taking off or landing: sand's, or the ground's. */
		int32 TakeOffEffect(const FWorldMapContext& Context, const FWorldMapTask& Task)
		{
			return Context.Ground.TerrainType(Task.Position.X, Task.Position.Z) == WorldMapTerrainType::Sand ? 0x3E : 0x3D;
		}

		void ClearDirection(FWorldMapTask& Task)
		{
			Task.Direction.Z = 0;
			Task.Direction.Y = 0;
			Task.Direction.X = 0;
		}

		void ForgetPlace(FWorldMapContext& Context)
		{
			Context.PlaceRecord.reset();
			Context.PlaceList = -1;
			Context.PlaceIndex = -1;
			Context.Place = -1;
			Context.Notice = -1;
		}

		void Leave(FWorldMapContext& Context)
		{
			Context.Running = 0;
			Context.ExitCode = 0;
		}

		/** A voyage's end: the field's map and scene, the camera's yaw, the place's own exit skipped. */
		void EndVoyage(FWorldMapContext& Context, uint16 Map, uint16 Scene)
		{
			SetStateWord(Context, WorldMapStateLayout::CameraYaw, Context.CameraYaw);
			SetStateWord(Context, WorldMapStateLayout::Place, static_cast<int16>(Map));
			SetStateWord(Context, WorldMapStateLayout::Scene, static_cast<int16>(Scene));
			Leave(Context);
			Context.SkipPlaceExit = 1;
		}

		void FadeOutVoyage(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			Task.Timer = VoyageFadeTicks;
			++Task.Step;
			SendMessage(Context, FWorldMapContext::FadeSlot, FadeOutMessage);
			Context.FadeBlend = 2;
			Context.FadeStep = 4;
		}

		/** The heading turned toward Goal by at most Limit from the heading the game state keeps. */
		void TurnToward(FWorldMapContext& Context, FWorldMapTask& Task, int32 Goal, int32 Limit)
		{
			const int32 Kept = static_cast<uint16>(StateWord(Context, WorldMapStateLayout::VehicleHeading));
			int32 Delta = Goal - Kept;
			if (std::abs(Delta) > 0x800)
			{
				Delta += Delta < 0 ? 0x1000 : -0x1000;
			}
			if (std::abs(Delta) > Limit)
			{
				Task.Heading = static_cast<int16>(Delta < 0 ? Kept - Limit : Kept + Limit);
			}
			Task.Heading = static_cast<int16>(Task.Heading & 0xFFF);
		}

		void HeadFor(FWorldMapContext& Context, FWorldMapTask& Task, const FWorldMapVector& Target)
		{
			const FWorldMapHeading Heading = WorldMapDirectionTo(Task.Position, Target, *Context.Arctangents);
			Task.Heading = Heading.Heading;
			Task.Direction.X = Heading.DirectionX;
			Task.Direction.Z = Heading.DirectionZ;
			Task.Word(WorldMapTaskWord::WalkX) = Target.X >> 12;
			Task.Word(WorldMapTaskWord::WalkZ) = Target.Z >> 12;
		}

		int32 WalkOn(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			return WalkTowardWorldMapPoint(Context.Ground, Task.Position, Task.Direction, Task.Speed, Task.Word(WorldMapTaskWord::WalkX),
				Task.Word(WorldMapTaskWord::WalkZ));
		}

		/** The first of 16 directions in which a gear's step from the ship is free, or -1. */
		int32 FindLandingDirection(const FWorldMapContext& Context, const FWorldMapTask& Task)
		{
			for (int32 Angle = 0; Angle < 0x1000; Angle += 0x100)
			{
				const FWorldMapVector Way{GteSin(Angle), 0, -GteCos(Angle), 0};
				FWorldMapVector Step;
				if (MoveOnWorldMapGround(Context.Ground, Task.Position, Way, Step, LandingStep, WorldMapTravel::Gears))
				{
					return Angle;
				}
			}
			return -1;
		}

		/** Docking: touching Thames or Shevat stands for their place. */
		void CheckDocking(FWorldMapContext& Context)
		{
			if (Context.Contact.Contact == 0 || (Context.Contact.Slot != Thames && Context.Contact.Slot != Shevat))
			{
				return;
			}
			const bool bThames = Context.Contact.Slot == Thames;
			const FWorldMapTrigger& Record = bThames ? Context.Tables->ThamesPlace : Context.Tables->ShevatPlace;
			Context.PlaceRecord = Record;
			Context.PlaceList = 4;
			Context.PlaceIndex = bThames ? 0 : 1;
			Context.Place = Record.Name;
		}

		/** The members' riding flags set for those present, and the tasks told the party is back on its feet in gears. */
		void Disembark(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			SendMessage(Context, FWorldMapContext::VehiclePartsSlot, SpinDownMessage);
			for (int32 PartySlot = 0; PartySlot < 3; ++PartySlot)
			{
				if (PartySlot != 0 && PartyCharacter(Context, PartySlot) == 0xFF)
				{
					continue;
				}
				SendMessage(Context, FWorldMapContext::LeaderSlot + PartySlot, BackOnFootMessage);
				SendMessage(Context, FWorldMapContext::GearSlot + PartySlot, BackOnFootMessage);
				SetRiding(Context, PartySlot, 1);
			}
			ClearDirection(Task);
			Task.Word(WorldMapTaskWord::Aboard) = 0;
			SetStateWord(Context, WorldMapStateLayout::VehicleFlags,
				static_cast<int16>(static_cast<uint16>(StateWord(Context, WorldMapStateLayout::VehicleFlags)) & 0x3FFF));
			ResetEncounters(Context);
		}

		/** Every member aboard: the ship goes into the sand or water, or climbs to fly. */
		FResult Board(FWorldMapContext& Context, FWorldMapTask& Task, int16 Message)
		{
			FResult Result = WorldMapTaskState::Update;
			SendMessage(Context, FWorldMapContext::VehiclePartsSlot, SpinUpMessage);
			SetControlled(Context, Task);
			const uint16 Flags = static_cast<uint16>(StateWord(Context, WorldMapStateLayout::VehicleFlags)) | WorldMapStateLayout::Aboard;
			SetStateWord(Context, WorldMapStateLayout::VehicleFlags, static_cast<int16>(Flags));
			switch (Flags & WorldMapStateLayout::VehicleTypeMask)
			{
			case 1:
			case 2:
				Task.Step = VehicleStep::Sinking;
				// The sand ship's mode is the message's number, 4.
				Context.TravelMode = (Flags & WorldMapStateLayout::VehicleTypeMask) == 2 ? WorldMapTravel::SeaShip : Message;
				Task.Word(WorldMapTaskWord::Draft) = Context.Ground.Height(Task.Position.X, Task.Position.Z) + ShipDraft;
				StartShipEffect(Context, Task, DustEffect);
				Context.Colliders.Count = 0;
				break;
			case 3:
			{
				SetSongVolume(Context, 0, WorldMapSongFadeTicks);
				const std::array<int16, 4> Place = ShipPlace(Task);
				Context.Particles.Start(TakeOffEffect(Context, Task), Place, std::nullopt);
				SendMessage(Context, FWorldMapContext::CameraTargetSlot, FollowHeadingMessage);
				SendMessage(Context, FWorldMapContext::CameraDistanceSlot, FlyingPresetsMessage);
				SendMessage(Context, FWorldMapContext::ScreenCentreSlot, LowerScreenMessage);
				Result = WorldMapTaskState::Waiting;
				Task.Step = VehicleStep::Climbing;
				Task.Countdown = 0x3C;
				Context.TravelMode = WorldMapTravel::Flying;
				Context.Colliders.Count = 0;
				break;
			}
			default:
				break;
			}
			for (int32 PartySlot = 0; PartySlot < 3; ++PartySlot)
			{
				if (PartySlot == 0 || PartyCharacter(Context, PartySlot) != 0xFF)
				{
					SetRiding(Context, PartySlot, 1);
				}
			}
			ResetEncounters(Context);
			ForgetPlace(Context);
			return Result;
		}

		void Fly(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			const EWorldMapInput Input = ReadFlightInput(Context, Task);
			if (Input == EWorldMapInput::Enter)
			{
				Leave(Context);
				return;
			}
			if (Input == EWorldMapInput::Disembark)
			{
				const int32 Type = Context.Ground.TerrainType(Task.Position.X, Task.Position.Z);
				if (!Context.Ground.CanTraverse(WorldMapTravel::Gears, Type))
				{
					return;
				}
				const int32 Direction = FindLandingDirection(Context, Task);
				if (Direction == -1)
				{
					return;
				}
				Task.Step = VehicleStep::Landing;
				Task.Word(WorldMapTaskWord::LandingDirection) = Direction;
				Task.Word(WorldMapTaskWord::Draft) = Context.Ground.Height(Task.Position.X, Task.Position.Z);
				SendMessage(Context, FWorldMapContext::ScreenCentreSlot, RaiseScreenMessage);
				SetSongVolume(Context, 0, WorldMapSongFadeTicks);
				// The landing's dust shows once, as the ship comes down near the ground.
				Task.Word(WorldMapTaskWord::Waypoint) = 1;
				Context.Particles.Stop(FlightSandEffect);
				Context.Particles.Stop(FlightSeaEffect);
				ForgetPlace(Context);
				Context.Contact = {};
				Context.Colliders.Count = 0;
				return;
			}
			int32& Throttle = Task.Word(WorldMapTaskWord::Throttle);
			FWorldMapVector& Out = Context.MoveResult;
			if (!MoveInWorldMapAir(Context.Ground, Task.Position, Task.Direction, Out, Throttle, Context.TravelMode))
			{
				Throttle = 0;
				Task.Direction.Z = 0;
				Task.Direction.X = 0;
			}
			else
			{
				Context.Contact = Context.Colliders.Find(Context.Ground, Out, ShipRadius, ShipHeight);
				if (Context.Contact.Contact == 2)
				{
					Throttle = 0;
				}
				else
				{
					Task.Position = Out;
				}
			}
			SetControlled(Context, Task);
			FindTrigger(Context, Task.Position, 2);
			CheckDocking(Context);
			TurnHull(Context, Task);
			const int32 Above = std::abs(Context.Ground.Height(Task.Position.X, Task.Position.Z) - Task.Position.Y);
			const int32 Type = Context.Ground.TerrainType(Task.Position.X, Task.Position.Z);
			if (Above < FlightDustHeight && Throttle != 0 && Type == WorldMapTerrainType::Sea)
			{
				StartShipEffect(Context, Task, FlightSeaEffect);
				Context.Particles.Stop(FlightSandEffect);
			}
			else if (Above < FlightDustHeight && Throttle != 0 && Type == WorldMapTerrainType::Sand)
			{
				StartShipEffect(Context, Task, FlightSandEffect);
				Context.Particles.Stop(FlightSeaEffect);
			}
			else
			{
				Context.Particles.Stop(FlightSandEffect);
				Context.Particles.Stop(FlightSeaEffect);
			}
			ClearDirection(Task);
			Context.Colliders.Count = 0;
		}

		/** The sailing ship's move: along the pad's direction, sliding along what blocks it, riding in the sand or the waves. */
		void Sail(FWorldMapContext& Context, FWorldMapTask& Task, bool bAtSea)
		{
			const EWorldMapInput Input = ReadShipInput(Context, Task);
			if (Input == EWorldMapInput::Enter)
			{
				if (Context.PlaceRecord && Context.PlaceRecord->Kind == StoryTunnelKind)
				{
					Task.Step = VehicleStep::Tunnelling;
					Task.Word(WorldMapTaskWord::Draft) = TunnelDepth;
					// The dust's turn is whatever the scratchpad held: none.
					Context.Particles.Start(DustEffect, ShipPlace(Task), FGteVector{});
					return;
				}
				Leave(Context);
				return;
			}
			if (Input == EWorldMapInput::Disembark && !bAtSea)
			{
				const int32 Direction = FindLandingDirection(Context, Task);
				if (Direction != -1)
				{
					Task.Step = VehicleStep::Rising;
					Task.Word(WorldMapTaskWord::LandingDirection) = Direction;
					Task.Word(WorldMapTaskWord::Draft) = Context.Ground.Height(Task.Position.X, Task.Position.Z);
					StartShipEffect(Context, Task, DustEffect);
					Context.Particles.Stop(WakeEffect);
					ForgetPlace(Context);
				}
				return;
			}
			TurnToward(Context, Task, Task.Heading, TurnPerTick);
			const int32 Speed = Task.Speed << 12;
			FWorldMapVector& Out = Context.MoveResult;
			bool bMoved = MoveOnWorldMapGround(Context.Ground, Task.Position, Task.Direction, Out, Speed, Context.TravelMode);
			if (!bMoved)
			{
				Task.Direction = Out;
				bMoved = MoveOnWorldMapGround(Context.Ground, Task.Position, Task.Direction, Out, Speed, Context.TravelMode);
				if (!bMoved)
				{
					Task.Direction.Z = 0;
					Task.Direction.X = 0;
				}
			}
			const auto Surface = [&](const FWorldMapVector& At) {
				return (bAtSea ? Context.Ground.SeaHeight(At.X, At.Z, Context.WavePhase) : Context.Ground.Height(At.X, At.Z)) + ShipDraft;
			};
			if (bMoved)
			{
				Out.Y = Surface(Out);
				Context.Contact = Context.Colliders.Find(Context.Ground, Out, ShipRadius, ShipHeight);
				if (Context.Contact.Contact == 2)
				{
					if (bAtSea)
					{
						Task.Position.Y = Surface(Task.Position);
					}
					Task.Word(WorldMapTaskWord::Throttle) = 0;
				}
				else
				{
					Task.Position = Out;
					if (bAtSea && (Task.Direction.X != 0 || Task.Direction.Z != 0))
					{
						Context.Encounters.Tick(*Context.Random);
					}
				}
			}
			else if (bAtSea)
			{
				Task.Position.Y = Surface(Task.Position);
			}
			SetControlled(Context, Task);
			FindTrigger(Context, Task.Position, 2);
			if (bAtSea)
			{
				CheckDocking(Context);
			}
			TurnHull(Context, Task);
			const bool bStill = Task.Direction.X == 0 && Task.Direction.Z == 0;
			if (!bAtSea)
			{
				if (bStill)
				{
					Context.Particles.Stop(WakeEffect);
				}
				else
				{
					StartShipEffect(Context, Task, WakeEffect);
				}
				ClearDirection(Task);
				if (Context.Ground.TerrainType(Task.Position.X, Task.Position.Z) != WorldMapTerrainType::Sand)
				{
					Context.Particles.Stop(WakeEffect);
					StartShipEffect(Context, Task, SprayEffect);
					Task.Step = VehicleStep::AtSea;
					Task.Timer = WakeTicks;
				}
				return;
			}
			Task.Timer = static_cast<int16>(Task.Timer - 1);
			if (Task.Timer < 1)
			{
				Task.Timer = 0;
				Context.Particles.Stop(SprayEffect);
			}
			else
			{
				StartShipEffect(Context, Task, SprayEffect);
			}
			if (bStill)
			{
				Context.Particles.Stop(WashEffect);
				Task.Timer = 0;
			}
			else
			{
				StartShipEffect(Context, Task, WashEffect);
			}
			ClearDirection(Task);
			// The program tests where its last move ended, even a refused one's slide.
			if (Context.Ground.TerrainType(Out.X, Out.Z) == WorldMapTerrainType::Sand)
			{
				Context.Particles.Stop(SprayEffect);
				Context.Particles.Stop(WashEffect);
				Task.Step = VehicleStep::Sailing;
			}
			Context.Colliders.Count = 0;
		}

		/** The voyage over land to the village (scene 3): waypoint after waypoint, the heading turning after the way. */
		void VoyageOverLand(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			const int16 Start = VehicleStep::VoyageToVillage;
			if (Task.Step == Start + 2)
			{
				Task.Timer = static_cast<int16>(Task.Timer - 1);
				if (Task.Timer < 0)
				{
					EndVoyage(Context, VillageMap, VillageScene);
				}
			}
			else
			{
				if (Task.Step == Start)
				{
					++Task.Step;
					Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z) + ShipDraft;
					int32& Waypoint = Task.Word(WorldMapTaskWord::Waypoint);
					const std::array<int16, 2>& Next = Context.Tables->VillageVoyage[static_cast<std::size_t>(Waypoint)];
					HeadFor(Context, Task, {Next[0] << 12, 0, Next[1] << 12, 0});
					Task.Word(WorldMapTaskWord::LandingDirection) = Task.Heading;
					if (++Waypoint == 2)
					{
						SendMessage(Context, FWorldMapContext::CameraTargetSlot, VillageYawMessage);
					}
					if (Waypoint == static_cast<int32>(Context.Tables->VillageVoyage.size()))
					{
						FadeOutVoyage(Context, Task);
					}
				}
				if (WalkOn(Context, Task) == Arrived)
				{
					--Task.Step;
				}
			}
			TurnToward(Context, Task, Task.Word(WorldMapTaskWord::LandingDirection), VoyageTurnPerTick);
			StartShipEffect(Context, Task, WakeEffect);
			TurnHull(Context, Task);
			SetControlled(Context, Task);
		}

		/** The voyage at sea to the factory (scene 4). */
		void VoyageAtSea(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			const int16 Start = VehicleStep::VoyageToFactory;
			if (Task.Step == Start + 2)
			{
				Task.Timer = static_cast<int16>(Task.Timer - 1);
				if (Task.Timer < 0)
				{
					EndVoyage(Context, FactoryMap, FactoryScene);
				}
			}
			else if (Task.Step == Start)
			{
				++Task.Step;
				Task.Position.Y = Context.Ground.SeaHeight(Task.Position.X, Task.Position.Z, Context.WavePhase) + ShipDraft;
				int32& Waypoint = Task.Word(WorldMapTaskWord::Waypoint);
				const std::array<int16, 2>& Next = Context.Tables->FactoryVoyage[static_cast<std::size_t>(Waypoint)];
				HeadFor(Context, Task, {Next[0] << 12, 0, Next[1] << 12, 0});
				Task.Word(WorldMapTaskWord::LandingDirection) = Task.Heading;
				if (++Waypoint == 2)
				{
					SendMessage(Context, FWorldMapContext::CameraTargetSlot, FactoryYawMessage);
				}
				if (Waypoint == static_cast<int32>(Context.Tables->FactoryVoyage.size()))
				{
					FadeOutVoyage(Context, Task);
				}
			}
			if (WalkOn(Context, Task) == Arrived)
			{
				--Task.Step;
			}
			TurnToward(Context, Task, Task.Word(WorldMapTaskWord::LandingDirection), VoyageTurnPerTick);
			Task.Position.Y = Context.Ground.SeaHeight(Task.Position.X, Task.Position.Z, Context.WavePhase) + ShipDraft;
			StartShipEffect(Context, Task, WashEffect);
			TurnHull(Context, Task);
			SetControlled(Context, Task);
		}

		/** The ship's straight run by its direction, Speed units a tick. */
		void Cruise(FWorldMapTask& Task, int32 Speed)
		{
			Task.Position.X += Task.Direction.X * Speed;
			Task.Position.Z += Task.Direction.Z * Speed;
		}

		/** The voyage to Nisan (scene 5): a timed run along 200h, rising out of the water, then the fade. */
		void VoyageToNisan(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			const int16 Start = VehicleStep::VoyageToNisan;
			const int16 Step = Task.Step;
			if (Step == Start)
			{
				++Task.Step;
				SendMessage(Context, FWorldMapContext::CameraDistanceSlot, CloseUpMessage);
				Task.Position.Y = -0x100000;
				Task.Heading = 0x200;
				Task.Direction.X = GteSin(0x200);
				Task.Direction.Z = -GteCos(Task.Heading);
				Task.Timer = 0x5A;
				Task.Word(WorldMapTaskWord::LandingDirection) = 0;
				TurnHull(Context, Task);
			}
			if (Step == Start || Step == Start + 1 || Step == Start + 2)
			{
				Task.Timer = static_cast<int16>(Task.Timer - 1);
				if (Task.Timer < 0)
				{
					if (Step == Start + 2)
					{
						Task.Timer = 0x78;
						++Task.Step;
						PlaySceneEffect(Context, NisanRiseSound);
					}
					else
					{
						Task.Timer = 0xB4;
						++Task.Step;
						SendMessage(Context, FWorldMapContext::CameraTargetSlot, NisanYawMessage);
					}
				}
				Cruise(Task, 12);
				Context.Ground.Wrap(Task.Position);
				const int32 Before = Task.Position.Y;
				Task.Position.Y = Before + 0x1000;
				if (static_cast<uint32>(Before + 0x10FFF) < 0x1FFFF)
				{
					StartShipEffect(Context, Task, FlightSeaEffect);
				}
				else
				{
					Context.Particles.Stop(FlightSeaEffect);
				}
				SetControlled(Context, Task);
				return;
			}
			if (Step == Start + 3)
			{
				Task.Timer = static_cast<int16>(Task.Timer - 1);
				if (Task.Timer < 0)
				{
					Task.Timer = 0x5A;
					++Task.Step;
					Context.Particles.Stop(NisanDustEffect);
				}
				Cruise(Task, 8);
				Context.Ground.Wrap(Task.Position);
				Task.Position.Y += 0x1000;
				if (Task.Step == Start + 4)
				{
					Context.Particles.Stop(NisanDustEffect);
				}
				else
				{
					std::array<int16, 4> Place = ShipPlace(Task);
					Place[1] = 0;
					Context.Particles.Start(NisanDustEffect, Place, ShipAngles(Context, Task));
				}
				SetControlled(Context, Task);
				return;
			}
			Task.Timer = static_cast<int16>(Task.Timer - 1);
			if (Task.Timer < 0)
			{
				if (Step == Start + 4)
				{
					FadeOutVoyage(Context, Task);
					Task.Timer = 0x80;
				}
				else
				{
					EndVoyage(Context, NisanMap, NisanScene);
				}
			}
			Cruise(Task, 8);
			SetControlled(Context, Task);
		}
	}

	FResult InitVehicle(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		Task.Hidden = 0;
		const FWorldMapVector Place = VehiclePlace(Context);
		Task.Position.X = Place.X;
		Task.Position.Y = Place.Y;
		Task.Position.Z = Place.Z;
		FResult Result = WorldMapTaskState::Update;
		Task.Direction.Z = 0;
		Task.Direction.Y = 0;
		Task.Direction.X = 0;
		Task.Word(WorldMapTaskWord::FlightStarted) = 0;
		Task.Word(WorldMapTaskWord::Throttle) = 0;
		Task.Word(WorldMapTaskWord::Aboard) = 0;
		Task.Word(WorldMapTaskWord::Draft) = LevelHeight;
		Task.Word(WorldMapTaskWord::Pitch) = 0;
		Task.Word(WorldMapTaskWord::PitchState) = 0;
		Task.Heading = StateWord(Context, WorldMapStateLayout::VehicleHeading);
		switch (VehicleFlags(Context) & WorldMapStateLayout::VehicleTypeMask)
		{
		case 0:
			Result = WorldMapTaskState::Idle;
			break;
		case 1:
		case 2:
			Task.Speed = 0xC;
			break;
		case 3:
		case 4:
			Task.Speed = 0x20;
			break;
		default:
			break;
		}
		switch (Context.TravelMode)
		{
		case 1:
			Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
			break;
		case 2:
		case 3:
			Task.Step = VehicleStep::ParkedNear;
			Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
			break;
		case 4:
		case 5:
			Task.Step = VehicleStep::Sailing;
			Task.Position.X = Context.Start.X;
			Task.Position.Z = Context.Start.Z;
			Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z) - 0x40000;
			Task.Heading = static_cast<int16>(Context.StartYaw);
			Task.Word(WorldMapTaskWord::Aboard) = Context.MemberCount;
			SetControlled(Context, Task);
			HideDeckAndParts(Context, 1);
			break;
		case 7:
			Task.Step = VehicleStep::Flying;
			Task.Position.X = Context.Start.X;
			Task.Position.Z = Context.Start.Z;
			Task.Heading = static_cast<int16>(Context.StartYaw);
			Task.Word(WorldMapTaskWord::Aboard) = Context.MemberCount;
			Task.Position.Y = Context.Start.Y;
			SetControlled(Context, Task);
			break;
		default:
			break;
		}
		PlaceHull(Context, Task);
		TurnHull(Context, Task);
		SetStateWord(Context, WorldMapStateLayout::VehicleX, static_cast<int16>(Task.Position.X >> 12));
		SetStateWord(Context, WorldMapStateLayout::VehicleY, static_cast<int16>(Task.Position.Y >> 12));
		SetStateWord(Context, WorldMapStateLayout::VehicleZ, static_cast<int16>(Task.Position.Z >> 12));
		SetStateWord(Context, WorldMapStateLayout::VehicleHeading, Task.Heading);
		switch (Context.SceneNumber)
		{
		case 2:
			Task.Step = VehicleStep::ReturnFromNisan;
			return Result;
		case 3:
			Task.Step = VehicleStep::VoyageToVillage;
			break;
		case 4:
			Task.Step = VehicleStep::VoyageToFactory;
			break;
		case 5:
			Task.Step = VehicleStep::VoyageToNisan;
			break;
		default:
			return Result;
		}
		Task.Word(WorldMapTaskWord::Waypoint) = 0;
		return Result;
	}

	FResult RestoreVehicle(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		PlaceHull(Context, Task);
		FResult Result = WorldMapTaskState::Update;
		if ((VehicleFlags(Context) & WorldMapStateLayout::VehicleTypeMask) == 0)
		{
			Result = WorldMapTaskState::Idle;
		}
		switch (Context.TravelMode)
		{
		case 1:
			Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
			break;
		case 2:
		case 3:
			Task.Step = VehicleStep::ParkedNear;
			Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
			break;
		case 4:
		case 5:
			HideDeckAndParts(Context, 1);
			break;
		case 6:
		case 7:
			HideDeckAndParts(Context, 0);
			break;
		default:
			break;
		}
		TurnHull(Context, Task);
		return Result;
	}

	FResult InitVehicleParts(FWorldMapContext& Context, int32 Slot)
	{
		AttachObject(Context, HullObject, FirstSpinningObject);
		AttachObject(Context, HullObject, SecondSpinningObject);
		FWorldMapTask& Task = TaskAt(Context, Slot);
		Task.Step = 0;
		const FWorldMapObject& First = Context.Objects[FirstSpinningObject];
		const FWorldMapObject& Second = Context.Objects[SecondSpinningObject];
		Task.Position = {First.X, First.Y, First.Z, First.W};
		Task.Direction = {Second.X, Second.Y, Second.Z, Second.W};
		Task.Word(WorldMapTaskWord::PartAngleA) = 0;
		Task.Word(WorldMapTaskWord::PartAngleB) = 0x40;
		if (Context.TravelMode < 8 && Context.TravelMode > 3)
		{
			Task.Step = 3;
			Task.Word(WorldMapTaskWord::PartSpinB) = 0x80;
			Task.Word(WorldMapTaskWord::PartSpinA) = 0x80;
		}
		return WorldMapTaskState::Update;
	}

	FResult RestoreVehicleParts(FWorldMapContext& Context, int32)
	{
		AttachObject(Context, HullObject, FirstSpinningObject);
		AttachObject(Context, HullObject, SecondSpinningObject);
		return WorldMapTaskState::Update;
	}

	FResult UpdateVehicleParts(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		if (Task.Message == SpinUpMessage)
		{
			Task.Message = 0;
			Task.Step = 1;
		}
		else if (Task.Message == SpinDownMessage)
		{
			Task.Message = 0;
			Task.Step = 2;
		}
		int32& SpinA = Task.Word(WorldMapTaskWord::PartSpinA);
		int32& SpinB = Task.Word(WorldMapTaskWord::PartSpinB);
		switch (Task.Step)
		{
		case 0:
			SpinA = 0;
			SpinB = 0;
			break;
		case 1:
			SpinA += 4;
			if (SpinA > 0x10)
			{
				SpinB += 4;
			}
			if (SpinA > 0x7F)
			{
				SpinA = 0x80;
			}
			if (SpinB > 0x7F)
			{
				SpinB = 0x80;
			}
			if (SpinA > 0x7F && SpinB > 0x7F)
			{
				Task.Step = 3;
			}
			break;
		case 2:
			SpinA -= 4;
			if (SpinA < 0x70)
			{
				SpinB -= 4;
			}
			if (SpinA < 0)
			{
				SpinA = 0;
			}
			if (SpinB < 0)
			{
				SpinB = 0;
			}
			if (SpinA == 0 && SpinB == 0)
			{
				Task.Step = 0;
			}
			break;
		default:
			break;
		}
		Task.Word(WorldMapTaskWord::PartAngleA) += SpinA;
		Task.Word(WorldMapTaskWord::PartAngleB) -= SpinB;
		FWorldMapObject& First = Context.Objects[FirstSpinningObject];
		FWorldMapObject& Second = Context.Objects[SecondSpinningObject];
		First.Matrix.M = RotationZyx({0, static_cast<int16>(Task.Word(WorldMapTaskWord::PartAngleA)), First.Angles[2]}).M;
		Second.Matrix.M = RotationZyx({0, static_cast<int16>(Task.Word(WorldMapTaskWord::PartAngleB)), Second.Angles[2]}).M;
		return WorldMapTaskState::Update;
	}

	FResult UpdateVehicle(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		FResult Result = WorldMapTaskState::Update;
		if (Task.Message == AboardMessage)
		{
			const int16 Message = Task.Message;
			Task.Message = 0;
			if (++Task.Word(WorldMapTaskWord::Aboard) == Context.MemberCount)
			{
				Result = Board(Context, Task, Message);
			}
		}
		switch (Task.Step)
		{
		case VehicleStep::Parked:
		case VehicleStep::ParkedNear:
			Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z) - 0x1000;
			Context.Colliders.Add(Slot, Task.Position, ParkedRadius, ParkedHeight);
			break;
		case VehicleStep::Flying:
			Fly(Context, Task);
			break;
		case VehicleStep::Sailing:
		case VehicleStep::AtSea:
			Sail(Context, Task, Task.Step == VehicleStep::AtSea);
			break;
		case VehicleStep::Climbing:
			Task.Position.Y -= 0x8000;
			if (Task.Position.Y < FlightHeight + 1)
			{
				Task.Position.Y = FlightHeight;
				if (Task.Message == BehindShipMessage)
				{
					Task.Word(WorldMapTaskWord::FlightStarted) = 0;
					Task.Step = VehicleStep::Flying;
					Task.Message = 0;
					PlaySong(Context, EWorldMapSong::Vehicle);
				}
			}
			SetControlled(Context, Task);
			break;
		case VehicleStep::Sinking:
			Task.Position.Y += 0x800;
			if (Task.Word(WorldMapTaskWord::Draft) <= Task.Position.Y)
			{
				Task.Position.Y = Task.Word(WorldMapTaskWord::Draft);
				Task.Step = VehicleStep::Sailing;
				HideDeckAndParts(Context, 1);
			}
			SetControlled(Context, Task);
			break;
		case VehicleStep::Landing:
			Task.Position.Y += 0x8000;
			if (Task.Position.Y < Task.Word(WorldMapTaskWord::Draft))
			{
				int32& DustOnce = Task.Word(WorldMapTaskWord::Waypoint);
				if (Task.Position.Y > -0x200000 && DustOnce != 0)
				{
					DustOnce = 0;
					const std::array<int16, 4> Place{static_cast<int16>(Task.Position.X >> 12), static_cast<int16>(Task.Word(WorldMapTaskWord::Draft) >> 12),
						static_cast<int16>(Task.Position.Z >> 12), 0};
					Context.Particles.Start(TakeOffEffect(Context, Task), Place, std::nullopt);
				}
			}
			else
			{
				Task.Position.Y = Task.Word(WorldMapTaskWord::Draft);
				Task.Step = VehicleStep::ParkedNear;
				PlaySong(Context, EWorldMapSong::Map);
				SendMessage(Context, FWorldMapContext::CameraTargetSlot, FollowPadMessage);
				SendMessage(Context, FWorldMapContext::CameraDistanceSlot, GroundPresetsMessage);
				Disembark(Context, Task);
				Context.Particles.Stop(0);
				Context.Particles.Stop(WakeEffect);
			}
			Context.Controlled.Y = Task.Position.Y;
			break;
		case VehicleStep::Rising:
			Task.Position.Y -= 0x800;
			if (Task.Position.Y <= Task.Word(WorldMapTaskWord::Draft))
			{
				Task.Position.Y = Task.Word(WorldMapTaskWord::Draft);
				Task.Step = VehicleStep::ParkedNear;
				Disembark(Context, Task);
				HideDeckAndParts(Context, 0);
			}
			Context.Controlled.Y = Task.Position.Y;
			break;
		case VehicleStep::Tunnelling:
			Task.Position.Y += 0x2000;
			if (Task.Word(WorldMapTaskWord::Draft) <= Task.Position.Y)
			{
				Task.Position.Y = Task.Word(WorldMapTaskWord::Draft);
				Context.Particles.Stop(DustEffect);
				HideDeckAndParts(Context, 1);
				Context.Objects[HullObject].Hidden = 1;
				++Task.Step;
				HeadFor(Context, Task, {NisanTunnelEnd[0] << 12, Task.Word(WorldMapTaskWord::Draft), NisanTunnelEnd[1] << 12, 0});
			}
			SetControlled(Context, Task);
			break;
		case VehicleStep::Tunnelling + 1:
			if (WalkOn(Context, Task) == Arrived)
			{
				++Task.Step;
			}
			Task.Position.Y = Task.Word(WorldMapTaskWord::Draft);
			SetControlled(Context, Task);
			break;
		case VehicleStep::Tunnelling + 2:
			Task.Step = VehicleStep::Entered;
			Leave(Context);
			break;
		case VehicleStep::ReturnFromNisan:
			Task.Position.Y = TunnelDepth;
			++Task.Step;
			HeadFor(Context, Task, {NisanTunnelExit[0] << 12, TunnelDepth, NisanTunnelExit[1] << 12, 0});
			SetControlled(Context, Task);
			HideDeckAndParts(Context, 1);
			Context.Objects[HullObject].Hidden = 1;
			TurnHull(Context, Task);
			break;
		case VehicleStep::ReturnFromNisan + 1:
			if (WalkOn(Context, Task) == Arrived)
			{
				Task.Position.Y = TunnelDepth;
				++Task.Step;
				Task.Word(WorldMapTaskWord::Draft) = Context.Ground.Height(Task.Position.X, Task.Position.Z) + ShipDraft;
				const std::array<int16, 4> Place{static_cast<int16>(Task.Position.X >> 12), static_cast<int16>(Task.Word(WorldMapTaskWord::Draft) >> 12),
					static_cast<int16>(Task.Position.Z >> 12), 0};
				Context.Particles.Start(DustEffect, Place, FGteVector{});
				Context.Objects[HullObject].Hidden = 0;
			}
			Task.Position.Y = TunnelDepth;
			SetControlled(Context, Task);
			break;
		case VehicleStep::ReturnFromNisan + 2:
			Task.Position.Y -= 0x2000;
			if (Task.Position.Y <= Task.Word(WorldMapTaskWord::Draft))
			{
				Task.Position.Y = Task.Word(WorldMapTaskWord::Draft);
				Context.Particles.Stop(DustEffect);
				Task.Step = VehicleStep::Sailing;
			}
			SetControlled(Context, Task);
			break;
		case VehicleStep::VoyageToVillage:
		case VehicleStep::VoyageToVillage + 1:
		case VehicleStep::VoyageToVillage + 2:
			VoyageOverLand(Context, Task);
			break;
		case VehicleStep::VoyageToFactory:
		case VehicleStep::VoyageToFactory + 1:
		case VehicleStep::VoyageToFactory + 2:
			VoyageAtSea(Context, Task);
			break;
		case VehicleStep::VoyageToNisan:
		case VehicleStep::VoyageToNisan + 1:
		case VehicleStep::VoyageToNisan + 2:
		case VehicleStep::VoyageToNisan + 3:
		case VehicleStep::VoyageToNisan + 4:
		case VehicleStep::VoyageToNisan + 5:
			VoyageToNisan(Context, Task);
			break;
		default:
			break;
		}
		for (const int32 Number : {HullObject, DeckObject})
		{
			FWorldMapObject& Part = Context.Objects[static_cast<std::size_t>(Number)];
			Part.X = Task.Position.X >> 12;
			Part.Y = Task.Position.Y >> 12;
			Part.Z = Task.Position.Z >> 12;
		}
		SetStateWord(Context, WorldMapStateLayout::VehicleX, static_cast<int16>(Task.Position.X >> 12));
		SetStateWord(Context, WorldMapStateLayout::VehicleY, static_cast<int16>(Task.Position.Y >> 12));
		SetStateWord(Context, WorldMapStateLayout::VehicleZ, static_cast<int16>(Task.Position.Z >> 12));
		SetStateWord(Context, WorldMapStateLayout::VehicleHeading, Task.Heading);
		if (Task.Step == VehicleStep::Flying || Task.Step == VehicleStep::Climbing || Task.Step == VehicleStep::Landing)
		{
			AddShadow(Context, VehicleShadow, Task.Position);
		}
		return Result;
	}
}
