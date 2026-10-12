#include "WorldMap/WorldMapCutsceneTables.h"

#include "Foundation/LittleEndian.h"

namespace Weltall
{
	namespace
	{
		/** Where a release's Worldmap program keeps each table, as offsets in the program. */
		struct FCutsceneTablesLayout
		{
			EDiscRelease Release;
			std::size_t Scene9Camera;
			std::size_t Scene12FleetCamera;
			std::size_t Scene12LateShipCamera;
			std::size_t Scene14Flyer;
		};

		constexpr std::array<FCutsceneTablesLayout, 2> Layouts{{
			{EDiscRelease::UsaDisc1, 0x2A900, 0x2AA08, 0x2AA78, 0x2A9A0},
			{EDiscRelease::UsaDisc2, 0x2A900, 0x2AA08, 0x2AA78, 0x2A9A0},
		}};

		/** The points the program reads of the paths that have no end: the path's own and those its last segments reach past it. */
		constexpr std::size_t Scene12FleetCameraPoints = 14;
		constexpr std::size_t Scene12LateShipCameraPoints = 8;
		constexpr std::size_t Scene14FlyerPoints = 12;

		constexpr std::size_t PointSize = 8;
		constexpr int16 PathEnd = -1;
		constexpr std::size_t PathLimit = 64;

		TResult<FWorldMapPath> ReadPath(std::span<const uint8> Program, std::size_t Offset)
		{
			FWorldMapPath Path;
			for (std::size_t At = Offset; Path.size() < PathLimit; At += PointSize)
			{
				if (At + PointSize > Program.size())
				{
					break;
				}
				std::array<int16, 4>& Point = Path.emplace_back();
				for (std::size_t Word = 0; Word < Point.size(); ++Word)
				{
					Point[Word] = LittleEndian::ReadS16(Program, At + 2 * Word);
				}
				if (Point[3] == PathEnd)
				{
					return Path;
				}
			}
			return MakeError("a world map cutscene's path has no end");
		}

		TResult<FWorldMapPath> ReadPoints(std::span<const uint8> Program, std::size_t Offset, std::size_t Count)
		{
			if (Offset + Count * PointSize > Program.size())
			{
				return MakeError("a world map cutscene's path lies past the program's end");
			}
			FWorldMapPath Path(Count);
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				for (std::size_t Word = 0; Word < Path[Index].size(); ++Word)
				{
					Path[Index][Word] = LittleEndian::ReadS16(Program, Offset + Index * PointSize + 2 * Word);
				}
			}
			return Path;
		}
	}

	TResult<FWorldMapCutsceneTables> ReadWorldMapCutsceneTables(std::span<const uint8> Program, EDiscRelease Release)
	{
		const FCutsceneTablesLayout* Layout = nullptr;
		for (const FCutsceneTablesLayout& Candidate : Layouts)
		{
			if (Candidate.Release == Release)
			{
				Layout = &Candidate;
			}
		}
		if (Layout == nullptr)
		{
			return MakeError("no world map cutscene tables are known for this release");
		}
		FWorldMapCutsceneTables Tables;
		TResult<FWorldMapPath> Scene9Camera = ReadPath(Program, Layout->Scene9Camera);
		if (!Scene9Camera)
		{
			return Scene9Camera.Error();
		}
		Tables.Scene9Camera = std::move(*Scene9Camera);
		struct FCountedPath
		{
			FWorldMapPath* Path;
			std::size_t Offset;
			std::size_t Count;
		};
		const FCountedPath Counted[] = {
			{&Tables.Scene12FleetCamera, Layout->Scene12FleetCamera, Scene12FleetCameraPoints},
			{&Tables.Scene12LateShipCamera, Layout->Scene12LateShipCamera, Scene12LateShipCameraPoints},
			{&Tables.Scene14Flyer, Layout->Scene14Flyer, Scene14FlyerPoints},
		};
		for (const FCountedPath& Entry : Counted)
		{
			TResult<FWorldMapPath> Points = ReadPoints(Program, Entry.Offset, Entry.Count);
			if (!Points)
			{
				return Points.Error();
			}
			*Entry.Path = std::move(*Points);
		}
		TResult<FWorldMapDirectorTables> Directors = ReadWorldMapDirectorTables(Program, Release);
		if (!Directors)
		{
			return Directors.Error();
		}
		Tables.Directors = std::move(*Directors);
		TResult<FWorldMapScriptTables> Scripts = ReadWorldMapScriptTables(Program, Release);
		if (!Scripts)
		{
			return Scripts.Error();
		}
		Tables.Scripts = std::move(*Scripts);
		return Tables;
	}
}
