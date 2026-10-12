// Finds where the drawing order and the depth disagree, place by place: each field map entered as a new game enters it,
// each world map era at each of its arrivals or in each of its ways of travel and each world map cutscene, each bout of
// the arena, each battle, or each save; a few of their frames drawn in software twice (in the game's order and by depth) under the game's framing and
// under the player's turned, tilted and zoomed (Support/SortingAudit.h, Support/WorldMapSortingAudit.h,
// Support/BattleSortingAudit.h).
//
//   WeltallSortingAudit OUT [FIRST [LAST]] [-worldmap | -travel | -cutscenes | -arena | -battles | -saves=FOLDER] [-scale=S] [-width=W]
//       [-margin=M] [-original | -perpixel] [-pictures]
//
// Field maps FIRST to LAST of disc 1 (0 to 729); with -worldmap the world map's eras FIRST to LAST (1 to 8, each at the
// first story value of its range) at every arrival of the era's scene (places "wE.A"); with -travel the same eras as the
// story travels them: in gears at every arrival ("gE.A"), and aboard the Yggdrasil of the era, on the sand ("yE.A") or the
// sea ("sE.A") nearest an arrival or flying high above it ("fE.A"), then landed; with -cutscenes the world map's
// cutscene scenes FIRST to LAST (8 to 18, places "cS"); with -arena Weltall's bouts against the arena's gears FIRST to
// LAST (0 to 48, places "aG"); with -battles the battles FIRST to LAST of the list of every scene and stage the field
// maps' formations name (OUT/battles.tsv, places "bD.M.F": the disc, the map, the formation); with -saves every original
// save on the card images in FOLDER
// entered at its place, on a field map or the world map (its frames' labels begin "sN-"). The view W screen pixels wide
// (16:9: 398 on the field and in battle, 384 on the world map) drawn S times finer (2), a piece counted when the order draws it over
// one more than M view units nearer (16), judged as the port's default sorting draws (the field's scenery sorted by depth
// too), with -perpixel as its choice that leaves the field's scenery in the order, or with -original as the original's
// painter's order does. Writes OUT/conflicts.tsv (each pair of pieces that disagree in a frame) and OUT/errors.tsv (the
// places that could not be played); with -pictures, for each place and class, the frame where it covers the most pixels
// about where they lie (OUT/pictures/PLACE-CLASS-FRAME.bmp: the painter's order, the same with the class's pixels tinted,
// the frame by depth). A run plays the same frames every time. The discs are the checkout's, in its Games folder.

#include "Game/KernelState.h"
#include "Media/GameLibrary.h"
#include "Battling/BattlingData.h"
#include "Tools/SortingAuditArenaRun.h"
#include "Tools/SortingAuditBattleRun.h"
#include "Tools/SortingAuditFieldRun.h"
#include "Support/PlayedGame.h"
#include "Tools/SortingAuditWorldMapRun.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace Weltall;
using namespace Weltall::Testing;

namespace
{
	/** The story values from which the world map's eras 1 to 8 are drawn: era E begins at EraStarts[E - 1]. */
	constexpr std::array<uint16, 8> EraStarts = {24, 54, 135, 149, 186, 198, 204, 237};
	/** The playable world map's scene, and the last era, which the second disc plays. */
	constexpr uint16 PlayableScene = 1;
	constexpr int32 SecondDiscEra = 8;

	enum class EPlaces : uint8
	{
		FieldMaps,
		WorldMapEras,
		WorldMapTravel,
		WorldMapCutscenes,
		ArenaBouts,
		Battles,
		Saves,
	};

	int Fail(const std::string& Why)
	{
		std::fprintf(stderr, "%s\n", Why.c_str());
		return 1;
	}

	int32 EraOf(uint16 Story)
	{
		return static_cast<int32>(std::count_if(EraStarts.begin(), EraStarts.end(), [Story](uint16 Start) { return Start <= Story; }));
	}

	/** Whether a save goes on on the world map once loaded. */
	bool IsWorldMapSave(const FPlayedGame& Game, const FOriginalSave& Save)
	{
		const auto Session = Game.Start(0, &Save);
		return Session && LoadedGameProgram((*Session)->Game()) == EGameProgram::WorldMap;
	}
}

int main(int ArgumentCount, char** Arguments)
{
	if (ArgumentCount < 2)
	{
		return Fail("usage: WeltallSortingAudit OUT [FIRST [LAST]] [-worldmap | -travel | -cutscenes | -arena | -battles | -saves=FOLDER] [-scale=S] [-width=W] "
					"[-margin=M] [-original | -perpixel] [-pictures]");
	}
	const std::filesystem::path Out(Arguments[1]);
	std::array<int32, 2> Range = {-1, -1};
	FSortingAuditOptions FieldOptions;
	FWorldMapSortingAuditOptions WorldMapOptions;
	FBattleSortingAuditOptions BattleOptions;
	EPlaces Places = EPlaces::FieldMaps;
	bool bPictures = false;
	std::filesystem::path SavesFolder;
	int32 Position = 0;
	for (int32 Argument = 2; Argument < ArgumentCount; ++Argument)
	{
		const std::string Text(Arguments[Argument]);
		if (Text.rfind("-scale=", 0) == 0)
		{
			FieldOptions.Scale = WorldMapOptions.Scale = BattleOptions.Scale = std::atoi(Text.c_str() + 7);
		}
		else if (Text.rfind("-width=", 0) == 0)
		{
			FieldOptions.Width = WorldMapOptions.Width = BattleOptions.Width = std::atoi(Text.c_str() + 7);
		}
		else if (Text.rfind("-margin=", 0) == 0)
		{
			FieldOptions.Margin = WorldMapOptions.Margin = BattleOptions.Margin = static_cast<float>(std::atof(Text.c_str() + 8));
		}
		else if (Text == "-original")
		{
			FieldOptions.Sorting = WorldMapOptions.Sorting = BattleOptions.Sorting = EFieldSorting::PaintersOrder;
		}
		else if (Text == "-perpixel")
		{
			FieldOptions.Sorting = WorldMapOptions.Sorting = BattleOptions.Sorting = EFieldSorting::DepthBuffer;
		}
		else if (Text == "-pictures")
		{
			bPictures = true;
		}
		else if (Text == "-worldmap")
		{
			Places = EPlaces::WorldMapEras;
		}
		else if (Text == "-travel")
		{
			Places = EPlaces::WorldMapTravel;
		}
		else if (Text == "-cutscenes")
		{
			Places = EPlaces::WorldMapCutscenes;
		}
		else if (Text == "-arena")
		{
			Places = EPlaces::ArenaBouts;
		}
		else if (Text == "-battles")
		{
			Places = EPlaces::Battles;
		}
		else if (Text.rfind("-saves=", 0) == 0)
		{
			Places = EPlaces::Saves;
			SavesFolder = Text.substr(7);
		}
		else if (Position < 2)
		{
			Range[static_cast<std::size_t>(Position++)] = std::atoi(Text.c_str());
		}
	}
	const auto [DefaultFirst, DefaultLast] = Places == EPlaces::WorldMapEras || Places == EPlaces::WorldMapTravel ? std::array<int32, 2>{1, SecondDiscEra}
		: Places == EPlaces::WorldMapCutscenes								 ? std::array<int32, 2>{8, 18}
		: Places == EPlaces::ArenaBouts										 ? std::array<int32, 2>{0, BattlingGears::Count - 1}
		: Places == EPlaces::Battles										 ? std::array<int32, 2>{0, std::numeric_limits<int32>::max()}
																			 : std::array<int32, 2>{0, 729};
	const int32 First = Range[0] < 0 ? DefaultFirst : Range[0];
	const int32 Last = Range[1] < 0 ? (Range[0] < 0 ? DefaultLast : First) : Range[1];

	const FGameLibrary Library = FGameLibrary::Scan(std::filesystem::path(WELTALL_GAMES_DIR));
	auto Opened = FPlayedGame::Open(Library);
	if (!Opened)
	{
		return Fail(Opened.Error().Message);
	}
	const FPlayedGame& Game = *Opened;

	std::filesystem::create_directories(Out / "pictures");
	std::ofstream Rows(Out / "conflicts.tsv");
	std::ofstream Errors(Out / "errors.tsv");
	Rows << "map\tframe\tzoom\ttilt\tturn\tclass\tover\tunder\tpixels\tseen\tgap\n";
	const auto Finish = [&](auto& Played, const std::string& Prefix)
	{
		if (auto Error = Played.Run())
		{
			Errors << Played.Place() << '\t' << Prefix << '\t' << Error->Message << '\n';
		}
		Played.WritePictures(Out / "pictures");
		Rows.flush();
		Errors.flush();
		std::printf("%s%s\n", Prefix.c_str(), Played.Place().c_str());
		std::fflush(stdout);
	};
	const auto AuditMap = [&](int32 Map, const FOriginalSave* Save, const std::string& Prefix)
	{
		FFieldSortingRun Played(Game, Map, FieldOptions, Rows, bPictures, Save, Prefix);
		Finish(Played, Prefix);
	};
	// A scene entered at an arrival with the story's progress at Story, on Disc, named Place in the report.
	const auto Entered = [](std::string Place, int32 Disc, uint16 Story, uint16 Scene, uint16 Entrance)
	{
		FWorldMapSortingEntry Entry;
		Entry.Place = std::move(Place);
		Entry.Disc = Disc;
		Entry.Story = Story;
		Entry.Scene = Scene;
		Entry.Entrance = Entrance;
		return Entry;
	};
	const auto AuditWorldMap = [&](FWorldMapSortingEntry Entry)
	{
		const std::string Prefix = Entry.Prefix;
		FWorldMapSortingRun Played(Game, std::move(Entry), WorldMapOptions, Rows, bPictures);
		Finish(Played, Prefix);
	};

	switch (Places)
	{
	case EPlaces::FieldMaps:
		for (int32 Map = First; Map <= Last; ++Map)
		{
			AuditMap(Map, nullptr, {});
		}
		break;
	case EPlaces::WorldMapEras:
		for (int32 Era = std::max(First, 1); Era <= std::min(Last, SecondDiscEra); ++Era)
		{
			const int32 Disc = Era == SecondDiscEra && Game.Discs[1].Iso != nullptr ? 1 : 0;
			const uint16 Story = EraStarts[static_cast<std::size_t>(Era - 1)];
			auto Arrivals = FWorldMapSortingRun::Arrivals(Game, Disc, Story);
			if (!Arrivals)
			{
				Errors << "w" << Era << "\t\t" << Arrivals.Error().Message << '\n';
				continue;
			}
			for (const int16 Arrival : *Arrivals)
			{
				AuditWorldMap(Entered("w" + std::to_string(Era) + "." + std::to_string(Arrival), Disc, Story, PlayableScene, static_cast<uint16>(Arrival)));
			}
		}
		break;
	case EPlaces::WorldMapTravel:
	{
		std::ofstream List(Out / "travels.tsv");
		List << "place\tdisc\tstory\tentrance\triding\tvehicle\tx\ty\tz\theading\n";
		for (int32 Era = std::max(First, 1); Era <= std::min(Last, SecondDiscEra); ++Era)
		{
			const int32 Disc = Era == SecondDiscEra && Game.Discs[1].Iso != nullptr ? 1 : 0;
			auto Travels = FWorldMapSortingRun::Travels(Game, Era, Disc, EraStarts[static_cast<std::size_t>(Era - 1)]);
			if (!Travels)
			{
				Errors << "w" << Era << "\t\t" << Travels.Error().Message << '\n';
				continue;
			}
			for (FWorldMapSortingEntry& Entry : *Travels)
			{
				const auto& [X, Y, Z, Heading] = Entry.Vehicle;
				List << Entry.Place << '\t' << Entry.Disc + 1 << '\t' << Entry.Story << '\t' << Entry.Entrance << '\t' << (Entry.bRiding ? 1 : 0) << '\t'
					 << Entry.VehicleType << '\t' << X << '\t' << Y << '\t' << Z << '\t' << Heading << '\n';
				List.flush();
				AuditWorldMap(std::move(Entry));
			}
		}
		break;
	}
	case EPlaces::WorldMapCutscenes:
		for (int32 Scene = First; Scene <= Last; ++Scene)
		{
			AuditWorldMap(Entered("c" + std::to_string(Scene), 0, 0, static_cast<uint16>(Scene), 0));
		}
		break;
	case EPlaces::ArenaBouts:
		for (int32 Opponent = First; Opponent <= Last; ++Opponent)
		{
			FArenaSortingRun Played(Game, static_cast<uint8>(Opponent), WorldMapOptions, Rows, bPictures);
			Finish(Played, {});
		}
		break;
	case EPlaces::Battles:
	{
		const std::vector<FBattleSortingEntry> Battles = FBattleSortingRun::Battles(Game);
		std::ofstream List(Out / "battles.tsv");
		List << "number\tplace\tscene\tstage\tflags\tgears\n";
		for (std::size_t Number = 0; Number < Battles.size(); ++Number)
		{
			const FBattleSortingEntry& Battle = Battles[Number];
			List << Number << '\t' << Battle.Place() << '\t' << int32{Battle.Record[0]} << '\t' << int32{Battle.Record[2]} << '\t' << int32{Battle.Record[1]} << '\t'
				 << (Battle.InGears() ? 1 : 0) << '\n';
		}
		for (int32 Number = std::max(First, 0); Number <= std::min(Last, static_cast<int32>(Battles.size()) - 1); ++Number)
		{
			FBattleSortingRun Played(Game, Battles[static_cast<std::size_t>(Number)], BattleOptions, Rows, bPictures);
			Finish(Played, {});
		}
		break;
	}
	case EPlaces::Saves:
	{
		const std::vector<FOriginalSave> Saves = ReadCardFolderSaves(SavesFolder);
		for (std::size_t Number = 0; Number < Saves.size(); ++Number)
		{
			const FOriginalSave& Save = Saves[Number];
			const std::string Prefix = "s" + std::to_string(Number) + "-";
			if (IsWorldMapSave(Game, Save))
			{
				AuditWorldMap({"w" + std::to_string(EraOf(Save.StoryProgress())), 0, 0, 0, 0, &Save, Prefix});
			}
			else
			{
				AuditMap(-1, &Save, Prefix);
			}
		}
		break;
	}
	}
	return 0;
}
