#include "WorldMap/WorldMapScriptTables.h"

#include "Foundation/LittleEndian.h"

#include <string>

namespace Weltall
{
	namespace
	{
		/** Where a release's Worldmap program keeps each table, as offsets in the program. */
		struct FScriptTablesLayout
		{
			EDiscRelease Release;
			std::size_t Scene17;
			std::size_t RibbonStops;
			std::size_t Scene18;
		};

		constexpr std::array<FScriptTablesLayout, 2> Layouts{{
			{EDiscRelease::UsaDisc1, 0x2AC68, 0x2AFCC, 0x2B170},
			{EDiscRelease::UsaDisc2, 0x2AC68, 0x2AFCC, 0x2B170},
		}};

		/** No script is longer than this many halfwords. */
		constexpr std::size_t ScriptLimit = 0x400;

		TResult<FWorldMapScript> ReadScript(std::span<const uint8> Program, std::size_t Offset)
		{
			FWorldMapScript Script;
			while (Script.size() < ScriptLimit)
			{
				const std::size_t At = Offset + 2 * Script.size();
				if (At + 2 > Program.size())
				{
					break;
				}
				const int32 Op = LittleEndian::ReadU16(Program, At);
				if (Op >= WorldMapScriptOp::Count)
				{
					return MakeError("a world map cutscene's script has operation " + std::to_string(Op) + ", which the program lacks");
				}
				const std::size_t Halfwords = static_cast<std::size_t>(WorldMapScriptOp::Halfwords(Op));
				if (At + 2 * Halfwords > Program.size())
				{
					break;
				}
				for (std::size_t Word = 0; Word < Halfwords; ++Word)
				{
					Script.push_back(LittleEndian::ReadS16(Program, At + 2 * Word));
				}
				if (Op == WorldMapScriptOp::End)
				{
					return Script;
				}
			}
			return MakeError("a world map cutscene's script has no end");
		}
	}

	int32 WorldMapScriptOp::Halfwords(int32 Op)
	{
		switch (Op)
		{
		case SendMessage:
		case SetStart:
		case SetPlace:
		case SongVolume:
		case FadeSound:
		case SetFade:
			return 4;
		default:
			return 2;
		}
	}

	TResult<FWorldMapScriptTables> ReadWorldMapScriptTables(std::span<const uint8> Program, EDiscRelease Release)
	{
		const FScriptTablesLayout* Layout = nullptr;
		for (const FScriptTablesLayout& Candidate : Layouts)
		{
			if (Candidate.Release == GameplayRelease(Release))
			{
				Layout = &Candidate;
			}
		}
		if (Layout == nullptr)
		{
			return MakeError("no world map cutscene scripts are known for this release");
		}
		FWorldMapScriptTables Tables;
		TResult<FWorldMapScript> Scene17 = ReadScript(Program, Layout->Scene17);
		if (!Scene17)
		{
			return Scene17.Error();
		}
		Tables.Scene17 = std::move(*Scene17);
		TResult<FWorldMapScript> Scene18 = ReadScript(Program, Layout->Scene18);
		if (!Scene18)
		{
			return Scene18.Error();
		}
		Tables.Scene18 = std::move(*Scene18);
		constexpr std::size_t RecordWords = std::tuple_size<FWorldMapRibbonStop::value_type>::value;
		constexpr std::size_t StopWords = RecordWords * std::tuple_size<FWorldMapRibbonStop>::value;
		if (Layout->RibbonStops + 2 * StopWords * Tables.RibbonStops.size() > Program.size())
		{
			return MakeError("the world map's ribbon stops lie past the program's end");
		}
		for (std::size_t Stop = 0; Stop < Tables.RibbonStops.size(); ++Stop)
		{
			for (std::size_t Ribbon = 0; Ribbon < Tables.RibbonStops[Stop].size(); ++Ribbon)
			{
				for (std::size_t Word = 0; Word < RecordWords; ++Word)
				{
					Tables.RibbonStops[Stop][Ribbon][Word] =
						LittleEndian::ReadS16(Program, Layout->RibbonStops + 2 * (Stop * StopWords + Ribbon * RecordWords + Word));
				}
			}
		}
		return Tables;
	}
}
