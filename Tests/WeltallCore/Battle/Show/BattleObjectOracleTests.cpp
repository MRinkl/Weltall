#include "Battle/Show/BattleShowObjects.h"
#include "Foundation/Sha1.h"
#include "Support/BattleObjectCases.h"
#include "Support/BattleObjectDigest.h"
#include "Support/GameDiscs.h"
#include "Support/OracleDigest.h"
#include "Support/XorShift32.h"

#include <catch_amalgamated.hpp>

#include <string>

using namespace Weltall;
using Weltall::Testing::FBattleObjectCase;
using Weltall::Testing::FBattleObjectCaseFrames;
using Weltall::Testing::FBattleObjectData;
using Weltall::Testing::FOracleDigest;
using Weltall::Testing::FXorShift32;

// Battle's 3D objects frame by frame against the original's: the
// same cases drawn from XorShift32, the same calls answered the same way, the same digest.
namespace
{
	constexpr uint32 ObjectSeed = 0x0B1EC7A5;
	constexpr int32 ObjectCases = 100;
	constexpr uint32 MostFrames = 200;

	void DigestState(FBattleObjectCase& Case, FOracleDigest& Digest)
	{
		Weltall::Testing::DigestBattleObjectsFrame(Digest, Case.Objects, Case.Camera, Case.Services, Case.Random);
	}
}

TEST_CASE("Battle objects step and draw as the original does frame by frame", "[Content][BattleObjects]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FBattleObjectData Data;
	REQUIRE(Weltall::Testing::ReadBattleObjectData(*Disc, Data));
	FBattleObjectCaseFrames Frames;
	Frames.MostFrames = MostFrames;
	Frames.DigestStart = DigestState;
	Frames.Draw = [](FBattleObjectCase& Case, const FGteMatrix& View, int32 Buffer) { Case.Objects.UpdateDraw(View, 0, Buffer); };
	Frames.DigestFrame = DigestState;
	FXorShift32 G(ObjectSeed);
	FSha1 Whole;
	std::string Cases;
	for (int32 Number = 0; Number < ObjectCases; ++Number)
	{
		FOracleDigest One;
		INFO("case " << Number);
		const std::string Ends = Weltall::Testing::RunBattleObjectCase(G, Data, One, Frames);
		const FSha1::FDigest Case = One.Finish();
		Whole.Update(Case);
		Cases += "objects " + std::to_string(Number) + " " + FSha1::ToHex(Case) + (Ends.empty() ? std::string() : " (ends: " + Ends + ")") + "\n";
	}
	INFO(Cases);
	CHECK(FSha1::ToHex(Whole.Finish()) == "526f25c71fc354e296f92f7f9dc49a331501bf76");
}
