#include "Field/FieldCharacters.h"
#include "Graphics/Gte.h"
#include "Graphics/SpriteSystem.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	constexpr uint32 OffScreen = 0x200;

	/** One sprite actor's sprite, made by hand: no file, no parts, no script. */
	class FOneSprite final : public IFieldActorModels
	{
	public:
		std::optional<FFieldSpriteResult> LoadSprite(const FFieldSpriteRequest&) override { return std::nullopt; }
		FSprite* ActorSprite(int32 Actor) override { return Actor == 0 ? &Sprite : nullptr; }
		FSpriteSystem* SpriteSystem() override { return &Kernel; }
		void SwapSprites(int32, int32) override {}
		bool LoadExtraAnimations(int32, int32) override { return false; }
		void UseExtraAnimations(int32) override {}

		FSprite Sprite;
		FSpriteSystem Kernel;
	};

	struct FScene
	{
		FOneSprite Models;
		FEventContext Context;
		FFieldView View;

		/** An actor standing X units right of the view's axis and 1000 ahead, the view straight down z. */
		explicit FScene(int32 X)
		{
			FFieldActor Record;
			Record.Flags = FieldActorFlag::NoModel;
			Record.Matrix = FGteMatrix::Identity().M;
			Record.Translation = {X, 0, 1000};
			Context.Actors = {Record};
			Context.States.resize(1);
			Context.States[0].Tints = {10, 20, 30, 40, 50, 60};
			Context.Models = &Models;
			View.View = FGteMatrix::Identity();
		}

		const FEventActorState& State() const { return Context.States[0]; }
	};
}

TEST_CASE("A field character falls off screen by where its point projects on the original's 320 x 224 view", "[Field]")
{
	FScene Inside(300);
	Inside.Context.Options = FEventOptions::Original();
	REQUIRE_FALSE(UpdateFieldCharacters(Inside.Context, Inside.View));
	CHECK((Inside.State().Flags1 & OffScreen) == 0);
	CHECK(Inside.Context.Actors[0].Position == Inside.Context.Actors[0].Translation);

	// 400 units right at depth 1000 and H 512 projects to x 364, past the 360 the original keeps.
	FScene Outside(400);
	Outside.Context.Options = FEventOptions::Original();
	Outside.Context.States[0].Flags1 = EventActorFlags1::SpriteHidden;
	REQUIRE_FALSE(UpdateFieldCharacters(Outside.Context, Outside.View));
	CHECK((Outside.State().Flags1 & OffScreen) != 0);
}

TEST_CASE("With the wide view's fix a character is on screen anywhere a 32:9 view shows it, through the view shown", "[Field]")
{
	// x 364 is in the widest view (up to 586 plus the original's margin of 40); 1200 units right, x 774, is not.
	FScene Seen(400);
	REQUIRE_FALSE(UpdateFieldCharacters(Seen.Context, Seen.View));
	CHECK((Seen.State().Flags1 & OffScreen) == 0);
	FScene Beyond(1200);
	REQUIRE_FALSE(UpdateFieldCharacters(Beyond.Context, Beyond.View));
	CHECK((Beyond.State().Flags1 & OffScreen) != 0);

	// The view the player was shown, turned to face it, brings it back.
	FScene Turned(1200);
	FGteMatrix Shown = FGteMatrix::Identity();
	Shown.T = {-1200, 0, 0};
	Turned.Context.ShownView = Shown;
	REQUIRE_FALSE(UpdateFieldCharacters(Turned.Context, Turned.View));
	CHECK((Turned.State().Flags1 & OffScreen) == 0);
}

TEST_CASE("A field character's drawing passes leave their tints and anchor groups on its sprite", "[Field]")
{
	FScene Plain(0);
	REQUIRE_FALSE(UpdateFieldCharacters(Plain.Context, Plain.View));
	CHECK(Plain.Models.Sprite.Red == 10);
	CHECK((Plain.Models.Sprite.DrawFlags >> SpriteDrawFlags::HiddenGroupsShift & 0xFF) == 0);

	FScene TwoPasses(0);
	TwoPasses.Context.States[0].Animation = static_cast<uint16>(-33);
	REQUIRE_FALSE(UpdateFieldCharacters(TwoPasses.Context, TwoPasses.View));
	CHECK(TwoPasses.Models.Sprite.Red == 40);
	CHECK((TwoPasses.Models.Sprite.DrawFlags >> SpriteDrawFlags::HiddenGroupsShift & 0xFF) == 0xF7);

	FScene Wading(0);
	Wading.Context.States[0].SpriteDraw = 0x40;
	REQUIRE_FALSE(UpdateFieldCharacters(Wading.Context, Wading.View));
	CHECK(Wading.Models.Sprite.Green == 50);
}

TEST_CASE("In fog a field character is tinted by the fog's depth cue at its point", "[Field]")
{
	FScene Fogged(0);
	FFieldFog& Fog = Fogged.Context.Render.Fog;
	Fog.bOn = true;
	Fog.Color = {100, 100, 100};
	Fog.FarColor = {200, 200, 200};
	Fog.Near = 500;
	Fog.Far = 1500;
	REQUIRE_FALSE(UpdateFieldCharacters(Fogged.Context, Fogged.View));
	FGte Gte;
	Gte.ProjectionDistance = 512;
	Gte.FarColor = {200 << 4, 200 << 4, 200 << 4};
	REQUIRE(Gte.SetFogNearFar(500, 1500, 512));
	Gte.Transform = FGteMatrix::Identity();
	Gte.Transform.T = {0, 0, 1000};
	const FGteColor Expected = Gte.DepthCueColor({100, 100, 100, 0}, Gte.Rtps({0, -20, 0}).DepthCue);
	CHECK(Fogged.Models.Sprite.Red == Expected.R);
	CHECK(Expected.R > 100);
	// The fog's tint stands: the actor's own tints are not applied over it.
	CHECK(Fogged.Models.Sprite.Red != 10);
}

TEST_CASE("A gear actor without a model loaded for it stops the characters' update", "[Field]")
{
	FScene Gear(0);
	Gear.Context.States[0].Flags1 = EventActorFlags1::Gear;
	CHECK(UpdateFieldCharacters(Gear.Context, Gear.View).has_value());
}
