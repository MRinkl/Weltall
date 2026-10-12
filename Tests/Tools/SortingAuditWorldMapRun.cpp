#include "Tools/SortingAuditWorldMapRun.h"

#include "Game/KernelState.h"
#include "Timing/PsxTiming.h"
#include "WorldMap/WorldMapCameraOffsets.h"
#include "WorldMap/WorldMapTerrain.h"

#include <algorithm>
#include <cstdlib>

namespace Weltall::Testing
{
	namespace
	{
		/** The player's framing a frame is audited under: the game's own, then the view turned, tilted and zoomed. */
		struct FFraming
		{
			const char* Name = "";
			FWorldMapCameraOffsets Offsets;
		};

		constexpr FFraming Framings[] = {{"game", {}}, {"turn45", {1.0f, 0.0f, 45.0f}}, {"turn-45", {1.0f, 0.0f, -45.0f}}, {"turn90", {1.0f, 0.0f, 90.0f}},
			{"turn-90", {1.0f, 0.0f, -90.0f}}, {"turn180", {1.0f, 0.0f, 180.0f}}, {"up", {1.0f, FWorldMapCameraOffsets::MaxTilt, 0.0f}},
			{"down", {1.0f, FWorldMapCameraOffsets::MinTilt, 0.0f}}, {"near", {FWorldMapCameraOffsets::MinZoom, 0.0f, 0.0f}},
			{"far", {FWorldMapCameraOffsets::MaxZoom, 0.0f, 0.0f}}};

		/** The world map's places in the game state from 400h: a place word there names the world map's location. */
		constexpr uint16 FirstWorldMapPlace = 0x400;

		/** The ticks of the map after the entry's fade at which a playable scene's frames are audited, its own fade-in past. */
		constexpr int32 Looks[] = {90, 240};
		/** After Circle aboard: the flying ship coming down or the sailing one rising, then the ship parked beside the gears. */
		constexpr int32 LandingLooks[] = {40, 200};

		/** The travel's party: Fei, Citan and Bart, each with his own gear (the same number), as the world map's oracles stage them. */
		constexpr std::array<uint8, 3> TravelParty{0, 2, 3};
		/** The vehicle flags' bits beside its type: the party starts at the vehicle, aboard. */
		constexpr uint16 StartAtVehicle = 0x2000;
		constexpr uint16 Aboard = 0x4000;
		/** The Yggdrasil's types: the sand ship, the Yggdrasil II (sand and sea), the flying Yggdrasil III. */
		constexpr uint16 SandShip = 1;
		constexpr uint16 SeaShip = 2;
		constexpr uint16 FlyingShip = 3;
		/**
		 * The story's values from which the bridge launches each type (its script on map 290): the sand ship once the
		 * Yggdrasil is the party's (era 2), the Yggdrasil II from era 4's first value, the Yggdrasil III from era 5's.
		 */
		constexpr uint16 SandShipFrom = 54;
		constexpr uint16 SeaShipFrom = 149;
		constexpr uint16 FlyingShipFrom = 186;
		/** The highest the flying ship climbs (y is down), among the clouds' layer. */
		constexpr int16 FlightCeiling = -640;
		/** A sailing ship's place: a terrain cell's middle within this many cells of the arrival, the ship's breadth about it of the same type. */
		constexpr int32 TerrainCell = 128;
		constexpr int32 TerrainReach = 24;
		constexpr int32 ShipBreadth = 48;
		/** Ship places nearer than this to one already listed are left out. */
		constexpr int32 ShipSpacing = 1024;
		/** A cutscene's frames: every this many ticks after the entry's fade, to its end or the last. */
		constexpr int32 CutsceneLookEvery = 45;
		constexpr int32 CutsceneLastLook = 3600;

		/** A session and the world map program it runs, declared after it so that it is destroyed first. */
		struct FStartedMap
		{
			std::unique_ptr<FGameSession> Session;
			std::unique_ptr<FWorldMapProgram> Program;
		};

		/** The entry's party in gears or aboard its ship, where it has one. */
		void StageTravel(const FWorldMapSortingEntry& Entry, FGameState& State)
		{
			if (!Entry.bRiding && Entry.VehicleType == 0)
			{
				return;
			}
			State.SetParty(TravelParty);
			for (std::size_t Slot = 0; Slot < TravelParty.size(); ++Slot)
			{
				State.SetCharacterGear(TravelParty[Slot], TravelParty[Slot]);
				State.SetPartySlotInGear(static_cast<int32>(Slot), Entry.bRiding ? 1 : 0);
			}
			if (Entry.VehicleType != 0)
			{
				const auto& [X, Y, Z, Heading] = Entry.Vehicle;
				State.SetWorldMapVehicle(X, Y, Z, Heading);
				State.SetWorldMapFlags(static_cast<uint16>(Entry.VehicleType | StartAtVehicle | Aboard));
			}
		}

		/** The world map started as the entry has it, culled as the port culls by default; a save's disc becomes the entry's. */
		TResult<FStartedMap> StartMap(const FPlayedGame& Game, FWorldMapSortingEntry& Entry)
		{
			auto Session = Game.Start(Entry.Disc, Entry.Save);
			if (Session && Entry.Save != nullptr && (*Session)->Game().SavedDisc() != Entry.Disc)
			{
				// A save of the second disc plays on that disc's own files.
				Entry.Disc = (*Session)->Game().SavedDisc();
				Session = Game.Start(Entry.Disc, Entry.Save);
			}
			if (!Session)
			{
				return Session.Error();
			}
			FStartedMap Started;
			Started.Session = std::move(*Session);
			FGameState& State = Started.Session->Game();
			if (Entry.Save == nullptr)
			{
				State.SetSavedEventVariable(0, Entry.Story);
				State.SetWorldMapPlace(FirstWorldMapPlace, Entry.Entrance, 0, Entry.Scene);
				StageTravel(Entry, State);
			}
			else if (LoadedGameProgram(State) != EGameProgram::WorldMap)
			{
				return MakeError("the save was made on a field map");
			}
			auto Program = FWorldMapProgram::Start(*Started.Session);
			if (!Program)
			{
				return Program.Error();
			}
			Started.Program = std::move(*Program);
			Started.Program->SetCulling(EWorldMapCulling::InRange);
			return Started;
		}

		/** A place in whole units, wrapped into the world. */
		std::array<int32, 2> Wrapped(const FWorldMapGround& Ground, int32 X, int32 Z)
		{
			FWorldMapVector Point{X << 12, 0, Z << 12};
			Ground.Wrap(Point);
			return {Point.X >> 12, Point.Z >> 12};
		}

		/** The middle of the cell nearest (X, Z) whose ground is of the type for a ship's breadth around it, if one lies within reach. */
		std::optional<std::array<int32, 2>> NearestTerrain(const FWorldMapGround& Ground, int32 X, int32 Z, int32 Type)
		{
			const auto IsOfType = [&](int32 AtX, int32 AtZ)
			{
				const auto [WrappedX, WrappedZ] = Wrapped(Ground, AtX, AtZ);
				return Ground.TerrainType(WrappedX << 12, WrappedZ << 12) == Type;
			};
			std::optional<std::array<int32, 2>> Nearest;
			int64 NearestDistance = 0;
			const int32 FirstX = X - X % TerrainCell + TerrainCell / 2;
			const int32 FirstZ = Z - Z % TerrainCell + TerrainCell / 2;
			for (int32 Row = -TerrainReach; Row <= TerrainReach; ++Row)
			{
				for (int32 Column = -TerrainReach; Column <= TerrainReach; ++Column)
				{
					const int32 AtX = FirstX + Column * TerrainCell;
					const int32 AtZ = FirstZ + Row * TerrainCell;
					const int64 Distance = int64{AtX - X} * (AtX - X) + int64{AtZ - Z} * (AtZ - Z);
					if ((Nearest && Distance >= NearestDistance) || !IsOfType(AtX, AtZ) || !IsOfType(AtX - ShipBreadth, AtZ - ShipBreadth) ||
						!IsOfType(AtX + ShipBreadth, AtZ - ShipBreadth) || !IsOfType(AtX - ShipBreadth, AtZ + ShipBreadth) ||
						!IsOfType(AtX + ShipBreadth, AtZ + ShipBreadth))
					{
						continue;
					}
					Nearest = Wrapped(Ground, AtX, AtZ);
					NearestDistance = Distance;
				}
			}
			return Nearest;
		}

		/** Whether a place lies within the ships' spacing of one of those listed. */
		bool IsNearAny(const FWorldMapGround& Ground, const std::vector<std::array<int32, 2>>& Listed, const std::array<int32, 2>& Place)
		{
			return std::any_of(Listed.begin(), Listed.end(),
				[&](const std::array<int32, 2>& Other)
				{
					int32 X = Place[0] - Other[0];
					int32 Z = Place[1] - Other[1];
					Ground.WrapDeltaUnits(X, Z);
					return std::abs(X) < ShipSpacing && std::abs(Z) < ShipSpacing;
				});
		}
	}

	TResult<std::vector<int16>> FWorldMapSortingRun::Arrivals(const FPlayedGame& Game, int32 Disc, uint16 Story)
	{
		FWorldMapSortingEntry Entry;
		Entry.Disc = Disc;
		Entry.Story = Story;
		auto Started = StartMap(Game, Entry);
		if (!Started)
		{
			return Started.Error();
		}
		std::vector<int16> Ids;
		for (const FWorldMapEntrance& Arrival : Started->Program->Runtime().Scene().Entrances)
		{
			Ids.push_back(Arrival.Id);
		}
		return Ids;
	}

	TResult<std::vector<FWorldMapSortingEntry>> FWorldMapSortingRun::Travels(const FPlayedGame& Game, int32 Era, int32 Disc, uint16 Story)
	{
		FWorldMapSortingEntry Base;
		Base.Disc = Disc;
		Base.Story = Story;
		auto Started = StartMap(Game, Base);
		if (!Started)
		{
			return Started.Error();
		}
		const FWorldMapRuntime& Runtime = Started->Program->Runtime();
		const FWorldMapGround& Ground = Runtime.Context().Ground;
		const uint16 Ship = Story >= FlyingShipFrom ? FlyingShip : Story >= SeaShipFrom ? SeaShip : Story >= SandShipFrom ? SandShip : 0;
		std::vector<FWorldMapSortingEntry> Entries;
		std::array<std::vector<std::array<int32, 2>>, 3> Listed;
		const auto Add = [&](char Kind, const FWorldMapEntrance& Arrival, std::size_t List, const std::array<int32, 2>& Place, int16 Y)
		{
			if (IsNearAny(Ground, Listed[List], Place))
			{
				return;
			}
			FWorldMapSortingEntry Entry = Base;
			Entry.Place = Kind + std::to_string(Era) + "." + std::to_string(Arrival.Id);
			Entry.Entrance = static_cast<uint16>(Arrival.Id);
			Entry.VehicleType = Ship;
			Entry.Vehicle = {static_cast<int16>(Place[0]), Y, static_cast<int16>(Place[1]), static_cast<int16>(0x400 * (Listed[List].size() % 4))};
			Entries.push_back(Entry);
			Listed[List].push_back(Place);
		};
		for (const FWorldMapEntrance& Arrival : Runtime.Scene().Entrances)
		{
			FWorldMapSortingEntry Riding = Base;
			Riding.Place = "g" + std::to_string(Era) + "." + std::to_string(Arrival.Id);
			Riding.Entrance = static_cast<uint16>(Arrival.Id);
			Riding.bRiding = true;
			Entries.push_back(Riding);
		}
		for (const FWorldMapEntrance& Arrival : Runtime.Scene().Entrances)
		{
			if (Ship == FlyingShip)
			{
				Add('f', Arrival, 0, Wrapped(Ground, Arrival.X, Arrival.Z), FlightCeiling);
				continue;
			}
			if (Ship == 0)
			{
				continue;
			}
			if (const auto Sand = NearestTerrain(Ground, Arrival.X, Arrival.Z, WorldMapTerrainType::Sand))
			{
				Add('y', Arrival, 1, *Sand, 0);
			}
			if (Ship == SeaShip)
			{
				if (const auto Sea = NearestTerrain(Ground, Arrival.X, Arrival.Z, WorldMapTerrainType::Sea))
				{
					Add('s', Arrival, 2, *Sea, 0);
				}
			}
		}
		return Entries;
	}

	std::optional<FError> FWorldMapSortingRun::Run()
	{
		if (auto Error = Start())
		{
			return Error;
		}
		Report.emplace(Rows, Entry.Place, static_cast<uint32>(Options.Scale * Options.Scale), bPictures);
		const bool bCutscene = Program->Runtime().IsCutscene();
		if (bCutscene)
		{
			for (int32 At = CutsceneLookEvery; At <= CutsceneLastLook && IsRunning(); At += CutsceneLookEvery)
			{
				if (auto Error = Advance(At))
				{
					return Error;
				}
				Look("t" + std::to_string(At));
			}
			return std::nullopt;
		}
		for (const int32 At : Looks)
		{
			if (auto Error = Advance(At))
			{
				return Error;
			}
			if (auto Error = LookAround("t" + std::to_string(At)))
			{
				return Error;
			}
		}
		if (Entry.VehicleType == 0 || !IsRunning())
		{
			return std::nullopt;
		}
		// Circle aboard: the sailing ship rises and lets the party out in gears; the flying one lands where its gears can step out.
		if (auto Error = Tick({}, FPadState{FPadState::Bit(EPadButton::Circle)}))
		{
			return Error;
		}
		const int32 Pressed = Ticks;
		for (const int32 After : LandingLooks)
		{
			if (auto Error = Advance(Pressed + After))
			{
				return Error;
			}
			if (auto Error = LookAround("l" + std::to_string(After)))
			{
				return Error;
			}
		}
		return std::nullopt;
	}

	void FWorldMapSortingRun::WritePictures(const std::filesystem::path& Folder) const
	{
		if (Report)
		{
			Report->WritePictures(Folder);
		}
	}

	std::optional<FError> FWorldMapSortingRun::Start()
	{
		Program.reset();
		auto Started = StartMap(Game, Entry);
		if (!Started)
		{
			return Started.Error();
		}
		Session = std::move(Started->Session);
		Program = std::move(Started->Program);
		Ticks = 0;
		return std::nullopt;
	}

	std::optional<FError> FWorldMapSortingRun::Tick(const FWorldMapCameraOffsets& Offsets, FPadState Held)
	{
		Session->WorldMapCameraOffsets() = Offsets;
		const std::vector<FPadState> PerVblank(static_cast<std::size_t>(Program->VblanksPerFrame()), Held);
		Game.Unheard->Advance(PsxTiming::FrameSeconds * static_cast<double>(PerVblank.size()));
		if (auto Error = Program->Frame(PerVblank))
		{
			return Error;
		}
		if (Program->ScreenFade().Picture == FWorldMapScreenFade::EPicture::None)
		{
			++Ticks;
		}
		return std::nullopt;
	}

	std::optional<FError> FWorldMapSortingRun::Advance(int32 To)
	{
		while (Ticks < To && IsRunning())
		{
			if (auto Error = Tick())
			{
				return Error;
			}
		}
		return std::nullopt;
	}

	std::optional<FError> FWorldMapSortingRun::LookAround(const std::string& Moment)
	{
		for (const FFraming& Framing : Framings)
		{
			if (!IsRunning())
			{
				return std::nullopt;
			}
			if (auto Error = Tick(Framing.Offsets))
			{
				return Error;
			}
			Look(Moment + "-" + Framing.Name);
		}
		return Tick();
	}

	void FWorldMapSortingRun::Look(const std::string& Shot)
	{
		const FWorldMapFrame* Latest = Program->LatestFrame();
		if (!IsRunning() || Latest == nullptr || Program->ScreenFade().Picture != FWorldMapScreenFade::EPicture::None)
		{
			return;
		}
		const FSortingFrameAudit Audit = AuditWorldMapSorting(*Latest, Program->Drawing().Vram(), Options);
		const FWorldMapCameraOffsets& Shown = Latest->ShownOffsets;
		Report->Add(Entry.Prefix + Shot, {Shown.Zoom, Shown.Tilt, Shown.Turn}, Audit,
			[&](uint32 Number) { return DescribeWorldMapSortingPiece(*Latest, Number, Audit.Pieces[Number]); });
	}
}
