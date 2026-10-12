#include "Support/GameDiscs.h"
#include "Support/MenuOracleRun.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using namespace Weltall::Testing;
using namespace Weltall::Testing::MenuPad;

// The Items screen against the original's main menu module run whole under the oracle harness,
// frame by frame.

TEST_CASE("The Items screen uses, swaps and scrolls items as the original does", "[Content][Menu]")
{
	// An Aquasol used on Fei, then on Citan until he is full (the buzzer); a swap; the pages scrolled by R1 and L1 and
	// past their ends; items that cannot be used here; a mark taken off.
	FMenuScenario Scenario{"items_use", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 60}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 10}, {Down, 2},
			{0, 6}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 10}, {Circle, 2}, {0, 20}, {Right, 2}, {0, 6}, {Cross, 2}, {0, 6}, {Down, 2},
			{0, 6}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 10}, {Left, 2}, {0, 6}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 6}, {Cross, 2}, {0, 12}, {Right, 2}, {0, 6},
			{Cross, 2}, {0, 6}, {Cross, 2}, {0, 12}, {R1, 2}, {0, 10}, {L1, 2}, {0, 10}, {Down, 2}, {0, 5}, {Down, 2}, {0, 5}, {Down, 2}, {0, 5}, {Down, 2},
			{0, 5}, {Down, 2}, {0, 5}, {Down, 2}, {0, 5}, {Down, 2}, {0, 5}, {Down, 2}, {0, 5}, {Down, 2}, {0, 5}, {Right, 2}, {0, 6}, {Right, 2}, {0, 6},
			{Cross, 2}, {0, 6}, {Circle, 2}, {0, 8}, {Up, 2}, {0, 5}, {Up, 2}, {0, 5}, {Up, 2}, {0, 5}, {Up, 2}, {0, 5}, {Left, 2}, {0, 6}, {Circle, 2},
			{0, 60}, {Circle, 2}, {0, 40}},
		760};
	Scenario.Pokes = {
		{0x1f90, {0x5, 0x3, 0x2, 0x1, 0xc, 0x1, 0x1, 0x2, 0x1, 0x1, 0x3, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x1, 0x1, 0x78}},
		{0x2026, {0x1, 0x6, 0xa, 0x32, 0x39, 0x33, 0x2, 0x3, 0x4, 0x7, 0xc, 0xd, 0xf, 0x13, 0x15, 0x19, 0x23, 0x2c, 0x3a, 0x5, 0x9, 0x8}},
		{0x2b8, {0xa, 0x0}},
		{0x400, {0x64, 0x0}},
		{0x404, {0x5, 0x0}},
	};
	CheckMenuScenario(Scenario, "68cbcd8e",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 b35e3c23 1bb4e316 cfff0d5f 7d0f66b1 "
		"3319d8f3 b44870a5 c09f8dd5 0453a722 7ecf4bec ddacb74f 2bb2ed1f 5269e916 fff89d45 d0cef32d f5b52e99 89b5305f f5797f1f 9cbcb186 "
		"7d84527b 77b57bb3 4d4f5777 eb61527c 3618a16c 77bcf410*7 803aa3e3 ef6bc85c*5 d5f28b6b 2e1ea9a2*5 d7921a59 778ed52b 96aa80e8 ac9a808a "
		"0a961c44 d1b36ec0 3a817c6b 047f799f 1c058e6c 32107c6b 2b488c96 ed02add4 db60e97e 632d1af9 c7163b8e 4d9dc665 c92da94a d7f07cf3 "
		"465f78c8 951b74a3 1f399c2c 947f691b 7b542bbd be76fb46 542daa40 235e72b0 de9346cb 729b4178 e81fdeea e1b677cc 579cf354 b3abbee6 "
		"f3486df3 3efa3c23 2456a935 b4d23f4f*6 6147dde2*6 ee6926a6*6 1e139a72*6 0f40430a*3 71327662 31cd386f c4ee3259 645eb5af*6 0893c5e2 "
		"3b00a8e2 40e11985 6d5d1c0f 186a82b1*19 602d169f fee0a7f4 460645bf 1ed316e8*9 300aa951 1ed316e8 e5a1247f*6 9a71dbb1 aaa47c39 "
		"28fc4b16 18bf7b3b*5 85e712d4*4 d13f1e22 b0cc86b9 5d4bc43a ae62e922*9 c511fea4 de093cef 5d4bc43a ae62e922*9 a31a67dd 42fe6858 "
		"2d8e6ad8 71ab180a*3 9c792ff9*6 8f313c30*6 c9783b55*4 c411447a c9783b55 11aa5f76 943c54cd*5 5c8c403e 5a7f495c 641dfd1e*4 fc9c8213*2 "
		"26823bfa fc9c8213 1b626d09 abf1530d 76098e7a*4 e913e354 76098e7a a017978c 4b8d4f10*5 b29807cd d7d4f348 406e1044 79452d41*3 "
		"0ad9b831*6 6d01c01a 4a2fb0f8 b6ac4364 113f99df*3 08967045*2 e9b42eac 08967045 27a7d63a 3c60f63a f1a04851*4 bbd1ce49 f1a04851 "
		"e1ddc93b*6 41e53a54 f24b8268 c6b5e2f1*4 e6ff7fa7*6 3d343ff4*2 6f3635c1 3d343ff4 7b7836eb 97ea56e6 5cc16596*4 7ba3ba40 5cc16596 "
		"09f7ccfe*6 4a01be01 951aaea4 5ddcc6ab*4 63f73ae4*6 97ea56e6*2 400ced68 97ea56e6 3a637b36 3aad0137 ada662dd*6 f0077c46*2 d18e7c15 "
		"f0077c46 6f4e6499 4b36da32 5ddcc6ab*2 03784179*4 5cdeeb70*2 b5478906 5cdeeb70 3b4abf20 41593b73 69d45c76*3 5f2aa57a 69d45c76 "
		"b7818264 020ad03c*4 efbb114a 020ad03c b6163a2e bf371e26*4 526f7d05 683e1579 82c34814 d36069d7*4 8fe7dba0 0ee95977 fc57a881 "
		"1eed9eda*3 98979790 9ad842a0 98979790 dc656f83 691163f9*2 074430e0*2 400a3261 074430e0 505db85f 0cc7960d 80bbeb9e*3 47966dca "
		"80bbeb9e 0ef41363 e76e384f*2 d320c9d1*2 b8aa5715 d320c9d1 3b8955d5 c535f053*4 ea172b16 d93b3a23 81a0016d 0a3072d1*4 8d4db011 "
		"777592c2 8d4db011 706e9c4d 8d4db011*2 c92fc41e*3 144d890e c92fc41e 3460e5dd 8b36ccfe*5 979ce081 864091e8 84dcdf6d*5 0a3072d1*3 "
		"8429c474 0a3072d1 272ec692 21fd025d*4 930a0774 21fd025d 5a485092 c543bf84*4 ff528ac0 e42ccc30 c5f87765 6c4ac84c*4 bffa1e0c 18ba80af "
		"3096a03b 3156c1f0*3 6b94a373 b7066e1a 6b94a373 02a8a415 3b006d1e*2 3b59a206*3 83775e67 7bdb7024 59defa0b da0a1cb0 3909fa1d 3204b465 "
		"eee15bac 95ed9da3 73a9719f 23f28479 c7b2656a 358dae7b 04aa726a 51b89175 5b96946f 0c8026cd 98308f4e 56245f0e a42663fa bd0a3368 "
		"fff9bdc8 6d946d33 5b53c30c 26c22d19*18 c275a2ec*21 64cb7e0f abd3ffb1 031e8202 a4bbd0b0 18e85dd3 413576ef 13133489 57eb441f 8fe39adf "
		"63513e18 f61d3002 f7915b77 25614004 6776aed3 919b038c ef1e9cba 4f1d37f7 2f23d67e 6c2b3bd2 1f6177ce bb2c4eab a81a50ef 5e073cb1 "
		"65cd87e8*2 24ddaa35*2");
}

TEST_CASE("The Items screen applies every kind of item effect as the original does", "[Content][Menu]")
{
	// EP (on a member already full: the buzzer), a stat, max HP, weight gained and lost until the slot empties, HP and EP
	// together, and the data's two debugging items.
	FMenuScenario Scenario{"items_effects", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 60}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 10}, {Down, 2},
			{0, 6}, {Cross, 2}, {0, 10}, {Circle, 2}, {0, 20}, {Right, 2}, {0, 6}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 20}, {Left, 2},
			{0, 6}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 20}, {Right, 2}, {0, 6}, {Cross, 2}, {0, 8}, {Cross, 2},
			{0, 20}, {Cross, 2}, {0, 20}, {Left, 2}, {0, 6}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 20}, {Right, 2},
			{0, 6}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 20}, {Left, 2}, {0, 6}, {Down, 2}, {0, 6}, {Cross, 2},
			{0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 20}, {Right, 2}, {0, 6}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 30}, {R1, 2},
			{0, 10}, {L1, 2}, {0, 10}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		900};
	Scenario.Pokes = {
		{0x1f90, {0x2, 0x1, 0x1, 0x1, 0x1, 0x2, 0x1, 0x1}},
		{0x2026, {0x6, 0x33, 0x22, 0x37, 0x4a, 0x48, 0x31, 0x21}},
		{0x404, {0x5, 0x0}},
	};
	CheckMenuScenario(Scenario, "7f934a89",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 9bbe65b4 034c6315 d88488ff 2529c9b3 "
		"9eaeab68 a2498826 ce15db4a 842b4656 b501bf33 50f3c9ec abd7e5b1 e5e933a3 0088a2e1 82b1662e e6855c7e 0ad73f59 58befd6f f777ca51 "
		"03c62596 3431a087 58729a54 67874260 3feb1613 53956db8*7 1900d666 b5cf6f7b*5 e9206efb ae0a545b*5 2f322e45 e8bda14b 2ab8ed82 bb3cd8bd "
		"e56985b8 9b4299c1 3b2c743f 9688996d f39491ab 0c9d168e 7908cc30 773a05bb b4f48ec5 f54a02f6 652fc5fa 162d4f0c 4bf6cb33 60551851 "
		"31b97fb5 a6e7936a 5c2f8c16 b1a1845f ba616132 70b828ac ecc93db7 d7d30e73 c75b4edc 78c02f29 7680c894 697ab4d9 171d8aa2 a7652e88 "
		"b9e22319 962c4a57 06989b0f eb666ce1*6 232cb8eb*6 18d1f341*6 b033b5f8*6 2b0f03c5*3 26fc007d 63f9eeb7 4beac024 9d3f1b95*6 1bbf3d42 "
		"fb65c40e cfcc9a72 41933672 75b66bac*19 08469316 af74ab3f 41933672 75b66bac*9 9757164b 75b66bac a98e6d6d*6 967986fc fde39064 "
		"f138b6e9 bc8b3588*5 08648d1b*4 7aaf273f fa3bd651 56ce38bd ce35f091*3 1616f41e*6 b653e401*6 0b491f67*4 06441daa 0b491f67 ed25398c "
		"28d1de44*5 b3c50a6d 783328da 28f24a36*4 ae7748fe*4 0b54974c 668f946d 4f977182 83906aae*13 02b900c2*6 dd350ed8 b7448609 2500394b "
		"6563d8d9 1b984ec0 2032798d*5 1cbc819f*6 de61d09a*6 6ebec4b1 dd7cc84d ea5442de 26c9f789*3 6e8e692e*2 764e7b78 6e8e692e 5f4df728 "
		"8c628b25 ad23daf9*4 33da7235 ad23daf9 680185fd*6 681379f8*2 2cec458f b8564415 1f287617 17ab81f2*3 f26e81f7*16 5588efc6 c34b9633 "
		"43c096cc 7579876b 1b73fd4b 40d59645 c277a081*6 7e97a130*6 e196e34c*4 6e84a9c2 e196e34c b1c8d4b9 b6436e18*5 1ab07287 4bbe3981 "
		"5ed5d743*4 37ad2340*4 434d9030 59700e9e 66a39799 f97072a4 1edb783e*18 86ffcd08 c68f0e29 4b1f3c10 21a08b68 9a0ca0ff adc26172*5 "
		"7d14f34f*6 06398e75*6 ef2f4b76 8917c5b4 2a9ec57f*4 140353dc*2 a0eb9b46 140353dc afc8a87c ca294bf0 c04e895e*4 820e55b1 c04e895e "
		"3443c918*2 ec405103*4 202cc3d1*2 ea8e0fff 2637dac8 6b3c071f 60ee2c4d*19 333ccd13 04b9a992 61574180 2b12395c c3e03f61 a34b73e5 "
		"4ae11e5a*6 55c2eb62*6 42d200c1*4 b03722a1 42d200c1 ff2458c7 302eaa28*5 94f3109c 78978e47 b0717092*4 f73d2a01*4 4f0c83bf 3e2a90c9 "
		"e5ecc543 29dd9c93*19 d19af660 e60a5bea b4043793 29dd9c93*9 d19af660 e60a5bea 71a4a993 b3c36a19 c46fda67 01ce90fe*5 0d9c89c5*6 "
		"3ef4d72a*2 8a01e748*4 5decc11a 162244a8 cadfe809*4 87c6ab81*2 1abaa648 87c6ab81 8709e9de 69c6fbd9 f62fe275*4 94cdc889 f62fe275 "
		"d6c3c5a1*6 1073dae2*2 21ed278d aaf8d4c3 342835c9 9e1c3e6f*19 a32ee415 15c29c10 06f5296e 8d668fcf 7c95c6f5 0e352300 6e58a1e6*2 "
		"06ba5d60*4 21360403*6 0b65bcfb*4 5aa1be18 0b65bcfb 3eaf78d8 0172ff62*5 b63bb754 482389f8 ff7baa53*4 8b2c8f79*4 0e0c5a89 63eb345f "
		"f975ce03 510096e5*19 55795397 b8d33230 303e3ae8 bf8aff7f*3 bf85f426*26 2ea97d3b bf85f426*11 2ea97d3b bf85f426*11 b3831e46 a9e52b45 "
		"2335bf66 5e377617*6 922b3211 98d9b742*5 6323de8b*6 137e9567*6 79c3ca7b*6 c4eeb930*6 98d9b742*6 6323de8b*6 137e9567*6 79c3ca7b*5 "
		"92a1c44c 8a3a02c4 8aea2aec f98763db d412ed5c e2dd9312 2a0eed48 f1750cc4 f5642d35 1050cdae 598f8817 fa71c394 365016cf 4f0008c1 "
		"18c59e45 a08e0ec8 af6f03fe 793ec1ad b695f92d 25a28a46 113e8abd d4430359 fc2f041d fadfb27a*45 dd74dfd1*60");
}
