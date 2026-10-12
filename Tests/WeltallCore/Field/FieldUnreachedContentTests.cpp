#include "Field/FieldRuntime.h"
#include "Support/FieldTickSession.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"

#include <catch_amalgamated.hpp>

#include <functional>
#include <string>
#include <vector>

using namespace Weltall;

// Cases no script of the game reaches, on a real map: the field stops at each, as the tests' checks against the
// original do, or given a record of such cases carries on past it with its natural outcome.

namespace
{
	constexpr int32 Map = 1;

	/**
	 * Map 1 entered, its entry run; then Poke changes the field and it runs a few ticks more. The first error, or
	 * none; RunningMap the map it runs at the end.
	 */
	std::optional<std::string> RunPoked(const Testing::FOracleFieldFiles& Files, const FMesResident& Resident, FUnreachedCases* Cases,
		const std::function<void(FEventContext&)>& Poke, int32* RunningMap = nullptr)
	{
		Testing::FOracleTickSession Session(Files, Map);
		FFieldServices Services = Session.Services(Files, Resident);
		Services.Unreached = Cases;
		FFieldRuntime Runtime(Services);
		REQUIRE_FALSE(Runtime.EnterMap(Map));
		for (int32 Tick = 0; Tick < FFieldRuntime::EntryTicks; ++Tick)
		{
			REQUIRE_FALSE(Runtime.Tick(Testing::OracleVblanks({}, Runtime.VblanksPerTick())));
		}
		Poke(Runtime.Context());
		std::optional<std::string> Stopped;
		for (int32 Tick = 0; Tick < 8 && !Stopped; ++Tick)
		{
			if (auto Error = Runtime.Tick(Testing::OracleVblanks({}, Runtime.VblanksPerTick())))
			{
				Stopped = Error->Message;
			}
		}
		if (RunningMap != nullptr)
		{
			*RunningMap = Runtime.Context().RunningMap;
		}
		return Stopped;
	}
}

TEST_CASE("A map change with a transition past the seven stops the field, or carried past cuts to the next map", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	// To map 2 by its entrance 0, with transition 9.
	const auto ChangeMap = [](FEventContext& Context)
	{
		Context.Requests.bMapChange = true;
		Context.Requests.MapTransition = 9;
		Context.MapNumber = 2;
		Context.Variables.Set(0x002, 0);
	};
	CHECK(RunPoked(*Files, Resident, nullptr, ChangeMap).has_value());
	FUnreachedCases Cases;
	int32 Running = 0;
	CHECK_FALSE(RunPoked(*Files, Resident, &Cases, ChangeMap, &Running).has_value());
	CHECK(Running == 2);
	CHECK(Cases.TakeNoted() == std::vector<std::string>{"a map change with a transition past the seven"});
}
