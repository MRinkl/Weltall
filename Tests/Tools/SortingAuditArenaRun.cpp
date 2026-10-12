#include "Tools/SortingAuditArenaRun.h"

#include "Battling/BattlingDrawing.h"
#include "Game/FieldProgram.h"
#include "Game/GameState.h"
#include "Timing/PsxTiming.h"

#include <array>
#include <vector>

namespace Weltall::Testing
{
	namespace
	{
		/** A forest map, whose scripts ask for nothing as the arena is asked for. */
		constexpr uint16 ForestMap = 15;
		constexpr uint8 FeiGear = 0;
		/** The bout's six bytes: against the computer at the easy level, a set of one win. */
		constexpr uint8 WinsPerSet = 1;
		constexpr uint16 AllCharacters = 0x1FF;
		/** The passes after the arena's start a frame is audited at, every so many, to the bout's end or the last. */
		constexpr int32 LookEvery = 40;
		constexpr int32 LastPass = 2400;

		/** The pad of a player who walks at the opponent and strikes: a button held for a few passes, then let go. */
		FPadState Striking(int32 Pass)
		{
			constexpr std::array<EPadButton, 4> Buttons{EPadButton::Square, EPadButton::Triangle, EPadButton::Cross, EPadButton::Square};
			FPadState Held{FPadState::Bit(EPadButton::Right)};
			if (Pass % 6 < 3)
			{
				Held.Buttons = static_cast<uint16>(Held.Buttons | FPadState::Bit(Buttons[static_cast<std::size_t>(Pass / 6 % 4)]));
			}
			return Held;
		}
	}

	std::optional<FError> FArenaSortingRun::Run()
	{
		auto Started = Game.Start(0, nullptr);
		if (!Started)
		{
			return Started.Error();
		}
		Session = std::move(*Started);
		Session->Game().SetParty({0, 1, 2});
		Session->Game().SetJoinedCharacters(AllCharacters);
		auto Loop = FGameLoop::EnterField(*Session, ForestMap);
		if (!Loop)
		{
			return Loop.Error();
		}
		GameLoop = std::move(*Loop);
		for (int32 Waited = 0; Waited < 100 && !(GameLoop->Field() != nullptr && GameLoop->Field()->Runtime().Context().bFieldRunning); ++Waited)
		{
			if (auto Error = Frame({}))
			{
				return Error;
			}
		}
		if (auto Error = Frame({}))
		{
			return Error;
		}
		if (GameLoop->Field() == nullptr)
		{
			return MakeError("the forest map did not start");
		}
		FEventRequests& Requests = GameLoop->Field()->Runtime().Context().Requests;
		Requests.BattlingParameters = {0, 0, FeiGear, Opponent, WinsPerSet, 0};
		Requests.bLeavingField = true;
		Requests.bBattling = true;
		for (int32 Waited = 0; Waited < 8 && GameLoop->Program() != EGameProgram::Battling; ++Waited)
		{
			if (auto Error = Frame({}))
			{
				return Error;
			}
		}
		if (GameLoop->Program() != EGameProgram::Battling)
		{
			return MakeError("the arena did not start");
		}
		Report.emplace(Rows, Place(), static_cast<uint32>(Options.Scale * Options.Scale), bPictures);
		const int32 Runs = GameLoop->ProgramRuns();
		for (int32 Pass = 0; Pass < LastPass && GameLoop->ProgramRuns() == Runs; ++Pass)
		{
			if (auto Error = Frame(Striking(Pass)))
			{
				return Error;
			}
			if (Pass % LookEvery == 0)
			{
				Look("p" + std::to_string(Pass));
			}
		}
		return std::nullopt;
	}

	void FArenaSortingRun::WritePictures(const std::filesystem::path& Folder) const
	{
		if (Report)
		{
			Report->WritePictures(Folder);
		}
	}

	std::optional<FError> FArenaSortingRun::Frame(FPadState Held)
	{
		const std::vector<FPadState> PerVblank(static_cast<std::size_t>(GameLoop->VblanksPerFrame()), Held);
		Game.Unheard->Advance(PsxTiming::FrameSeconds * static_cast<double>(PerVblank.size()));
		return GameLoop->Frame(PerVblank);
	}

	void FArenaSortingRun::Look(const std::string& Shot)
	{
		const FBattlingGameProgram* Arena = GameLoop->Battling();
		const FBattlingFrame* Latest = Arena != nullptr ? Arena->LatestFrame() : nullptr;
		// The bout's own frames: not the winner screen's, the pause's picture or the demonstration's trails.
		if (Latest == nullptr || Latest->Columns != FBattlingFrame::ScreenWidth || Latest->PictureBlurs != 0 || Latest->bTrails)
		{
			return;
		}
		Options.Lines = FBattlingFrame::ScreenHeight;
		Options.Clear = static_cast<uint32>(Latest->Clear.R) << 16 | static_cast<uint32>(Latest->Clear.G) << 8 | Latest->Clear.B;
		Options.bShadowsOnFloors = true;
		const FSortingFrameAudit Audit = AuditWorldMapSorting(Latest->Scene, Arena->Vram(), Options);
		Report->Add(Shot, {}, Audit, [&](uint32 Number) { return DescribeWorldMapSortingPiece(Latest->Scene, Number, Audit.Pieces[Number]); });
	}
}
