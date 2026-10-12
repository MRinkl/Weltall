#include "Game/WorldMapProgram.h"
#include "Graphics/AffineBlend.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "WorldMap/WorldMapContext.h"
#include "WorldMap/WorldMapFrameBlend.h"
#include "WorldMap/WorldMapRuntime.h"
#include "WorldMap/WorldMapScene.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

using namespace Weltall;

// The world map's trees by Blackmoon Forest, where they stand closer than they are wide: which of two that overlap
// is drawn over the other, and where each stands, as the camera turns and the party walks.

namespace
{
	constexpr FPadState NoButtons{};
	constexpr FPadState UpHeld{FPadState::Bit(EPadButton::Up)};
	/** Era 1 (the story's value 30), at the scene's first entrance, facing north-west: the forest fills the view. */
	constexpr uint16 Story = 30;
	constexpr uint16 Place = 0x401;
	constexpr uint16 Entrance = 1;
	constexpr uint16 CameraYaw = 0xC00;
	constexpr uint16 Scene = 1;
	/** The ticks of the scene's own fade-in. */
	constexpr int32 FadeInTicks = 62;
	/** Half the original's screen, widened to 16:9, in its pixels. */
	constexpr float HalfWidth = 192.0f;
	constexpr float HalfHeight = 108.0f;

	struct FForest
	{
		std::unique_ptr<Testing::FGameLoopSession> Game;
		std::unique_ptr<FWorldMapProgram> Program;

		const FWorldMapFrame& Frame() const { return *Program->LatestFrame(); }

		void Run(FPadState Held, int32 Ticks = 1)
		{
			for (int32 Tick = 0; Tick < Ticks; ++Tick)
			{
				std::vector<FPadState> Vblanks(static_cast<std::size_t>(Program->VblanksPerFrame()), Held);
				const std::optional<FError> Error = Program->Frame(Vblanks);
				INFO((Error ? Error->Message : std::string()));
				REQUIRE_FALSE(Error);
			}
		}

		void Turn(float Degrees)
		{
			FFieldViewInput Input;
			Input.Turn = Degrees;
			Program->AddViewInput(Input);
			Run(NoButtons);
		}
	};

	/** The world map entered by the forest under the fixes, faded in, the party standing where it arrives. */
	FForest EnterForest(const FIso9660& Disc, const FWorldMapOptions& Fixes)
	{
		FForest Forest;
		Forest.Game = Testing::FGameLoopSession::Start(Disc);
		FGameSession& Session = *Forest.Game->Session;
		FPlayerSettings Settings = Session.Settings();
		Settings.Fixes.WorldMap = Fixes;
		Session.SetSettings(Settings);
		Session.Game().SetSavedEventVariable(0, Story);
		Session.Game().SetWorldMapPlace(Place, Entrance, CameraYaw, Scene);
		auto Started = FWorldMapProgram::Start(Session);
		INFO((Started ? std::string() : Started.Error().Message));
		REQUIRE(Started);
		Forest.Program = std::move(*Started);
		Forest.Program->SetCulling(EWorldMapCulling::InRange);
		Forest.Run(NoButtons, FWorldMapProgram::EntryFadeFrames + FadeInTicks + 2);
		REQUIRE(Forest.Program->LatestFrame() != nullptr);
		return Forest;
	}

	/** A tree of a frame: its anchor's key, its place in the frame's drawing order, its picture on the screen and its depth. */
	struct FDrawnTree
	{
		uint32 Key = 0;
		int32 Order = 0;
		float Left = 0.0f;
		float Right = 0.0f;
		float Top = 0.0f;
		float Bottom = 0.0f;
		float Depth = 0.0f;
	};

	std::vector<FDrawnTree> TreesOnScreen(const FWorldMapFrame& Frame)
	{
		std::vector<FDrawnTree> Trees;
		const float Distance = static_cast<float>(Frame.ProjectionDistance);
		for (std::size_t Index = 0; Index < Frame.Polygons.size(); ++Index)
		{
			const FWorldMapPolygon& Polygon = Frame.Polygons[Index];
			if (Polygon.Layer != EWorldMapLayer::Billboard || Polygon.View[0][2] < 1.0f)
			{
				continue;
			}
			FDrawnTree Tree;
			Tree.Key = Frame.Anchors[Polygon.Anchor].Key;
			Tree.Order = static_cast<int32>(Index);
			Tree.Depth = Polygon.View[0][2];
			Tree.Left = Polygon.View[0][0] * Distance / Tree.Depth;
			Tree.Right = Polygon.View[1][0] * Distance / Tree.Depth;
			Tree.Top = Polygon.View[0][1] * Distance / Tree.Depth;
			Tree.Bottom = Polygon.View[2][1] * Distance / Tree.Depth;
			if (Tree.Right > -HalfWidth && Tree.Left < HalfWidth && Tree.Bottom > -HalfHeight && Tree.Top < HalfHeight)
			{
				Trees.push_back(Tree);
			}
		}
		return Trees;
	}

	bool Overlap(const FDrawnTree& A, const FDrawnTree& B)
	{
		// By a quarter of a tree each way: the quads' rims are clear texels.
		const float Across = std::min(A.Right, B.Right) - std::max(A.Left, B.Left);
		const float Down = std::min(A.Bottom, B.Bottom) - std::max(A.Top, B.Top);
		return Across > (A.Right - A.Left) / 4.0f && Down > (A.Bottom - A.Top) / 4.0f;
	}

	/** Which tree of each overlapping pair is drawn over the other, tick after tick: how often each pair changed places. */
	class FPlacesChanged
	{
	public:
		void Watch(const std::vector<FDrawnTree>& Trees)
		{
			std::map<uint64, bool> Now;
			for (std::size_t First = 0; First < Trees.size(); ++First)
			{
				for (std::size_t Second = First + 1; Second < Trees.size(); ++Second)
				{
					if (!Overlap(Trees[First], Trees[Second]))
					{
						continue;
					}
					const bool bSwap = Trees[First].Key > Trees[Second].Key;
					const FDrawnTree& A = Trees[bSwap ? Second : First];
					const FDrawnTree& B = Trees[bSwap ? First : Second];
					const uint64 Pair = static_cast<uint64>(A.Key) << 32 | B.Key;
					const bool bFirstOver = A.Order > B.Order;
					Now.emplace(Pair, bFirstOver);
					if (const auto Found = Before.find(Pair); Found != Before.end() && Found->second != bFirstOver)
					{
						++Changes[Pair];
					}
				}
			}
			Before = std::move(Now);
		}

		int32 Pairs() const { return static_cast<int32>(Changes.size()); }

		/** The pairs that changed places, changed back and changed again. */
		int32 BackAndForth() const
		{
			return static_cast<int32>(std::count_if(Changes.begin(), Changes.end(), [](const auto& Pair) { return Pair.second > 2; }));
		}

	private:
		std::map<uint64, bool> Before;
		std::map<uint64, int32> Changes;
	};

	/** The view turned a sixth of a turn, a degree a tick, then a walk into the view of two seconds. */
	constexpr int32 TurnTicks = 60;
	constexpr int32 WalkTicks = 60;

	FPlacesChanged PlayThrough(FForest& Forest, bool& bFarthestFirst, std::size_t& MostTrees)
	{
		FPlacesChanged Places;
		bFarthestFirst = true;
		MostTrees = 0;
		for (int32 Tick = 0; Tick < TurnTicks + WalkTicks; ++Tick)
		{
			if (Tick < TurnTicks)
			{
				Forest.Turn(-1.0f);
			}
			else
			{
				Forest.Run(UpHeld);
			}
			const std::vector<FDrawnTree> Trees = TreesOnScreen(Forest.Frame());
			bFarthestFirst = bFarthestFirst
				&& std::is_sorted(Trees.begin(), Trees.end(), [](const FDrawnTree& A, const FDrawnTree& B) { return A.Depth > B.Depth; });
			MostTrees = std::max(MostTrees, Trees.size());
			Places.Watch(Trees);
		}
		return Places;
	}

	std::array<float, 3> FootOf(const FWorldMapPolygon& Tree)
	{
		return {(Tree.View[2][0] + Tree.View[3][0]) / 2.0f, (Tree.View[2][1] + Tree.View[3][1]) / 2.0f, (Tree.View[2][2] + Tree.View[3][2]) / 2.0f};
	}

	int32 WrappedFrom(int32 Along, int32 Target, int32 World)
	{
		const int32 Delta = (Along - Target) % World;
		return Delta < -World / 2 ? Delta + World : Delta >= World / 2 ? Delta - World : Delta;
	}
}

TEST_CASE("Overlapping trees are drawn the farther first, where the original's order swaps them back and forth", "[Game][WorldMap][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// The original's ordering table holds sixteen units of depth an entry, the trees of one in their lists' order: of two
	// that overlap the one drawn over the other changes each time the camera's move takes either across an entry's edge.
	FForest AsTheOriginal = EnterForest(*Disc, FWorldMapOptions::Original());
	bool bOriginalFarthestFirst = false;
	std::size_t MostTrees = 0;
	const FPlacesChanged Original = PlayThrough(AsTheOriginal, bOriginalFarthestFirst, MostTrees);
	REQUIRE(MostTrees > 300);
	CHECK_FALSE(bOriginalFarthestFirst);
	CHECK(Original.BackAndForth() > 50);

	// By their depths alone a pair changes places only as one tree passes behind the other: once in a turn this short,
	// and once more at most as the game's own camera tilts over the walk.
	FForest Fixed = EnterForest(*Disc, FWorldMapOptions());
	bool bFarthestFirst = false;
	const FPlacesChanged InDepthOrder = PlayThrough(Fixed, bFarthestFirst, MostTrees);
	INFO("the original's order: " << Original.Pairs() << " pairs changed places, " << Original.BackAndForth() << " back and forth; by depth: "
		<< InDepthOrder.Pairs() << " and " << InDepthOrder.BackAndForth() << "; " << MostTrees << " trees");
	CHECK(bFarthestFirst);
	CHECK(InDepthOrder.BackAndForth() == 0);
	CHECK(InDepthOrder.Pairs() > 0);

	// Nothing else of the frame moves: the same trees, each in the entry the original links it into.
	const auto Entries = [](const FWorldMapFrame& Frame)
	{
		std::map<uint32, uint16> Entry;
		for (const FWorldMapPolygon& Polygon : Frame.Polygons)
		{
			if (Polygon.Layer == EWorldMapLayer::Billboard)
			{
				Entry.emplace(Frame.Anchors[Polygon.Anchor].Key, Polygon.Bucket);
			}
		}
		return Entry;
	};
	CHECK(Entries(Fixed.Frame()) == Entries(AsTheOriginal.Frame()));
	CHECK(Fixed.Frame().Polygons.size() == AsTheOriginal.Frame().Polygons.size());

	// The fix switched off in the menu holds from the map's next tick.
	FPlayerSettings Settings = Fixed.Game->Session->Settings();
	Settings.Fixes.WorldMap.bTreesInDepthOrder = false;
	Fixed.Game->Session->SetSettings(Settings);
	Fixed.Run(NoButtons);
	const std::vector<FDrawnTree> Trees = TreesOnScreen(Fixed.Frame());
	CHECK_FALSE(std::is_sorted(Trees.begin(), Trees.end(), [](const FDrawnTree& A, const FDrawnTree& B) { return A.Depth > B.Depth; }));
}

TEST_CASE("A tree stands where its foot lies to the last bit, and moves there between ticks", "[Game][WorldMap][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FForest Forest = EnterForest(*Disc, FWorldMapOptions());
	const FWorldMapScene& Map = Forest.Program->Runtime().Scene();
	std::vector<int32> Matches;
	std::vector<std::pair<uint32, int32>> Scratch;
	int32 Stood = 0;
	int32 Moved = 0;
	float WholeUnitsOff = 0.0f;
	for (int32 Tick = 0; Tick < 40; ++Tick)
	{
		const FWorldMapFrame Before = Forest.Frame();
		if (Tick < 20)
		{
			Forest.Turn(1.5f);
		}
		else
		{
			Forest.Run(UpHeld);
		}
		const FWorldMapFrame& Frame = Forest.Frame();
		const FAffine Camera = FAffine::From(Frame.Camera);
		MatchWorldMapAnchors(Before, Frame, Matches, Scratch);
		FWorldMapFrameBlend Blend;
		BlendWorldMapFrames(Frame, &Before, Matches, 0.5f, Blend);
		REQUIRE(Blend.bBlended);
		std::map<uint32, std::array<float, 3>> FeetBefore;
		for (const FWorldMapPolygon& Polygon : Before.Polygons)
		{
			if (Polygon.Layer == EWorldMapLayer::Billboard)
			{
				FeetBefore.emplace(Before.Anchors[Polygon.Anchor].Key, FootOf(Polygon));
			}
		}
		for (const FWorldMapPolygon& Polygon : Frame.Polygons)
		{
			if (Polygon.Layer != EWorldMapLayer::Billboard)
			{
				continue;
			}
			// Its place in its block's list through the frame's camera, with nothing dropped.
			const FWorldMapAnchor& Anchor = Frame.Anchors[Polygon.Anchor];
			const FWorldMapBillboard& Tree = Map.Billboards[Anchor.Key >> 16 & 0xFF][Anchor.Key & 0xFFFF];
			const std::array<float, 3> Turned = Camera.Turn({static_cast<float>(WrappedFrom(Tree.X, Frame.TargetX, Frame.WorldUnitsX)),
				static_cast<float>(Tree.Y), static_cast<float>(-WrappedFrom(Tree.Z, Frame.TargetZ, Frame.WorldUnitsZ))});
			const std::array<float, 3> Foot = FootOf(Polygon);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				REQUIRE(std::fabs(Foot[Axis] - (Turned[Axis] + Camera.T[Axis])) < 0.01f);
				WholeUnitsOff = std::max(WholeUnitsOff, std::fabs(Foot[Axis] - static_cast<float>(Anchor.Matrix.T[Axis])));
			}
			++Stood;
			// Half-way between two ticks it is drawn half-way between where the two stand it.
			const auto Then = FeetBefore.find(Anchor.Key);
			if (Then == FeetBefore.end() || Blend.Paired[Polygon.Anchor] == 0)
			{
				continue;
			}
			const std::array<float, 3> Between = MoveViewPoint(Blend.Moves[Polygon.Anchor], Foot);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				REQUIRE(std::fabs(Between[Axis] - (Then->second[Axis] + Foot[Axis]) / 2.0f) < 0.01f);
			}
			++Moved;
		}
	}
	CHECK(Stood > 4000);
	CHECK(Moved > 4000);
	// The GTE's whole units alone would stand a tree up to a unit off, each tree by a part of its own.
	CHECK(WholeUnitsOff > 0.9f);
}
