#pragma once

#include "Foundation/Sha1.h"
#include "Support/BattleSpriteDigest.h"
#include "Support/OracleDigest.h"

#include <optional>

namespace Weltall
{
	class FBattleShow;
	class FPsxRandom;
	struct FBattleContext;
}

namespace Weltall::Testing
{
	class FBattleSoundRecorder;

	/**
	 * A battle show's frame digested as the show's oracle digests it: the
	 * files numbered as the oracle numbers them (the scene's sprite files, the files read after in their order, a
	 * sequence extension's resources by its pack), the camera and flags, the objects, VRAM, and a played frame whole.
	 */
	class FBattleShowDigester
	{
	public:
		FBattleShowDigester(FBattleShow& InShow, const FBattleContext& InContext, const FPsxRandom& InRandom, FBattleSoundRecorder& InSound);

		/** The camera, the flags the frame sets, the counted tasks and seed, the common effects' resource and the sounds played (digest_state). */
		void DigestState(FOracleDigest& One);
		/** The 3D objects as the oracle digests them, with the words their sequences share with the show. */
		void DigestObjects(FOracleDigest& One);
		/** VRAM's hash as the frame left it (hashed again only once it has changed). */
		void DigestVram(FOracleDigest& One);
		/** The update list whole (digest_tasks). */
		void DigestTasks(FOracleDigest& One);
		/** The playback's state: the controller, the record played, the targets, the action file, the flags, the slots' marks (digest_playback). */
		void DigestPlayback(FOracleDigest& One);
		/** A frame as it is shown: the tasks whole, the playback's state, the show's, VRAM (digest_played). */
		void DigestPlayed(FOracleDigest& One);

		FBattleSpriteDigester Digester;

	private:
		FBattleShow& Show;
		const FBattleContext& Context;
		const FPsxRandom& Random;
		FBattleSoundRecorder& Sound;
		std::optional<FSha1::FDigest> VramHash;
		uint32 HashedRevision = 0;
	};
}
