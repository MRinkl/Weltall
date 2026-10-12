#include "Support/StorySweepExplore.h"

#include "Game/KernelState.h"

#include <algorithm>
#include <iterator>
#include <vector>

namespace Weltall::Testing
{
	namespace
	{
		/** A place's field map, without the bits that say the party is in gears. */
		constexpr uint16 PlaceMapBits = 0x3FFF;
		/** The map a new game begins on. */
		constexpr uint16 NewGameMap = 4;
		/** The saved variable (its place among them) that names the entrance a loaded game resumes at: each map's scripts set it. */
		constexpr int32 LoadedEntrance = 0x2A;
		/** What a loaded game leaves the program after the load, which the field's scripts can read. */
		constexpr uint8 LoadedResult = 2;
		/** How often a place's triggers are gone through. */
		constexpr int32 Rounds = 2;
		/** How often a talk or a touch that goes on saying something new is begun again. */
		constexpr int32 MoreTalks = 3;
		/** How many of the nearest zones are walked into at once after a stretch that changed the map's floor. */
		constexpr std::size_t OpenedWays = 3;

		/** A world map visit's key counts its entrance in the bits above its place. */
		constexpr int32 EntranceShift = 16;
		/** The 64-bit FNV-1a digest's start and its prime. */
		constexpr uint64 DigestStart = 0xCBF29CE484222325ull;
		constexpr uint64 DigestPrime = 0x100000001B3ull;

		std::pair<int32, uint16> PlaceOf(const FGameState& State)
		{
			return {State.Place() & PlaceMapBits, State.SavedEventVariable(0)};
		}

		bool IsOnWorldMap(const FGameState& State)
		{
			return LoadedGameProgram(State) == EGameProgram::WorldMap;
		}

		/** The player has the party at the place: on its field map, or travelling the world map it names. */
		bool IsAt(const FStorySweepStretch& Stretch, const std::pair<int32, uint16>& Place)
		{
			return (Stretch.End == EStorySweepEnd::Control || Stretch.End == EStorySweepEnd::WorldMap) && Stretch.Map == Place.first && Stretch.Story == Place.second;
		}
	}

	std::vector<FGameState> StorySweepRoots(const FPlayedGame& Game, std::span<const FOriginalSave> Saves)
	{
		std::vector<FGameState> Roots;
		if (auto NewGame = Game.Start(0, nullptr))
		{
			Roots.push_back((*NewGame)->Game());
			Roots.back().SetPlace(NewGameMap);
		}
		for (const FOriginalSave& Save : Saves)
		{
			const auto Loaded = Game.Start(0, &Save);
			if (!Loaded)
			{
				continue;
			}
			// As the game's own load leaves it: the party by the save point, not where it came onto the map; on the
			// world map, where it stood as the menu opened.
			FGameState& Root = Roots.emplace_back((*Loaded)->Game());
			if (!IsOnWorldMap(Root))
			{
				Root.SetFieldEntrance(Root.SavedEventVariable(LoadedEntrance));
			}
			Root.ProgramResult = LoadedResult;
		}
		return Roots;
	}

	FStorySweepExplorer::FPlaceKey FStorySweepExplorer::KeyOf(const FGameState& State, int32 WorldMapEntrance, const std::optional<FWalkedFloor>& Floor) const
	{
		const auto [Map, Story] = PlaceOf(State);
		// The world map come onto by another entrance is another place: the land its travel reaches may be another.
		if (IsOnWorldMap(State))
		{
			return {Map | (WorldMapEntrance + 1) << EntranceShift, Story, 0, -1};
		}
		uint64 Digest = DigestStart;
		for (const int32 Variable : Run.TestedVariables(Map))
		{
			for (const uint32 Part : {static_cast<uint32>(Variable), static_cast<uint32>(State.SavedEventVariable(Variable))})
			{
				Digest = (Digest ^ Part) * DigestPrime;
			}
		}
		return {Map, Story, Digest, Floor ? Floor->Layer * FloorLayerSpan + Floor->First : -1};
	}

	void FStorySweepExplorer::Explore(const FGameState& Root, int32 MostPlaces)
	{
		Pending.clear();
		if (Offered.insert(KeyOf(Root, -1, std::nullopt)).second)
		{
			Pending.push_back({Root, std::nullopt, std::nullopt, std::nullopt});
		}
		for (int32 Count = 0; !Pending.empty() && Count < MostPlaces; ++Count)
		{
			// The place furthest on in the story, a map not seen at that value before one that was, the latest kept
			// among those: the story is followed before its sides, a new place before one come back to, and what the
			// play changed last before what it left behind: a town's own chain of steps at one value of the progress
			// (Nisan's, its saved variable 102h counted from 1 to 19 over eight of its maps) makes every map of the town
			// another place at each step.
			const auto Latest = std::max_element(Pending.rbegin(), Pending.rend(), [this](const FPlace& A, const FPlace& B)
				{
					const uint16 StoryA = A.State.SavedEventVariable(0);
					const uint16 StoryB = B.State.SavedEventVariable(0);
					return StoryA != StoryB ? StoryA < StoryB : Explored.count(PlaceOf(A.State)) > Explored.count(PlaceOf(B.State));
				});
			const auto Next = std::prev(Latest.base());
			const FPlace Place = std::move(*Next);
			Pending.erase(Next);
			Explored.insert(PlaceOf(Place.State));
			ExplorePlace(Place);
		}
	}

	void FStorySweepExplorer::Offer(const FGameState& State, const FGameState& From, std::optional<FStorySweepTrigger> Trigger)
	{
		if (Offered.insert(KeyOf(State, Run.WorldMapEntrance(), Run.Floor())).second)
		{
			Pending.push_back({State, From, Trigger, Run.Standing()});
		}
	}

	bool FStorySweepExplorer::Return(const FGameState& State)
	{
		return !Run.Resume(State) && IsAt(Run.Play(Options.Scenes), PlaceOf(State));
	}

	bool FStorySweepExplorer::Arrive(const FPlace& Place)
	{
		if (!Place.From || !Place.Trigger || !Return(*Place.From) || !Run.Ask(*Place.Trigger))
		{
			return false;
		}
		const auto [Map, Story] = PlaceOf(Place.State);
		const FStorySweepStretch Played = Run.Play(Options.Triggers);
		Report(Map, Story, "arrival", Run, Played);
		return IsAt(Played, {Map, Story});
	}

	uint16 FStorySweepExplorer::EntranceToGoOnBy(const FPlace& Place, int32 Map, uint16 Story, uint16 Entrance)
	{
		const uint16 Loaded = Place.State.SavedEventVariable(LoadedEntrance);
		if (!Place.Leader || Loaded == Entrance || Run.Reaches(*Place.Leader))
		{
			return Entrance;
		}
		FGameState Again = Place.State;
		Again.SetFieldEntrance(Loaded);
		if (!Run.Resume(Again))
		{
			const FStorySweepStretch Scene = Run.Play(Options.Scenes);
			Report(Map, Story, "scene at entrance " + std::to_string(Loaded), Run, Scene);
			if (IsAt(Scene, {Map, Story}) && Run.Reaches(*Place.Leader))
			{
				return Loaded;
			}
		}
		Return(Place.State);
		return Entrance;
	}

	bool FStorySweepExplorer::TakeOpenedWays(const FPlace& Place, int32 Map, uint16 Story, uint16 Entrance, const FStorySweepTrigger& Opener, FGameState& Home)
	{
		std::vector<FStorySweepTrigger> Zones = Run.ZonesByNearness();
		Zones.resize(std::min(Zones.size(), OpenedWays));
		for (const FStorySweepTrigger& Zone : Zones)
		{
			if (!Run.Ask(Zone))
			{
				continue;
			}
			const FStorySweepStretch Played = Run.Play(Options.Triggers);
			Report(Map, Story, Zone.Name() + " after " + Opener.Name() + StorySweepOpeningSuffix(Played.Opening), Run, Played);
			if (IsAt(Played, {Map, Story}))
			{
				continue;
			}
			if (Played.End == EStorySweepEnd::Control || Played.End == EStorySweepEnd::WorldMap)
			{
				Offer(Run.State(), Home, Zone);
			}
			// The place again, its floor as its map loads it: the rest of what was opened is gone with that.
			if (!Return(Home))
			{
				if (!Arrive(Place))
				{
					return false;
				}
				Home = Run.State();
				Home.SetFieldEntrance(Entrance);
			}
			return true;
		}
		return true;
	}

	void FStorySweepExplorer::ExplorePlace(const FPlace& Place)
	{
		const auto [Map, Story] = PlaceOf(Place.State);
		const bool bWorldMap = IsOnWorldMap(Place.State);
		// The entrance the place is entered by, which each return to it keeps: the field writes its state's own every frame.
		uint16 Entrance = Place.State.Scene();
		if (auto Error = Run.Resume(Place.State))
		{
			FStorySweepStretch Unplayed;
			Unplayed.End = EStorySweepEnd::Stops;
			Unplayed.Detail = Error->Message;
			Report(Map, Story, "scene", Run, Unplayed);
			return;
		}
		const FStorySweepStretch Scene = Run.Play(Options.Scenes);
		// A place a trigger led to is come to again by it where the kept state's own entry does not give the party back there.
		const bool bEntered = IsAt(Scene, {Map, Story});
		Report(Map, Story, bEntered || !Place.Trigger ? "scene" : "re-entry", Run, Scene);
		if (!bEntered)
		{
			// The scenes led on by themselves: where they give the party back is a place of its own.
			if (Scene.End == EStorySweepEnd::Control || Scene.End == EStorySweepEnd::WorldMap)
			{
				Offer(Run.State(), Place.State, std::nullopt);
			}
			if (!Arrive(Place))
			{
				return;
			}
		}
		else if (!bWorldMap)
		{
			Entrance = EntranceToGoOnBy(Place, Map, Story, Entrance);
		}
		// What the game goes on from after a stretch that left: a field map entered by the place's entrance, the
		// world map as the party came onto it.
		const auto Kept = [&]()
		{
			FGameState State = Run.State();
			if (!bWorldMap)
			{
				State.SetFieldEntrance(Entrance);
			}
			return State;
		};
		FGameState Home = Kept();
		// The floor the place gives the party back on: a stretch whose own crossings left the leader where no walk gets
		// back to it (a drop no way leads back up from) is undone, as a player would have to leave the map to get back.
		// Where the scripts moved the party (a lift), the stay goes on from there.
		const std::optional<FWalkedFloor> Entered = Run.Floor();
		const bool bRoomy = !Run.IsHemmedIn();
		// A second round takes what the first could not walk to (a talk past a touch not met by then) and whoever
		// answers only since. The world map's places are each come to from where the party came onto it.
		std::vector<FStorySweepTrigger> Tried;
		for (int32 Round = 0; Round < (bWorldMap ? 1 : Rounds); ++Round)
		{
			// The talks before the touches and the zones, which are the ways out: what the talks changed leaves with the party.
			std::vector<FStorySweepTrigger> Triggers = Run.Triggers();
			std::stable_sort(Triggers.begin(), Triggers.end(), [](const FStorySweepTrigger& A, const FStorySweepTrigger& B) { return A.Kind < B.Kind; });
			for (const FStorySweepTrigger& Trigger : Triggers)
			{
				const bool bTried = std::any_of(Tried.begin(), Tried.end(), [&](const FStorySweepTrigger& Other) { return Other.Kind == Trigger.Kind && Other.Number == Trigger.Number; });
				// A talk or a touch that said something new, or asked what it has more answers to, is begun again: who is
				// talked to twice may say more, and a question's other answer may be the one that leads on (the box of
				// Nortune's ring, map 199 at 105, asks "Hey, are you ready?" when touched, and only "Let's rock" goes on).
				for (int32 Again = 0; !bTried && Again <= MoreTalks && Run.Ask(Trigger); ++Again)
				{
					const FStorySweepStretch Played = Run.Play(Options.Triggers);
					const bool bStayed = !bWorldMap && IsAt(Played, {Map, Story});
					// One not reached that left everything as it was has its line from the last round alone.
					const bool bUnreached = Played.Opening == EStorySweepOpening::Unreached && bStayed;
					// A walk into a zone that a random battle broke into is taken again in the next round: the battle had the
					// party before the zone did.
					const bool bBroken = Trigger.Kind == EStorySweepTrigger::Area && bStayed && Played.Path.find("battle") != std::string::npos;
					if (Again > 0 || !bUnreached || Round + 1 == Rounds)
					{
						if (Again == 0 && (!bBroken || Round + 1 == Rounds))
						{
							Tried.push_back(Trigger);
						}
						Report(Map, Story, Trigger.Name() + (Again > 0 ? " again" : "") + StorySweepOpeningSuffix(Played.Opening), Run, Played);
					}
					// A leader the stretch left hemmed in goes on from before it, as one left on a floor it cannot leave does.
					const bool bHemmedIn = bStayed && bRoomy && Run.IsHemmedIn();
					if (bStayed && !bHemmedIn && (!Entered || !Run.MadeCrossing() || Run.ReachesFloor(*Entered)))
					{
						Home = Kept();
						// What it opened is walked into now: the map entered anew, as the place goes on after any
						// stretch that leaves it, has its floor as before (a well's lid closed again).
						if (Run.FloorChanged() && Trigger.Kind != EStorySweepTrigger::Area && !TakeOpenedWays(Place, Map, Story, Entrance, Trigger, Home))
						{
							return;
						}
						const bool bAsked = Trigger.Kind == EStorySweepTrigger::Talk || Trigger.Kind == EStorySweepTrigger::Touch;
						if (bAsked && Played.Opening == EStorySweepOpening::Answered && (Run.SaidMore() || Run.AsksMore()))
						{
							continue;
						}
						break;
					}
					// Where it led is a place of its own: another map; or this one, left where the place does not go on from
					// (a floor with no way back, a corner): what it changed is then kept for a visit of its own, if the
					// map's scripts would tell it from this one.
					if (Played.End == EStorySweepEnd::Control || Played.End == EStorySweepEnd::WorldMap)
					{
						Offer(bStayed ? Kept() : Run.State(), Home, Trigger);
					}
					if (Return(Home))
					{
						break;
					}
					// The place as it first was, without what its talks changed since.
					if (!Arrive(Place))
					{
						return;
					}
					Home = Kept();
					break;
				}
			}
		}
	}
}
