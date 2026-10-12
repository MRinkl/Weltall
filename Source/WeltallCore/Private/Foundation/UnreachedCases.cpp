#include "Foundation/UnreachedCases.h"

#include <algorithm>
#include <utility>

namespace Weltall
{
	bool FUnreachedCases::Recover(std::string_view Case)
	{
		if (bStops)
		{
			return false;
		}
		if (std::find(Seen.begin(), Seen.end(), Case) == Seen.end())
		{
			Seen.emplace_back(Case);
			Noted.emplace_back(Case);
		}
		return true;
	}

	std::vector<std::string> FUnreachedCases::TakeNoted()
	{
		return std::exchange(Noted, {});
	}
}
