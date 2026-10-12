#pragma once

#include "Field/FieldDrawingTables.h"
#include "Field/FieldFrame.h"
#include "Support/FieldOracleMap.h"
#include "Support/OracleDigest.h"
#include "Text/MesResident.h"

#include <functional>
#include <optional>
#include <string>

namespace Weltall::Testing
{
	/** What the drawing oracles' maps are played with: the tick oracle's files and the field's drawing tables. */
	struct FFieldDrawFiles
	{
		std::optional<FOracleFieldFiles> Files;
		std::optional<FFieldDrawingTables> Tables;
	};

	/** Reads them from disc 1; false (the reason reported) when it cannot. */
	bool LoadFieldDrawFiles(const FIso9660& Disc, FFieldDrawFiles& Draw);

	/**
	 * A map played as the tick oracle plays it (FieldTickOracleTests.cpp), drawn by the field's
	 * presenter, each of the map's own ticks' frames digested by Digest, up to Ticks of them, a map
	 * change or a request: the digests' first 8 hex digits, space-separated.
	 */
	std::string RunDrawnMap(const FFieldDrawFiles& Draw, const FMesResident& Resident, int32 Map, int32 Ticks,
		const std::function<void(FOracleDigest&, const FFieldFrame&)>& Digest);
}
