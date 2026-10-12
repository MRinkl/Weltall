#include "Field/FieldMap.h"
#include "Field/FieldScene.h"
#include "Field/FieldSceneJoins.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/FieldPlaySession.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

using namespace Weltall;

// The joins of real maps' scenery: the doors of Lahan's houses in their frames, what every map's joins keep to, and the
// doors standing as built in a played map.

namespace
{
	/** Map 9, Lahan's tavern: its two doors (actors 19 and 20, a quad each) in the wall that holds them (actor 35). */
	constexpr int32 TavernMap = 9;
	constexpr uint16 TavernDoors[2] = {19, 20};
	constexpr uint16 TavernWall = 35;
	/** Map 6, a house whose door (actor 10) is two quads, one above the other, beside jambs of a single edge (actor 34). */
	constexpr int32 SplitDoorMap = 6;
	constexpr uint16 SplitDoor = 10;
	constexpr uint16 SplitDoorWall = 34;
	constexpr int32 SettledTicks = 40;

	FGameFileIndex ReadIndex(const FIso9660& Disc)
	{
		auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		return std::move(*Index);
	}

	FFieldScene SceneOf(const FIso9660& Disc, const FGameFileIndex& Index, int32 Number)
	{
		auto Map = LoadFieldMap(Disc.Image(), Index, Number);
		INFO("map " << Number << ": " << (Map ? std::string() : Map.Error().Message));
		REQUIRE(Map);
		auto Scene = BuildFieldScene(*Map);
		INFO("map " << Number << ": " << (Scene ? std::string() : Scene.Error().Message));
		REQUIRE(Scene);
		return std::move(*Scene);
	}

	/** A point of a model's space in the world, as the map's data places the model. */
	std::array<double, 3> InWorld(const FFieldSceneObject& Object, const std::array<double, 3>& Local)
	{
		std::array<double, 3> World{};
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			World[Row] = (Object.Built.M[Row][0] * Local[0] + Object.Built.M[Row][1] * Local[1] + Object.Built.M[Row][2] * Local[2]) / FGteMatrix::One
				+ Object.Built.T[Row];
		}
		return World;
	}

	/** How far a join draws its vertex from where the map's data puts it. */
	double Moved(const FFieldScene& Scene, const FFieldSceneJoin& Join)
	{
		const FGteVector& Own = Scene.PartOf(Scene.Objects[Join.Object]).Vertices[Join.Vertex];
		const std::array<double, 3> From = InWorld(Scene.Objects[Join.Object], {static_cast<double>(Own[0]), static_cast<double>(Own[1]), static_cast<double>(Own[2])});
		const std::array<double, 3> To = InWorld(Scene.Objects[Join.Anchor], {Join.Point[0], Join.Point[1], Join.Point[2]});
		return std::sqrt((From[0] - To[0]) * (From[0] - To[0]) + (From[1] - To[1]) * (From[1] - To[1]) + (From[2] - To[2]) * (From[2] - To[2]));
	}

	/** The joins of the object an actor's model is. */
	std::vector<FFieldSceneJoin> JoinsOfActor(const FFieldScene& Scene, uint16 Actor)
	{
		std::vector<FFieldSceneJoin> Joins;
		for (const FFieldSceneJoin& Join : Scene.Joins)
		{
			if (Scene.Objects[Join.Object].Actor == Actor)
			{
				Joins.push_back(Join);
			}
		}
		return Joins;
	}
}

TEST_CASE("The doors of Lahan's houses are drawn in their frames", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FGameFileIndex Index = ReadIndex(*Disc);

	// The tavern's doors stand 0.4 to 0.9 of a unit off the corners of their doorways.
	const FFieldScene Tavern = SceneOf(*Disc, Index, TavernMap);
	for (const uint16 Door : TavernDoors)
	{
		const std::vector<FFieldSceneJoin> Joins = JoinsOfActor(Tavern, Door);
		REQUIRE(Joins.size() == 4);
		for (const FFieldSceneJoin& Join : Joins)
		{
			CHECK(Tavern.Objects[Join.Anchor].Actor == TavernWall);
			// A corner of the wall's own: whole units.
			CHECK(Join.Point[0] == std::round(Join.Point[0]));
			CHECK(Join.Point[2] == std::round(Join.Point[2]));
			CHECK(Moved(Tavern, Join) > 0.3);
			CHECK(Moved(Tavern, Join) < 0.9);
		}
	}
	CHECK(Tavern.Joins.size() == 8);
	CHECK(JoinsOfActor(Tavern, TavernWall).empty());

	// A door of two quads: the corners where they meet go onto the jambs' edges, 22 units up them.
	const FFieldScene House = SceneOf(*Disc, Index, SplitDoorMap);
	const std::vector<FFieldSceneJoin> Joins = JoinsOfActor(House, SplitDoor);
	REQUIRE(Joins.size() == 6);
	int32 OnEdges = 0;
	for (const FFieldSceneJoin& Join : Joins)
	{
		CHECK(House.Objects[Join.Anchor].Actor == SplitDoorWall);
		CHECK(Moved(House, Join) == Catch::Approx(0.9).margin(0.05));
		OnEdges += Join.Point[1] == -22.0f ? 1 : 0;
	}
	CHECK(OnEdges == 2);
}

TEST_CASE("Every join of the first hundred maps fits a model to another within the rule's reach", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FGameFileIndex Index = ReadIndex(*Disc);
	int32 Joined = 0;
	int32 MapsJoined = 0;
	for (int32 Number = 0; Number < 100; ++Number)
	{
		auto Map = LoadFieldMap(Disc->Image(), Index, Number);
		REQUIRE(Map);
		auto Scene = BuildFieldScene(*Map);
		if (!Scene)
		{
			continue;
		}
		INFO("map " << Number);
		MapsJoined += Scene->Joins.empty() ? 0 : 1;
		for (std::size_t At = 0; At < Scene->Joins.size(); ++At)
		{
			const FFieldSceneJoin& Join = Scene->Joins[At];
			++Joined;
			REQUIRE(Join.Object < Scene->Objects.size());
			REQUIRE(Join.Anchor < Scene->Objects.size());
			REQUIRE(Join.Object != Join.Anchor);
			const FFieldSceneObject& Fitted = Scene->Objects[Join.Object];
			const FFieldSceneObject& Kept = Scene->Objects[Join.Anchor];
			REQUIRE(Join.Vertex < Scene->PartOf(Fitted).Vertices.size());
			if (At > 0)
			{
				const FFieldSceneJoin& Before = Scene->Joins[At - 1];
				REQUIRE(std::pair(Before.Object, Before.Vertex) < std::pair(Join.Object, Join.Vertex));
			}
			REQUIRE(Scene->JoinOf(Join.Object, Join.Vertex) == &Join);
			// Neither turns to the screen nor moves its vertices, and no vertex goes farther than two seams' reach.
			CHECK(Fitted.MatrixMode == EFieldMatrixMode::Placed);
			CHECK(Kept.MatrixMode == EFieldMatrixMode::Placed);
			CHECK_FALSE(Scene->PartOf(Fitted).Mime.has_value());
			CHECK_FALSE(Scene->PartOf(Kept).Mime.has_value());
			CHECK(Moved(*Scene, Join) < 3.0);
			// What moves nothing is no join: the two stand under different placements, or the vertex moves.
			CHECK((Moved(*Scene, Join) > 1.0e-3 || !(Fitted.Built == Kept.Built)));
		}
	}
	CHECK(MapsJoined > 50);
	CHECK(Joined > 1000);
}

TEST_CASE("In a played map the doors stand as the map's data places them", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Testing::EnterFieldMap(*Disc, TavernMap);
	Session->Run(SettledTicks);
	const FFieldScene& Scene = Session->Play->Drawing().Scene();
	REQUIRE(Scene.Joins.size() == 8);
	REQUIRE(Session->Frame.Scenery.Objects.size() == Scene.Objects.size());
	// The doors' scripts give them a hidden sprite and leave them where the map has them: the joins hold.
	for (const FFieldSceneJoin& Join : Scene.Joins)
	{
		CHECK(Session->Frame.Scenery.Objects[Join.Object].bAsBuilt);
		CHECK(Session->Frame.Scenery.Objects[Join.Anchor].bAsBuilt);
	}
	int32 AsBuilt = 0;
	for (const FFieldObjectView& Object : Session->Frame.Scenery.Objects)
	{
		AsBuilt += Object.bAsBuilt ? 1 : 0;
	}
	// Every model of the tavern does, those placed off the map's origin too: the game keeps their places as low halves.
	CHECK(AsBuilt == static_cast<int32>(Scene.Objects.size()));
}
