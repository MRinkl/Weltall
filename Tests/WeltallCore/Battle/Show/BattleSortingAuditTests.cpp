#include "Support/BattleSortingAudit.h"

#include "Battle/Show/BattleObjectsFrame.h"
#include "Battle/Show/BattleShowObjects.h"
#include "Gears/GearPackets.h"
#include "Graphics/MdlPackets.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <cstddef>
#include <vector>

using namespace Weltall;
using namespace Weltall::Testing;

namespace
{
	constexpr uint8 Stage = static_cast<uint8>(FBattleObjects::StageSlot);
	/** A flat untextured quad's GPU code, opaque. */
	constexpr uint8 FlatQuadCode = 0x28;
	/** A 4-bit page at VRAM (0, 0) and its CLUT at (0, 256): entry 1 opaque red, entry 2 a semi-transparent grey. */
	constexpr uint16 SpritePage = 0;
	constexpr uint16 SpriteClut = 256 << 6;
	/** A quad's colour code: raw texels; raw and semi-transparent. */
	constexpr uint32 RawQuad = 0x2D000000;
	constexpr uint32 RawBlendedQuad = 0x2F000000;
	constexpr uint32 OpaqueTile = 0x60000000;

	/** A quad standing up to the eye at Depth, from Left to Right across and from Top to Bottom. */
	std::array<FGteVector, 4> Standing(int16 Left, int16 Right, int16 Top, int16 Bottom, int16 Depth)
	{
		return {{{Left, Top, Depth}, {Right, Top, Depth}, {Left, Bottom, Depth}, {Right, Bottom, Depth}}};
	}

	/** A quad lying Height below the eye from Near to Far ahead, Left to Right across: its far corners first. */
	std::array<FGteVector, 4> Lying(int16 Left, int16 Right, int16 Height, int16 Near, int16 Far)
	{
		return {{{Left, Height, Far}, {Right, Height, Far}, {Left, Height, Near}, {Right, Height, Near}}};
	}

	/** A battle frame for the audit, its pieces linked in the order they are added (the first drawn first), seen through the identity view. */
	struct FBattlePiecesFrame
	{
		static constexpr std::size_t Room = 8;

		FBattlePiecesFrame()
		{
			Frame.CentreX = 160;
			Frame.CentreY = 112;
			Frame.ProjectionDistance = 256;
			Frame.View = FGteMatrix::Identity();
			Frame.Vram = &Vram;
			Parts.reserve(Room);
			Packets.reserve(Room);
			for (int16 Row = 0; Row < 16; ++Row)
			{
				Vram.Upload({{0, Row, 4, 1}, {0x1111, 0x1111, 0x2222, 0x2222}});
			}
			Vram.Upload({{0, 256, 3, 1}, {0x0000, 0x001F, 0xA108}});
			// The objects' shadows: their page's texels C0h-FEh, CLUT entry 1 at (48, 460), a semi-transparent grey.
			for (int16 Row = 0xC0; Row <= 0xFE; ++Row)
			{
				Vram.Upload({{944, Row, 16, 1}, std::vector<uint16>(16, 0x1111)});
			}
			Vram.Upload({{48, 460, 2, 1}, {0x0000, 0xA108}});
		}

		/** A flat quad of the object in Slot (the stage's: its scenery). */
		uint32 AddPart(uint8 Slot, const std::array<FGteVector, 4>& Corners, FMdlColor Colour)
		{
			REQUIRE(Parts.size() < Room);
			FMdlPart& Part = Parts.emplace_back();
			Part.Vertices.assign(Corners.begin(), Corners.end());
			FMdlPrimitive& Primitive = Part.Primitives.emplace_back();
			Primitive.Type = EMdlPrimitiveType::FlatQuad;
			Primitive.Vertices = {0, 1, 2, 3};
			FGearBonePackets& Built = Packets.emplace_back();
			Built.Packets.push_back({FlatQuadCode, {Colour, Colour, Colour, Colour}});
			Built.Textures.emplace_back();
			FBattleDrawnPart& Drawn = Objects.Parts.emplace_back();
			Drawn.Matrix = FGteMatrix::Identity();
			Drawn.Part = &Part;
			Drawn.Packets = &Built;
			Drawn.Slot = Slot;
			FBattleDrawnPolygon& Polygon = Objects.Polygons.emplace_back();
			Polygon.Part = static_cast<uint32>(Objects.Parts.size() - 1);
			Polygon.Colors[0] = Colour;
			return LinkObject(EBattleObjectDraw::Polygon, Objects.Polygons.size() - 1);
		}

		/** Slot's floor shadow over the corners (+x, +z), (-x, +z), (+x, -z), (-x, -z). */
		uint32 AddShadow(uint8 Slot, const std::array<FGteVector, 4>& Corners)
		{
			FBattleDrawnShadow& Shadow = Objects.Shadows.emplace_back();
			Shadow.Matrix = FGteMatrix::Identity();
			Shadow.Corners = Corners;
			Shadow.Colour = {128, 128, 128};
			Shadow.Slot = Slot;
			return LinkObject(EBattleObjectDraw::Shadow, Objects.Shadows.size() - 1);
		}

		/** A quad of the drawer's kind over the corners, its texels the page's red ones (a shadow's: the semi-transparent grey). */
		uint32 AddQuad(EBattleShowDrawer Kind, const std::array<FGteVector, 4>& Corners)
		{
			FBattleShowQuad& Entry = Frame.Quads.emplace_back();
			Entry.Drawer = static_cast<uint16>(Frame.Drawers.size());
			Frame.Drawers.push_back({static_cast<uint32>(Frame.Drawers.size() + 1), Kind, {}});
			Entry.Quad.Matrix = FGteMatrix::Identity();
			Entry.Quad.Corners = Corners;
			const bool bShadow = Kind == EBattleShowDrawer::Shadow;
			Entry.Quad.U0 = bShadow ? 8 : 0;
			Entry.Quad.U1 = bShadow ? 16 : 8;
			Entry.Quad.V1 = 16;
			Entry.Quad.TexturePage = SpritePage;
			Entry.Quad.Clut = SpriteClut;
			Entry.Quad.ColorCode = (bShadow ? RawBlendedQuad : RawQuad) | 0x808080;
			Entry.Quad.OtIndex = Corners[0][2] / 16;
			return Link(EBattleShowLink::Quad, Frame.Quads.size() - 1);
		}

		/** A spark's opaque square about its place. */
		uint32 AddTile(const FGteVector& Place, int16 Size)
		{
			FBattleShowTile& Tile = Frame.Tiles.emplace_back();
			Tile.Place = Place;
			Tile.Size = Size;
			Tile.ColorCode = OpaqueTile | 0x00FF00;
			return Link(EBattleShowLink::Tile, Frame.Tiles.size() - 1);
		}

		FSortingFrameAudit Audit(EFieldSorting Sorting = EFieldSorting::DepthBufferWithScenery) const
		{
			FBattleSortingAuditOptions Options;
			Options.Width = 320;
			Options.Scale = 1;
			Options.Sorting = Sorting;
			return AuditBattleSorting(Frame, Options);
		}

		FBattleShowFrame Frame;
		FBattleObjectsFrame Objects;
		FVram Vram;
		std::vector<FMdlPart> Parts;
		std::vector<FGearBonePackets> Packets;

	private:
		uint32 Link(EBattleShowLink Kind, std::size_t Index)
		{
			Frame.Order.push_back({Kind, static_cast<uint32>(Index), 0});
			return static_cast<uint32>(Frame.Order.size() - 1);
		}

		uint32 LinkObject(EBattleObjectDraw Kind, std::size_t Index)
		{
			Objects.Links.push_back({Kind, static_cast<uint32>(Index), 0});
			Frame.Objects = &Objects;
			return Link(EBattleShowLink::Object, Objects.Links.size() - 1);
		}
	};

	std::string ClassOf(const FSortingFrameAudit& Audit, const FSortingConflict& Conflict)
	{
		return SortingClass(Audit.Pieces[Conflict.Over].Kind, Audit.Pieces[Conflict.Under].Kind);
	}
}

TEST_CASE("The battle's sorting audit counts the stage's farther piece drawn after a nearer one, but where the scenery is sorted too",
	"[Battle][SortingAudit]")
{
	FBattlePiecesFrame Pieces;
	const uint32 Near = Pieces.AddPart(Stage, Standing(-150, 150, -150, 150, 1000), {120, 120, 120});
	const uint32 Far = Pieces.AddPart(Stage, Standing(-150, 150, -150, 150, 2000), {200, 200, 200});
	CHECK(Pieces.Audit().Conflicts.empty());
	for (const EFieldSorting Sorting : {EFieldSorting::PaintersOrder, EFieldSorting::DepthBuffer})
	{
		const FSortingFrameAudit Audit = Pieces.Audit(Sorting);
		REQUIRE(Audit.Conflicts.size() == 1);
		CHECK(Audit.Conflicts[0].Over == Far);
		CHECK(Audit.Conflicts[0].Under == Near);
		CHECK(Audit.Conflicts[0].Gap == Catch::Approx(1000.0f));
		// The far quad's whole picture: 300 units at 2000, H 256.
		CHECK(Audit.Conflicts[0].Pixels == Catch::Approx(38.4 * 38.4).epsilon(0.1));
		CHECK(ClassOf(Audit, Audit.Conflicts[0]) == "scenery over scenery");
	}
	CHECK(DescribeBattleSortingPiece(Pieces.Frame, Far, Pieces.Audit().Pieces[Far]) == "scenery bone 0 primitive 0 entry 0");
}

TEST_CASE("The battle's sorting audit leaves the 3D objects among themselves to the depth sortings", "[Battle][SortingAudit]")
{
	FBattlePiecesFrame Pieces;
	Pieces.AddPart(0, Standing(-150, 150, -150, 150, 1000), {0, 0, 255});
	const uint32 Far = Pieces.AddPart(1, Standing(-150, 150, -150, 150, 2000), {0, 255, 0});
	const FSortingFrameAudit Painted = Pieces.Audit(EFieldSorting::PaintersOrder);
	REQUIRE(Painted.Conflicts.size() == 1);
	CHECK(ClassOf(Painted, Painted.Conflicts[0]) == "gear over gear");
	CHECK(DescribeBattleSortingPiece(Pieces.Frame, Far, Painted.Pieces[Far]) == "gear slot 1 bone 0 primitive 0 entry 0");
	CHECK(Pieces.Audit(EFieldSorting::DepthBuffer).Conflicts.empty());
	CHECK(Pieces.Audit().Conflicts.empty());
}

TEST_CASE("The battle's sorting audit counts a gear's shadow over its legs in the order, and under the depth sortings only close to its floor",
	"[Battle][SortingAudit]")
{
	// The eye 100 units above the floor; a gear 1000 away standing on it; its shadow lying from 800 to 1400, linked after it.
	FBattlePiecesFrame Pieces;
	const uint32 Gear = Pieces.AddPart(0, Standing(-100, 100, -200, 100, 1000), {0, 0, 255});
	const uint32 Shadow = Pieces.AddShadow(0, {{{300, 100, 1400}, {-300, 100, 1400}, {300, 100, 800}, {-300, 100, 800}}});
	const FSortingFrameAudit Painted = Pieces.Audit(EFieldSorting::PaintersOrder);
	REQUIRE(Painted.Conflicts.size() == 1);
	CHECK(Painted.Conflicts[0].Over == Shadow);
	CHECK(Painted.Conflicts[0].Under == Gear);
	CHECK(ClassOf(Painted, Painted.Conflicts[0]) == "gear shadow over gear");
	CHECK(DescribeBattleSortingPiece(Pieces.Frame, Shadow, Painted.Pieces[Shadow]) == "gear shadow slot 0 entry 0");
	CHECK(Pieces.Audit(EFieldSorting::DepthBuffer).Conflicts.empty());
	CHECK(Pieces.Audit().Conflicts.empty());
}

TEST_CASE("The battle's sorting audit leaves a sprite's shadow card as the game draws it, but over a gear", "[Battle][SortingAudit]")
{
	SECTION("over the stage's floor nearer than the card: the design")
	{
		FBattlePiecesFrame Pieces;
		Pieces.AddPart(Stage, Lying(-1000, 1000, 100, 400, 3000), {128, 128, 128});
		const uint32 Card = Pieces.AddQuad(EBattleShowDrawer::Shadow, Standing(-80, 80, 80, 140, 1500));
		const FSortingFrameAudit Painted = Pieces.Audit(EFieldSorting::PaintersOrder);
		REQUIRE_FALSE(Painted.Conflicts.empty());
		CHECK(ClassOf(Painted, Painted.Conflicts[0]) == "shadow over scenery");
		CHECK(DescribeBattleSortingPiece(Pieces.Frame, Card, Painted.Pieces[Card]) == "shadow shadow (key 1) entry 0");
		CHECK(Pieces.Audit().Conflicts.empty());
	}
	SECTION("over a gear's polygon well above its floor: not drawn there by the depth sortings")
	{
		FBattlePiecesFrame Pieces;
		Pieces.AddPart(0, Standing(-100, 100, -200, 60, 1000), {0, 0, 255});
		// A card 3000 away, seen over the gear's lower part: its floor some 50 units below where the gear stands there.
		Pieces.AddQuad(EBattleShowDrawer::Shadow, Standing(-80, 80, 94, 176, 3000));
		CHECK_FALSE(Pieces.Audit(EFieldSorting::PaintersOrder).Conflicts.empty());
		CHECK(Pieces.Audit().Conflicts.empty());
	}
}

TEST_CASE("The battle's sorting audit keeps a spark in the order over a gear, and leaves a sprite to the depth sortings", "[Battle][SortingAudit]")
{
	SECTION("a spark inside a gear, linked after it")
	{
		FBattlePiecesFrame Pieces;
		Pieces.AddPart(0, Standing(-150, 150, -150, 150, 1000), {0, 0, 255});
		const uint32 Spark = Pieces.AddTile({0, 0, 1100}, 40);
		const FSortingFrameAudit Audit = Pieces.Audit();
		REQUIRE(Audit.Conflicts.size() == 1);
		CHECK(ClassOf(Audit, Audit.Conflicts[0]) == "particle and gear");
		CHECK(DescribeBattleSortingPiece(Pieces.Frame, Spark, Audit.Pieces[Spark]) == "particle tile (key 0) entry 0");
	}
	SECTION("a sprite behind the stage's piece linked after it")
	{
		FBattlePiecesFrame Pieces;
		Pieces.AddQuad(EBattleShowDrawer::Sprite, Standing(-60, 60, -60, 60, 1000));
		Pieces.AddPart(Stage, Standing(-150, 150, -150, 150, 2000), {200, 200, 200});
		const FSortingFrameAudit Painted = Pieces.Audit(EFieldSorting::PaintersOrder);
		REQUIRE(Painted.Conflicts.size() == 1);
		CHECK(ClassOf(Painted, Painted.Conflicts[0]) == "sprite and scenery");
		CHECK(Pieces.Audit().Conflicts.empty());
	}
}

TEST_CASE("The battle's sorting audit draws the frame's scene alone: nothing linked from its fade on", "[Battle][SortingAudit]")
{
	FBattlePiecesFrame Pieces;
	Pieces.AddPart(Stage, Standing(-150, 150, -150, 150, 1000), {120, 120, 120});
	Pieces.Frame.Order.push_back({EBattleShowLink::Fade, 0, 2});
	Pieces.AddPart(Stage, Standing(-150, 150, -150, 150, 2000), {200, 200, 200});
	const FSortingFrameAudit Audit = Pieces.Audit(EFieldSorting::PaintersOrder);
	CHECK(Audit.Pieces.size() == 1);
	CHECK(Audit.Conflicts.empty());
}
