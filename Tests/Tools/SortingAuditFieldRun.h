#pragma once

#include "Foundation/Result.h"
#include "Game/FieldProgram.h"
#include "Game/GameLoop.h"
#include "Game/GameSession.h"
#include "Support/SortingAudit.h"
#include "Support/PlayedGame.h"
#include "Tools/SortingAuditReport.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace Weltall::Testing
{
	/**
	 * One field map's audit, entered as a new game enters it or from a save made on it: its session played on, a few of
	 * its frames audited under the game's framing and the player's turned, tilted and zoomed (frames 20, 60 and 150 after
	 * the entry), its rows written.
	 */
	class FFieldSortingRun
	{
	public:
		/** Map: the field map a new game enters; with a save, the save's own (Prefix before its frames' labels). */
		FFieldSortingRun(const FPlayedGame& InGame, int32 InMap, const FSortingAuditOptions& InOptions, std::ofstream& InRows, bool bInPictures,
			const FOriginalSave* InSave = nullptr, std::string InPrefix = {})
			: Game(InGame), Map(InMap), Options(InOptions), Rows(InRows), bPictures(bInPictures), Save(InSave), Prefix(std::move(InPrefix))
		{
		}

		/** An error where the map cannot be played, or the save was made elsewhere than on a field map. */
		std::optional<FError> Run();

		/** The worst frame of each class the run found (FSortingAuditReport::WritePictures). */
		void WritePictures(const std::filesystem::path& Folder) const;

		/** The place's name in the report: the field map the run plays, the save's once it has started. */
		std::string Place() const { return std::to_string(Map); }

	private:
		std::optional<FError> Start(int32 Number);
		const FFieldProgram* Field() const { return GameLoop ? GameLoop->Field() : nullptr; }
		/** Whether the field still plays the map entered, as its first load. */
		bool OnMap() const { return Field() != nullptr && Field()->Runtime().MapLoads() == FirstLoads; }
		std::optional<FError> Frame(std::size_t Framing);
		std::optional<FError> Advance(int32 Frames);
		void Look(const std::string& Shot);

		const FPlayedGame& Game;
		int32 Map = 0;
		const FSortingAuditOptions& Options;
		std::ofstream& Rows;
		bool bPictures = false;
		const FOriginalSave* Save = nullptr;
		/** Before each frame's label: which save the map was entered from. */
		std::string Prefix;
		std::optional<FSortingAuditReport> Report;
		std::unique_ptr<FGameSession> Session;
		std::unique_ptr<FGameLoop> GameLoop;
		int32 FirstLoads = -1;
	};
}
