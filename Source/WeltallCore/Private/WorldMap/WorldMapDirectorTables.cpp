#include "WorldMap/WorldMapDirectorTables.h"

#include "Foundation/LittleEndian.h"

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		/** Where a script's steps and waits lie. */
		struct FDirectorScriptPlace
		{
			std::size_t Steps;
			std::size_t Waits;
		};

		/** Where a release's Worldmap program keeps each table, as offsets in the program. */
		struct FDirectorTablesLayout
		{
			EDiscRelease Release;
			FDirectorScriptPlace Scene13;
			std::size_t ShotSounds;
			std::size_t ShotStarts;
			std::size_t ShotFieldEntrances;
			std::array<FDirectorScriptPlace, FWorldMapDirectorTables::ShotEntrances> ShotScripts;
			std::size_t ShotHolds;
			std::size_t ShotRingScales;
			FDirectorScriptPlace Scene16;
		};

		constexpr std::array<FDirectorTablesLayout, 2> DirectorLayouts{{
			{EDiscRelease::UsaDisc1, {0x2ABA8, 0x2ABBC}, 0x2AAB0, 0x2AAC4, 0x2AADC, {{{0x2AAE4, 0x2AB00}, {0x2AB1C, 0x2AB30}, {0x2AB44, 0x2AB58}}}, 0x2AB84,
				0x2AB9C, {0x2ABD0, 0x2AC1C}},
			{EDiscRelease::UsaDisc2, {0x2ABA8, 0x2ABBC}, 0x2AAB0, 0x2AAC4, 0x2AADC, {{{0x2AAE4, 0x2AB00}, {0x2AB1C, 0x2AB30}, {0x2AB44, 0x2AB58}}}, 0x2AB84,
				0x2AB9C, {0x2ABD0, 0x2AC1C}},
		}};

		/** The step that ends a scene, and so its script; none is longer than this. */
		constexpr uint16 ScriptLeaveStep = 0x40;
		constexpr std::size_t DirectorScriptLimit = 64;
		/** A start point and a hold point: x, y, z and a pad. */
		constexpr std::size_t DirectorPointSize = 8;

		TResult<FWorldMapDirectorScript> ReadDirectorScript(std::span<const uint8> Program, const FDirectorScriptPlace& Place)
		{
			FWorldMapDirectorScript Script;
			for (std::size_t Index = 0; Index < DirectorScriptLimit; ++Index)
			{
				if (Place.Steps + 2 * Index + 2 > Program.size() || Place.Waits + 2 * Index + 2 > Program.size())
				{
					break;
				}
				Script.Steps.push_back(ReadU16(Program, Place.Steps + 2 * Index));
				Script.Waits.push_back(ReadU16(Program, Place.Waits + 2 * Index));
				if (Script.Steps.back() == ScriptLeaveStep)
				{
					return Script;
				}
			}
			return MakeError("a world map cutscene's script has no end");
		}
	}

	TResult<FWorldMapDirectorTables> ReadWorldMapDirectorTables(std::span<const uint8> Program, EDiscRelease Release)
	{
		const FDirectorTablesLayout* Layout = nullptr;
		for (const FDirectorTablesLayout& Candidate : DirectorLayouts)
		{
			if (Candidate.Release == Release)
			{
				Layout = &Candidate;
			}
		}
		if (Layout == nullptr)
		{
			return MakeError("no world map cutscene scripts are known for this release");
		}
		constexpr std::size_t Entrances = FWorldMapDirectorTables::ShotEntrances;
		const auto Fits = [&Program](std::size_t Offset, std::size_t Bytes) { return Offset + Bytes <= Program.size(); };
		if (!Fits(Layout->ShotSounds, Entrances * 6) || !Fits(Layout->ShotStarts, Entrances * DirectorPointSize) || !Fits(Layout->ShotFieldEntrances, Entrances * 2)
			|| !Fits(Layout->ShotHolds, Entrances * DirectorPointSize) || !Fits(Layout->ShotRingScales, 2 * FWorldMapDirectorTables::ShotRings))
		{
			return MakeError("the world map's program ends before its cutscene scripts");
		}
		FWorldMapDirectorTables Tables;
		TResult<FWorldMapDirectorScript> Scene13 = ReadDirectorScript(Program, Layout->Scene13);
		if (!Scene13)
		{
			return Scene13.Error();
		}
		Tables.Scene13 = std::move(*Scene13);
		for (std::size_t Entrance = 0; Entrance < Entrances; ++Entrance)
		{
			FWorldMapShotEntrance& Shot = Tables.Scene15[Entrance];
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Shot.Start[Axis] = ReadS16(Program, Layout->ShotStarts + Entrance * DirectorPointSize + 2 * Axis);
				Shot.Sounds[Axis] = ReadU16(Program, Layout->ShotSounds + (Entrance * 3 + Axis) * 2);
			}
			Shot.Hold = {ReadS16(Program, Layout->ShotHolds + Entrance * DirectorPointSize), ReadS16(Program, Layout->ShotHolds + Entrance * DirectorPointSize + 4)};
			Shot.FieldEntrance = ReadU16(Program, Layout->ShotFieldEntrances + 2 * Entrance);
			TResult<FWorldMapDirectorScript> Script = ReadDirectorScript(Program, Layout->ShotScripts[Entrance]);
			if (!Script)
			{
				return Script.Error();
			}
			Shot.Script = std::move(*Script);
		}
		for (std::size_t Ring = 0; Ring < Tables.ShotRingScales.size(); ++Ring)
		{
			Tables.ShotRingScales[Ring] = ReadU16(Program, Layout->ShotRingScales + 2 * Ring);
		}
		TResult<FWorldMapDirectorScript> Scene16 = ReadDirectorScript(Program, Layout->Scene16);
		if (!Scene16)
		{
			return Scene16.Error();
		}
		Tables.Scene16 = std::move(*Scene16);
		return Tables;
	}
}
