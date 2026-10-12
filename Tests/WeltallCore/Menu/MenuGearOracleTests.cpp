#include "Support/GameDiscs.h"
#include "Support/MenuOracleRun.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using namespace Weltall::Testing;
using namespace Weltall::Testing::MenuPad;

// The Gear screen against the original's main menu module run whole under the oracle harness,
// frame by frame.

TEST_CASE("The Gear screen equips, repairs and boards a gear, as the original does", "[Content][Menu]")
{
	// Weltall's sheet, a part changed in the gear's Equip, the gear's abilities (none usable here), its own Fix
	// Frame HP (from a Repair Circuit) used twice, boarding and leaving it, then Citan's Heimdal by R1.
	FMenuScenario Scenario{"gear_screen", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 80}, {Cross, 2}, {0, 20}, {Down, 2}, {0, 6},
			{Cross, 2}, {0, 10}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 12}, {Circle, 2}, {0, 30}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 30}, {Circle, 2}, {0, 30},
			{Up, 2}, {0, 6}, {Cross, 2}, {0, 30}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 10}, {Circle, 2}, {0, 10}, {Circle, 2}, {0, 30},
			{Up, 2}, {0, 6}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 20}, {R1, 2}, {0, 30}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		900};
	Scenario.bGearLocked = false;
	Scenario.Pokes = {
		{0x2184, {0x1, 0x2, 0x1, 0x3, 0x1, 0x1}},
		{0x221a, {0x2, 0x5, 0x2b, 0x2d, 0x31, 0xa}},
		{0x16c6, {0x0, 0xc0}},
		{0x983, {0x5a}},
		{0x9d8, {0xe8, 0x3, 0x0, 0x0}},
	};
	CheckMenuScenario(Scenario, "b7f45a2c",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 61063503 d27c2f4e 1f1805ef 4c89d363 "
		"8a1d2501 0267fb36 e25c6b51 1090cb74 5c70056f de367a9a d49f1f91 435d2c11 86f8ac12 e012d66d 9dbf5ac0 a33373d1 8f86fd20 a5fe68f1 "
		"0b92b3c6 93045b6e 465bf2ca a12b58e6 8b8d13b3 06fa6e02*7 ac3edd3b c0b733c4*5 fd3c0f14 47cee290*5 9a63c179 bfd20251 80cb3a39*4 "
		"d1256ddb f09fa615*5 ed840002 165a0ec1 c4fcf861 f5faa8c0 dd126222 648c9292 84cd4a02 ca0df0f6 7bc939cd 02aa681f a5d843d8 246fe921 "
		"c81b5a44 2ad1c1dd 8b9b321b b36074a9 df85df1e e73864f9 e567a12c 2fbcc16d 3308c865 9a73ba86 4c3e0cfe a547afdd 4ffa2ae6 fe952b8f "
		"b38d3909 d4db4b2a f042843e 617acda3 418a9f60 1761df24 e1b43cb7 f8ef9858 114afbbc 26e2b905 8b5df6c1 ba49b379 5e166808*2 0f666555*2 "
		"7026c3db*8 4140dd67*32 6e80fb00 267e5ac0 a8ff926b f5f8cdbb*19 c6d1bb00 f5f8cdbb df03d913 227a78ac*5 1ca170f0 ab3d0770 d5c7991a "
		"11fcff00*4 c1ea0e9c*5 17005a55 02d17363 23b40d08 bfa77d2f*4 e163edfe 936ffd7e 29306c43 87bea6f4 02c53aa9*11 4943560d 7c69d685 "
		"8b6c2a92 f6fa3d92*2 18633635 f6645dd3*26 5f872966 f6645dd3 67811550*6 193805c3 74ccfbac 6bbc8ee0 a6a625da*4 6170fff8*6 cbabfc1d*6 "
		"f781651f*6 3bb7a472*6 a6a625da 4c119e70 e5d08601 fda044e6 15bde1b5*2 c55a78e9 a38053fe*6 9cf9ece9*20 e82d7540 9cf9ece9 debfe273*6 "
		"4927b147 77929243 8b4c5ca0 5dec6f00*4 d832b331*6 6a62e43d*6 bd69a2c3*6 c71494c7*6 5dec6f00 79a51c46 e3213fb8 0c9523ba 9878da7d*19 "
		"2a7fae2f 08efd6b5 9838bdd6 a3f25ae2*9 9fde5039 3679da91 2ef83303 0b18be50*9 4c48e946 99dd84aa 21533e82 ef56232d 5dec6f00*2 "
		"d832b331*6 06060e1a 3f9f0353 b23215ba f336aa07*2 34618719 939ee11e*26 4b504419 939ee11e cd34ce3e*6 6b00057c af5b8d9a 972b34df*2 "
		"64d70cc2 4917e2b1*17 613762b2 07a88bc7 26f1df98*2 bcd63d2d 6d6dffd0*17 db350c55 6d6dffd0 6bfd21aa db04741b 93b35087 88f70033*27 "
		"a81a4bf0 b49c59bf fb077985 05338d95 7809bc5f 037ad725 fd5f0fe5 f7c23137 d752a3ce 94a576f3 053ddcd9 a94dafb8 29c9f2af 18050bbd "
		"2d83ae80 53bef5a6 d44c5aa8 6d354217 1316446b b7d157ae e033826d e134297a 0ad4cfd6 03ec19b4*39 63d0f61c 7304a9ab 9e7ee57c 74799a93 "
		"443901e6 7cee5c30 a92d1530 97cb8adf 9d876b20 f9f1c2d9 43a16c96 17ccf310 f940a528 22dd7ce6 19eae243 344921d0 e9193e1b 7bfe989b "
		"11221c60 b72171cf 8e31deea 60755d2f 489b9e83 41dd0f7e*2 24ddaa35*3");
}

TEST_CASE("The Gear screen passes over members without a gear and refuses Chu-Chu's commands, as the original does", "[Content][Menu]")
{
	// Elly without a gear passed over, Chu-Chu's gear made from her own figures and every command of hers refused,
	// boarding locked, Emeralda's Crescens piloted by her grown self, R1 wrapping past Elly and L1 back.
	FMenuScenario Scenario{"gear_members", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 80}, {Cross, 2}, {0, 20}, {Up, 2}, {0, 6},
			{Cross, 2}, {0, 20}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 20}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 20}, {R1, 2}, {0, 60}, {Cross, 2}, {0, 20}, {R1, 2},
			{0, 60}, {L1, 2}, {0, 60}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		700};
	Scenario.Pokes = {
		{0x1d34, {0x1, 0x7, 0x6}},
		{0x1d30, {0xc2, 0x0, 0xff, 0xff}},
		{0x3b0, {0xff}},
		{0x22b6, {0x0, 0x90}},
	};
	CheckMenuScenario(Scenario, "b3a08652",
		"974e84a1 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 18ae916b 578e235d 429a50d5 d23faf73 "
		"91f3b741 b2ffa9d7 98df0786 312edeac 0b3f2280 d79df163 677d0231 9adb96f0 2f67da67 6f6e6d5c 57cf55d8 236811bc 0d4e57c4 f4297080 "
		"888f268d a2eb3fc9 98d5135c 18934963 f601e8a1 df0ed118*7 e02c9a0d 5b62f6f5*5 b9ddb40e 3950eae0*5 23108519 89e0774b 4bc8f89a*4 "
		"eaa560cc 90577d4b*5 022b51fc 7f2e55a2 1db38079 dd9487c8 41240fe6 e89c6128 e030a0f7 2ee094b2 bea6a1ae d6153852 ad6fd0e7 ee3cb095 "
		"c1f8ec17 918e3301 2f790508 c3133398 06df79d1 d43553b8 1943f52c 107dadbe 5a667e74 7701e065 bdc3944a 5ac1419b eeb6ac8e d5bfba8b "
		"ca59def2 f2a40029 4175c5d3 b90ddd2e 14f39700 082601a9 3e144579 1595b6a3 8cecb06c c883ef2c acd13aef 537096bc aeb0d949*2 fdc604b3*2 "
		"1ba0de5a*8 4bbf7a7b*32 aaad86fa 54a4b2c0 c7670b8e*2 196d0480 4bbf7a7b*17 382004a4 4bbf7a7b 3276cc2a*4 87a5d3b4*2 f446f912 fa49b332 "
		"9a3b1b84*2 e20414ab 87a5d3b4*17 7ae55c29 87a5d3b4 a2b52b71*6 4d1b4494 6f51e399 ef64a469*2 20b07baa a2b52b71*17 3c1e196c a2b52b71 "
		"059ebaff*4 83e888a1*2 bdc849d0 c8910982 78159d43*2 21f572b2 83e888a1*17 8a1ca563 83e888a1 70192cae 1e0a14eb 95eed03c f74df24e*31 "
		"633200ec*26 e6d496a0 91282538 ab264282*2 14b3e546 633200ec*17 0d5b524b 633200ec a621ca0f f13e9d68 cda00380 b577d83e*7 b4d51219*50 "
		"6531a2b0 b4d51219 f5a3ead3 f3508fd1 bc065e39 20191ae1*5 56b3b504*52 25c27cef ad4644c7 85cca089 9dcd0bcb 4769cb40 34691180 fa7e0bd5 "
		"d1319048 32a1947e e96afb97 3da24655 8473eabb ee738923 142098a5 1f2ccda6 91038cd7 74be0b83 ee917097 dfb12600 8e52d0b7 76458632 "
		"afe46167 861b309d 9c78b9d8*39 f317a60e d9cc4d55 684f4172 e72eb2cc 5877be04 128a0a46 8f362f9f 2073403a f6cdce0e 48c8bf19 fb58c71f "
		"6af6273e a6ea7411 65b21b6a 577bfa14 b4582525 440b4a5e 63cbb28e 5ed53bbd 1b873fbd 166ebd07 7b81ebd1 8b43de61 41dd0f7e*2 24ddaa35*3");
}

TEST_CASE("Billy's gear changes its guns and their ammunition on two pages, as the original does", "[Content][Menu]")
{
	// Renmazuo's guns in the sheet's attack; a gun changed, which turns to the second page with its list open, the
	// ammunition changed there; the last slot's list left with Cancel; the pages by Left and Right.
	FMenuScenario Scenario{"gear_billy", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 80}, {Cross, 2}, {0, 20}, {Cross, 2},
			{0, 10}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 14}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 14}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 10}, {Circle, 2},
			{0, 10}, {Left, 2}, {0, 8}, {Down, 2}, {0, 6}, {Right, 2}, {0, 8}, {Circle, 2}, {0, 30}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		700};
	Scenario.Pokes = {
		{0x1d34, {0x4, 0x0, 0x2}},
		{0x1d30, {0x15, 0x0, 0xff, 0xff}},
		{0x20bc, {0x1, 0x1, 0x3, 0x2, 0x1, 0x1}},
		{0x2120, {0x20, 0x21, 0x33, 0x36, 0x39, 0x3d}},
	};
	CheckMenuScenario(Scenario, "596373d1",
		"98ef8d81 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 5f8b79b9 a384293f f6311119 9ccc7cbd "
		"6e90a01d 6581d2af 48f65027 d04001c9 efaa9b13 6eb46c5d 7f47d72c 057727f6 e1854b42 33c8da9d a3b0fe72 0fc45ab5 9b6505af ca4e266f "
		"93f2a89d 2e64ccdf 40510fe2 82f4c3e4 a537937e 70ec48d8*7 8b48caff 12727fdd*5 2f5605da d1f77c6a*5 fe90ec9c dacddad1 dea8ecab*4 "
		"79bf5acf 6be3f172*5 c5f6f00a e19f64a2 7ee248b0 17d38f3b f0e2569c 7596f8cc 7ad73262 426e6b3e c6489aff 6e89fc2e 21a8bf9b b3d0e2bd "
		"6d47cc5d b9e6a5ea b60f3992 9e6d087c a8f50951 9497262a b55cc997 93cc3a38 7308a64d d4992397 c5fffc26 92f40523 56c914e0 ecd6cb23 "
		"6d07a619 e9a4ed0a cd794b24 18947501 236dc287 938eb062 445b92f9 1d9913e0 2dbd40f3 2045e170 b5771159 954eecff 488f6bcb*2 5c65ebb3*2 "
		"39e677db*8 c77a3325*32 c4a924f9 267e5ac0 745852b7 a0d6af54*19 03552914 08d950c5 fd40e4f7 af56c00a*4 35493941*5 a2a934ef 154ce5c4 "
		"5e3cddad 3d09752a*4 b1eae6e3 c9141aa4 0703cdc9 164de20b*2 74f52a37 ef6f8ad6*6 0162916f*5 9ec58f50 92bc6d8c 11ea230e*5 0e6f018c "
		"77a5b6f7 9e01e664 2d0e8ab6 9e01e664*13 f1e37a09 9e01e664 6cefc07f adced981*5 55c307e8 8559052b f9a1e249 736a4612 b8875342*6 "
		"53038671*2 79ad13ab 1b8c0d64 df72a129 1b8c0d64*9 f21b4709 1b8c0d64 0e69e590 fa8fe2d2*7 9c9df16e fa8fe2d2 132134ca 383dac88*5 "
		"d673d1cd 383dac88 5dc850e0 49b1cb87*7 a660995c 5fd4c681 d675e190 2357bf27*2 0530ef22 93f30faf*26 164cc8a8 f6ad6d30 3f0ec185 "
		"85534072 a92907d7 3244d675 653effbc a645236c 9792233c 56ee14c8 a2a31c8a f8cd376a b7c2aecd ee263e43 e7c4731e 692d2a49 e555af05 "
		"d88ffc65 24e63e2e 4c10e7ab 3e50dde0 5df86b8b 86e174c9 92ca2e2c*11 7ed4a655*28 7e0f3a33 de97f078 b8fec99c ea5ebd82 1c36ff06 ff407f28 "
		"36664910 2a55c84c 7f744a7b 754e38c6 48f28c25 d91f5c4f dce49a6d 7c3b12a4 2f054210 f0e0b991 4068ff2e 48c513e5 6255cfe7 5b61dd28 "
		"b7c592f6 84be9efd 02d07bce 41dd0f7e*2 24ddaa35*3");
}

TEST_CASE("A gear's repair measures its own gear with the fix", "[Content][Menu]")
{
	// Citan's Heimdal (whole: 2500 of 2500 HP, a Repair Circuit) uses Fix Frame HP from the Gear screen; Fei's Weltall,
	// in the first slot, has a maximum of 1800.
	constexpr std::size_t Heimdal = 0x978 + 3 * 0xA4;
	FMenuScenario Scenario{"gear_repair_whole", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 80}, {R1, 2}, {0, 60}, {Up, 2}, {0, 6},
			{Up, 2}, {0, 6}, {Cross, 2}, {0, 30}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 20}, {Circle, 2}, {0, 10}, {Circle, 2}, {0, 30}, {Circle, 2},
			{0, 60}, {Circle, 2}, {0, 40}},
		600};
	Scenario.Pokes = {{Heimdal + 0x09 + 2, {90}}};
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Fuel = [](const FMenuRun& Run) { return Run.Game.Block()[Heimdal + 0x38] | Run.Game.Block()[Heimdal + 0x39] << 8; };

	// The original compares the gear's HP with the first slot's gear's maximum: it repairs a whole gear and spends the
	// fuel (a ninth of a fifth of 2500, in tens).
	const FMenuRun Original = RunMenuScenario(*Disc, Scenario);
	CHECK(Fuel(Original) == 1200 - 50);

	// Fixed, the gear is whole: the buzzer, no fuel spent.
	Scenario.Options = FMenuOptions();
	const FMenuRun Fixed = RunMenuScenario(*Disc, Scenario);
	CHECK(Fuel(Fixed) == 1200);
}
