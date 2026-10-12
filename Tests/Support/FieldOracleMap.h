#pragma once

#include "Field/FieldPartySprites.h"
#include "Field/FieldScreenOverlays.h"
#include "Foundation/Types.h"
#include "Game/GameState.h"
#include "Graphics/GteMath.h"
#include "Media/GameFileIndex.h"
#include "Media/Iso9660.h"
#include "Support/FieldSongsModel.h"
#include "Support/OracleDigest.h"

#include <array>
#include <optional>
#include <span>
#include <string>

namespace Weltall
{
	class FFieldSprites;
	class FPsxRandom;
}

namespace Weltall::Testing
{
	/** What the field oracles read from the first disc for every map. */
	struct FOracleFieldFiles
	{
		const FIso9660* Disc = nullptr;
		FGameFileIndex Index;
		FGteArctangents Arctangents;
		/** A zeroed game state with a new game's party bytes. */
		FGameState NewGame;
		/** The party's sprite files and the field's party as the field enters a map on foot with that party. */
		std::optional<FPartySpriteFiles> EnteredParty;
		std::array<uint8, 3> EnteredFieldParty{};
		/** Where the cockpit's instruments lie, for their quads. */
		FFieldCockpitHudLayout CockpitHud;
	};

	/** The oracles' files from the disc; false when one cannot be read. */
	bool LoadOracleFieldFiles(const FIso9660& Disc, std::optional<FOracleFieldFiles>& Files);

	/**
	 * A real map laid out as the init oracle lays it out (a new game's party with its
	 * sprite files loaded as the field enters the map, the map's actors reset and placed), its init
	 * run, then Ticks ticks of events, SpriteTicks ticks of every script actor's sprite animation (in
	 * actor order, then the spawned children) and the frames the sprites queued built; the digest the
	 * oracle takes of it. Scripts, when given, stand in for the map's own script section.
	 */
	/**
	 * The init oracle's digest of a map as it stands: the
	 * scripts' state, the records, the field's words, the party and its sprite files, every sprite and
	 * spawned child, and the uploads asked for. bDrawn: the original has drawn the sprites since
	 * (DigestSprite).
	 */
	void DigestOracleMap(FOracleDigest& Digest, const FEventContext& Context, const FPsxRandom& Random, const FFieldSongsModel& Songs,
		const FPartySpriteFiles& Party, const FGameState& Game, const FFieldSprites& Sprites, std::span<const FSpriteFile> Files, bool bDrawn = false);

	std::string RunOracleMap(const FOracleFieldFiles& Files, int32 Number, std::optional<std::span<const uint8>> Scripts = std::nullopt, int32 Ticks = 0,
		int32 SpriteTicks = 0);
}
