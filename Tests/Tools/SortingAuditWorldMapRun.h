#pragma once

#include "Foundation/Result.h"
#include "Game/GameSession.h"
#include "Game/WorldMapProgram.h"
#include "Input/PadState.h"
#include "Support/WorldMapSortingAudit.h"
#include "Support/PlayedGame.h"
#include "Tools/SortingAuditReport.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Weltall::Testing
{
	/** Where a world map run enters: a scene at one of its arrivals with the story's progress at a value, or a save made on the world map. */
	struct FWorldMapSortingEntry
	{
		/** The place's name in the report: "w2.13" (era 2's arrival 13), "c14" (cutscene scene 14), or the save's era. */
		std::string Place;
		/** The disc the session plays (0 or 1); a save plays its own. */
		int32 Disc = 0;
		uint16 Story = 0;
		uint16 Scene = 1;
		/** The arrival's id in the scene's list (FWorldMapScene::Entrances). */
		uint16 Entrance = 0;
		const FOriginalSave* Save = nullptr;
		/** Before each frame's label: which save it was entered from. */
		std::string Prefix;
		/** The party of three in their gears (FWorldMapSortingRun::TravelParty), riding them at the arrival. */
		bool bRiding = false;
		/** The Yggdrasil the party starts aboard, its type as the game state holds it (1 to 3; 0: none), and its place (x, y, z, heading: whole units). */
		uint16 VehicleType = 0;
		std::array<int16, 4> Vehicle{};
	};

	/**
	 * One world map place's audit: the scene entered, played without a button pressed, and a few of its frames audited, a
	 * playable scene's under the game's framing and the player's turned, tilted and zoomed, a cutscene's as its camera
	 * shows them every few seconds to its end. A party aboard the Yggdrasil then lands or disembarks, and the ship parked
	 * beside their gears is audited too.
	 */
	class FWorldMapSortingRun
	{
	public:
		FWorldMapSortingRun(const FPlayedGame& InGame, FWorldMapSortingEntry InEntry, const FWorldMapSortingAuditOptions& InOptions,
			std::ofstream& InRows, bool bInPictures)
			: Game(InGame), Entry(std::move(InEntry)), Options(InOptions), Rows(InRows), bPictures(bInPictures)
		{
		}

		/** The arrivals' ids of the scene a session on Disc enters at the story's value Story (one of the scenes 0 to 7: the era's). */
		static TResult<std::vector<int16>> Arrivals(const FPlayedGame& Game, int32 Disc, uint16 Story);

		/**
		 * The era's travel as the story has it there, named Era: the party in gears at each arrival ("gE.A"); the
		 * Yggdrasil the bridge launches at the story's value, aboard: the sand ship on the sand nearest each arrival
		 * ("yE.A"), the Yggdrasil II also on the sea nearest it ("sE.A"), the Yggdrasil III flying at its ceiling above it
		 * ("fE.A"), a ship's place left out within a block's half of another's.
		 */
		static TResult<std::vector<FWorldMapSortingEntry>> Travels(const FPlayedGame& Game, int32 Era, int32 Disc, uint16 Story);

		/** An error where the scene cannot be played, or the save was made elsewhere than on the world map. */
		std::optional<FError> Run();

		/** The worst frame of each class the run found (FSortingAuditReport::WritePictures). */
		void WritePictures(const std::filesystem::path& Folder) const;

		/** The place's name in the report. */
		const std::string& Place() const { return Entry.Place; }

	private:
		std::optional<FError> Start();
		bool IsRunning() const { return Program && Program->State() == EGameProgramState::Running; }
		/** A tick of the program with the buttons Held, the player's offsets Offsets. */
		std::optional<FError> Tick(const FWorldMapCameraOffsets& Offsets = {}, FPadState Held = {});
		/** Ticks until To ticks of the map have run, or the program ends. */
		std::optional<FError> Advance(int32 To);
		/** The frame audited under each of the player's framings, a tick each, the shots named Moment and the framing. */
		std::optional<FError> LookAround(const std::string& Moment);
		void Look(const std::string& Shot);

		const FPlayedGame& Game;
		FWorldMapSortingEntry Entry;
		const FWorldMapSortingAuditOptions& Options;
		std::ofstream& Rows;
		bool bPictures = false;
		std::optional<FSortingAuditReport> Report;
		std::unique_ptr<FGameSession> Session;
		std::unique_ptr<FWorldMapProgram> Program;
		int32 Ticks = 0;
	};
}
