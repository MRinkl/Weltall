#include "Support/GameDiscs.h"
#include "Support/MenuOracleRun.h"

#include <catch_amalgamated.hpp>

#include <cstdint>
#include <vector>

using namespace Weltall;
using namespace Weltall::Testing;
using namespace Weltall::Testing::MenuPad;

// The Menu program against the original's main menu module run whole under the oracle harness,
// frame by
// frame: every GPU packet a frame draws, the sounds it plays and the images it loads into VRAM, and
// what the menu leaves the game.

TEST_CASE("The title screen reveals its commands and waits as the original does", "[Content][Menu]")
{
	CheckMenuScenario({"title_idle", EMenuMode::LoadGame, 0, {}, 120}, "3fa897b6", "44223061 ef6008a9 854c0222*2 7c54d75c*2 8172c16c*2 538d6d19 e8e08344*111");
}

TEST_CASE("The title's Sound option opens on the output mode and moves as the original does", "[Content][Menu]")
{
	CheckMenuScenario({"title_sound", EMenuMode::LoadGame, 0, {{0, 20}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 40}, {Up, 2}, {0, 30}}, 160}, "8722d03d",
		"44223061 ef6008a9 854c0222*2 7c54d75c*2 8172c16c*2 538d6d19 e8e08344*12 706ddb42 9f75cfa7*5 41422f63 ff3e3c0f ccfb054c f861b048 54059480*2 "
		"15472a2e*2 6fa48d6d*2 d76a30db*2 36706a5d*2 2654e085*28 aea0661e 2654e085 3f835d6d*89");
}

TEST_CASE("The Normal Menu opens, reveals its commands and slides the party's panels in as the original does", "[Content][Menu]")
{
	CheckMenuScenario({"normal_idle", EMenuMode::Normal, 0, {}, 100}, "5a03a3dd",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 61063503 d27c2f4e 1f1805ef 4c89d363 "
		"8a1d2501 0267fb36 e25c6b51 1090cb74 5c70056f de367a9a d49f1f91 435d2c11 86f8ac12 e012d66d 9dbf5ac0 a33373d1 8f86fd20 a5fe68f1 0b92b3c6 "
		"93045b6e 465bf2ca a12b58e6 8b8d13b3 06fa6e02*21 14f3ca23*40");
}

TEST_CASE("Cancel closes the Normal Menu, its panels sliding out, as the original does", "[Content][Menu]")
{
	CheckMenuScenario({"normal_exit", EMenuMode::Normal, 0, {{0, 60}, {Circle, 2}, {0, 10}}, 200}, "1b0f1cbd",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 61063503 d27c2f4e 1f1805ef 4c89d363 "
		"8a1d2501 0267fb36 e25c6b51 1090cb74 5c70056f de367a9a d49f1f91 435d2c11 86f8ac12 e012d66d 9dbf5ac0 a33373d1 8f86fd20 a5fe68f1 0b92b3c6 "
		"93045b6e 465bf2ca a12b58e6 8b8d13b3 06fa6e02*21 14f3ca23 0a5ab005 9111c0c0 a0a39c72 dad10fc7 f2319bb9 5db479cc abbfc4d6 46fc32bf "
		"abe0317f eb5a62e0 1829c421 ed1dda16 fc1dc2a9 3b3f1230 132e54b9 3363cebf 56b4a4ae 772d844e 77903f8f ebb3a0a0 a51c3ee1 61bd8622 29a2849c "
		"c874b5a4*2 24ddaa35*2");
}

TEST_CASE("The Normal Menu's arc moves, repeats and wraps, and remembers its command, as the original does", "[Content][Menu]")
{
	FMenuScenario Scenario{"normal_moves", EMenuMode::Normal, 0,
		{{0, 50}, {Up, 3}, {0, 3}, {Up, 40}, {0, 5}, {Down, 2}, {0, 2}, {Down, 2}, {0, 30}, {Circle, 2}, {0, 10}}, 240};
	Scenario.PlayTime = 0x123456;
	Scenario.Seed = 0x2468ACE;
	Scenario.LastCommand = 2;
	CheckMenuScenario(Scenario, "e5c65c8f",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 61063503 d27c2f4e 1f1805ef 4c89d363 "
		"8a1d2501 0267fb36 e25c6b51 1090cb74 5c70056f de367a9a d49f1f91 435d2c11 86f8ac12 e012d66d 9dbf5ac0 a33373d1 8f86fd20 a5fe68f1 0b92b3c6 "
		"93045b6e 465bf2ca 2b458662 f1b76f76 9906fa83*12 2428913a 0d554c72*2 3b972edf*3 8b3e1a76 61ea405a*32 e566a93f 7b0c6050*3 a6e83494 "
		"1a2d3379*7 cccc7579 7b0c6050*3 a6e83494 61ea405a*7 d2ec9cfc*24 86fe07fb a91edbac 1724c4e1 6e28343a 5d1cbff7 373b4d59 db5c2ed5 67cd347a "
		"e8e36776 06bd9ae6 fcdde276 90649ec5 28d5282c c5e3bdc7 787991bb 8c6f68a9 e02141cb 10cc6bd2 0d7c2c2b e6a08008 c927127f 32a9ca12 ac56ad9e "
		"b94ff923*2 24ddaa35*3");
}

TEST_CASE("Where saving is allowed the Normal Menu opens on File and keeps no command, as the original does", "[Content][Menu]")
{
	FMenuScenario Scenario{"normal_saving", EMenuMode::Normal, 1, {{0, 45}, {Down, 2}, {0, 5}, {Circle, 2}, {0, 10}}, 160};
	Scenario.LastCommand = 4;
	CheckMenuScenario(Scenario, "0767bcd2",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 61063503 d27c2f4e 1f1805ef 4c89d363 "
		"8a1d2501 0267fb36 e25c6b51 1090cb74 5c70056f de367a9a d49f1f91 435d2c11 86f8ac12 e012d66d 9dbf5ac0 a33373d1 8f86fd20 a5fe68f1 0b92b3c6 "
		"93045b6e 465bf2ca a12b58e6 8b8d13b3 5840e9c7*7 75ea656f 607cb107*6 27689db6 ee45365c 28d2ab5b d87050e8 a8e304ed 59f39a72 8044b555 "
		"a12db7fe 2064cef9 d1ec6928 a9b70542 e1321d22 dcf11cbb 8f46aea6 f0294fad b5f2aa89 7f056afb f7f46f26 33d50851 ab4d921f b5f02a2d 633b2da7 "
		"bb7acee8 01a3d420*2 24ddaa35*2");
}

TEST_CASE("The Status screen shows each member's sheet, stats and equipment and moves its inner arc as the original does", "[Content][Menu]")
{
	FMenuScenario Scenario{"normal_status", EMenuMode::Normal, 0,
		{{0, 45}, {Cross, 2}, {0, 120}, {R1, 2}, {0, 60}, {L1, 2}, {0, 40}, {Down, 2}, {0, 20}, {Up, 2}, {0, 20}, {Up, 2}, {0, 20}, {Circle, 2}, {0, 100},
			{Circle, 2}, {0, 40}},
		520};
	CheckMenuScenario(Scenario, "5b539121",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 61063503 d27c2f4e 1f1805ef 4c89d363 "
		"8a1d2501 0267fb36 e25c6b51 1090cb74 5c70056f de367a9a d49f1f91 435d2c11 86f8ac12 e012d66d 9dbf5ac0 a33373d1 8f86fd20 a5fe68f1 0b92b3c6 "
		"93045b6e 465bf2ca a12b58e6 8b8d13b3 06fa6e02*7 45b8edeb 4ecc9e18 32b63bc8 62445cda 4dd424f1 bd0fb1ae e35a956b f73c2b98 335e9523 "
		"251b31cc 5f28f1b5 c0174d38 128033a9 d4cfd7ae 93d25ca2 bfa0a1b3 7b377030 6343122f 4da7234c f68117b6 80db5936 273d05c8 74ec4039 af8fdad9 "
		"33d95943 3c34da2b a6215c86 09f24d53 994dd30f 5147d2d8 2b4248cf 73efe041 676cd319 0d7cb64a 4e13d726 23d01a63 07ee841f c00b7899 "
		"de144c24*36 abbb4e23*48 8d564841 abbb4e23 c16bb2f2 96fb08a2 d1a389af 890163fb*7 b0886698*50 3bc85aa4 b0886698 bb59f207 d3b81835 "
		"89931ad7 fa67ce32*5 71cfa015*32 b61df131 71cfa015 bbd0f816*20 3aa23bc2 bbd0f816 71cfa015*4 f2ff1560*16 3d2a96ab f2ff1560 f5e94a81*20 "
		"5226ce88 f322d220 d9506622 3a064568 db04215f 210028d2 d5ad0553 61de2fa8 0123bb58 1376d7ef 1c5909a1 e0412008 0c1c9299 76a4fa1e d2c15255 "
		"785a138f 450f3490 ec088cb9 a1e202a2 9b635dba 897b8dd6 7357a73b 1cc40a47 d5bf968f*59 4dfaa598*20 bb0fb3b3 9fa231f5 015d2025 0e87f6f6 "
		"8207e9ff 892813b7 f4b50e71 c22cfc25 7980e6f7 dd824b0c 585783d5 7cc9a0d9 ad828622 c6c831ee c0733907 8eb18d9e 259db9bf 5e7f1e75 d835f9ab "
		"39eb436e 91189253 c1b1b91b 47dbc6ff 75070a98*2 24ddaa35*3");
}

TEST_CASE("The Skills screen lists each member's Deathblows and the Status screen comes back after it, as the original does", "[Content][Menu]")
{
	FMenuScenario Scenario{"normal_skills", EMenuMode::Normal, 0,
		{{0, 45}, {Cross, 2}, {0, 120}, {Cross, 2}, {0, 40}, {R1, 2}, {0, 40}, {L1, 2}, {0, 30}, {Circle, 2}, {0, 40}, {Circle, 2}, {0, 100}, {Circle, 2},
			{0, 40}},
		600};
	CheckMenuScenario(Scenario, "383dcd52",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 61063503 d27c2f4e 1f1805ef 4c89d363 "
		"8a1d2501 0267fb36 e25c6b51 1090cb74 5c70056f de367a9a d49f1f91 435d2c11 86f8ac12 e012d66d 9dbf5ac0 a33373d1 8f86fd20 a5fe68f1 0b92b3c6 "
		"93045b6e 465bf2ca a12b58e6 8b8d13b3 06fa6e02*7 45b8edeb 4ecc9e18 32b63bc8 62445cda 4dd424f1 bd0fb1ae e35a956b f73c2b98 335e9523 "
		"251b31cc 5f28f1b5 c0174d38 128033a9 d4cfd7ae 93d25ca2 bfa0a1b3 7b377030 6343122f 4da7234c f68117b6 80db5936 273d05c8 74ec4039 af8fdad9 "
		"33d95943 3c34da2b a6215c86 09f24d53 994dd30f 5147d2d8 2b4248cf 73efe041 676cd319 0d7cb64a 4e13d726 23d01a63 07ee841f c00b7899 "
		"de144c24*36 abbb4e23*48 8660ff77 85f17bfc 7b8492ed*40 3cec1fd5 7b8492ed d6c0da5e*40 ad7728b6 d6c0da5e 7b8492ed*30 765a17ed cedcc13e "
		"48468da1 94844ff7*39 1df251be b7401562 c91293f7 ddc49639 8819f9bc d718fe99 fe3ff4a6 827c77e8 c29b6ed5 d2a99dc2 142135f0 5bf81287 "
		"9c3b6704 3b698105 4e3dd6dd 9d96379e ab9a5e22 e81059a2 11f6b071 92969acb 73c217b6 98cd3943 8cb35217 187c0683*11 d5bf968f*60 4dfaa598*8 "
		"bb0fb3b3 9fa231f5 015d2025 0e87f6f6 8207e9ff 892813b7 f4b50e71 c22cfc25 7980e6f7 dd824b0c 585783d5 7cc9a0d9 ad828622 c6c831ee c0733907 "
		"8eb18d9e 259db9bf 5e7f1e75 d835f9ab 39eb436e 91189253 c1b1b91b 47dbc6ff 75070a98*2 24ddaa35*3");
}

TEST_CASE("The Skills screen shows learned Deathblows with their combos and the others' learning, as the original does", "[Content][Menu]")
{
	// Fei has learned his first three Deathblows, Citan his first and fifth; the upper tier is open; both have counters under way.
	const auto Counters = [](uint32 Step, uint32 Modulus)
	{
		std::vector<uint8> Bytes;
		for (uint32 Counter = 1; Counter <= 7; ++Counter)
		{
			const uint32 Value = Counter * Step % Modulus;
			Bytes.push_back(static_cast<uint8>(Value));
			Bytes.push_back(static_cast<uint8>(Value >> 8));
		}
		return Bytes;
	};
	FMenuScenario Scenario{"normal_deathblows", EMenuMode::Normal, 0, {{0, 45}, {Cross, 2}, {0, 120}, {Cross, 2}, {0, 40}, {R1, 2}, {0, 40}, {Circle, 2}, {0, 30}}, 330};
	Scenario.Pokes = {{0x16C0, {0x00, 0xE0}}, {0x22B6, {0x00, 0x40}}, {0x26C + 0x90, Counters(37, 400)}, {0x26C + 2 * 0xA4 + 0x90, Counters(91, 700)},
		{0x16C0 + 2 * 0x20, {0x00, 0x88}}};
	CheckMenuScenario(Scenario, "2a348d91",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 61063503 d27c2f4e 1f1805ef 4c89d363 "
		"8a1d2501 0267fb36 e25c6b51 1090cb74 5c70056f de367a9a d49f1f91 435d2c11 86f8ac12 e012d66d 9dbf5ac0 a33373d1 8f86fd20 a5fe68f1 0b92b3c6 "
		"93045b6e 465bf2ca a12b58e6 8b8d13b3 06fa6e02*7 45b8edeb 4ecc9e18 32b63bc8 62445cda 4dd424f1 bd0fb1ae e35a956b f73c2b98 335e9523 "
		"251b31cc 5f28f1b5 c0174d38 128033a9 d4cfd7ae 93d25ca2 bfa0a1b3 7b377030 6343122f 4da7234c f68117b6 80db5936 273d05c8 74ec4039 af8fdad9 "
		"33d95943 3c34da2b a6215c86 09f24d53 994dd30f 5147d2d8 2b4248cf 73efe041 676cd319 0d7cb64a 4e13d726 23d01a63 07ee841f c00b7899 "
		"de144c24*36 abbb4e23*48 8660ff77 85f17bfc b698cac8 36103cc1*39 041e003f 36103cc1 cef31bd3 08600e8c*39 91ce213e cedcc13e 48468da1 "
		"94844ff7*75");
	// Fixed, a Deathblow at 100 % shows its bar full (54 pixels); the original's is empty, the longest 49 (92 %).
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CHECK(RunMenuScenario(*Disc, Scenario).LongestLearningBar == 49);
	FMenuScenario Fixed = Scenario;
	Fixed.Options = FMenuOptions();
	CHECK(RunMenuScenario(*Disc, Fixed).LongestLearningBar == 54);
}
