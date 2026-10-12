#include "Battle/Show/BattleGearOpening.h"
#include "Battle/Show/BattleObjectFiles.h"
#include "Gears/GearPoseTrack.h"
#include "Gears/GearSkeleton.h"
#include "Graphics/GteVectors.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

using namespace Weltall;

namespace
{
	/** The party's gears: the numbers their field sprites and battle models share. */
	constexpr int32 PartyGears = 17;
	constexpr int32 RestRecord = 0;
	constexpr int32 MoveRecord = 2;
	constexpr int32 LeapRecord = 3;
	constexpr int32 DashRecord = 25;

	/** A skeleton of two bones, the second hanging from the first. */
	const std::vector<FGearSkeletonEntry> TwoBones = {{0xFFFF, 0xFFFF}, {0xFFFF, 0}};

	/** The pose of a skeleton whose first bone is turned by Angles and stands at Position. */
	FGearPose PoseTurned(const FGteVector& Angles, const std::array<int32, 3>& Position = {})
	{
		std::vector<FGearBone> Bones;
		MakeGearSkeleton(TwoBones, Bones);
		Bones[1].Angles = Angles;
		Bones[1].Position = Position;
		return GearPoseOf(Bones);
	}

	/** The first bone's local turn a pose gives a skeleton. */
	FGteMatrix TurnGiven(const FGearPose& Pose)
	{
		std::vector<FGearBone> Bones;
		MakeGearSkeleton(TwoBones, Bones);
		ApplyGearPose(Pose, Bones);
		return Bones[1].Local;
	}

	/** How far apart two rotations' entries are at most, 4096 being 1. */
	int32 Apart(const FGteMatrix& A, const FGteMatrix& B)
	{
		int32 Most = 0;
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				Most = std::max(Most, std::abs(A.M[Row][Column] - B.M[Row][Column]));
			}
		}
		return Most;
	}

	/** How far apart two poses' bones are at most: their turns (a quaternion's part, either sign of the whole) and their places. */
	std::pair<float, float> Apart(const FGearPose& A, const FGearPose& B)
	{
		float Turn = 0.0f;
		float Place = 0.0f;
		for (std::size_t Bone = 1; Bone < A.size(); ++Bone)
		{
			const std::array<float, 4>& P = A[Bone].Rotation;
			const std::array<float, 4>& Q = B[Bone].Rotation;
			const float Side = P[0] * Q[0] + P[1] * Q[1] + P[2] * Q[2] + P[3] * Q[3] < 0.0f ? -1.0f : 1.0f;
			for (std::size_t Part = 0; Part < 4; ++Part)
			{
				Turn = std::max(Turn, std::fabs(P[Part] - Side * Q[Part]));
			}
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Place = std::max(Place, std::fabs(A[Bone].Position[Axis] - B[Bone].Position[Axis]));
			}
		}
		return {Turn, Place};
	}
}

TEST_CASE("A pose keeps each bone's turn whatever angles wrote it, and gives it back to a skeleton", "[Gears]")
{
	// Every turn of the kernel's rotation comes back within the rounding of a 4.12 entry.
	for (const FGteVector& Angles : {FGteVector{0, 0, 0}, FGteVector{0x400, 0, 0}, FGteVector{0x123, 0x456, 0x789}, FGteVector{0x800, 0, 0}, FGteVector{0x7FF, 0x800, 0x801},
			 FGteVector{-0x300, 0xC00, 0x155}})
	{
		INFO("angles " << Angles[0] << ", " << Angles[1] << ", " << Angles[2]);
		CHECK(Apart(TurnGiven(PoseTurned(Angles)), RotationXyz(Angles)) <= 6);
	}
	// Two sets of angles that are one turn make one pose: half a turn about x and y is half a turn about z.
	const FGearPose One = PoseTurned({0x800, 0x800, 0});
	const FGearPose Other = PoseTurned({0, 0, 0x800});
	CHECK(Apart(TurnGiven(One), TurnGiven(Other)) <= 6);
	CHECK(Apart(One, Other).first < 0.01f);

	// A pose places the bones and leaves the root to its host; the bones' matrices follow at the next computing.
	std::vector<FGearBone> Bones;
	MakeGearSkeleton(TwoBones, Bones);
	ComputeGearBones(Bones, 0x1000);
	Bones[0].Angles = {1, 2, 3};
	FGearPose Pose = PoseTurned({0x400, 0, 0}, {10, 20, 30});
	Pose[2].Position = {0.0f, -5.0f, 0.0f};
	ApplyGearPose(Pose, Bones);
	CHECK(Bones[0].Angles == FGteVector{1, 2, 3});
	CHECK(Bones[1].bWorldDirty == 1);
	CHECK(Bones[1].bRotationDirty == 0);
	ComputeGearBones(Bones, 0x1000);
	// Turned a quarter about x, the first bone puts its child's 5 up along -z.
	CHECK(Bones[2].World.T == std::array<int32, 3>{10, 20, 25});
}

TEST_CASE("Two poses blend by weight, each bone turning the short way", "[Gears]")
{
	// From an eighth of a turn short of whole to an eighth past it: through no turn at all, not round by the half turn.
	const FGearPose From = PoseTurned({0x0E00, 0, 0}, {100, -200, 0});
	const FGearPose To = PoseTurned({0x0200, 0, 0}, {300, 200, -40});
	FGearPose Blended;
	BlendGearPoses(From, To, 0.5f, Blended);
	REQUIRE(Blended.size() == 3);
	CHECK(Apart(TurnGiven(Blended), FGteMatrix::Identity()) <= 6);
	CHECK(Blended[1].Position == std::array<float, 3>{200.0f, 0.0f, -20.0f});
	// The same turns written with other angles blend the same way.
	const FGearPose FromOther = PoseTurned({0x0600, 0x800, 0x800});
	FGearPose BlendedOther;
	BlendGearPoses(FromOther, To, 0.5f, BlendedOther);
	CHECK(Apart(TurnGiven(BlendedOther), FGteMatrix::Identity()) <= 6);

	BlendGearPoses(From, To, 0.0f, Blended);
	CHECK(Apart(From, Blended).first < 0.001f);
	BlendGearPoses(From, To, 1.0f, Blended);
	CHECK(Blended == To);
	// Poses of different skeletons do not blend: the one blended to is taken.
	BlendGearPoses(FGearPose(1), To, 0.5f, Blended);
	CHECK(Blended == To);
}

TEST_CASE("A pose track is sampled between its ticks and goes round", "[Gears]")
{
	FGearPoseTrack Track;
	Track.Ticks = {PoseTurned({0, 0, 0}, {0, 0, 0}), PoseTurned({0x500, 0, 0}, {40, 0, 0}), PoseTurned({0xA00, 0, 0}, {80, 0, 0})};
	CHECK(Track.Cycle() == 3 * FGearPoseTrack::TickPhase);
	FGearPose Pose;
	Track.Sample(FGearPoseTrack::TickPhase / 2, Pose);
	CHECK(Apart(TurnGiven(Pose), RotationXyz({0x280, 0, 0})) <= 6);
	CHECK(Pose[1].Position[0] == 20.0f);
	// Past the last tick the track blends back into its first: on round the turn, not back the way it came.
	Track.Sample(2 * FGearPoseTrack::TickPhase + FGearPoseTrack::TickPhase / 2, Pose);
	CHECK(Apart(TurnGiven(Pose), RotationXyz({0xD00, 0, 0})) <= 6);
	CHECK(Pose[1].Position[0] == 40.0f);
	// A phase past the cycle, or before it, goes round.
	FGearPose Again;
	Track.Sample(3 * FGearPoseTrack::TickPhase + FGearPoseTrack::TickPhase / 2, Again);
	Track.Sample(FGearPoseTrack::TickPhase / 2, Pose);
	CHECK(Again == Pose);
	Track.Sample(-5 * FGearPoseTrack::TickPhase / 2, Again);
	CHECK(Again == Pose);
	FGearPoseTrack Empty;
	Empty.Sample(0, Pose);
	CHECK(Pose.empty());
}

TEST_CASE("Every party gear's battle model stands, moves, dashes and leaps by records played out, its opening read to where the battle places it",
	"[Gears][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Objects = FBattleObjectDisc::Load(Disc->Image(), *Index);
	REQUIRE(Objects);
	for (int32 Gear = 0; Gear < PartyGears; ++Gear)
	{
		INFO("gear " << Gear);
		auto Files = Objects->ReadGear(Gear, 0);
		REQUIRE(Files);
		const std::vector<FGearSkeletonEntry>& Skeleton = Files->Model.Skeleton;
		const FBattleGearOpening Opening = FBattleGearOpening::Read(Files->Motion);
		CHECK(Opening.bComplete);
		for (const FBattleGearOpening::FShownBone& Bone : Opening.Shown)
		{
			CHECK(Bone.Bone > 0);
			CHECK(static_cast<std::size_t>(Bone.Bone) <= Skeleton.size());
		}

		const auto Rest = ReadGearPose(Skeleton, Files->Motion, RestRecord);
		REQUIRE(Rest);
		REQUIRE(Rest->size() == Skeleton.size() + 1);
		const auto Move = BakeGearRecord(Skeleton, Files->Motion, MoveRecord, RestRecord);
		const auto Dash = BakeGearRecord(Skeleton, Files->Motion, DashRecord, RestRecord);
		const auto Leap = BakeGearRecord(Skeleton, Files->Motion, LeapRecord, RestRecord);
		REQUIRE(Move);
		REQUIRE(Dash);
		REQUIRE(Leap);
		// The move covers ground each cycle; the dash is a pose; the leap a pose or a short record.
		CHECK(Move->Ticks.size() >= 4);
		CHECK(Move->Ticks.size() <= 30);
		CHECK(Move->Stride > 2000);
		CHECK(Dash->Ticks.size() == 1);
		CHECK(Dash->Stride == 0);
		CHECK_FALSE(Leap->Ticks.empty());
		// The loop's last tick leads back into its first: no bone further from it than twice what it moves between two ticks elsewhere.
		std::pair<float, float> Most{0.0f, 0.0f};
		for (std::size_t Tick = 0; Tick + 1 < Move->Ticks.size(); ++Tick)
		{
			const std::pair<float, float> Step = Apart(Move->Ticks[Tick], Move->Ticks[Tick + 1]);
			Most = {std::max(Most.first, Step.first), std::max(Most.second, Step.second)};
		}
		const std::pair<float, float> Round = Apart(Move->Ticks.back(), Move->Ticks.front());
		INFO("round " << Round.first << ", " << Round.second << "; most " << Most.first << ", " << Most.second);
		CHECK(Round.first <= 2.0f * Most.first + 0.02f);
		CHECK(Round.second <= 2.0f * Most.second + 16.0f);
	}
	// A record past a gear's bank is none.
	auto Weltall = Objects->ReadGear(0, 0);
	REQUIRE(Weltall);
	CHECK_FALSE(BakeGearRecord(Weltall->Model.Skeleton, Weltall->Motion, 63, RestRecord));
	CHECK_FALSE(ReadGearPose(Weltall->Model.Skeleton, Weltall->Motion, 63));
}

TEST_CASE("A bone turned about an axis of the model carries the bones below it round its own place", "[Gears]")
{
	// The first bone off to one side, the second hanging below it.
	FGearPose Pose = PoseTurned({0, 0, 0}, {100, 0, 0});
	Pose[2].Position = {0.0f, 200.0f, 0.0f};
	const std::array<float, 3> Hanging = GearBoneModelPlace(Pose, TwoBones, 2);
	CHECK(Hanging[0] == Catch::Approx(100.0f));
	CHECK(Hanging[1] == Catch::Approx(200.0f));

	// A quarter turn about the model's z at the first bone: what hung below it now lies across, the bone itself stays.
	const float Half = 0.78539816f;
	TurnGearBoneInModel(Pose, TwoBones, 1, {0.0f, 0.0f, std::sin(Half), std::cos(Half)});
	const std::array<float, 3> Out = GearBoneModelPlace(Pose, TwoBones, 2);
	CHECK(Out[0] == Catch::Approx(-100.0f).margin(0.01));
	CHECK(Out[1] == Catch::Approx(0.0f).margin(0.01));
	CHECK(Out[2] == Catch::Approx(0.0f).margin(0.01));
	const std::array<float, 3> Pivot = GearBoneModelPlace(Pose, TwoBones, 1);
	CHECK(Pivot[0] == Catch::Approx(100.0f));

	// Turned the same way below a bone already turned, the axis is still the model's: the second bone, a quarter turn
	// about y first, swings across the same way.
	FGearPose Below = PoseTurned({0, 0x400, 0});
	Below[2].Position = {0.0f, 200.0f, 0.0f};
	TurnGearBoneInModel(Below, TwoBones, 2, {0.0f, 0.0f, std::sin(Half), std::cos(Half)});
	std::vector<FGearSkeletonEntry> ThreeBones = TwoBones;
	ThreeBones.push_back({0xFFFF, 1});
	Below.push_back({});
	Below[3].Position = {0.0f, 200.0f, 0.0f};
	const std::array<float, 3> Tip = GearBoneModelPlace(Below, ThreeBones, 3);
	const std::array<float, 3> Elbow = GearBoneModelPlace(Below, ThreeBones, 2);
	CHECK(Tip[0] - Elbow[0] == Catch::Approx(-200.0f).margin(0.05));
	CHECK(Tip[1] - Elbow[1] == Catch::Approx(0.0f).margin(0.05));
}

TEST_CASE("A limb of one pose copied onto another carries the bones below it and leaves the rest", "[Gears]")
{
	// The first bone hangs from the model; an arm of two bones and a leg hang from it.
	const std::vector<FGearSkeletonEntry> Body = {{0xFFFF, 0xFFFF}, {0xFFFF, 0}, {0xFFFF, 1}, {0xFFFF, 0}};
	FGearPose Standing(Body.size() + 1);
	FGearPose Lunging(Body.size() + 1);
	for (std::size_t Bone = 0; Bone < Standing.size(); ++Bone)
	{
		Standing[Bone].Position = {static_cast<float>(Bone), 0.0f, 0.0f};
		Lunging[Bone].Position = {0.0f, static_cast<float>(Bone), 0.0f};
		Lunging[Bone].Rotation = {0.0f, std::sin(0.5f), 0.0f, std::cos(0.5f)};
	}
	FGearPose Mixed = Lunging;
	CopyGearLimb(Standing, Body, 2, Mixed);
	CHECK(Mixed[0] == Lunging[0]);
	CHECK(Mixed[1] == Lunging[1]);
	CHECK(Mixed[2] == Standing[2]);
	CHECK(Mixed[3] == Standing[3]);
	CHECK(Mixed[4] == Lunging[4]);

	// A bone past the skeleton, or poses of other sizes, change nothing.
	FGearPose Unchanged = Lunging;
	CopyGearLimb(Standing, Body, 5, Unchanged);
	CopyGearLimb(FGearPose(2), Body, 2, Unchanged);
	CHECK(Unchanged == Lunging);
}
