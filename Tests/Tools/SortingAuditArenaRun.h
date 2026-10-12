#pragma once

#include "Foundation/Result.h"
#include "Game/BattlingGameProgram.h"
#include "Game/GameLoop.h"
#include "Game/GameSession.h"
#include "Support/WorldMapSortingAudit.h"
#include "Support/PlayedGame.h"
#include "Tools/SortingAuditReport.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>

namespace Weltall::Testing
{
	/**
	 * One bout of the Battling arena audited: Weltall against a gear's computer for a set of one win, started as a field
	 * script asks for it, fought with a pad that walks at the opponent and strikes, a frame audited every few passes to the
	 * bout's end.
	 */
	class FArenaSortingRun
	{
	public:
		FArenaSortingRun(const FPlayedGame& InGame, uint8 InOpponent, const FWorldMapSortingAuditOptions& InOptions, std::ofstream& InRows,
			bool bInPictures)
			: Game(InGame), Opponent(InOpponent), Options(InOptions), Rows(InRows), bPictures(bInPictures)
		{
		}

		/** An error where the bout cannot be played. */
		std::optional<FError> Run();

		/** The worst frame of each class the run found (FSortingAuditReport::WritePictures). */
		void WritePictures(const std::filesystem::path& Folder) const;

		/** The place's name in the report: "a" and the opponent's gear. */
		std::string Place() const { return "a" + std::to_string(Opponent); }

	private:
		std::optional<FError> Frame(FPadState Held);
		void Look(const std::string& Shot);

		const FPlayedGame& Game;
		uint8 Opponent = 0;
		FWorldMapSortingAuditOptions Options;
		std::ofstream& Rows;
		bool bPictures = false;
		std::optional<FSortingAuditReport> Report;
		std::unique_ptr<FGameSession> Session;
		std::unique_ptr<FGameLoop> GameLoop;
	};
}
