#pragma once

#include "Game/GameState.h"
#include "Support/StorySweepRun.h"

#include <functional>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Weltall::Testing
{
	/** Told each stretch an exploration plays: the place it began at (its map and the story's progress), what began it, how it went. */
	using FStorySweepReport = std::function<void(int32 Map, uint16 Story, const std::string& Start, const FStorySweepRun& Run, const FStorySweepStretch& Stretch)>;

	/**
	 * The states the game is explored from: a new game's, then each save's, in the order given, each as the game's own
	 * load leaves it (on a field map the party at the entrance by the save point, on the world map where it stood).
	 */
	std::vector<FGameState> StorySweepRoots(const FPlayedGame& Game, std::span<const FOriginalSave> Saves);

	/**
	 * The game played on from states it made itself, as far as talks and touches lead: every state is one a player
	 * reaches, so what stops or never ends here does so in the game. From a state (a save's) the party's place is
	 * entered as a loaded game enters it and its scenes played; then every talk its actors answer is begun, then every
	 * touch, then every zone walked into, each as a player begins it. A place is a field map at a value of the story's
	 * progress, as the story's saved variables its scripts test stand: where a stretch ends with the player in control
	 * at a place not explored yet, the game's state there is kept and the place explored in its turn, the one furthest
	 * on in the story first, a map not seen at that value before one seen. What the talks change of a place
	 * (its people talked to, its chests opened) goes with the party through the place's ways out; after a stretch that
	 * left the place the game goes on from the place again. So it does after one that left the leader where the place
	 * cannot go on from (a floor no way leads back from, a corner the map's people closed): what that stretch changed
	 * is then a place of its own, where the map's scripts would tell it from this one.
	 *
	 * Going on from a kept state enters its map anew, as a game loaded there would: where a map's entry reads what the
	 * stay itself changed (a lift's cabin, which rides or not by where it stands and where the party came from), that
	 * entry is none the game makes. A place not come back to that way is come to again as it first was: from the
	 * state the stretch that reached it began at, by the same trigger.
	 *
	 * The world map is a place too, one for each entrance the party comes onto it by: the places its travel reaches
	 * from there (StorySweepWorldMap.h) are each entered in turn, the party stood in them as a game saved there
	 * would be, and what lies behind them explored as the rest.
	 */
	class FStorySweepExplorer
	{
	public:
		FStorySweepExplorer(FStorySweepRun& InRun, const FStorySweepOptions& InOptions, FStorySweepReport InReport)
			: Run(InRun), Options(InOptions), Report(std::move(InReport))
		{
		}

		/** Explores from Root, at most MostPlaces places; a place explored from an earlier root is not explored again. */
		void Explore(const FGameState& Root, int32 MostPlaces);

	private:
		/** A place kept for its turn: the game's state as a stretch reached it, and the way the game came there. */
		struct FPlace
		{
			FGameState State;
			/** The state the stretch that reached it began at: none for a root. */
			std::optional<FGameState> From;
			/** What began that stretch: none where the scenes of From's place led on by themselves. */
			std::optional<FStorySweepTrigger> Trigger;
			/** Where the leader stood on the field map as the state was kept: none for a root, or on the world map. */
			std::optional<FFieldStanding> Leader;
		};

		/** Keeps State for its place's turn, unless the place was offered before or is none of the field's. */
		void Offer(const FGameState& State, const FGameState& From, std::optional<FStorySweepTrigger> Trigger);
		void ExplorePlace(const FPlace& Place);
		/** The game gone on from State again, its place's scenes played: whether the player has the party there. */
		bool Return(const FGameState& State);
		/** The game brought to the place again the way it first came there: whether the player has the party there. */
		bool Arrive(const FPlace& Place);
		/**
		 * After a stretch (Opener's) that changed the floor of the place's map: its nearest zones walked into at once,
		 * until one leads away, which is offered; the place is then come back to, Home the state it goes on from.
		 * False where the place could not be come back to.
		 */
		bool TakeOpenedWays(const FPlace& Place, int32 Map, uint16 Story, uint16 Entrance, const FStorySweepTrigger& Opener, FGameState& Home);
		/**
		 * The entrance the place is gone on from by, the game entered by Entrance and its scenes played: where the walk
		 * from where that entry stands the leader does not get to where the stretch that reached the place left him (a
		 * scene that walked him past a door it then shut), the entrance a game loaded there resumes at, which the map's
		 * scripts name in a saved variable, if the walk from there does. The game is left entered by the entrance given.
		 */
		uint16 EntranceToGoOnBy(const FPlace& Place, int32 Map, uint16 Story, uint16 Entrance);

		/**
		 * What a place is known by: its map (the world map with the entrance come onto it by), the story's progress,
		 * a digest of the story's saved variables its map's scripts test, and the floor the party comes onto it by.
		 * A place come to again with one of those variables changed is another: what the party did elsewhere may have
		 * its people say, or its ways lead, otherwise. So is one come onto by another floor: a map of paths that do not
		 * meet (the lake of Nisan: the town's paths, the ring round the water, the bridge) is as many places.
		 */
		using FPlaceKey = std::tuple<int32, uint16, uint64, int32>;
		FPlaceKey KeyOf(const FGameState& State, int32 WorldMapEntrance, const std::optional<FWalkedFloor>& Floor) const;

		FStorySweepRun& Run;
		const FStorySweepOptions& Options;
		FStorySweepReport Report;
		std::set<FPlaceKey> Offered;
		/** The maps explored at each value of the progress: one of them come to again waits behind those not yet seen. */
		std::set<std::pair<int32, uint16>> Explored;
		std::vector<FPlace> Pending;
	};
}
