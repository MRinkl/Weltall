#include "Tools/SortingAuditFieldRun.h"

#include "Field/FieldFrame.h"
#include "Field/FieldPlayerCamera.h"
#include "Timing/PsxTiming.h"

#include <vector>

namespace Weltall::Testing
{
	namespace
	{
		/** The player's framing a frame is audited under: the game's own, then the view turned, tilted and zoomed. */
		struct FFraming
		{
			const char* Name = "";
			float Zoom = 1.0f;
			float Tilt = 0.0f;
			float Turn = 0.0f;
		};

		constexpr FFraming Framings[] = {{"game", 1.0f, 0.0f, 0.0f}, {"default", FFieldCameraOffsets::DefaultZoom, 0.0f, 0.0f}, {"turn45", 1.0f, 0.0f, 45.0f},
			{"turn-45", 1.0f, 0.0f, -45.0f}, {"turn90", 1.0f, 0.0f, 90.0f}, {"turn-90", 1.0f, 0.0f, -90.0f}, {"turn180", 1.0f, 0.0f, 180.0f},
			{"up", 1.0f, FFieldCameraOffsets::MaxTilt, 0.0f}, {"down", 1.0f, FFieldCameraOffsets::MinTilt, 0.0f},
			{"near", FFieldCameraOffsets::MinZoom, 0.0f, 0.0f}, {"far", FFieldCameraOffsets::MaxZoom, 0.0f, 0.0f}};

		/** The game state's place: the map's number in its low bits (the gears' bits above them), the world map's from 400h. */
		constexpr uint16 PlaceMapBits = 0x7FF;
		constexpr int32 FirstWorldMapPlace = 0x400;

		/** The game frames after the map's entry its frames are audited at: the first under the game's framing only. */
		constexpr int32 FirstLook = 20;
		constexpr int32 Looks[] = {60, 150};
	}

	std::optional<FError> FFieldSortingRun::Run()
	{
		if (auto Error = Start(0))
		{
			return Error;
		}
		if (Save != nullptr && Session->Game().SavedDisc() != 0)
		{
			// A save of the second disc plays on that disc's own files.
			if (auto Error = Start(Session->Game().SavedDisc()))
			{
				return Error;
			}
		}
		Map = Session->Game().Place() & PlaceMapBits;
		if (Map >= FirstWorldMapPlace)
		{
			return MakeError("the save was made on the world map");
		}
		Report.emplace(Rows, std::to_string(Map), static_cast<uint32>(Options.Scale * Options.Scale), bPictures);
		auto Loop = FGameLoop::EnterField(*Session, Session->Game().Place());
		if (!Loop)
		{
			return Loop.Error();
		}
		GameLoop = std::move(*Loop);
		if (auto Error = Advance(FirstLook))
		{
			return Error;
		}
		Look("t" + std::to_string(FirstLook));
		int32 Played = FirstLook;
		for (const int32 At : Looks)
		{
			if (auto Error = Advance(At - Played))
			{
				return Error;
			}
			if (!OnMap())
			{
				break;
			}
			Played = At;
			for (std::size_t Framing = 0; Framing < std::size(Framings); ++Framing)
			{
				if (auto Error = Frame(Framing))
				{
					return Error;
				}
				++Played;
				Look("t" + std::to_string(At) + "-" + Framings[Framing].Name);
			}
			if (auto Error = Frame(0))
			{
				return Error;
			}
			++Played;
		}
		return std::nullopt;
	}

	void FFieldSortingRun::WritePictures(const std::filesystem::path& Folder) const
	{
		if (Report)
		{
			Report->WritePictures(Folder);
		}
	}

	std::optional<FError> FFieldSortingRun::Start(int32 Number)
	{
		GameLoop.reset();
		Session.reset();
		auto Started = Game.Start(Number, Save);
		if (!Started)
		{
			return Started.Error();
		}
		Session = std::move(*Started);
		if (Save == nullptr)
		{
			Session->Game().SetPlace(static_cast<uint16>(Map));
		}
		return std::nullopt;
	}

	std::optional<FError> FFieldSortingRun::Frame(std::size_t Framing)
	{
		if (FFieldProgram* Program = GameLoop->Field())
		{
			FFieldCameraOffsets& Offsets = Session->CameraOffsets();
			Offsets.Zoom = Framings[Framing].Zoom;
			Offsets.Tilt = Framings[Framing].Tilt;
			FFieldViewInput Turn;
			Turn.Turn = Framings[Framing].Turn - Offsets.Turn;
			if (Turn.Turn != 0.0f)
			{
				Program->AddViewInput(Turn);
			}
		}
		const std::vector<FPadState> Held(static_cast<std::size_t>(GameLoop->VblanksPerFrame()));
		Game.Unheard->Advance(PsxTiming::FrameSeconds * static_cast<double>(Held.size()));
		if (FirstLoads < 0 && Field() != nullptr)
		{
			FirstLoads = Field()->Runtime().MapLoads();
		}
		return GameLoop->Frame(Held);
	}

	std::optional<FError> FFieldSortingRun::Advance(int32 Frames)
	{
		for (int32 Number = 0; Number < Frames && (FirstLoads < 0 || OnMap()); ++Number)
		{
			if (auto Error = Frame(0))
			{
				return Error;
			}
		}
		return std::nullopt;
	}

	void FFieldSortingRun::Look(const std::string& Shot)
	{
		if (!OnMap())
		{
			return;
		}
		const FFieldFrame* Latest = Field()->LatestFrame();
		if (Latest == nullptr || !Latest->bSceneDrawn)
		{
			return;
		}
		const FFieldPresenter& Drawing = Field()->Drawing();
		const FSortingFrameAudit Audit = AuditFieldSorting(*Latest, Drawing.Scene(), Drawing.Vram(), Options);
		const FFieldViewOffsets& Shown = Latest->ShownOffsets;
		Report->Add(Prefix + Shot, {Shown.Zoom, Shown.Tilt, Shown.Turn}, Audit,
			[&](uint32 Number) { return DescribeSortingPiece(*Latest, Drawing.Scene(), Number, Audit.Pieces[Number]); });
	}
}
