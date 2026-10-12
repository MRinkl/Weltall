#include "Game/PsxRandom.h"
#include "Gears/GearModels.h"
#include "Support/GameDiscs.h"
#include "Support/GearDisc.h"
#include "Support/GearDigest.h"
#include "Support/OracleDigest.h"
#include "Support/XorShift32.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <optional>
#include <vector>

using namespace Weltall;
using Testing::FGearDisc;
using Testing::FOracleDigest;
using Testing::FXorShift32;

// Digests of the game's own gears' module run on real gears, under the oracle:
// each case loads one gear model (every model
// once, one case in four a second one), places it as the field places a gear actor each tick, plays
// random animations of its scripts, feeds its root motion back into the actor, snaps poses through
// the first ticks one case in three and runs the gear shop's cadence one case in six, for 48 ticks.
// Each tick's digest covers the module's words, every model's state, every bone, a random bone's
// matrix, the cloths' points and the whole channel pool.

namespace
{
	constexpr uint32 Seed = 0x6EA2;
	constexpr int32 Cases = 72;
	constexpr int32 Ticks = 48;
	constexpr int32 PoolSize = 0x40;
	/** What the gear shop passes for its extra vblanks: a word whose sign bit makes the counter negative every other frame. */
	constexpr uint32 ShopExtraVblanks = 0x801CB480;
	constexpr uint32 FieldExtraVblanks = 1;

	constexpr const char* Expected[Cases] = {
		"6ccefdaaf4765806b9b04f869f0b93032cdeb09f",
		"830ccea8a6daf70aa03b84a9920861a5eb1c2a24",
		"91fd6978894913dc7e205e37f587afa23ae67915",
		"f44e35b5dc68e9f69124349b96920e9595bbc7cf",
		"1bab918acb327882057098a19b5c5bf30264f16a",
		"4f066a099b5ed44e6ca0110ca922ed6869a154b0",
		"297f5d7fcb66e519d51cc50e22c9c8f8c61a95a4",
		"58f1a4eca0b3d40168e96ca66fa908ab8253beda",
		"e95a80278dc39a510440292a3e4bc398a680bc74",
		"2f6bda979f33a0dab41edfa13a5351dd7be2d6ff",
		"41d271a086998c538cf71e886cf090a50325575a",
		"57b98ad4967e7019ac5bdbb2dd0531a28bfb6b21",
		"fabad930b2d731e69569a5d778014cf7cbed5d17",
		"f1f144648f877e6b07baa7933cd09e7174a5171b",
		"fc671f3d5fb831490ab90e88091bd1817be9aa2d",
		"c430316c01b06a6c15fbbd75506e3b690dfa89fa",
		"7fb11f56b2708cbbc808e7be5f7be797d4cde81d",
		"0713ff6fc3895753ea5fd27ef8aef51333bfd9bb",
		"b2265cf4ae660a962af70e20d43de851f71a477a",
		"2b659b127e6fdb68f3d73b481d70082afe66a796",
		"770f30ec247a627b4feed7db453c87f638865c4c",
		"ea731369c2588f22bb90cb3ad81db378616a51fe",
		"5e0d1d348317f86f7804b01ec4c374ccf733481d",
		"68e52507aca5a974a9531d37ec9c7f78468624d2",
		"40b915c7d5b94248673a0262708193c9e2a2e158",
		"55b81c8e2b598300a87216dcd8085bd680011f08",
		"63c50a63dbfcc6be03e4a2683df145704588a25d",
		"4a7dd315e63d753376dac556723ec51222e0b02b",
		"8f440d98f4bda18f798d512ef659eb5209e134bc",
		"e3ae993465d804a7ef5b771d1b57f42628a89e8a",
		"76489ee7bf0bdb6a27884bb009f16f8e976cb258",
		"1a87ae9d86a191c20d1c9301d68525dd737a4e32",
		"83417c357e6f9d84ca8ba39ac289978410d0e0c8",
		"3fd6e8e8f7f57bfbd2587a6291728862e6fe0b8e",
		"805d0022f2805aa06dd1839efa9a7731f00479a4",
		"82d3f31d9fc1315e19c0827847a095d6924b97a3",
		"8d8b7544d38c95cf392ca11205e4b561e38022fc",
		"3259999ab42d48acce75cc1a3e80a17aec22c502",
		"080015d3e150b1d9d814511ac4124fe985194e5d",
		"4c0d49674ffac08a1ebccde39d23596d60c0781e",
		"a78f5374098406e4542d0901cd125576792220f2",
		"bf2e5b0a79a06e0df63509769f623590ee1394f3",
		"9b970feb05bbe45b10328c56a78225ad2cfba5ba",
		"7f1e4f0743255c6d33586aa6c444c35c1aa360e8",
		"1282368a43f38a2b2f5f64927dbc568938ade457",
		"7b96ef912d727ed2754860d1c213529a17111427",
		"c4c235a060b3a1e1294149ffb78277c80b57b590",
		"641daa0211bf5ea2157e930ab99831edfde8d2bd",
		"6740f57f05e82109339e5b8aa200b7164550696e",
		"4e75499914030f81e7ba79b5068916bd46470374",
		"44f9099fa8e4649f4e1ecd1aac40ca38fa7143fd",
		"1b3a204eae39edd93ea8132bae85867bcd183a49",
		"8656f95ea97ee2dbffda4158ab487263939c59c1",
		"f8c7be74651af361c4d893f8d89979629f22800a",
		"85ac34706e41f5fc5571bdf38deb92b4286e112f",
		"be18eb07930c52bb7f5a1212a4f7873f165b6ab1",
		"9d2cbe185aa23de2ecf18d6ab54ff651eb345090",
		"400a583ab863f2b2406ec7323bce1cfa651523ce",
		"3806db229be751ad99403caa1bf763001c69295e",
		"c3596fb63b6e6ad5b599b424606de9a3cf13ce13",
		"e42012ec934a59467b4411f23cd4a10d20518a67",
		"24d1a1297c3872c2ddef50bc91179670fd5e3e25",
		"c6985518745e0ca2e3a0662487f4cee62750705b",
		"b1b61842064c2c3bbb2cf17752c63312bf4b222b",
		"14e95c92a40b3ad22b2a1919b4613e1aafc32bae",
		"cd3a5719971bafaa8ada96c4ab354237efae3af6",
		"0d61cae0ff5831f186b3bdf202c31b18e4451efa",
		"e33dcc4c62abc81fc8173f6159b9722cf739e770",
		"96521a996dcfbbe4751f9e76309e76f68178ea64",
		"92d8a808f35ab659047601255bfb9fc63381ac4d",
		"8166582af4abf8004073a91c384f207c110025ae",
		"7da994ea3cc13bc2b4fbf97f564c4d33a3a3e9dc",
	};

	struct FCaseActor
	{
		int32 X = 0;
		int32 Y = 0;
		int32 Z = 0;
		int32 Yaw = 0;
		int32 Scale = 0;
	};

	uint32 Below(FXorShift32& Random, uint32 Count)
	{
		return Random.Next() % Count;
	}

	int32 Spread(FXorShift32& Random, int32 Range)
	{
		return static_cast<int32>(Below(Random, static_cast<uint32>(2 * Range))) - Range;
	}

	/** The field's drawing's writes into a gear actor's model: the shadow on, shown, the yaw, the scale, the floor and the root's place. */
	void PlaceActor(FGearModel& Model, FCaseActor& Actor, int16 LoadScale, bool bYawFromModel)
	{
		Model.Flags = static_cast<uint16>(Model.Flags & 0xFFFE);
		Model.bShown = 1;
		FGearBone& Root = Model.Bones[0];
		if (bYawFromModel)
		{
			Actor.Yaw = static_cast<int16>(Root.Angles[1] - 0xC00);
		}
		else
		{
			Root.Angles[1] = static_cast<int16>(Actor.Yaw + 0xC00);
		}
		Model.Scale = static_cast<int16>((Actor.Scale * LoadScale) >> 12);
		Model.FloorY = static_cast<int16>(Actor.Y);
		Root.Position[0] = Actor.X;
		Root.Position[2] = Actor.Z;
	}

	void DigestTick(FOracleDigest& Digest, FGearModels& Gears, const FPsxRandom& Random, const std::vector<int32>& Slots, FXorShift32& Draws)
	{
		Digest.I32(Gears.VblankCounter, Gears.PlayMask, Gears.PlaySlot, Gears.Wind, Gears.WindPhase, Gears.bSnapPoses ? 1 : 0, Random.Seed());
		for (const int32 Slot : Slots)
		{
			const FGearModel& Model = *Gears.Model(Slot);
			Testing::DigestGearModel(Digest, Model);
			for (std::size_t Bone = 0; Bone < Model.Bones[0].Index; ++Bone)
			{
				Testing::DigestGearBone(Digest, Model.Bones[Bone]);
			}
			const std::optional<FGteMatrix> Matrix = Gears.BoneMatrix(Slot, static_cast<int32>(Below(Draws, Model.Bones[0].Index)));
			REQUIRE(Matrix);
			for (const auto& Row : Matrix->M)
			{
				Digest.I16(Row[0], Row[1], Row[2]);
			}
			Digest.I32(Matrix->T[0], Matrix->T[1], Matrix->T[2]);
			Testing::DigestGearCloths(Digest, Model);
		}
		Testing::DigestGearPool(Digest, Gears.Pool());
	}

	/** One case of the oracle's, drawn from the same generator. */
	std::string RunCase(const FGearDisc& Disc, FXorShift32& Draws, int32 Number)
	{
		FPsxRandom Random(Draws.Next());
		FGearModels Gears;
		Gears.Random = &Random;
		Gears.Arctangents = &Disc.Arctangents;
		REQUIRE_FALSE(Gears.Init(PoolSize));
		// The oracle leaves the packets unbuilt: any lighting builds them.
		Gears.BuildLighting = FGte{};
		std::vector<int32> Models = {Number % FGearFileNumbers::ModelCount};
		if (Below(Draws, 4) == 0)
		{
			Models.push_back(static_cast<int32>(Below(Draws, FGearFileNumbers::ModelCount)));
		}
		std::vector<FCaseActor> Actors;
		std::vector<int32> Slots;
		std::vector<int32> Scripts;
		std::vector<int16> LoadScales;
		for (std::size_t Slot = 0; Slot < Models.size(); ++Slot)
		{
			FGearFiles Files = Disc.Files[static_cast<std::size_t>(Models[Slot])];
			Scripts.push_back(static_cast<int32>(Files.Motion.ScriptEntries) - 1);
			LoadScales.push_back(Files.Model.Header.Scale);
			const int32 SlotNumber = static_cast<int32>(Slot);
			const FGearTexturePlace Place{static_cast<int16>(0x240 - SlotNumber * 0x40), 0x100, 0, static_cast<int16>(0xFC + SlotNumber)};
			REQUIRE_FALSE(Gears.Load(SlotNumber, std::move(Files.Model), std::move(Files.Motion), Place, {0, 0, 0}));
			FCaseActor Actor;
			Actor.X = Spread(Draws, 0x800);
			Actor.Y = Spread(Draws, 0x100);
			Actor.Z = Spread(Draws, 0x800);
			Actor.Yaw = static_cast<int32>(Below(Draws, 0x1000));
			Actor.Scale = Below(Draws, 4) != 0 ? 0x1000 : 0x800 + static_cast<int32>(Below(Draws, 0x1000));
			Actors.push_back(Actor);
			Slots.push_back(SlotNumber);
		}
		const int32 SnapTicks = Below(Draws, 3) == 0 ? static_cast<int32>(Below(Draws, 8)) : 0;
		const uint32 Extra = Below(Draws, 6) == 0 ? ShopExtraVblanks : FieldExtraVblanks;
		const bool bYawFromModel = Below(Draws, 5) == 0;
		// The camera's view: drawing only, but drawn from the generator.
		Below(Draws, 4);
		Spread(Draws, 0x200);
		Spread(Draws, 0x200);
		Below(Draws, 0x2000);
		if (SnapTicks != 0)
		{
			Gears.SetSnapPoses(true);
		}
		FOracleDigest Digest;
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			if (Tick == SnapTicks && SnapTicks != 0)
			{
				Gears.SetSnapPoses(false);
			}
			for (const int32 Slot : Slots)
			{
				if (Tick == 0 || Below(Draws, 12) == 0)
				{
					REQUIRE_FALSE(Gears.PlayAnimation(Slot, 0, static_cast<int32>(Below(Draws, static_cast<uint32>(Scripts[static_cast<std::size_t>(Slot)])))));
				}
			}
			for (const int32 Slot : Slots)
			{
				PlaceActor(*Gears.Model(Slot), Actors[static_cast<std::size_t>(Slot)], LoadScales[static_cast<std::size_t>(Slot)], bYawFromModel);
			}
			if (const auto Error = Gears.Tick(Extra))
			{
				FAIL("case " << Number << " tick " << Tick << ": " << Error->Message);
			}
			for (const int32 Slot : Slots)
			{
				const FGearModel& Model = *Gears.Model(Slot);
				FCaseActor& Actor = Actors[static_cast<std::size_t>(Slot)];
				Actor.X -= Model.RootMotion[0];
				Actor.Z -= Model.RootMotion[2];
			}
			DigestTick(Digest, Gears, Random, Slots, Draws);
		}
		return Digest.Hex();
	}

	void CheckCases(int32 First, int32 End)
	{
		REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
		const std::optional<FGearDisc> Gears = Testing::ReadGearDisc(*Disc);
		REQUIRE(Gears);
		FXorShift32 Draws(Seed);
		for (int32 Number = 0; Number < End; ++Number)
		{
			const std::string Digest = RunCase(*Gears, Draws, Number);
			if (Number >= First)
			{
				INFO("case " << Number);
				CHECK(Digest == Expected[Number]);
			}
		}
	}
}

TEST_CASE("The gears' models run as the game's module runs them, cases 0 to 23", "[Gears][Content][Oracle]")
{
	CheckCases(0, 24);
}

TEST_CASE("The gears' models run as the game's module runs them, cases 24 to 47", "[Gears][Content][Oracle]")
{
	CheckCases(24, 48);
}

TEST_CASE("The gears' models run as the game's module runs them, cases 48 to 71", "[Gears][Content][Oracle]")
{
	CheckCases(48, 72);
}
