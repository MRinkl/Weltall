#include "Support/GameDiscs.h"
#include "Support/MenuOracleRun.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using namespace Weltall::Testing;
using namespace Weltall::Testing::MenuPad;

// The Equip screen against the original's main menu module run whole under the oracle harness,
// frame by frame.

TEST_CASE("The Equip screen tries accessories on, keeps one, puts one back and takes one off, as the original does", "[Content][Menu]")
{
	// Fei's slots, an accessory list scrolled and one chosen, a list left with Cancel; Citan's accessory taken off.
	FMenuScenario Scenario{"equip_accessories", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 60}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 12}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4},
			{Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Up, 2},
			{0, 4}, {Up, 2}, {0, 4}, {Cross, 2}, {0, 12}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 8}, {Down, 2}, {0, 4}, {Circle, 2}, {0, 8},
			{R1, 2}, {0, 20}, {Up, 2}, {0, 4}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 12}, {L1, 2}, {0, 20}, {Up, 2}, {0, 4}, {Up, 2}, {0, 4}, {Cross, 2},
			{0, 12}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		560};
	Scenario.Pokes = {
		{0x1e00, {0x1, 0x2, 0x3, 0x63, 0x5, 0x1, 0x1, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1}},
		{0x1ec8, {0x2, 0x3, 0x11, 0x12, 0x5a, 0x5b, 0x5d, 0x5e, 0x62, 0x63, 0x68, 0x74, 0x19}},
	};
	CheckMenuScenario(Scenario, "22b1176f",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 61063503 d27c2f4e 1f1805ef 4c89d363 "
		"8a1d2501 0267fb36 e25c6b51 1090cb74 5c70056f de367a9a d49f1f91 435d2c11 86f8ac12 e012d66d 9dbf5ac0 a33373d1 8f86fd20 a5fe68f1 "
		"0b92b3c6 93045b6e 465bf2ca a12b58e6 8b8d13b3 06fa6e02*7 ac3edd3b c0b733c4*5 d3ae6d97 adf15a5e f7b0750b 73129ad0 26050b0f 0bed1f05 "
		"1fc38884 15a6a7f0 55c9966f c32b1dc0 c51f3d0f fd6c431d 41d32409 9b3901fd c11e4fbf f1eb3e8e fbbfc637 c9516df9 bd886d52 4050e529 "
		"601c7640 4a821641 c0c5a0f2 edb35d3d 22ed2c9f 5993e73b e7bfd118 728885f5 98e90e28 ea434c14 2ec181e2 85501208 cb725511 9ce38759 "
		"55937d86 e30183ce 4ccedcb2*26 9611a748 4ccedcb2 4614ca92 86b39c65*5 9b058600 8645314d f264dd6e a22e0e5f*4 f0bc1400*6 7c2f9555 "
		"1c9acb3c 7c2f9555 03945264 bf860665*2 d87b2037 18c254dc d87b2037 e6dad3b4 aa3617aa*2 4fe3481b 51e49bd8 4fe3481b b49d7c4c af4cd89d*2 "
		"54413961 70342256 54413961 c65e55a0 78684db4*2 59232f5f a0d1b498 59232f5f 92a3bb10 6584ca8d*2 c2bc7010 41ca1dda c2bc7010 28690dc0 "
		"65dba3d9*2 77f7f1ad f90472d9 77f7f1ad 422b33c6 a00ccc06*2 fdaf93bd 410b1b85 fdaf93bd 0993f56f 0066a82a*2 12a0296b 13fd0f29 12a0296b "
		"e1e222f9 4230b929*2 3a7f9285 f8b3d2c7 3a7f9285 8e03e21a d43e0d80*2 3d2c8180 16c7e325 3d2c8180 f358fd3b 11f005d4*2 0e0db349 4f819d0d "
		"0e0db349 e8bb2c27 443a3897*2 7d2794aa 38aa4514 0bd1842e a78e4b42 9979cea9*11 19138abf 9979cea9 ee128215 666dd165*3 11d03277 "
		"666dd165 022d0385 1730d852*3 8744549b cf74d8bb 8a9f0934 01eb0462 1e56cee5*6 4aa41f04 2e28fb58 dda2ab29 4fb6b342*3 aa31d319 e41e9d0f "
		"132bdca3 1730d852*7 84a992e6 1730d852 43f4531f 7b84867b*19 ce45f6b9 7b84867b 25b34905 4f4072b4*3 67fc0fed 67474070 a1fc7479 "
		"05392182*2 8dda4bcf*5 9f04e2bf e7aade92 97eb6390 f5128132*11 07d637a4 f5128132 7fa6df96 71a4ea01*19 0da1857b 71a4ea01 9961b5ab "
		"2718343e*3 666a2c3d 2718343e dd6f6bb0*4 09ed249b 7620ecc9 c90d6e2e c8150524*3 e06180f6*6 d6b69127*2 59ad93bb 7c2b4b09 801cf168 "
		"7c2b4b09*59 56212342 c0de3760 3eac4cb5 a03b7188 5d3dcf87 c9c9b34b a25abbac 392654ac 99702e3c e87358f1 85761e48 ad2d778a bd9a7398 "
		"4772b279 fcfcc9ec cc5554db 0b30e207 5553c36e 95f3b4f6 353f755c 122797f6 c8293743 094cff79 33c7a09d 8dd3336c b37422e6*33 b500e713*4 "
		"4a9ef499 eb6fbcf2 4b789713 14179151 f0ba8fc5 b4fc19a9 e8c97943 9e7b7b35 565cee97 51324971 82f0fe0e 5203b2aa 94615104 d22e88dc "
		"4a7375cd 993b26bf 5414a531 39a7e08e 31045a41 11992eb8 b986c85a df25386a ff2928c3 1ebc1838*2 24ddaa35*3");
}

TEST_CASE("The Equip screen changes Billy's ammunition and his gun, turning his pages, as the original does", "[Content][Menu]")
{
	// Billy's second page: a load changed while its type's count is full, another while partly spent; then his gun,
	// which returns its load and opens the second page's list at once.
	FMenuScenario Scenario{"equip_billy", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 60}, {R1, 2}, {0, 20}, {Right, 2}, {0, 8}, {Cross, 2}, {0, 10}, {Down, 2}, {0, 6}, {Cross, 2},
			{0, 12}, {Down, 2}, {0, 6}, {Down, 2}, {0, 6}, {Down, 2}, {0, 6}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 10}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 12},
			{Left, 2}, {0, 8}, {Cross, 2}, {0, 10}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 14}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 12}, {Right, 2}, {0, 8},
			{L1, 2}, {0, 20}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		620};
	Scenario.Pokes = {
		{0x1d34, {0x0, 0x4, 0x2}},
		{0x1d30, {0xff, 0x7, 0xff, 0x7}},
		{0x22b9, {0x64}},
		{0x22b8, {0x1e}},
		{0x1d38, {0x1, 0x2, 0x2, 0x1, 0x3, 0x1, 0x1, 0x3, 0x1, 0x1, 0x2}},
		{0x1d9c, {0x20, 0x21, 0x32, 0x33, 0x35, 0x36, 0x38, 0x43, 0x44, 0x39, 0x3a}},
	};
	CheckMenuScenario(Scenario, "8e76e0dd",
		"b77d9678 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 00fb1a72 a051cb19 db682c6c 7cca24b7 "
		"435f4c0a 98b7c3e1 9c093b80 462c4517 01ea4b47 6c4e7b5a febfcaa6 c1bf6ca6 8afc0497 f47efe53 40a3400e 40788830 c1dee485 de2d2deb "
		"f15a2759 3c4f7c5b d59c8c6b 75f805f0 86fa92a7 814f7619*7 18a6d041 8b3f93a0*5 4da4fade b1777d96 def7c943 3b6f86e5 d7e40db5 6344fde4 "
		"adc9efa6 3ab2870b f2bac4b5 29b1e71b fd2d6c8d 8d93423c 4de68317 5698ba9c 1f14c16a 252d6512 5b6acce9 f211f7c7 0d1fde4b 790f601e "
		"8db92bc2 25501c6d e6e80e94 edb35d3d 22ed2c9f 5993e73b e7bfd118 728885f5 98e90e28 ea434c14 2ec181e2 85501208 cb725511 9ce38759 "
		"55937d86 e30183ce 4ccedcb2*26 9611a748 4ccedcb2 a42da77b d8a705af*19 82e02572 d8a705af 65775926 2c5a3833*7 49bc50b1 2c02f0b7 "
		"60710c58 f76f4a30*4 ac5a21d9*5 8086a878 d6b8270e 5a205a6a 8cc995af*4 b3f8ca57 2953f0c5 3f1c5453 0a7ed687 6971aade*11 6dac10fd "
		"6971aade 824e99a5 37a9cd83*5 3f18c305 37a9cd83 ad3ca58d 98db8540*5 f8729d71 98db8540 a5d71ff5 200529e1*5 58ad82c1 200529e1 ad3ca58d "
		"98db8540*5 9851b3bf 14a44db6 e22fcd66 b362631d e2464154*6 4e158ded*2 f00592fa 4e158ded cb55d153 96c43a17 5491f5f3*4 515040f1 "
		"b09f640b bf579fb9 b09f640b*11 cab1277c b09f640b 4c2af587 5bac23a5*7 fd320586 b960a927 70fda9ab 970a19d3*4 f9d260ae*5 805cf713 "
		"e76b1dbf b203aa18 a1ae584c*4 b083ee68 a7c11d72 fd89abbb 9239ed4b 97cc62db 6f9d1eff 6403e4c4*6 5cb62f2c*5 79d75794 230722c9 "
		"a332198f*5 02cf241e 9983a8ba 8a779f99 be42c34c 8a779f99*11 4da1aa09 8a779f99 fe0545e0 3a361082*7 0a0de1eb 3a361082 cbf3ba9c "
		"4ccedcb2*19 97b3d903 1cdf5924 bcfbaabb 432bd85a aa8302e2 6a665d79 e8c42ac1 0bce8eca 4590961b e179ce73 f982dab2 046eccb7 89888268 "
		"33b324a2 b6225073 735d0a2c 987328e7 2ecf52f3 c535b5ed 9ddc4151 be54b740 fcc90e03 c270a30f b335a4b6 4477af4b fd7ef515*37 23aa178f "
		"88631cfd e53bdfaa a9d4a880 4e9ab186 f8a519ab 80370381 9f9af2ba 695c123a 49560508 8c919f34 4928deba 599e7ac8 7161a703 587f1d40 "
		"857be611 3ca4d721 6b4bbf86 6883f654 2cdf91bf 95a9b6c0 1fd89942 150036e1 1ebc1838*2 24ddaa35*3");
}

TEST_CASE("The Equip screen opened from the Status screen changes an accessory and returns to the sheet, as the original does", "[Content][Menu]")
{
	// Equip from the Status screen has no flight of its own and the sheet is drawn again after it.
	FMenuScenario Scenario{"status_equip", EMenuMode::Normal, 0,
		{{0, 45}, {Cross, 2}, {0, 120}, {Up, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 10}, {Down, 2}, {0, 6}, {Down, 2}, {0, 6},
			{Cross, 2}, {0, 12}, {R1, 2}, {0, 20}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		520};
	Scenario.Pokes = {
		{0x1e00, {0x1, 0x2, 0x3, 0x63, 0x5, 0x1, 0x1, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1}},
		{0x1ec8, {0x2, 0x3, 0x11, 0x12, 0x5a, 0x5b, 0x5d, 0x5e, 0x62, 0x63, 0x68, 0x74, 0x19}},
	};
	CheckMenuScenario(Scenario, "36550e80",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 61063503 d27c2f4e 1f1805ef 4c89d363 "
		"8a1d2501 0267fb36 e25c6b51 1090cb74 5c70056f de367a9a d49f1f91 435d2c11 86f8ac12 e012d66d 9dbf5ac0 a33373d1 8f86fd20 a5fe68f1 "
		"0b92b3c6 93045b6e 465bf2ca a12b58e6 8b8d13b3 06fa6e02*7 45b8edeb 4ecc9e18 32b63bc8 62445cda 4dd424f1 bd0fb1ae e35a956b f73c2b98 "
		"335e9523 251b31cc 5f28f1b5 c0174d38 128033a9 d4cfd7ae 93d25ca2 bfa0a1b3 7b377030 6343122f 4da7234c f68117b6 80db5936 273d05c8 "
		"74ec4039 af8fdad9 33d95943 3c34da2b a6215c86 09f24d53 994dd30f 5147d2d8 2b4248cf 73efe041 676cd319 0d7cb64a 4e13d726 23d01a63 "
		"07ee841f c00b7899 de144c24*36 abbb4e23*48 8d564841 abbb4e23 836fc8c8*8 23b039e8 3abea5d2 982f21ae 9f05eac1*19 547f98f1 9f05eac1 "
		"55c40d25 d30721ad*5 6a83f6ab af0672fc 9dc72fc1 b237586d*4 dcab196c*5 25a0c4aa 6d699343 8e75eb06 cde25353*4 4c5f0108 729d3be9 "
		"4c5f0108 2a5f105a 2fd69335*2 30f3bead*3 c9842a86 37ea9b98 49f9cd42 7c9ef483*11 97d79c76 7c9ef483 e84ad821 f5983a11*19 f6068993 "
		"1a1cafb8 8af22654 eb8b0e03 59b40e25 421f7d59 59b40e25*56 78397b2d cbcf3b9d f27b8579 7b2f2ec3 d9a1c4b5 bac7323b 86c31f42 db149696 "
		"b565a6f3 ad5a4af5 0056f3e5 12960f3e 58979e94 7fa14405 af8d39c3 94ab8d6a 5ce9908e 0be82ff7 43c94221 f9eba1b7 844a1cd8 833fcf29 "
		"f33fdfc6 187c0683*3 d5bf968f*36 6ab0fae2 2c80c407 960744d2 ce3951e9 9f63c14d 8ff0442e c0f4e97f 319743cd dfd27494 a0efde4c 00d5c3fb "
		"c0eb6777 ade541cc b3c64c4d 5103650d dd2168da e0543032 932d9065 a680ec5f 632fc098 5770e7f1 d7a2af89 448791e2 75070a98*2 24ddaa35*3");
}

TEST_CASE("Billy's ammunition keeps a partly spent load and is not refilled with the fix", "[Content][Menu]")
{
	// Billy's hand gun holds a full load of 50; 51's count is partly spent (30). He loads 51.
	constexpr std::size_t Counts = 0x2286;
	constexpr std::size_t WeaponCounts = 0x1D38;
	constexpr std::size_t WeaponItems = 0x1D9C;
	constexpr std::size_t HandGunLoad = 0x26C + 4 * 0xA4 + 0x6F;
	FMenuScenario Scenario{"ammunition_kept", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 60}, {R1, 2}, {0, 20}, {Right, 2}, {0, 8}, {Cross, 2}, {0, 10}, {Down, 2}, {0, 6}, {Cross, 2},
			{0, 12}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		400};
	Scenario.Pokes = {{0x1D34, {0, 4, 2}}, {0x1D30, {0xFF, 0x07, 0xFF, 0x07}}, {Counts + 50, {100, 30}}, {WeaponCounts, {2, 1}}, {WeaponItems, {50, 51}}};
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);

	// The original loses the full load of 50 and fills 51 again.
	const FMenuRun Original = RunMenuScenario(*Disc, Scenario);
	CHECK(Original.Game.Block()[HandGunLoad] == 51);
	CHECK(Original.Game.Block()[WeaponItems] == 50);
	CHECK(Original.Game.Block()[WeaponCounts] == 2);
	CHECK(Original.Game.Block()[WeaponItems + 1] == 0);
	CHECK(Original.Game.Block()[Counts + 51] == 100);

	// Fixed, 50 goes back to the inventory with its shots, and 51 goes on from 30.
	Scenario.Options = FMenuOptions();
	const FMenuRun Fixed = RunMenuScenario(*Disc, Scenario);
	CHECK(Fixed.Game.Block()[HandGunLoad] == 51);
	CHECK(Fixed.Game.Block()[WeaponItems] == 50);
	CHECK(Fixed.Game.Block()[WeaponCounts] == 3);
	CHECK(Fixed.Game.Block()[WeaponItems + 1] == 0);
	CHECK(Fixed.Game.Block()[Counts + 50] == 100);
	CHECK(Fixed.Game.Block()[Counts + 51] == 30);
}
