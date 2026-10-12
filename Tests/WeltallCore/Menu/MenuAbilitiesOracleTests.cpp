#include "Support/GameDiscs.h"
#include "Support/MenuOracleRun.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using namespace Weltall::Testing;
using namespace Weltall::Testing::MenuPad;

// The Abilities screen against the original's main menu module run whole under the oracle harness,
// frame by frame.

TEST_CASE("The Abilities screen heals with an Ether until its EP runs out, as the original does", "[Content][Menu]")
{
	// Fei's InnerHealing on himself and on Citan until his EP runs out (the window closes); entries not learned or not
	// usable here; Citan's list by R1, his Sazanami on a party already whole (the buzzer).
	FMenuScenario Scenario{"abilities_use", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 60}, {Right, 2}, {0, 6}, {Cross, 2}, {0, 20}, {Cross, 2},
			{0, 10}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 10}, {Down, 2},
			{0, 6}, {Cross, 2}, {0, 10}, {Left, 2}, {0, 6}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 10}, {R1, 2}, {0, 12}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 10},
			{Up, 2}, {0, 6}, {Cross, 2}, {0, 10}, {Circle, 2}, {0, 12}, {L1, 2}, {0, 12}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		700};
	Scenario.Pokes = {
		{0x16c2, {0x0, 0xe0}},
		{0x1702, {0x0, 0x80}},
		{0x2b8, {0x14, 0x0}},
		{0x400, {0x3c, 0x0}},
	};
	CheckMenuScenario(Scenario, "ea57754d",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 4a095940 88e5721c a139e9b7 4de879b9 "
		"5bc397b8 d94bb2de ea21d046 19ddf1a2 7e91eb28 fb08e8d3 a0b6e612 b4a32e4a 034758a0 8296e322 9bc14a43 b7afa9f7 06b07ac9 affcc59a "
		"d39cb40c 1f3782d0 13c0cae1 7fb52dd5 9dee4d4b 101949df*7 63cc4759 6792b6d3*5 70adeb39 a28d347f*5 beea600f 5ff98526 a7744ec3*4 "
		"252916d2 09eac020 0c38a815 a185f2e9 41feb4b9 d873d6d3 3bf2bd07 8c04011c 252f7bfe 298a8e38 756860db 671c8246 eddbe5b5 4b040e90 "
		"0df91e84 af571675 c6e4993f 43a9047f 8755fbbc f538a1ba bac23b15 3a0e7548 ea654de5 cb3717b3 857c0f38 c02fa735 ca07108a 0d81fe61 "
		"4ddcfe58 67d5e0ab 6fa7182f 63032b42 8ad8d166 d9e75cdc a7f1a483 1774df82 1f3fe4d5*6 60ec1c25*6 a1d033ca*6 175bb9f4*6 1774df82*2 "
		"1bb559f8 1774df82 4496a6d2 8ab4c102 1f58d757*4 251e1a5c b19a26d9 62a94793 931ac25b*19 dfafa139 b5920802 e9fc2d40 041c5c07*9 "
		"3bbd57e5 041c5c07 34c5478d*6 200422c3 87d473a9 a6840415 8ea1e1c4*9 62f7126b a3ec4bf0 704ea82e 62e8c4d3*9 96478c11 d3c4f8a2 916824c1 "
		"f703e9f1*9 8749317f b13d72fe 0d190622 a3125b8f f345b4bd 0fd63dcb*5 6993f845*2 2cfa543f 6993f845*3 456ca04f*6 9e784bdf*2 5b6145c7 "
		"9e784bdf 53b99f42*2 3fa72c32*4 e94423da 3fa72c32 edfc92bf*6 15b155c6*4 8bb3b41a 15b155c6 e9730215 0d8ae23b*5 e441bc83 cf77612a "
		"1d704666 8150fd2f*3 efc68ac7*2 dbe696e9 efc68ac7*3 0539de53*6 2a0975f4*2 8c37033f 2a0975f4 3b236936 7e82509e d8fb6476*6 fc930919*4 "
		"ae549f66 7a6c7f1d 5c710f6a a4b621e3*19 5c4ecdb5 f6a8b0ab 5c710f6a a4b621e3*9 4d4c9549 a4b621e3 74e70069*6 a14278e3 f59ebe95 "
		"49a6312f 74e70069*9 000e1316 bf443023 fd40de93 943b54d5 8425f2bd*5 6c2c6c76*5 87acc579 7e82509e e3675277 2a0975f4*4 92a7f449*6 "
		"8150fd2f 332e0179 f3d1bf6a e4d2357c 9e01ff87 1c27ea84 c4f6e94a 695ed259 e7d7fc63 a6f8b790 e9700410 d87f76b9 2a3274e7 3b4beb1b "
		"e85480d4 edec00df c1339a8f 334e89d4 adbaaf7f 67a61a9e 2becce41 39838719 e91c15e6 63806d6c 342c1b77 31ce9fed*16 07c0d8c5*22 a281688b "
		"c4fe7b0a d883917b ee158fae c512c01e 99119414 d2ac9c2a 6ef98252 639d77ab 723278de b498a1ab e325150c 897db9e4 9bf3edf5 852b4d29 "
		"f8dc7fc8 3aa4150f e47e2180 b1bdfef7 e57ed649 835263f9 a02aa3b4 19d14ecc 4561532e*2 24ddaa35*3");
}

TEST_CASE("The Abilities screen opened from the Status screen returns to the sheet, as the original does", "[Content][Menu]")
{
	// From the Status screen's inner arc: no flight of its own; an Ether used; back to the sheet.
	FMenuScenario Scenario{"status_abilities", EMenuMode::Normal, 0,
		{{0, 45}, {Cross, 2}, {0, 120}, {Down, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Right, 2}, {0, 6}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 10}, {Circle, 2},
			{0, 10}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		420};
	Scenario.Pokes = {
		{0x16c2, {0x0, 0xc0}},
		{0x2b8, {0x14, 0x0}},
	};
	CheckMenuScenario(Scenario, "29767ad7",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 c9902298 fd292891 a3822177 94630c2b "
		"3f493649 0b61d44f 6f68f26b a94e367c b7b4ea81 9b2cbb20 82d6aee9 25edfbdd 9a91258b 91e99c27 3c04a611 6fb3e3c0 3da3b10b 36aa1e83 "
		"971d545c 43dd3cd4 46c4079c 5b4d9648 62ec7d70 94f0023e*7 7f4ff125 9c68f561 de1290b0 a3d10a13 4997e2de b192aca3 5952389f 3635b61f "
		"dd8d4c32 6faacf0b d4547d4e 4163cac1 a3309f88 f119dc01 0b36a1ff 12bd466a 762b1f6c 14eaa496 c064ceee 491f7d3b 2df1bce4 3e8b866d "
		"7fa34873 e5e6c6cf a505ff1a a4b2e5e5 9f0646ca 65c23362 52d1ca64 a7b6e2fe d55d0639 3200c62d c469c300 ffbfa1eb ed6222fd d263604f "
		"db34bed2 c03f7b2c 707609dc*36 6feb81ad*48 f61fc4ab 6feb81ad f7412d77*8 8e606116 abaa8ec7 2834f5bd bcf5b98d*4 ee9872bd*6 77d10b7b*6 "
		"3ed2c771*3 e60e6b4c 3ed2c771 28e153da a4ab14ea*5 da50422f 3eb5fd8b b6119fb0 5efb1f1b*19 c6b772b1 b07764b7 6cb22afe e9329f73*9 "
		"46bbebea 1e503d04 af251cd4 bda11d7e 287e98b6*4 8f11ffe8*4 49617f30 1adb4d4c a43a518f 0c024d6c*2 e800ec6d 0c024d6c*40 0806403c*16 "
		"2a4abc1e 062a5ce6 ee2b3e17 3f5dd2cf 72e4c5eb c758b448 b73f9dbf ede7ead9 299cf361 1c19ddc1 a7106ef0 37c6d90f 44201d7a 67abe022 "
		"0bcac461 41713e26 06978b7c 575538d4 e904df5f 92ffda76 5d031302 35610bd9 8d778914 e8f8d123*21 d0af8d71*18 6b601227 d11fb244 807e2de7 "
		"c67c4862 49c9cbf9 13054f80 0afa5ea6 49026ab5 c182981c 420caff5 55c79d1d 9f742807 0d5b5961 4570349b 17c1e358 d6c0f43d 5a678308 "
		"dd76f992 d8f9e556 4bd500b3 d4f1666d 1610437d 448791e2 75070a98*2 24ddaa35*3");
}

TEST_CASE("An Ether heals and costs as in battle with the fix", "[Content][Menu]")
{
	// Fei (Ether 7, a bonus of 3, Ether Doubler) heals Citan (60 of 200 HP) with InnerHealing (power 5, 2 EP).
	constexpr std::size_t Fei = 0x26C;
	constexpr std::size_t Citan = 0x26C + 2 * 0xA4;
	FMenuScenario Scenario{"ether_as_battle", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 60}, {Right, 2}, {0, 6}, {Cross, 2}, {0, 20}, {Down, 2},
			{0, 6}, {Cross, 2}, {0, 10}, {Circle, 2}, {0, 12}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		400};
	Scenario.Pokes = {{0x16C0 + 2, {0x00, 0xC0}}, {Fei + 0x2B, {3}}, {Fei + 0x8A, {0x00, 0x20}}, {Citan + 0x4C, {60, 0}}};
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Points = [](const FMenuRun& Run, std::size_t Offset) { return Run.Game.Block()[Offset] | Run.Game.Block()[Offset + 1] << 8; };

	// The original heals with the base Ether alone and charges the cost as stored.
	const FMenuRun Original = RunMenuScenario(*Disc, Scenario);
	CHECK(Points(Original, Citan + 0x4C) == 60 + 7 * 5);
	CHECK(Points(Original, Fei + 0x50) == 10 - 2);

	// Fixed, the Ether with its bonus, doubled, and the cost doubled.
	Scenario.Options = FMenuOptions();
	const FMenuRun Fixed = RunMenuScenario(*Disc, Scenario);
	CHECK(Points(Fixed, Citan + 0x4C) == 60 + (7 + 3) * 5 * 2);
	CHECK(Points(Fixed, Fei + 0x50) == 10 - 4);
}
