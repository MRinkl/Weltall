#include "Battle/Show/BattleObjectsFrame.h"
#include "Battle/Show/BattleShowObjects.h"
#include "Gears/GearPackets.h"
#include "Support/BattleFrameDigest.h"
#include "Support/BattleObjectCases.h"
#include "Support/GameDiscs.h"
#include "Support/OracleDigest.h"
#include "Support/XorShift32.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <numeric>
#include <set>
#include <span>
#include <string>
#include <vector>

using namespace Weltall;
using Weltall::Testing::FBattleObjectCase;
using Weltall::Testing::FBattleObjectCaseFrames;
using Weltall::Testing::FBattleObjectData;
using Weltall::Testing::FOracleDigest;
using Weltall::Testing::FXorShift32;

// Battle's stage and 3D objects as the original draws them: the objects'
// oracle cases, from their own seed and shorter, with the kernel's MDL builder and part drawing running; each frame the
// stage's model, then the objects (their shadows, shown parts and cloths) and the trails, into the ordering table. Each
// frame's digest walks the table far to near: what drew each packet, its entry, its code, its corners on the screen, its
// colours (none for the static textured triangle, the environment-mapped one, and G4 and GT4, which keep their build's
// light in the original), its texels, CLUT and page. Part "reach" makes each case on a stage and a scene the random ones
// miss, some with their view aimed at an entry of the scene and their sequences started on that entry's object.
namespace
{
	using Weltall::Testing::FBattleObjectCaseChoice;

	constexpr uint32 DrawSeed = 0x0D7A3B11;
	constexpr int32 DrawCases = 40;
	constexpr uint32 MostFrames = 40;
	constexpr uint32 ReachSeed = 0x5EAC4D07;
	constexpr int32 ReachCases = 12;
	/**
	 * Each reach case's stage, scene, entry aimed at and sequence started, in turn: stages 26 and 35 draw kind 5, stage 54
	 * kind 7; scene 62's entry 1 is environment-mapped and its entry 2 billboarded (kind 2); scene 38's entry 0 starts a
	 * trail on the screen in its sequence 0, on stage 18, whose record has a trail pool (26, 35 and 54 have none).
	 */
	const std::array<FBattleObjectCaseChoice, 6> ReachChoices{{{26, 62, 1, {}}, {18, 38, 0, 0}, {54, 62, {}, {}}, {26, 38, {}, {}}, {35, 62, 1, {}},
		{54, 38, {}, {}}}};

	/** What drew a link, as the oracle numbers them. */
	enum : int32
	{
		DrawnByPart = 0,
		DrawnByCloth = 2,
	};

	constexpr const char* Expected[DrawCases] = {
		"e6d0746d4e8c78c14111cfa5ff7305aa04cbd884",
		"4a5d716ffb634a3bc9a9da0914258e962b010318",
		"55aad8941a2347b47df67a300e8d57f0a6aab4f4",
		"acf00fe4c3f2432326f89daad25d1e9bf4408306",
		"1410a2b5fc89d043b59e436c12be1b0013f5fb32",
		"0a59728ddaf4154b65b9b3eebe362b98f7322113",
		"173a3108eea447feadb62d0e1da953251369ad46",
		"b7d6a95e19432efc63f94e206cd10e8b76b58087",
		"687c62b5c22894b2f81a9e6388d20a5c81ab4c8c",
		"245748c4c77adb783e8f486707a1b128c3b28496",
		"03ebeba83efeabba8e3609866ab0055988191556",
		"013f3a729ea0bc1ad0d54d3dbc1cbb55f6890499",
		"0d1140afbd7de9258d306b143f1a28291fd4b10d",
		"1b33a3acfbc8942ecd33429e584b4fc124f66654",
		"8757c035b6b25a879e229b5c59b7588885a78ec0",
		"bd25a82274d80deb89e44e70217c8d5b30871665",
		"91623a7949e2bbb4fa72da6f9c4ac55adb04658d",
		"1031c63cd03005d187962cef45aa944885bbd8fb",
		"76b7d7cdc8d4fff1b03a1ac1c76abd6c8acddb04",
		"9e1e2bfcaf22ee5e85a37b28fe4f32787b93458e",
		"a7a8c2748e3032dfbf683afc6bf5f4283ad096aa",
		"34fe510a33049d7683f5af6c07418b3b4701d652",
		"8262d915ad6c58a7e00743c5b045a958c2285ee4",
		"9a8ed466bbf027a7e1d545cd0a99cbbf7ae23e6f",
		"8235b57a3007a0c812a294ab5d8a262819185b96",
		"578d55eee69593460e180cbe87f0fdef2fa2dad3",
		"945eff2d955e786c3d6f97026e68f892047c9eee",
		"195f744bb558f1ae7c25da7f54708ef9c1b8b2be",
		"d77ac618bc6ab8c22386b4938ccdcd81f8183d27",
		"8e54fbea6b268763b17255a8e94e87108411d9ad",
		"4682c13048d86b6c80df1f7234f78db5eea30c9a",
		"71d979b65dadc5daef3abf620ac2fb977ef82beb",
		"5d37774ec38cef98b9405b24b2c13903691ff7ea",
		"ced5d441236d20a354375c9a92a599a71a255c43",
		"4f9af7cdf82bfec7fb33ffbe07509a8be05e1d00",
		"63936fcdd2e1fdfad22893d7818b1d2025db21be",
		"e1a7ed5c249cfffb2f62fcef4d7b966385ac9566",
		"8f4a5512a4b070ac654d70e82fd88d7bce08c05a",
		"cef84827c3d406a60aaa8cbebe790161a29999b7",
		"3603be971ac99c18e8261a54d5bce7258b5593a5",
	};

	constexpr const char* ReachExpected[ReachCases] = {
		"1be67b5678bf85392d9cf396e0d844dd500fa5b9",
		"2c389037944af2678b80a9ccae80fc1b5bc6b61a",
		"23d87d615785bc07fc840300bae46d9ec8314c98",
		"908a4ff38d69996c9e01a499f6bf3717506b251e",
		"65de4e1822e02f457966bba160d5e23e51114422",
		"a5b24c0968289eca0b3aeafae35044afc856d45e",
		"d73fab73fa9ad7895183ecc6fdeda6432d16e3fb",
		"e6af75c1696faf7f17d8ecfb57d925c6ea29acda",
		"ead21c394de1bd07249177d3e704542cd19a193f",
		"cb647ac3434da673eeb9ffc16e5b1e833372529f",
		"cb7f0669ca95680a928ffdd33bf0148195bfacf0",
		"81fff000e182cf2d41e1fcc52414753b8c600e7f",
	};

	/** The frame's links as the GPU draws them: the ordering table far to near, within an entry the last linked first. */
	void DigestDrawing(FBattleObjectCase& Case, FOracleDigest& Digest)
	{
		const FBattleObjectsFrame& Frame = Case.Objects.Frame();
		std::vector<std::size_t> Order(Frame.Links.size());
		std::iota(Order.begin(), Order.end(), std::size_t{0});
		std::sort(Order.begin(), Order.end(), [&Frame](std::size_t A, std::size_t B)
		{
			const int32 EntryA = Frame.Links[A].Entry;
			const int32 EntryB = Frame.Links[B].Entry;
			return EntryA != EntryB ? EntryA > EntryB : A > B;
		});
		for (const std::size_t Index : Order)
		{
			Weltall::Testing::DigestBattleObjectLink(Digest, Frame, Case.Gte, Frame.Links[Index]);
		}
	}

	/** Each frame of a case drawn as the harness draws it (the stage's model, then the objects), under Culling. */
	FBattleObjectCaseFrames DrawnFrames(EFieldCulling Culling)
	{
		FBattleObjectCaseFrames Frames;
		Frames.MostFrames = MostFrames;
		Frames.Draw = [Culling](FBattleObjectCase& Case, const FGteMatrix& View, int32 Buffer) {
			Case.Gte.AverageScale3 = 0x155;
			Case.Gte.AverageScale4 = 0x100;
			Case.Objects.Culling = Culling;
			Case.Objects.DrawStage(View, true);
			Case.Objects.UpdateDraw(View, 0, Buffer);
		};
		return Frames;
	}

	/** A part of the oracle: its seed, its cases' digests and, for the reach part, the choices its cases take in turn. */
	struct FDrawPart
	{
		uint32 Seed = 0;
		std::span<const char* const> Expected;
		std::span<const FBattleObjectCaseChoice> Choices;
	};

	void CheckCases(const FDrawPart& Part, int32 First, int32 End)
	{
		REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
		FBattleObjectData Data;
		REQUIRE(Weltall::Testing::ReadBattleObjectData(*Disc, Data));
		FBattleObjectCaseFrames Frames = DrawnFrames(EFieldCulling::Original);
		Frames.DigestFrame = DigestDrawing;
		FXorShift32 G(Part.Seed);
		for (int32 Number = 0; Number < End; ++Number)
		{
			const FBattleObjectCaseChoice Choice =
				Part.Choices.empty() ? FBattleObjectCaseChoice{} : Part.Choices[static_cast<std::size_t>(Number) % Part.Choices.size()];
			FOracleDigest Digest;
			const std::string Ends = Weltall::Testing::RunBattleObjectCase(G, Data, Digest, Frames, Choice);
			if (Number >= First)
			{
				INFO("case " << Number << (Ends.empty() ? std::string() : " (ends: " + Ends + ")"));
				CHECK(Digest.Hex() == Part.Expected[static_cast<std::size_t>(Number)]);
			}
		}
	}

	const FDrawPart RandomPart{DrawSeed, Expected, {}};
	const FDrawPart ReachPart{ReachSeed, ReachExpected, ReachChoices};

	/** What drew each of a frame's polygons and cloth triangles: a polygon by its slot, bone and number, a triangle by its slot, cloth and number. */
	std::set<uint64> DrawnKeys(const FBattleObjectsFrame& Frame)
	{
		std::set<uint64> Keys;
		for (const FBattleObjectLink& Link : Frame.Links)
		{
			if (Link.Kind == EBattleObjectDraw::Polygon)
			{
				const FBattleDrawnPolygon& Polygon = Frame.Polygons[Link.Index];
				const FBattleDrawnPart& Part = Frame.Parts[Polygon.Part];
				Keys.insert(uint64{DrawnByPart} << 56 | uint64{Part.Slot} << 32 | uint64{Part.Bone} << 16 | Polygon.Number);
			}
			else if (Link.Kind == EBattleObjectDraw::Cloth)
			{
				const FGearDrawnClothTriangle& Triangle = Frame.Cloths[Link.Index];
				Keys.insert(uint64{DrawnByCloth} << 56 | uint64{Triangle.Slot} << 32 | uint64{Triangle.Cloth} << 16 | Triangle.Triangle);
			}
		}
		return Keys;
	}

	/** Each frame's drawn keys over every case, the cases drawn under Culling. */
	std::vector<std::set<uint64>> DrawnKeysOfCases(const FBattleObjectData& Data, EFieldCulling Culling)
	{
		std::vector<std::set<uint64>> Frames;
		FBattleObjectCaseFrames How = DrawnFrames(Culling);
		How.DigestFrame = [&Frames](FBattleObjectCase& Case, FOracleDigest&) { Frames.push_back(DrawnKeys(Case.Objects.Frame())); };
		FXorShift32 G(DrawSeed);
		for (int32 Number = 0; Number < DrawCases; ++Number)
		{
			FOracleDigest Digest;
			Weltall::Testing::RunBattleObjectCase(G, Data, Digest, How);
		}
		return Frames;
	}
}

TEST_CASE("Battle's stage and objects draw as the original does in cases 0 to 19", "[Content][BattleObjects]")
{
	CheckCases(RandomPart, 0, 20);
}

TEST_CASE("Battle's stage and objects draw as the original does in cases 20 to 39", "[Content][BattleObjects]")
{
	CheckCases(RandomPart, 20, 40);
}

TEST_CASE("Battle's stage and objects draw as the original does on the stages and scenes the random cases miss", "[Content][BattleObjects]")
{
	CheckCases(ReachPart, 0, ReachCases);
}

TEST_CASE("Battle's objects drawn in range keep every cloth triangle the original draws, and draw more", "[Content][BattleObjects]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FBattleObjectData Data;
	REQUIRE(Weltall::Testing::ReadBattleObjectData(*Disc, Data));
	const std::vector<std::set<uint64>> Original = DrawnKeysOfCases(Data, EFieldCulling::Original);
	const std::vector<std::set<uint64>> InRange = DrawnKeysOfCases(Data, EFieldCulling::InRange);
	REQUIRE(Original.size() == InRange.size());
	// The polygons need not include the original's: it keeps some with a corner behind the eye, by their saturated corners.
	std::array<std::size_t, 2> More{};
	for (std::size_t Frame = 0; Frame < Original.size(); ++Frame)
	{
		INFO("frame " << Frame);
		for (const uint64 Key : Original[Frame])
		{
			if (Key >> 56 == DrawnByCloth)
			{
				CHECK(InRange[Frame].contains(Key));
			}
		}
		for (const uint64 Key : InRange[Frame])
		{
			if (!Original[Frame].contains(Key))
			{
				++More[Key >> 56 == DrawnByCloth ? 1 : 0];
			}
		}
	}
	CHECK(More[0] > 0);
	CHECK(More[1] > 0);
}
