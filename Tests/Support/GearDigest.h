#pragma once

#include "Support/OracleDigest.h"

namespace Weltall
{
	class FGearModels;
	struct FEventContext;
	struct FGearBone;
	struct FGearModel;
	class FGearChannelPool;
}

namespace Weltall::Testing
{
	/** A model's state as the gears' oracle reads the original's. */
	void DigestGearModel(FOracleDigest& Digest, const FGearModel& Model);
	void DigestGearBone(FOracleDigest& Digest, const FGearBone& Bone);
	/** Every cloth point's position and normal. */
	void DigestGearCloths(FOracleDigest& Digest, const FGearModel& Model);
	/** The pool's lowest free and capacity, then every channel. */
	void DigestGearPool(FOracleDigest& Digest, const FGearChannelPool& Pool);
	/** The module's words, every loaded model with its bones and cloths, the pool (digest_models). */
	void DigestGearModels(FOracleDigest& Digest, const FGearModels& Models);
	/** The tick oracle's gear part: nothing unless the map's load loaded gears. */
	void DigestFieldGears(FOracleDigest& Digest, const FEventContext& Context);
}
