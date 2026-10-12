#include "Field/FieldPlay.h"
#include "Support/FieldPlaySession.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <vector>

using namespace Weltall;

// A new game's field played on real maps with nobody at the controller: what each tick's frame
// holds for the renderer, as the field's drawing leaves it.

namespace
{
	/** Map 35: the party and a crowd in a village square, the camera following the leader. */
	constexpr int32 CrowdedMap = 35;
	/** Map 4: the opening's narration, typed over black. */
	constexpr int32 NarrationMap = 4;
	/** Map 239: the player takes control soon after entering, and Start pauses the field. */
	constexpr int32 PausableMap = 239;
	/** Map 5: a save point, whose sprite's spawned children draw its models from the first ticks. */
	constexpr int32 SavePointMap = 5;
	constexpr uint16 ShadowPage = 0x5A;

	std::unique_ptr<Testing::FFieldPlaySession> Enter(const FIso9660& Disc, int32 Map)
	{
		return Testing::EnterFieldMap(Disc, Map);
	}

	int32 EntryOf(const FFieldFrame& Frame, const FFieldDrawItem& Item)
	{
		return Testing::FieldDrawEntry(Frame, Item);
	}

	/** Plays the map until the player has control. */
	void RunToPlayerControl(Testing::FFieldPlaySession& Session)
	{
		for (int32 Tick = 0; Tick < 400 && !(Tick >= FFieldRuntime::EntryTicks && Session.Play->Field().Context().bPlayerControlled); ++Tick)
		{
			Session.Run(1);
		}
		REQUIRE(Session.Play->Field().Context().bPlayerControlled);
	}

	/** Start at a tick's first vertical blank, then the frames up to the first of the pause. */
	void Pause(Testing::FFieldPlaySession& Session)
	{
		std::vector<FPadState> Held(static_cast<std::size_t>(Session.Play->VblanksPerTick()));
		Held[0] = {FPadState::Bit(EPadButton::Start)};
		REQUIRE_FALSE(Session.Play->Tick(Held, EFieldCulling::InRange, Session.Frame));
		Session.Run(3);
		REQUIRE(Session.Play->Field().LastFrame() == EFieldFrame::Paused);
	}

	/** The compass's quads keep keys of their own. */
	constexpr uint32 CompassKeys = 0xC0000;
}

TEST_CASE("A played map draws nothing but its 2D layer while it settles, then its scenery and characters", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Enter(*Disc, CrowdedMap);
	for (int32 Tick = 0; Tick < FEventContext::MapStartSettleTicks; ++Tick)
	{
		Session->Run(1);
		CHECK_FALSE(Session->Frame.bSceneDrawn);
		CHECK(Session->Frame.Tick == static_cast<uint32>(Tick));
	}
	Session->Run(40);
	const FFieldFrame& Frame = Session->Frame;
	CHECK(Frame.bSceneDrawn);
	CHECK(Frame.View.View == Session->Play->Field().Context().Camera.View.View);
	REQUIRE(Frame.Characters.size() > 10);
	REQUIRE_FALSE(Frame.Quads.empty());
	// Every quad of the characters and every polygon of the scenery the frame draws, once each.
	std::size_t Quads = 0;
	std::size_t Particles = 0;
	for (const FFieldDrawItem& Item : Frame.Order)
	{
		Quads += Item.Kind == EFieldDrawKind::Character ? 1 : 0;
		Particles += Item.Kind == EFieldDrawKind::Particle ? 1 : 0;
	}
	CHECK(Quads == Frame.Quads.size());
	CHECK(Particles == Frame.Particles.size());
	CHECK(Frame.Order.size() - Quads - Particles == Frame.Scenery.DrawOrder.size());
	const bool bShadow = std::any_of(Frame.Quads.begin(), Frame.Quads.end(), [](const FFieldCharacterQuad& Quad) { return Quad.Quad.TexturePage == ShadowPage; });
	CHECK(bShadow);
}

TEST_CASE("A played map's frame orders its characters among the scenery far to near, as the ordering table draws them", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Enter(*Disc, CrowdedMap);
	Session->Run(60);
	const FFieldFrame& Frame = Session->Frame;
	const FFieldScene& Scene = Session->Play->Drawing().Scene();
	std::size_t First = 0;
	while (First < Frame.Order.size()
		&& (Frame.Order[First].Kind == EFieldDrawKind::Backdrop
			|| (Frame.Order[First].Kind == EFieldDrawKind::Scenery && Scene.Objects[Scene.Polygons[Frame.Order[First].Index].Object].bBackground)))
	{
		++First;
	}
	for (std::size_t Item = First + 1; Item < Frame.Order.size(); ++Item)
	{
		const FFieldDrawItem& Before = Frame.Order[Item - 1];
		const FFieldDrawItem& After = Frame.Order[Item];
		INFO("order " << Item);
		REQUIRE(EntryOf(Frame, Before) >= EntryOf(Frame, After));
		// Within an entry the particles, linked after the characters, and the characters, linked after the scenery, are drawn first.
		if (EntryOf(Frame, Before) == EntryOf(Frame, After))
		{
			REQUIRE(Testing::FieldDrawRank(Before) <= Testing::FieldDrawRank(After));
		}
	}
}

TEST_CASE("A played map's characters keep their keys from tick to tick, and move a little between them", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Enter(*Disc, CrowdedMap);
	Session->Run(50);
	const std::vector<FFieldCharacterView> Before = Session->Frame.Characters;
	Session->Run(1);
	std::size_t Paired = 0;
	for (const FFieldCharacterView& Character : Session->Frame.Characters)
	{
		for (const FFieldCharacterView& Earlier : Before)
		{
			if (Earlier.BlendKey == Character.BlendKey)
			{
				++Paired;
				CHECK(Earlier.bOnFloor == Character.bOnFloor);
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					CHECK(std::abs(Earlier.World[Axis] - Character.World[Axis]) < 64);
				}
			}
		}
	}
	CHECK(Paired == Session->Frame.Characters.size());
}

TEST_CASE("A played map's frames that run no tick, as the pause's, show the last tick's characters still", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Enter(*Disc, PausableMap);
	for (int32 Tick = 0; Tick < 400 && !(Tick >= FFieldRuntime::EntryTicks && Session->Play->Field().Context().bPlayerControlled); ++Tick)
	{
		Session->Run(1);
	}
	REQUIRE(Session->Play->Field().Context().bPlayerControlled);
	// Start at the tick's first vertical blank: the pause begins with the next frame.
	std::vector<FPadState> Held(static_cast<std::size_t>(Session->Play->VblanksPerTick()));
	Held[0] = {FPadState::Bit(EPadButton::Start)};
	REQUIRE_FALSE(Session->Play->Tick(Held, EFieldCulling::InRange, Session->Frame));
	REQUIRE(Session->Play->Field().LastFrame() == EFieldFrame::Tick);
	const std::vector<FFieldCharacterView> Ticked = Session->Frame.Characters;
	const std::size_t Quads = Session->Frame.Quads.size();
	REQUIRE_FALSE(Ticked.empty());
	Session->Run(3);
	REQUIRE(Session->Play->Field().LastFrame() == EFieldFrame::Paused);
	REQUIRE(Session->Frame.Characters.size() == Ticked.size());
	for (std::size_t Character = 0; Character < Ticked.size(); ++Character)
	{
		CHECK(Session->Frame.Characters[Character].BlendKey == Ticked[Character].BlendKey);
		CHECK(Session->Frame.Characters[Character].World == Ticked[Character].World);
	}
	CHECK(Session->Frame.Quads.size() == Quads);
}

TEST_CASE("A played map's pause loads the kernel's picture into the buffer on display and draws it over everything", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Enter(*Disc, PausableMap);
	RunToPlayerControl(*Session);
	Pause(*Session);
	const FVramImage& Picture = Session->Play->Drawing().Tables().PauseImage;
	REQUIRE((Picture.Rect.Width == 40 && Picture.Rect.Height == 8));
	const int32 BufferY = 256 * ((Session->Play->Field().Context().DrawBuffer + 1) & 1);
	const FVram& Vram = Session->Play->Drawing().Vram();
	int32 Different = 0;
	for (int32 Y = 0; Y < Picture.Rect.Height; ++Y)
	{
		for (int32 X = 0; X < Picture.Rect.Width; ++X)
		{
			const uint16 Unit = Vram.At(FFieldDrawingTables::PauseX + Picture.Rect.X + X, FFieldDrawingTables::PauseY + Picture.Rect.Y + BufferY + Y);
			Different += Unit != Picture.Units[static_cast<std::size_t>(Y * Picture.Rect.Width + X)] ? 1 : 0;
		}
	}
	CHECK(Different == 0);
	// Not all one colour: the word is there.
	CHECK(std::any_of(Picture.Units.begin(), Picture.Units.end(), [&Picture](uint16 Unit) { return Unit != Picture.Units.front(); }));
	const FDrawPrimitive& Last = Session->Frame.Screen.Primitives.back();
	REQUIRE(Last.Texture);
	CHECK(Last.Texture->Page.Depth == ETextureDepth::Direct15);
	CHECK(Last.Texture->Page.Y == BufferY);
	CHECK(Last.Corners[0].X == static_cast<float>(FFieldDrawingTables::PauseX + Picture.Rect.X));
}

TEST_CASE("A played map draws the compass's shadow, dial, letters and needle, and loads its palette each tick", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Enter(*Disc, PausableMap);
	RunToPlayerControl(*Session);
	Session->Run(1);
	const FFieldRenderState& Render = Session->Play->Field().Context().Render;
	REQUIRE(Render.Compass.bDrawn);
	const auto IsCompass = [](const FDrawPrimitive& Primitive) { return (Primitive.BlendKey & ~0xFFFFu) == CompassKeys; };
	const std::vector<FDrawPrimitive>& Screen = Session->Frame.Screen.Primitives;
	CHECK(std::count_if(Screen.begin(), Screen.end(), IsCompass) == 4 + 16 + 4 + 1);
	// The shadow first, subtracted; all of it in the screen's lower right.
	const auto First = std::find_if(Screen.begin(), Screen.end(), IsCompass);
	REQUIRE(First != Screen.end());
	CHECK(First->Blend == EDrawBlend::SemiTransparent);
	CHECK(First->Texture->Page.Blend == EBlendMode::Subtract);
	for (const FDrawPrimitive& Primitive : Screen)
	{
		if (IsCompass(Primitive))
		{
			CHECK(Primitive.Corners[0].X > 160.0f);
			CHECK(Primitive.Corners[0].Y > 112.0f);
		}
	}
	// The dial's palette row: its colours, each direction's black or not by the camera's bits.
	const FVram& Vram = Session->Play->Drawing().Vram();
	uint32 Inked = 0;
	for (int32 Unit = 0; Unit < 128; ++Unit)
	{
		Inked |= Vram.At(Unit, 251);
	}
	CHECK(Inked != 0);
}

TEST_CASE("A played map's spawned children draw their models into the scene between the characters and the scenery of their entries", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Enter(*Disc, SavePointMap);
	Session->Run(40);
	const FFieldFrame& Frame = Session->Frame;
	const FFieldSpriteModelsFrame& Models = Frame.SpriteModels;
	REQUIRE_FALSE(Models.Models.empty());
	const auto Drawn = std::count_if(Models.Polygons.begin(), Models.Polygons.end(), [](const FFieldSpriteModelPolygon& Polygon) { return Polygon.bDrawn; });
	CHECK(Drawn > 0);
	int32 Ordered = 0;
	for (std::size_t Item = 0; Item < Frame.Order.size(); ++Item)
	{
		if (Frame.Order[Item].Kind != EFieldDrawKind::SpriteModel)
		{
			continue;
		}
		++Ordered;
		const FFieldSpriteModelPolygon& Polygon = Models.Polygons[Frame.Order[Item].Index];
		CHECK(Polygon.bDrawn);
		// What follows in the same entry is another of the children's polygons or the scenery.
		if (Item + 1 < Frame.Order.size() && EntryOf(Frame, Frame.Order[Item + 1]) == Polygon.Bucket)
		{
			const EFieldDrawKind Next = Frame.Order[Item + 1].Kind;
			CHECK((Next == EFieldDrawKind::SpriteModel || Next == EFieldDrawKind::Scenery));
		}
	}
	CHECK(Ordered == Drawn);
	// Every polygon keeps its key from tick to tick while its child lives.
	const uint32 Key = Models.Polygons.front().BlendKey;
	Session->Run(1);
	CHECK(std::any_of(Session->Frame.SpriteModels.Polygons.begin(), Session->Frame.SpriteModels.Polygons.end(),
		[Key](const FFieldSpriteModelPolygon& Polygon) { return Polygon.BlendKey == Key; }));
}

TEST_CASE("A played map types its windows' text into VRAM and draws its lines from there", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Enter(*Disc, NarrationMap);
	Session->Run(120);
	const FEventContext& Context = Session->Play->Field().Context();
	REQUIRE(Context.Messages.OpenMask != 0);
	const FVram& Vram = Session->Play->Drawing().Vram();
	const FFieldDrawingTables& Tables = Session->Play->Drawing().Tables();
	// The message font's CLUTs, and the window's first line in its place.
	CHECK(Vram.At(257, 240) == Tables.TextClut[1]);
	int32 Window = 0;
	while ((Context.Messages.OpenMask >> Window & 1) == 0)
	{
		++Window;
	}
	const std::array<int16, 2>& Place = Tables.MessageText[static_cast<std::size_t>(Window)];
	uint32 Inked = 0;
	for (int32 Row = 0; Row < 13; ++Row)
	{
		for (int32 Unit = 0; Unit < 32; ++Unit)
		{
			Inked |= Vram.At(Place[0] + Unit, Place[1] + Row);
		}
	}
	CHECK(Inked != 0);
	const auto IsLine = [](const FDrawPrimitive& Primitive) { return Primitive.Texture && Primitive.Texture->Clut == FClutPosition{256, 240}; };
	CHECK(std::any_of(Session->Frame.Screen.Primitives.begin(), Session->Frame.Screen.Primitives.end(), IsLine));
}
