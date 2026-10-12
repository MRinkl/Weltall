#include "Support/GameDiscs.h"
#include "Support/WorldMapScenario.h"
#include "WorldMap/WorldMapRuntime.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace Weltall;

namespace
{
	/** The scene's effect calls, as "effect N" and "protected effect N"; the rest of the sound passed over. */
	class FEffectCalls final : public IWorldMapSound
	{
	public:
		std::vector<std::string> Lines;

		void SetSceneGroup(int32) override {}
		void StopFieldSong() override {}
		void LoadBank() override {}
		void RegisterEffects() override {}
		void UnregisterEffects() override {}
		void LoadSong(EWorldMapSong) override {}
		void PlaySong(int32, int32) override {}
		void ResumeSong(int32, int32) override {}
		void SetSongVolume(int32, int32) override {}
		void FreeSong() override {}
		void StopAllSongs() override {}
		void KeepSong() override {}
		void TakeKeptSong() override {}
		void PlayEffect(int32 Effect) override { Lines.push_back("effect " + std::to_string(Effect)); }
		void PlayProtectedEffect(int32 Effect) override { Lines.push_back("protected effect " + std::to_string(Effect)); }
		void StopAllEffects() override {}
		void SetEffectVolume(int32, int32) override {}
		void FadeEffectVolume(int32, int32, int32) override {}
		void Pause() override {}
		void Resume() override {}

		bool Has(const std::string& Line) const { return std::find(Lines.begin(), Lines.end(), Line) != Lines.end(); }
	};
}

// The world map's fixes of the original's quirks, each against the original's behavior, which the world map
// oracles check.

TEST_CASE("The story's landmark comes back after a battle while the story shows it, or in the original stays hidden", "[WorldMap][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	Testing::FWorldMapOracleFiles Files;
	const std::optional<FError> Read = Files.Read(*Disc);
	REQUIRE_FALSE(Read);
	const auto [Scenario, Number] = Testing::FindWorldMapScenario("era4-story");
	REQUIRE(Scenario != nullptr);
	constexpr std::size_t Landmark = 69;

	// The entry at story 153 shows it; a return from a battle then shows it or hides it.
	const auto HiddenAfterBattle = [&](const FWorldMapOptions& Options)
	{
		Testing::FWorldMapOracleSession Session(*Scenario);
		FWorldMapServices Services = Session.Services(Files);
		Services.Options = Options;
		FWorldMapRuntime Entered(Services);
		REQUIRE_FALSE(Entered.Enter());
		const FWorldMapContext& Before = Entered.Context();
		REQUIRE(Before.Objects[Landmark].Hidden == 0);

		FWorldMapBattleKeep Keep;
		Keep.Tasks = Before.Tasks;
		for (FWorldMapTask& Task : Keep.Tasks)
		{
			Task.Sprite = FWorldMapTask::NoSprite;
		}
		Keep.Controlled = Before.Controlled;
		Keep.ControlledHeading = Before.ControlledHeading;
		Keep.CameraPitch = Before.CameraPitch;
		Keep.CameraYaw = Before.CameraYaw;
		Keep.CameraDistance = Before.CameraDistance;
		Keep.ScreenCentreY = Before.ScreenCentreY;
		Keep.CameraTarget = Before.CameraTarget;
		Services.BattleKeep = &Keep;
		// The scene word (2320h) marks the return from a battle, as the request for one leaves it.
		Session.Game.Block()[0x2321] |= 0x80;
		FWorldMapRuntime Returned(Services);
		REQUIRE_FALSE(Returned.Enter());
		return Returned.Context().Objects[Landmark].Hidden;
	};
	CHECK(HiddenAfterBattle(FWorldMapOptions()) == 0);
	CHECK(HiddenAfterBattle(FWorldMapOptions::Original()) != 0);
}


TEST_CASE("Scene 14's noise sweep plays on a pair of its own, or in the original on the tone's", "[WorldMap][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	Testing::FWorldMapOracleFiles Files;
	REQUIRE_FALSE(Files.Read(*Disc));
	const auto [Scenario, Number] = Testing::FindWorldMapScenario("scene14");
	REQUIRE(Scenario != nullptr);
	const auto Calls = [&](const FWorldMapOptions& Options)
	{
		Testing::FWorldMapOracleSession Session(*Scenario);
		FEffectCalls Sound;
		Session.Sound = &Sound;
		Session.Options = Options;
		Testing::FWorldMapOracleHooks Hooks;
		Testing::RunWorldMapScenario(Files, Number, Scenario->Ticks, Session, Hooks);
		return Sound;
	};
	const FEffectCalls Fixed = Calls(FWorldMapOptions());
	CHECK(Fixed.Has("effect 11"));
	CHECK_FALSE(Fixed.Has("protected effect 11"));
	CHECK(Fixed.Has("protected effect 12"));
	const FEffectCalls Original = Calls(FWorldMapOptions::Original());
	CHECK(Original.Has("protected effect 11"));
	CHECK(Original.Has("protected effect 12"));
}

TEST_CASE("Scene 18's effects wait for Mahanon's stages, or in the original run unseen from the load", "[WorldMap][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	Testing::FWorldMapOracleFiles Files;
	REQUIRE_FALSE(Files.Read(*Disc, EDiscRelease::UsaDisc2));
	const auto [Scenario, Number] = Testing::FindWorldMapScenario("scene18");
	REQUIRE(Scenario != nullptr);
	// The particles alive at the scene's tick 300 of effects 1 and 19, which nothing in the scene starts.
	const auto Alive = [&](const FWorldMapOptions& Options)
	{
		Testing::FWorldMapOracleSession Session(*Scenario);
		Session.Options = Options;
		Testing::FWorldMapOracleHooks Hooks;
		int32 Count = -1;
		Hooks.Ticked = [&Count](FWorldMapRuntime& Runtime, int32 Tick)
		{
			if (Tick == 300)
			{
				const auto& Particles = Runtime.Context().Particles.Particles();
				Count = static_cast<int32>(std::count_if(Particles.begin(), Particles.end(), [](const FWorldMapParticle& Particle)
				{
					const int32 Effect = Particle.Emitter / FWorldMapParticles::EmittersPerEffect;
					return Particle.Shape != 0 && (Effect == 1 || Effect == 19);
				}));
			}
		};
		Testing::RunWorldMapScenario(Files, Number, 301, Session, Hooks);
		return Count;
	};
	CHECK(Alive(FWorldMapOptions()) == 0);
	CHECK(Alive(FWorldMapOptions::Original()) > 0);
}
