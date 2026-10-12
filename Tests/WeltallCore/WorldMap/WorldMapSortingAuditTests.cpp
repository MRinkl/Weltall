#include "Support/WorldMapSortingAudit.h"

#include <catch_amalgamated.hpp>

#include <utility>

using namespace Weltall;
using namespace Weltall::Testing;

namespace
{
	/** A world map frame of flat untextured triangles for the audit, in the order they are added (the first drawn first). */
	struct FWorldMapPiecesFrame
	{
		FWorldMapPiecesFrame()
		{
			Frame.ScreenCenterX = 160;
			Frame.ScreenCenterY = 140;
			Frame.ProjectionDistance = 256;
		}

		/**
		 * A triangle 300 view units across at Depth, about the projection's centre: facing the eye, or turned away; drawn
		 * whichever way it faces, or only facing the eye (one-sided).
		 */
		uint32 Add(EWorldMapLayer Layer, float Depth, bool bFacing = true, bool bOneSided = false)
		{
			const uint32 Number = static_cast<uint32>(Frame.Polygons.size());
			FWorldMapPolygon& Polygon = Frame.Polygons.emplace_back();
			Polygon.Layer = Layer;
			Polygon.CornerCount = 3;
			Polygon.View = {{{-150.0f, -150.0f, Depth}, {150.0f, -150.0f, Depth}, {0.0f, 150.0f, Depth}, {}}};
			if (!bFacing)
			{
				std::swap(Polygon.View[1], Polygon.View[2]);
			}
			Polygon.bOneSided = bOneSided;
			// Each its own grey, so that the pictures in the two orders differ where the triangles' places do.
			const uint8 Grey = static_cast<uint8>(80 + 60 * Number);
			Polygon.Colors[0] = {Grey, Grey, Grey};
			Polygon.Bucket = static_cast<uint16>(Depth / 16.0f);
			return Number;
		}

		/** As Add, semi-transparent: averaged with what lies under it. */
		uint32 AddBlend(EWorldMapLayer Layer, float Depth)
		{
			const uint32 Number = Add(Layer, Depth);
			Frame.Polygons[Number].bSemiTransparent = true;
			return Number;
		}

		/** A quad of the sky behind everything, 8000 view units across at 4000. */
		void AddSky()
		{
			FWorldMapPolygon& Polygon = Frame.Polygons.emplace_back();
			Polygon.Layer = EWorldMapLayer::Sky;
			Polygon.CornerCount = 4;
			Polygon.View = {{{-4000.0f, -4000.0f, 4000.0f}, {4000.0f, -4000.0f, 4000.0f}, {-4000.0f, 4000.0f, 4000.0f}, {4000.0f, 4000.0f, 4000.0f}}};
			Polygon.Colors[0] = {0x40, 0x60, 0xF0};
		}

		FSortingFrameAudit Audit(EFieldSorting Sorting = EFieldSorting::DepthBufferWithScenery) const
		{
			FWorldMapSortingAuditOptions Options;
			Options.Width = 320;
			Options.Scale = 1;
			Options.Sorting = Sorting;
			return AuditWorldMapSorting(Frame, Vram, Options);
		}

		FWorldMapFrame Frame;
		FVram Vram;
	};
}

TEST_CASE("The world map's sorting audit finds what the order draws over something nearer, but in the scenery sorted by depth", "[WorldMap][SortingAudit]")
{
	SECTION("the nearer drawn last: as the depth has it")
	{
		FWorldMapPiecesFrame Pieces;
		Pieces.Add(EWorldMapLayer::Terrain, 2000.0f);
		Pieces.Add(EWorldMapLayer::Terrain, 1000.0f);
		CHECK(Pieces.Audit(EFieldSorting::PaintersOrder).Conflicts.empty());
	}
	SECTION("the farther drawn last: over the nearer, as the world map keeps the order but for its sprites, and its scenery when sorted too")
	{
		FWorldMapPiecesFrame Pieces;
		const uint32 Near = Pieces.Add(EWorldMapLayer::Terrain, 1000.0f);
		const uint32 Far = Pieces.Add(EWorldMapLayer::Model, 2000.0f);
		CHECK(Pieces.Audit().Conflicts.empty());
		for (const EFieldSorting Sorting : {EFieldSorting::PaintersOrder, EFieldSorting::DepthBuffer})
		{
			const FSortingFrameAudit Audit = Pieces.Audit(Sorting);
			REQUIRE(Audit.Conflicts.size() == 1);
			CHECK(Audit.Conflicts[0].Over == Far);
			CHECK(Audit.Conflicts[0].Under == Near);
			CHECK(Audit.Conflicts[0].Gap == Catch::Approx(1000.0f));
			// The far triangle's whole picture lies inside the near one's: about half of 300 x 300 units at 2000, H 256.
			CHECK(Audit.Conflicts[0].Pixels == Catch::Approx(0.5 * 38.4 * 38.4).epsilon(0.15));
			CHECK(Audit.Conflicts[0].Seen == Audit.Conflicts[0].Pixels);
			CHECK(SortingClass(Audit.Pieces[Far].Kind, Audit.Pieces[Near].Kind) == "place model over ground");
		}
	}
}

TEST_CASE("The world map's sorting audit counts the party's ship against the scenery, which keeps the game's order for it", "[WorldMap][SortingAudit]")
{
	// The ship's hull rides under the sea's surface: the order draws it after the nearer sea, over it.
	FWorldMapPiecesFrame Pieces;
	const uint32 Sea = Pieces.Add(EWorldMapLayer::Terrain, 1000.0f);
	const uint32 Hull = Pieces.Add(EWorldMapLayer::Model, 2000.0f);
	Pieces.Frame.Polygons[Hull].bVehicle = true;
	for (const EFieldSorting Sorting : {EFieldSorting::DepthBufferWithScenery, EFieldSorting::DepthBuffer})
	{
		const FSortingFrameAudit Audit = Pieces.Audit(Sorting);
		REQUIRE(Audit.Conflicts.size() == 1);
		CHECK(Audit.Conflicts[0].Over == Hull);
		CHECK(Audit.Conflicts[0].Under == Sea);
		CHECK(SortingClass(Audit.Pieces[Hull].Kind, Audit.Pieces[Sea].Kind) == "ship over ground");
		CHECK(DescribeWorldMapSortingPiece(Pieces.Frame, Hull, Audit.Pieces[Hull]).rfind("ship object", 0) == 0);
	}
}

TEST_CASE("The world map's sorting audit leaves a sprite to the port's depth sorting", "[WorldMap][SortingAudit]")
{
	FWorldMapPiecesFrame Pieces;
	const uint32 Sprite = Pieces.Add(EWorldMapLayer::Sprite, 1000.0f);
	const uint32 Tree = Pieces.Add(EWorldMapLayer::Billboard, 2000.0f);
	const FSortingFrameAudit Painted = Pieces.Audit(EFieldSorting::PaintersOrder);
	REQUIRE(Painted.Conflicts.size() == 1);
	CHECK(SortingClass(Painted.Pieces[Tree].Kind, Painted.Pieces[Sprite].Kind) == "sprite and tree");
	CHECK(Pieces.Audit(EFieldSorting::DepthBuffer).Conflicts.empty());
	CHECK(Pieces.Audit().Conflicts.empty());
}

TEST_CASE("The world map's sorting audit draws a one-sided polygon only where it faces the eye", "[WorldMap][SortingAudit]")
{
	SECTION("turned away and one-sided: not drawn, so nothing is drawn over it")
	{
		FWorldMapPiecesFrame Pieces;
		Pieces.Add(EWorldMapLayer::Terrain, 1000.0f, false, true);
		Pieces.Add(EWorldMapLayer::Terrain, 2000.0f);
		CHECK(Pieces.Audit(EFieldSorting::PaintersOrder).Conflicts.empty());
	}
	SECTION("turned away and drawn whichever way it faces")
	{
		FWorldMapPiecesFrame Pieces;
		Pieces.Add(EWorldMapLayer::Terrain, 1000.0f, false);
		Pieces.Add(EWorldMapLayer::Terrain, 2000.0f);
		CHECK(Pieces.Audit(EFieldSorting::PaintersOrder).Conflicts.size() == 1);
	}
	SECTION("facing the eye and one-sided: drawn")
	{
		FWorldMapPiecesFrame Pieces;
		Pieces.Add(EWorldMapLayer::Terrain, 1000.0f, true, true);
		Pieces.Add(EWorldMapLayer::Terrain, 2000.0f);
		CHECK(Pieces.Audit(EFieldSorting::PaintersOrder).Conflicts.size() == 1);
	}
}

TEST_CASE("The world map's sorting audit leaves a cloud behind the scenery to the port where it is sorted too, never a particle", "[WorldMap][SortingAudit]")
{
	SECTION("a cloud before the ground that is drawn after it: covered in the order, as the port draws it")
	{
		FWorldMapPiecesFrame Pieces;
		const uint32 Cloud = Pieces.AddBlend(EWorldMapLayer::Cloud, 1000.0f);
		const uint32 Ground = Pieces.Add(EWorldMapLayer::Terrain, 2000.0f);
		const FSortingFrameAudit Painted = Pieces.Audit(EFieldSorting::PaintersOrder);
		REQUIRE(Painted.Conflicts.size() == 1);
		CHECK(Painted.Conflicts[0].Over == Ground);
		CHECK(Painted.Conflicts[0].Under == Cloud);
		CHECK(SortingClass(Painted.Pieces[Ground].Kind, Painted.Pieces[Cloud].Kind) == "ground over cloud");
		CHECK(Pieces.Audit().Conflicts.size() == 1);
	}
	SECTION("a cloud drawn over nearer ground: against its depth")
	{
		FWorldMapPiecesFrame Pieces;
		Pieces.Add(EWorldMapLayer::Terrain, 1000.0f);
		Pieces.AddBlend(EWorldMapLayer::Cloud, 2000.0f);
		CHECK(Pieces.Audit(EFieldSorting::PaintersOrder).Conflicts.size() == 1);
		CHECK(Pieces.Audit().Conflicts.empty());
	}
	SECTION("a particle keeps the order: an effect's flat quad is not cut by the ground")
	{
		FWorldMapPiecesFrame Pieces;
		Pieces.Add(EWorldMapLayer::Terrain, 1000.0f);
		Pieces.AddBlend(EWorldMapLayer::Particle, 2000.0f);
		CHECK(Pieces.Audit().Conflicts.size() == 1);
	}
}

TEST_CASE("The world map's sorting audit draws the sky behind everything and never counts it", "[WorldMap][SortingAudit]")
{
	FWorldMapPiecesFrame Pieces;
	Pieces.AddSky();
	Pieces.Add(EWorldMapLayer::Terrain, 1000.0f);
	const FSortingFrameAudit Audit = Pieces.Audit(EFieldSorting::PaintersOrder);
	CHECK(Audit.Conflicts.empty());
	// The top left corner shows the sky in both pictures, the projection's centre the ground.
	constexpr uint32 Sky = 0x4060F0;
	CHECK(Audit.Picture[0] == Sky);
	CHECK(Audit.DepthPicture[0] == Sky);
	const std::size_t Centre = static_cast<std::size_t>(140 * Audit.PictureWidth + Audit.PictureWidth / 2);
	CHECK(Audit.Picture[Centre] == 0x8C8C8C);
	CHECK(Audit.DepthPicture[Centre] == 0x8C8C8C);
}
