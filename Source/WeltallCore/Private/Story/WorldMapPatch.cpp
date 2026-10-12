#include "Story/WorldMapPatch.h"

#include "Field/EventAssemblyText.h"
#include "Foundation/NumberText.h"
#include "Text/MesText.h"

#include <algorithm>
#include <array>

namespace Weltall
{
	namespace
	{
		using EventAssemblyText::WordsOf;

		constexpr int64 LastEra = 8;
		constexpr int64 LastCoordinate = 0x7FFF;
		constexpr int64 LastFieldMap = 1023;
		constexpr int64 LastEntrance = 0xFF;
		/** The world map's entrance list ends with -1; 4095 is a new game's own. */
		constexpr int64 LastArrival = 4094;
		constexpr int64 LastStory = 0x7FFF;
		/** The minimap's last place at a fixed spot: those after it are the ship's, Shevat's and the Thames'. */
		constexpr int64 LastDot = 23;
		/** The place-name window's width, in the units a message's window is measured in, and its one line. */
		constexpr int32 NameWidthUnits = 33;
		constexpr int16 EnterOnConfirm = 0;
		constexpr int16 EnterOnTouch = 1;
		constexpr std::string_view PlaceShape = "a place reads 'place on foot x X z Z width W height H to map M entrance E name N', "
												"'in gears' or 'in vehicle' for 'on foot', 'on touch' before 'to' on foot";

		/** The trigger lists by the words after "place". */
		constexpr std::array<std::pair<std::string_view, std::string_view>, 3> ListWords{{{"on", "foot"}, {"in", "gears"}, {"in", "vehicle"}}};

		FError LineError(int32 Line, std::string_view Words)
		{
			std::string Message = "line ";
			NumberText::Append(Message, Line);
			Message += ": ";
			Message += Words;
			return MakeError(std::move(Message));
		}

		std::optional<int16> Coordinate(std::string_view Word, int64 Last = LastCoordinate)
		{
			const std::optional<int64> Value = NumberText::NumberWithin(Word, 0, Last);
			return Value ? std::optional<int16>(static_cast<int16>(*Value)) : std::nullopt;
		}
	}

	TResult<FWorldMapPatch> FWorldMapPatch::Parse(std::string_view Text)
	{
		FWorldMapPatch Patch;
		bool bEra = false;
		std::string Naming;
		std::string Told;
		bool bBrace = false;
		for (int32 Number = 1; !Text.empty(); ++Number)
		{
			const std::size_t End = std::min(Text.find('\n'), Text.size());
			std::string_view Line = Text.substr(0, End);
			Text.remove_prefix(std::min(End + 1, Text.size()));
			if (Line.ends_with('\r'))
			{
				Line.remove_suffix(1);
			}
			if (!Naming.empty() && bBrace)
			{
				if (Line != "}")
				{
					Told += Told.empty() ? "" : "\n";
					Told += Line;
					continue;
				}
				const std::optional<FAddedMessage> Name = MesMessageOfText(Told);
				if (!Name)
				{
					return LineError(Number, "a character or a mark the messages lack");
				}
				if (Name->Lines > 1 || Name->WidthUnits > NameWidthUnits)
				{
					return LineError(Number, "a place's name is one line of 16 letters at most");
				}
				Patch.Names.emplace_back(std::move(Naming), *Name);
				Naming.clear();
				Told.clear();
				bBrace = false;
				continue;
			}
			const std::vector<std::string_view> Words = WordsOf(Line);
			if (!Naming.empty())
			{
				if (Words.size() != 1 || Words.front() != "{")
				{
					return LineError(Number, "a name's text begins after a line of '{'");
				}
				bBrace = true;
			}
			else if (Words.empty())
			{
			}
			else if (Words.front() == "world")
			{
				const std::optional<int64> Era = Words.size() == 3 && Words[1] == "map" ? NumberText::NumberWithin(Words[2], 0, LastEra) : std::nullopt;
				if (!Era || bEra)
				{
					return LineError(Number, "'world map E' takes the era, 0 to 8, once");
				}
				Patch.EraNumber = static_cast<int32>(*Era);
				bEra = true;
			}
			else if (!bEra)
			{
				return LineError(Number, "the text begins 'world map E'");
			}
			else if (Words.front() == "message")
			{
				const bool bNamed = Words.size() == 2 && !Words[1].starts_with('#');
				const bool bTaken = bNamed && std::any_of(Patch.Names.begin(), Patch.Names.end(), [&Words](const auto& Name) { return Name.first == Words[1]; });
				if (!bNamed || bTaken)
				{
					return LineError(Number, "'message Name' names a place's name, once");
				}
				Naming = std::string(Words[1]);
			}
			else if (Words.front() == "place")
			{
				// place on foot x X z Z width W height H [on touch] to map M entrance E name N
				const bool bTouch = Words.size() == 20 && Words[11] == "on" && Words[12] == "touch";
				const std::size_t To = bTouch ? 13 : 11;
				const bool bShaped = (Words.size() == 18 || bTouch) && Words[3] == "x" && Words[5] == "z" && Words[7] == "width" && Words[9] == "height"
					&& Words[To] == "to" && Words[To + 1] == "map" && Words[To + 3] == "entrance" && Words[To + 5] == "name";
				const auto List = std::find_if(ListWords.begin(), ListWords.end(), [&Words](const auto& Pair) { return Words.size() > 2 && Words[1] == Pair.first && Words[2] == Pair.second; });
				const std::optional<int16> X = bShaped ? Coordinate(Words[4]) : std::nullopt;
				const std::optional<int16> Z = bShaped ? Coordinate(Words[6]) : std::nullopt;
				const std::optional<int16> Width = bShaped ? Coordinate(Words[8]) : std::nullopt;
				const std::optional<int16> Height = bShaped ? Coordinate(Words[10]) : std::nullopt;
				const std::optional<int16> Map = bShaped ? Coordinate(Words[To + 2], LastFieldMap) : std::nullopt;
				const std::optional<int16> Entrance = bShaped ? Coordinate(Words[To + 4], LastEntrance) : std::nullopt;
				if (List == ListWords.end() || !X || !Z || !Width || !Height || !Map || !Entrance || (bTouch && List != ListWords.begin()))
				{
					return LineError(Number, PlaceShape);
				}
				FPlace Place;
				Place.List = static_cast<int32>(List - ListWords.begin());
				Place.Record = {*X, *Z, *Width, *Height, *Map, *Entrance, 0, bTouch ? EnterOnTouch : EnterOnConfirm};
				const std::string_view Name = Words[To + 6];
				if (Name.starts_with('#'))
				{
					const std::optional<int16> Own = Coordinate(Name.substr(1));
					if (!Own)
					{
						return LineError(Number, "a name of the scene's own is '#' and its number");
					}
					Place.Record.Name = *Own;
				}
				else if (std::none_of(Patch.Names.begin(), Patch.Names.end(), [&Name](const auto& Each) { return Each.first == Name; }))
				{
					return LineError(Number, "a place's name is a message before it, or one of the scene's own by number (#20)");
				}
				else
				{
					Place.Name = std::string(Name);
				}
				Patch.Places.push_back(std::move(Place));
			}
			else if (Words.front() == "arrival")
			{
				const bool bShaped = Words.size() == 6 && Words[2] == "x" && Words[4] == "z";
				const std::optional<int16> Id = bShaped ? Coordinate(Words[1], LastArrival) : std::nullopt;
				const std::optional<int16> X = bShaped ? Coordinate(Words[3]) : std::nullopt;
				const std::optional<int16> Z = bShaped ? Coordinate(Words[5]) : std::nullopt;
				if (!Id || !X || !Z)
				{
					return LineError(Number, "an arrival reads 'arrival N x X z Z'");
				}
				Patch.Arrivals.push_back({*X, *Id, *Z});
			}
			else if (Words.front() == "dot")
			{
				const std::optional<int64> Dot = Words.size() == 2 ? NumberText::NumberWithin(Words[1], 0, LastDot) : std::nullopt;
				if (!Dot)
				{
					return LineError(Number, "a dot reads 'dot N', one of the minimap's places, 0 to 23");
				}
				Patch.Dots |= 1u << *Dot;
			}
			else if (Words.front() == "vehicle")
			{
				// vehicle away at story S, or S-T
				const bool bShaped = Words.size() == 5 && Words[1] == "away" && Words[2] == "at" && Words[3] == "story";
				const std::size_t Dash = bShaped ? Words[4].find('-', 1) : std::string_view::npos;
				const std::optional<int64> First = bShaped ? NumberText::NumberWithin(Words[4].substr(0, Dash), 0, LastStory) : std::nullopt;
				const std::optional<int64> Last = !First ? std::nullopt
					: Dash == std::string_view::npos ? First : NumberText::NumberWithin(Words[4].substr(Dash + 1), 0, LastStory);
				if (!First || !Last || *Last < *First)
				{
					return LineError(Number, "the ship away reads 'vehicle away at story S', or S-T");
				}
				Patch.VehicleAway.emplace_back(static_cast<uint16>(*First), static_cast<uint16>(*Last));
			}
			else
			{
				return LineError(Number, "a line is a message, a place, an arrival, a dot or the ship away");
			}
		}
		if (!bEra)
		{
			return MakeError("the text begins 'world map E'");
		}
		if (!Naming.empty())
		{
			return MakeError("the name '" + Naming + "' has no text");
		}
		return Patch;
	}

	bool FWorldMapPatch::KeepsVehicleAway(uint16 Story) const
	{
		return std::any_of(VehicleAway.begin(), VehicleAway.end(), [Story](const auto& Range) { return Range.first <= Story && Story <= Range.second; });
	}

	std::optional<FError> FWorldMapPatch::Apply(FWorldMapScene& Scene) const
	{
		for (const FWorldMapEntrance& Arrival : Arrivals)
		{
			if (std::any_of(Scene.Entrances.begin(), Scene.Entrances.end(), [&Arrival](const FWorldMapEntrance& Own) { return Own.Id == Arrival.Id; }))
			{
				return MakeError("arrival " + std::to_string(Arrival.Id) + " is one of the world map's own");
			}
		}
		const TResult<FMessageTable> Own = Scene.Messages();
		if (!Own)
		{
			return Own.Error();
		}
		for (const FPlace& Place : Places)
		{
			if (!Scene.TriggerLists[static_cast<std::size_t>(Place.List)])
			{
				return MakeError("the world map has no list of places " + std::string(ListWords[static_cast<std::size_t>(Place.List)].first) + " "
					+ std::string(ListWords[static_cast<std::size_t>(Place.List)].second));
			}
			if (Place.Name.empty() && Place.Record.Name >= Own->Count())
			{
				return MakeError("the world map has no name " + std::to_string(Place.Record.Name));
			}
		}
		std::vector<FAddedMessage> Added;
		for (const auto& Name : Names)
		{
			Added.push_back(Name.second);
		}
		std::vector<uint8> Messages = Scene.MessageBytes;
		const std::optional<int32> First = Added.empty() ? std::optional<int32>(Own->Count()) : AppendMessages(Messages, Added);
		if (!First)
		{
			return MakeError("the world map's names cannot take more");
		}
		Scene.MessageBytes = std::move(Messages);
		for (const FPlace& Place : Places)
		{
			FWorldMapTrigger Record = Place.Record;
			if (!Place.Name.empty())
			{
				const auto Named = std::find_if(Names.begin(), Names.end(), [&Place](const auto& Name) { return Name.first == Place.Name; });
				Record.Name = static_cast<int16>(*First + (Named - Names.begin()));
			}
			Scene.TriggerLists[static_cast<std::size_t>(Place.List)]->push_back(Record);
		}
		Scene.Entrances.insert(Scene.Entrances.end(), Arrivals.begin(), Arrivals.end());
		Scene.StoryDots |= Dots;
		return std::nullopt;
	}
}
