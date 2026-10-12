#include "Support/StorySweepWorldMap.h"

#include "Game/GameState.h"
#include "WorldMap/WorldMapContext.h"
#include "WorldMap/WorldMapMovement.h"
#include "WorldMap/WorldMapScene.h"
#include "WorldMap/WorldMapTerrain.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <utility>

namespace Weltall::Testing
{
	namespace
	{
		/** How far apart the points of the ground a travel is followed over lie: half a terrain cell. */
		constexpr int32 Step = 64;
		// The scene's lists of places by how the party travels.
		constexpr int32 FootList = 0;
		constexpr int32 GearList = 1;
		constexpr int32 ShipList = 2;
		/** A record's kinds: entered by the confirm button (0 to 2), 1 as soon as a party on foot stands in it. */
		constexpr int16 LastEnteredKind = 2;
		constexpr int16 WalkedIntoKind = 1;
		// The game state's words the world map starts from: the party's place on foot (whole units), whether the
		// menu was open as the state was kept (the party then starts where it stood), each slot's riding byte, the
		// ship's place.
		constexpr std::size_t PartyX = 0x1820;
		constexpr std::size_t PartyZ = 0x1822;
		constexpr std::size_t ShipX = 0x182C;
		constexpr std::size_t ShipY = 0x182E;
		constexpr std::size_t ShipZ = 0x1830;
		constexpr std::size_t ShipHeading = 0x1832;
		constexpr std::size_t MenuOpen = 0x1836;
		constexpr std::size_t Riding = 0x22B1;
		constexpr std::size_t PartySlots = 3;
		/** The last of the ship's steps in which the pad has it: parked, flying, sailing the sand, at sea. */
		constexpr int16 LastSteeredStep = 4;

		void PutWord(FGameState& State, std::size_t Offset, uint16 Value)
		{
			std::memcpy(State.Block().data() + Offset, &Value, sizeof(Value));
		}

		/** The ground a way of travelling reaches from a start, as points Step apart over the wrapping world. */
		class FTravelledGround
		{
		public:
			FTravelledGround(const FWorldMapContext& Context, int32 Mode, const std::vector<FWorldMapTrigger>* WalkedInto)
				: Ground(Context.Ground), TravelMode(Mode)
			{
				Columns = Ground.Blocks().BlocksX() * FWorldMapTerrain::BlockSide / Step;
				Rows = Ground.Blocks().BlocksZ() * FWorldMapTerrain::BlockSide / Step;
				Reached.assign(static_cast<std::size_t>(Columns) * static_cast<std::size_t>(Rows), 0);
				std::vector<std::pair<int32, int32>> Pending;
				const int32 FromColumn = Wrapped((Context.Controlled.X >> 12) / Step, Columns);
				const int32 FromRow = Wrapped((Context.Controlled.Z >> 12) / Step, Rows);
				Reached[Index(FromColumn, FromRow)] = 1;
				Pending.emplace_back(FromColumn, FromRow);
				while (!Pending.empty())
				{
					const auto [AtColumn, AtRow] = Pending.back();
					Pending.pop_back();
					// A strip that takes the party in ends the way: nothing is walked to through it.
					if (WalkedInto != nullptr && IsIn(*WalkedInto, AtColumn * Step + Step / 2, AtRow * Step + Step / 2, true))
					{
						continue;
					}
					for (int32 Along = -1; Along <= 1; ++Along)
					{
						for (int32 Across = -1; Across <= 1; ++Across)
						{
							const int32 Column = Wrapped(AtColumn + Across, Columns);
							const int32 Row = Wrapped(AtRow + Along, Rows);
							if (Reached[Index(Column, Row)] == 0 && Crosses(Column * Step + Step / 2, Row * Step + Step / 2))
							{
								Reached[Index(Column, Row)] = 1;
								Pending.emplace_back(Column, Row);
							}
						}
					}
				}
			}

			/** Whether the travel reaches the place (whole units): ground it may stand on, by a point it came to. */
			bool Reaches(int32 X, int32 Z) const
			{
				return Crosses(X, Z) && Reached[Index(Wrapped(X / Step, Columns), Wrapped(Z / Step, Rows))] != 0;
			}

			/** Whether a place lies in one of the records' rectangles: any of them, or only those walked into. */
			static bool IsIn(const std::vector<FWorldMapTrigger>& Records, int32 X, int32 Z, bool bWalkedIntoOnly)
			{
				return std::any_of(Records.begin(), Records.end(), [&](const FWorldMapTrigger& Record)
					{
						return (!bWalkedIntoOnly || Record.Kind == WalkedIntoKind) && X >= Record.X && X <= Record.X + Record.Width && Z >= Record.Z
							&& Z <= Record.Z + Record.Height;
					});
			}

		private:
			static int32 Wrapped(int32 Value, int32 Count)
			{
				return ((Value % Count) + Count) % Count;
			}

			std::size_t Index(int32 Column, int32 Row) const
			{
				return static_cast<std::size_t>(Row) * static_cast<std::size_t>(Columns) + static_cast<std::size_t>(Column);
			}

			bool Crosses(int32 X, int32 Z) const
			{
				return Ground.CanTraverse(TravelMode, Ground.TerrainType(X << 12, Z << 12));
			}

			const FWorldMapGround& Ground;
			int32 TravelMode = 0;
			int32 Columns = 0;
			int32 Rows = 0;
			std::vector<uint8> Reached;
		};

		/** Where the travel can stand in a record's rectangle: its middle, else one of the points a quarter in from its corners. */
		std::optional<std::pair<int32, int32>> StandIn(const FWorldMapTrigger& Record, const FTravelledGround& Travelled)
		{
			const std::array<std::pair<int32, int32>, 5> Parts = {{{2, 2}, {1, 1}, {3, 1}, {1, 3}, {3, 3}}};
			for (const auto& [Across, Along] : Parts)
			{
				const int32 X = Record.X + Record.Width * Across / 4;
				const int32 Z = Record.Z + Record.Height * Along / 4;
				if (Travelled.Reaches(X, Z))
				{
					return std::pair{X, Z};
				}
			}
			return std::nullopt;
		}

		/** The list's records the travel reaches and the button or the walk enters, each field map and entry once. */
		void AddWays(const FWorldMapContext& Context, int32 List, const FTravelledGround& Travelled, bool bOnFoot, std::vector<FWorldMapWay>& Ways)
		{
			const std::optional<std::vector<FWorldMapTrigger>>& Records = Context.Scene->TriggerLists[static_cast<std::size_t>(List)];
			if (!Records)
			{
				return;
			}
			std::vector<std::pair<int16, int16>> Entered;
			for (const FWorldMapWay& Way : Ways)
			{
				const FWorldMapTrigger& Record = (*Context.Scene->TriggerLists[static_cast<std::size_t>(Way.List)])[static_cast<std::size_t>(Way.Record)];
				Entered.emplace_back(Record.Map, Record.Scene);
			}
			for (std::size_t Number = 0; Number < Records->size(); ++Number)
			{
				const FWorldMapTrigger& Record = (*Records)[Number];
				const std::pair<int16, int16> Leads{Record.Map, Record.Scene};
				if (Record.Kind < 0 || Record.Kind > LastEnteredKind || std::find(Entered.begin(), Entered.end(), Leads) != Entered.end())
				{
					continue;
				}
				if (const auto Place = StandIn(Record, Travelled))
				{
					Entered.push_back(Leads);
					Ways.push_back({List, static_cast<int32>(Number), Place->first, Place->second, bOnFoot});
				}
			}
		}
	}

	std::vector<FWorldMapWay> WorldMapWays(const FWorldMapContext& Context)
	{
		std::vector<FWorldMapWay> Ways;
		if (Context.Scene == nullptr || Context.bCutscene)
		{
			return Ways;
		}
		const std::optional<std::vector<FWorldMapTrigger>>& FootRecords = Context.Scene->TriggerLists[FootList];
		const std::vector<FWorldMapTrigger>* WalkedInto = FootRecords ? &*FootRecords : nullptr;
		switch (Context.TravelMode)
		{
		case WorldMapTravel::OnFoot:
			AddWays(Context, FootList, FTravelledGround(Context, WorldMapTravel::OnFoot, WalkedInto), false, Ways);
			break;
		case WorldMapTravel::Gears:
			AddWays(Context, GearList, FTravelledGround(Context, WorldMapTravel::Gears, nullptr), false, Ways);
			AddWays(Context, FootList, FTravelledGround(Context, WorldMapTravel::OnFoot, WalkedInto), true, Ways);
			break;
		case WorldMapTravel::SandShip:
		case WorldMapTravel::SeaShip:
		case WorldMapTravel::Flying:
			AddWays(Context, ShipList, FTravelledGround(Context, Context.TravelMode, nullptr), false, Ways);
			break;
		default:
			break;
		}
		return Ways;
	}

	bool IsWorldMapVoyage(const FWorldMapContext& Context)
	{
		const FWorldMapTask& Ship = Context.Tasks[FWorldMapContext::VehicleSlot];
		const bool bShip = Ship.Update == EWorldMapRoutine::UpdateVehicle;
		return !Context.bCutscene && bShip && Ship.Step > LastSteeredStep;
	}

	void KeepShipOnWorldMap(FGameState& State, const FWorldMapContext& Context)
	{
		if (Context.bCutscene || Context.TravelMode < WorldMapTravel::SandShip || Context.TravelMode > WorldMapTravel::Flying)
		{
			return;
		}
		PutWord(State, ShipX, static_cast<uint16>(Context.Controlled.X >> 12));
		PutWord(State, ShipY, static_cast<uint16>(Context.Controlled.Y >> 12));
		PutWord(State, ShipZ, static_cast<uint16>(Context.Controlled.Z >> 12));
		PutWord(State, ShipHeading, static_cast<uint16>(Context.ControlledHeading));
		PutWord(State, MenuOpen, 1);
	}

	void StandOnWorldMap(FGameState& State, const FWorldMapWay& Way)
	{
		PutWord(State, MenuOpen, 1);
		if (Way.List == ShipList)
		{
			PutWord(State, ShipX, static_cast<uint16>(Way.X));
			PutWord(State, ShipZ, static_cast<uint16>(Way.Z));
			return;
		}
		PutWord(State, PartyX, static_cast<uint16>(Way.X));
		PutWord(State, PartyZ, static_cast<uint16>(Way.Z));
		if (Way.bOnFoot)
		{
			std::fill_n(State.Block().begin() + Riding, PartySlots, uint8{0});
		}
	}
}
