#include "Support/FieldDrawRun.h"

#include "Field/FieldPresenter.h"
#include "Field/FieldRuntime.h"
#include "Media/PsxExecutable.h"
#include "Support/FieldTickSession.h"

#include <catch_amalgamated.hpp>

#include <sstream>

namespace Weltall::Testing
{
	bool LoadFieldDrawFiles(const FIso9660& Disc, FFieldDrawFiles& Draw)
	{
		if (!LoadOracleFieldFiles(Disc, Draw.Files))
		{
			return false;
		}
		const auto Executable = ReadBootExecutable(Disc);
		if (!Executable)
		{
			UNSCOPED_INFO(Executable.Error().Message);
			return false;
		}
		auto Tables = FFieldDrawingTables::Read(Disc.Image(), Draw.Files->Index, *Executable, EDiscRelease::UsaDisc1);
		if (!Tables)
		{
			UNSCOPED_INFO(Tables.Error().Message);
			return false;
		}
		Draw.Tables = std::move(*Tables);
		return true;
	}

	std::string RunDrawnMap(const FFieldDrawFiles& Draw, const FMesResident& Resident, int32 Map, int32 Ticks,
		const std::function<void(FOracleDigest&, const FFieldFrame&)>& Digest)
	{
		const FOracleFieldFiles& Files = *Draw.Files;
		FOracleTickSession Session(Files, Map);
		FFieldRuntime Runtime(Session.Services(Files, Resident));
		if (const auto Error = Runtime.EnterMap(Map))
		{
			FAIL(Error->Message);
		}
		auto Presenter = FFieldPresenter::Start(*Draw.Tables, Files.Disc->Image(), Files.Index);
		if (!Presenter)
		{
			FAIL(Presenter.Error().Message);
		}
		if (const auto Error = Presenter->EnterMap(Runtime))
		{
			FAIL(Error->Message);
		}
		FFieldFrame Frame;
		std::ostringstream Digests;
		int32 Tick = 0;
		const int32 Loads = Runtime.MapLoads();
		for (const FOraclePadStep& Step : OraclePadScript(Map, Ticks))
		{
			if (const auto Error = Presenter->Tick(Runtime, OracleVblanks(Step, Runtime.VblanksPerTick()), EFieldCulling::InRange, Frame))
			{
				FAIL("tick " << Tick << ": " << Error->Message);
			}
			// The drawing is checked on the map's own ticks, up to a map change.
			if (Runtime.LastFrame() != EFieldFrame::Tick || Runtime.MapLoads() != Loads)
			{
				break;
			}
			FOracleDigest Tick8;
			Digest(Tick8, Frame);
			Digests << (Tick == 0 ? "" : " ") << Tick8.Hex().substr(0, 8);
			if (Runtime.Request().Kind != EFieldRequest::None)
			{
				break;
			}
			++Tick;
		}
		return Digests.str();
	}
}
