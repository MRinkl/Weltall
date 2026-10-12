#pragma once

#include "Foundation/Result.h"
#include "Game/GameLoop.h"
#include "Game/GameSession.h"
#include "Support/BattleSortingAudit.h"
#include "Support/PlayedGame.h"
#include "Tools/SortingAuditReport.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Weltall::Testing
{
	/** A battle the audit fights: a formation of a field map's encounter data, on its disc. */
	struct FBattleSortingEntry
	{
		int32 Disc = 0;
		uint16 Map = 0;
		uint8 Formation = 0;
		std::array<uint8, 0x20> Record{};

		/** Its name in the report: "b", the disc, the map and the formation ("b1.15.3"). */
		std::string Place() const;
		/** Whether one of its enemies is gear-sized: the party then rides its gears. */
		bool InGears() const;
	};

	/**
	 * One battle audited: a new game's party of Fei, Elly and Citan, each with their own gear and riding it against a
	 * gear-sized enemy, sent into the formation from a quiet field map as a script sends it, fought with a pad that
	 * presses at random (confirms mostly, the attacks, the directions, a cancel now and then), a frame of the show audited
	 * every second to the battle's end or the last.
	 */
	class FBattleSortingRun
	{
	public:
		FBattleSortingRun(const FPlayedGame& InGame, FBattleSortingEntry InEntry, const FBattleSortingAuditOptions& InOptions, std::ofstream& InRows,
			bool bInPictures)
			: Game(InGame), Entry(InEntry), Options(InOptions), Rows(InRows), bPictures(bInPictures)
		{
		}

		/**
		 * Every formation with enemies in the encounter data of the discs' field maps, the first of each scene and stage
		 * (and whether the party rides), in the order of the discs and the maps.
		 */
		static std::vector<FBattleSortingEntry> Battles(const FPlayedGame& Game);

		/** An error where the battle cannot be fought. */
		std::optional<FError> Run();

		/** The worst frame of each class the run found (FSortingAuditReport::WritePictures). */
		void WritePictures(const std::filesystem::path& Folder) const;

		std::string Place() const { return Entry.Place(); }

	private:
		std::optional<FError> Frame(FPadState Held);
		void Look(const std::string& Shot);

		const FPlayedGame& Game;
		FBattleSortingEntry Entry;
		FBattleSortingAuditOptions Options;
		std::ofstream& Rows;
		bool bPictures = false;
		std::optional<FSortingAuditReport> Report;
		/** The battle's own sound, which outlives its session. */
		std::unique_ptr<FGameSoundSession> Sound;
		std::unique_ptr<FUnheardGameSound> Unheard;
		std::unique_ptr<FGameSession> Session;
		std::unique_ptr<FGameLoop> GameLoop;
	};
}
