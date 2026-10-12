#include "Tools/StorySweepList.h"

#include "Field/EventScripts.h"
#include "Field/FieldArchive.h"
#include "Field/FieldMap.h"
#include "Support/StoryValues.h"

#include <cstdlib>
#include <deque>
#include <map>
#include <optional>
#include <set>

namespace Weltall::Testing
{
	namespace
	{
		std::optional<FStoryReferences> ReadMapScripts(const FPlayedDisc& Disc, int32 Map)
		{
			const auto Archive = LoadFieldArchive(Disc.Iso->Image(), *Disc.Index, Map);
			if (!Archive)
			{
				return std::nullopt;
			}
			const auto Scripts = FEventScripts::Parse(Archive->Section(EFieldSection::EventScripts));
			if (!Scripts)
			{
				return std::nullopt;
			}
			return FindStoryReferences(*Scripts);
		}

		/** The values the nearest map that compares the story's progress compares it with: nearest by map changes, then by number. */
		const std::set<uint16>* NearestCompared(const std::map<int32, FStoryReferences>& Read, const std::map<int32, std::set<int32>>& Neighbours, int32 Map)
		{
			std::set<int32> Seen{Map};
			std::deque<int32> Pending{Map};
			while (!Pending.empty())
			{
				const int32 At = Pending.front();
				Pending.pop_front();
				const auto Known = Read.find(At);
				if (Known != Read.end() && !Known->second.Compared.empty())
				{
					return &Known->second.Compared;
				}
				const auto Next = Neighbours.find(At);
				for (const int32 Other : Next != Neighbours.end() ? Next->second : std::set<int32>{})
				{
					if (Seen.insert(Other).second)
					{
						Pending.push_back(Other);
					}
				}
			}
			const std::set<uint16>* Nearest = nullptr;
			int32 Distance = FieldMapCount;
			for (const auto& [Other, Scripts] : Read)
			{
				if (!Scripts.Compared.empty() && std::abs(Other - Map) < Distance)
				{
					Distance = std::abs(Other - Map);
					Nearest = &Scripts.Compared;
				}
			}
			return Nearest;
		}
	}

	std::vector<FStorySweepMap> ListStorySweepMaps(const FPlayedGame& Game)
	{
		std::map<int32, FStoryReferences> Read;
		for (int32 Map = 0; Map < FieldMapCount; ++Map)
		{
			if (auto Scripts = ReadMapScripts(Game.Discs[0], Map))
			{
				Read.emplace(Map, std::move(*Scripts));
			}
		}
		// A map change leads both ways: the map arrived at is as near the one left as the reverse.
		std::map<int32, std::set<int32>> Neighbours;
		for (const auto& [Map, Scripts] : Read)
		{
			for (const int32 Other : Scripts.Neighbours)
			{
				Neighbours[Map].insert(Other);
				Neighbours[Other].insert(Map);
			}
		}
		std::vector<FStorySweepMap> Maps;
		for (const auto& [Map, Scripts] : Read)
		{
			FStorySweepMap Entry;
			Entry.Map = Map;
			if (!Scripts.Compared.empty())
			{
				Entry.Stories = StoryValuesToldApart(Scripts.Compared);
			}
			else if (const std::set<uint16>* Nearest = NearestCompared(Read, Neighbours, Map))
			{
				Entry.bBorrowed = true;
				Entry.Stories.assign(Nearest->begin(), Nearest->end());
			}
			Maps.push_back(std::move(Entry));
		}
		return Maps;
	}
}
