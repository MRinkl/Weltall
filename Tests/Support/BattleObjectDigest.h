#pragma once

#include "Support/OracleDigest.h"

namespace Weltall
{
	class FBattleObjects;
	class FPsxRandom;
	struct FBattleCamera;
}

namespace Weltall::Testing
{
	class FRecordingObjectServices;

	/**
	 * The words the sequences share with the rest of the show, as the objects' digest takes them: the effect op 71
	 * asked for, the game's seed, the kernel's count of the tasks made and whether it counts them, the record played.
	 */
	struct FBattleObjectWords
	{
		int32 RequestedEffect = 0;
		uint32 Seed = 0;
		uint32 TasksMade = 0;
		uint32 TasksCounted = 0;
		int32 Record = 0;
	};

	/**
	 * Battle's objects in a frame as the oracles digest the original's: every slot's object (its words, places in its
	 * packs, motion, key
	 * events, trails, CLUT animations, cloths and bones), the track pool, the module's and the model camera's words,
	 * the point lights, the light and colour matrices, the shared words, the trails' pool.
	 */
	void DigestBattleObjects(FOracleDigest& Digest, const FBattleObjects& Objects, const FBattleCamera& Camera, const FBattleObjectWords& Words);

	/** One frame of the objects' oracle: the objects with the recording services' words, then the calls made since the last frame. */
	void DigestBattleObjectsFrame(FOracleDigest& Digest, const FBattleObjects& Objects, const FBattleCamera& Camera, FRecordingObjectServices& Services,
		const FPsxRandom& Random);
}
