#include "Field/FieldGearDrawing.h"
#include "Gears/GearPackets.h"
#include "Support/SortingAudit.h"

#include <catch_amalgamated.hpp>

#include <deque>

using namespace Weltall;
using namespace Weltall::Testing;

namespace
{
	constexpr uint8 FlatTriangle = 0x20;
	constexpr uint8 FlatQuad = 0x28;
	constexpr uint8 SemiTransparent = 0x02;
	constexpr uint16 Projection = 256;

	/**
	 * A frame of gears' pieces for the audit, with no scenery: flat triangles facing the eye at their depths, each its
	 * own bone's part, and a gear's floor shadow, in the order they are added (the first drawn first).
	 */
	struct FGearPiecesFrame
	{
		FGearPiecesFrame()
		{
			Frame.bSceneDrawn = true;
			Frame.View.View = FGteMatrix::Identity();
			Frame.View.ProjectionDistance = Projection;
			Frame.Gears.Layout = 1;
		}

		/** A triangle across the screen's centre, Size view units across, at Depth; blended (semi-transparent) when asked. */
		uint32 AddTriangle(int32 Depth, int16 Size = 300, bool bBlended = false)
		{
			const uint8 Code = static_cast<uint8>(bBlended ? FlatTriangle | SemiTransparent : FlatTriangle);
			FMdlPart& Part = Parts.emplace_back();
			const int16 Half = static_cast<int16>(Size / 2);
			Part.Vertices = {{static_cast<int16>(-Half), static_cast<int16>(-Half), 0}, {Half, static_cast<int16>(-Half), 0}, {0, Half, 0}};
			FMdlPrimitive& Triangle = Part.Primitives.emplace_back();
			Triangle.Type = EMdlPrimitiveType::FlatTriangle;
			Triangle.Code = Code;
			Triangle.Vertices = {0, 1, 2, 0};
			FGearBonePackets& Built = Packets.emplace_back();
			Built.Packets.resize(1);
			Built.Packets[0].Code = Code;
			Built.Textures = {{}};
			FGteMatrix Matrix = FGteMatrix::Identity();
			Matrix.T = {0, 0, Depth};
			// Each its own grey, so that the pictures in the two orders differ where the triangles' places do.
			const uint8 Grey = static_cast<uint8>(80 + 60 * Frame.Gears.Polygons.size());
			return Add(&Built, &Part, Matrix, 0, 0, {Grey, Grey, Grey});
		}

		/** A quad of the scenery across the screen's centre, 400 view units across, at Depth: a placed model of its own. */
		uint32 AddScenery(int32 Depth)
		{
			const uint8 Grey = static_cast<uint8>(100 + 40 * Scene.Objects.size());
			FMdlPart& Part = Scene.Models.emplace_back().Parts.emplace_back();
			Part.Vertices = {{-200, -200, 0}, {200, -200, 0}, {-200, 200, 0}, {200, 200, 0}};
			FMdlPrimitive& Quad = Part.Primitives.emplace_back();
			Quad.Type = EMdlPrimitiveType::StaticFlatQuad;
			Quad.Code = FlatQuad;
			Quad.Color = {Grey, Grey, Grey};
			Quad.Vertices = {0, 1, 2, 3};
			FFieldSceneObject& Object = Scene.Objects.emplace_back();
			Object.Model = static_cast<uint16>(Scene.Models.size() - 1);
			Object.Placement = FGteMatrix::Identity();
			Object.Placement.T = {0, 0, Depth};
			Object.Bounds = {{0, 0, 0}, 5000};
			Object.FirstPolygon = static_cast<uint32>(Scene.Polygons.size());
			Object.PolygonCount = 1;
			FFieldScenePolygon& Polygon = Scene.Polygons.emplace_back();
			Polygon.Object = static_cast<uint16>(Scene.Objects.size() - 1);
			Polygon.Packet.Code = FlatQuad;
			Polygon.Packet.Colors[0] = Quad.Color;
			Frame.View.Scene = Frame.View.View;
			Frame.View.ScreenRotation = FGteMatrix::Identity();
			Frame.View.Billboard = FGteMatrix::Identity();
			EvaluateFieldScene(Scene, Frame.View, EFieldCulling::InRange, Frame.Scenery);
			Frame.Order.push_back({static_cast<uint32>(Scene.Polygons.size() - 1), EFieldDrawKind::Scenery});
			return static_cast<uint32>(Frame.Order.size() - 1);
		}

		/** A gear's floor shadow lying 100 view units below the eye, from Near to Far ahead. */
		uint32 AddShadow(int32 Near, int32 Far)
		{
			constexpr int16 Half = 40;
			const int16 Scale = static_cast<int16>((Far - Near) / (2 * Half));
			FGteMatrix Matrix;
			Matrix.M = {{{static_cast<int16>(Scale * FGteMatrix::One), 0, 0}, {0, static_cast<int16>(Scale * FGteMatrix::One), 0},
				{0, 0, static_cast<int16>(Scale * FGteMatrix::One)}}};
			Matrix.T = {0, 100, (Near + Far) / 2};
			return Add(nullptr, nullptr, Matrix, Half, Half, {0x40, 0x40, 0x40});
		}

		FSortingFrameAudit Audit(float Margin = 16.0f, EFieldSorting Sorting = EFieldSorting::DepthBufferWithScenery) const
		{
			FSortingAuditOptions Options;
			Options.Width = 320;
			Options.Scale = 1;
			Options.Margin = Margin;
			Options.Sorting = Sorting;
			return AuditFieldSorting(Frame, Scene, Vram, Options);
		}

		/** As the original's painter's order draws. */
		FSortingFrameAudit Original(float Margin = 16.0f) const { return Audit(Margin, EFieldSorting::PaintersOrder); }

		FFieldFrame Frame;
		FFieldScene Scene;
		FVram Vram;

	private:
		uint32 Add(const FGearBonePackets* Built, const FMdlPart* Part, const FGteMatrix& Matrix, int16 HalfWidth, int16 HalfDepth, FMdlColor Color)
		{
			FFieldGearsFrame& Gears = Frame.Gears;
			const uint32 Index = static_cast<uint32>(Gears.Polygons.size());
			Gears.Items.push_back({0, Built, Part, HalfWidth, HalfDepth, Index, 1});
			FFieldGearObject& Object = Gears.Objects.emplace_back();
			Object.Matrix = Matrix;
			Object.bDrawn = true;
			FFieldGearPolygon& Polygon = Gears.Polygons.emplace_back();
			Polygon.Colors.fill(Color);
			Polygon.bDrawn = true;
			Frame.Order.push_back({Index, EFieldDrawKind::Gear, EFieldGearDraw::Polygon});
			return static_cast<uint32>(Frame.Order.size() - 1);
		}

		// Kept where the frame's layout points at them.
		std::deque<FMdlPart> Parts;
		std::deque<FGearBonePackets> Packets;
	};

	/** Fills the gears' floor shadow's texels: every one blends (its colour subtracted). */
	void FillShadowTexels(FVram& Vram)
	{
		const FTexturePage Page = FTexturePage::FromAttribute(FieldGearShadow::TexturePage);
		const FClutPosition Clut = FClutPosition::FromAttribute(FieldGearShadow::Clut);
		Vram.Upload({{static_cast<int16>(Page.X), static_cast<int16>(Page.Y + FieldGearShadow::FirstRow), 4, 16}, std::vector<uint16>(64, 0x1111)});
		Vram.Upload({{static_cast<int16>(Clut.X + 1), static_cast<int16>(Clut.Y), 1, 1}, {0xFFFF}});
	}
}

TEST_CASE("The sorting audit finds what the order draws over something nearer, and nothing in the depth's own order", "[Field][SortingAudit]")
{
	SECTION("the nearer drawn last: as the depth has it")
	{
		FGearPiecesFrame Pieces;
		Pieces.AddTriangle(2000);
		Pieces.AddTriangle(1000);
		CHECK(Pieces.Original().Conflicts.empty());
	}
	SECTION("the farther drawn last: over the nearer wherever they overlap")
	{
		FGearPiecesFrame Pieces;
		const uint32 Near = Pieces.AddTriangle(1000);
		const uint32 Far = Pieces.AddTriangle(2000);
		const FSortingFrameAudit Audit = Pieces.Original();
		REQUIRE(Audit.Conflicts.size() == 1);
		CHECK(Audit.Conflicts[0].Over == Far);
		CHECK(Audit.Conflicts[0].Under == Near);
		CHECK(Audit.Conflicts[0].Gap == Catch::Approx(1000.0f));
		// The far triangle's whole picture lies inside the near one's: about half of 300 x 300 units at 2000, H 256.
		CHECK(Audit.Conflicts[0].Pixels == Catch::Approx(0.5 * 38.4 * 38.4).epsilon(0.15));
		CHECK(Audit.Conflicts[0].Seen == Audit.Conflicts[0].Pixels);
		CHECK(SortingClass(Audit.Pieces[Far].Kind, Audit.Pieces[Near].Kind) == "gear over gear");
		// The port sorts a gear's polygons by their depth.
		CHECK(Pieces.Audit().Conflicts.empty());
	}
	SECTION("within the margin they count as one surface, as a decal on what it was drawn over")
	{
		FGearPiecesFrame Pieces;
		Pieces.AddTriangle(1000);
		Pieces.AddTriangle(1010);
		CHECK(Pieces.Original().Conflicts.empty());
		CHECK(Pieces.Original(4.0f).Conflicts.size() == 1);
	}
}

TEST_CASE("The sorting audit leaves to the port a model against the scenery, and the scenery against itself when it is sorted too",
	"[Field][SortingAudit]")
{
	SECTION("the scenery drawn after a nearer gear")
	{
		FGearPiecesFrame Pieces;
		const uint32 Gear = Pieces.AddTriangle(1000);
		const uint32 Scenery = Pieces.AddScenery(2000);
		const FSortingFrameAudit Painted = Pieces.Original();
		REQUIRE(Painted.Conflicts.size() == 1);
		CHECK(SortingClass(Painted.Pieces[Painted.Conflicts[0].Over].Kind, Painted.Pieces[Painted.Conflicts[0].Under].Kind) == "scenery over gear");
		CHECK(Painted.Conflicts[0].Over == Scenery);
		CHECK(Painted.Conflicts[0].Under == Gear);
		CHECK(Pieces.Audit(16.0f, EFieldSorting::DepthBuffer).Conflicts.empty());
		CHECK(Pieces.Audit().Conflicts.empty());
	}
	SECTION("the scenery drawn after nearer scenery")
	{
		FGearPiecesFrame Pieces;
		Pieces.AddScenery(1000);
		Pieces.AddScenery(2000);
		CHECK(Pieces.Original().Conflicts.size() == 1);
		CHECK(Pieces.Audit(16.0f, EFieldSorting::DepthBuffer).Conflicts.size() == 1);
		CHECK(Pieces.Audit().Conflicts.empty());
	}
}

TEST_CASE("The sorting audit leaves to the port what it sorts: a blend over a nearer model, never one drawn before what covers it",
	"[Field][SortingAudit]")
{
	SECTION("blended and farther, drawn after the nearer: the port tests it against the depth")
	{
		FGearPiecesFrame Pieces;
		Pieces.AddTriangle(1000);
		Pieces.AddTriangle(2000, 300, true);
		CHECK(Pieces.Original().Conflicts.size() == 1);
		CHECK(Pieces.Audit().Conflicts.empty());
	}
	SECTION("blended and nearer, drawn before the farther: what is drawn after covers it in the port too")
	{
		FGearPiecesFrame Pieces;
		const uint32 Near = Pieces.AddTriangle(1000, 300, true);
		const uint32 Far = Pieces.AddTriangle(2000);
		const FSortingFrameAudit Audit = Pieces.Audit();
		REQUIRE(Audit.Conflicts.size() == 1);
		CHECK(Audit.Conflicts[0].Over == Far);
		CHECK(Audit.Conflicts[0].Under == Near);
	}
}

TEST_CASE("The sorting audit judges a shadow over a gear by its height above its floor, as the port draws it", "[Field][SortingAudit]")
{
	// A triangle 300 units across at 900, its lowest point 150 below the eye, so 50 into the floor 100 below it; the
	// shadow's floor runs from 800 to 1200 under it, linked after it as the original links a shadow by its nearest corner.
	FGearPiecesFrame Pieces;
	FillShadowTexels(Pieces.Vram);
	const uint32 Gear = Pieces.AddTriangle(900);
	const uint32 Shadow = Pieces.AddShadow(800, 1200);

	// The original's order darkens the gear wherever the floor behind it lies more than the margin farther.
	FSortingAuditOptions Original;
	Original.Width = 320;
	Original.Scale = 1;
	Original.Sorting = EFieldSorting::PaintersOrder;
	const FSortingFrameAudit Painted = AuditFieldSorting(Pieces.Frame, Pieces.Scene, Pieces.Vram, Original);
	REQUIRE(Painted.Conflicts.size() == 1);
	CHECK(Painted.Conflicts[0].Over == Shadow);
	CHECK(Painted.Conflicts[0].Under == Gear);
	CHECK(Painted.Conflicts[0].Pixels > 100);
	CHECK(SortingClass(Painted.Pieces[Shadow].Kind, Painted.Pieces[Gear].Kind) == "gear shadow over gear");

	// The port draws a shadow over a gear only where the gear stands no more than 16 units above the floor: within the margin.
	CHECK(Pieces.Audit().Conflicts.empty());
	CHECK(Pieces.Audit(4.0f).Conflicts.size() == 1);
}

TEST_CASE("The sorting audit draws nothing for transparent texels or for a blend that changes nothing", "[Field][SortingAudit]")
{
	FGearPiecesFrame Pieces;
	Pieces.AddTriangle(900);
	Pieces.AddShadow(800, 1200);
	// The shadow's texels left at 0 are transparent: nothing is drawn over the gear.
	CHECK(Pieces.Audit().Conflicts.empty());

	// Black subtracted takes nothing away.
	const FTexturePage Page = FTexturePage::FromAttribute(FieldGearShadow::TexturePage);
	const FClutPosition Clut = FClutPosition::FromAttribute(FieldGearShadow::Clut);
	Pieces.Vram.Upload({{static_cast<int16>(Page.X), static_cast<int16>(Page.Y + FieldGearShadow::FirstRow), 4, 16}, std::vector<uint16>(64, 0x1111)});
	Pieces.Vram.Upload({{static_cast<int16>(Clut.X + 1), static_cast<int16>(Clut.Y), 1, 1}, {0x8000}});
	FSortingAuditOptions Original;
	Original.Width = 320;
	Original.Scale = 1;
	Original.Sorting = EFieldSorting::PaintersOrder;
	CHECK(AuditFieldSorting(Pieces.Frame, Pieces.Scene, Pieces.Vram, Original).Conflicts.empty());
}
