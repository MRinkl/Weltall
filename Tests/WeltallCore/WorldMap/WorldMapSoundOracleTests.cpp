#include "Support/GameDiscs.h"
#include "Support/OracleDigest.h"
#include "Support/WorldMapScenario.h"
#include "WorldMap/WorldMapRuntime.h"
#include "WorldMap/WorldMapSound.h"

#include <catch_amalgamated.hpp>

#include <map>
#include <span>
#include <string>
#include <vector>

using namespace Weltall;

// The world map's calls to the sound driver against the original's, on the tick oracle's scenarios: the lines
// oracle records, each with the loop's tick it came in.

namespace
{
	const std::map<std::string, std::string> OriginalDigests{
		{"era0-foot", "48da25f4"},
		{"era0-gears", "4952e6ac"},
		{"era1-battle", "c8af1d36"},
		{"era1-foot", "4952e6ac"},
		{"era1-menu", "3fe5cc5f"},
		{"era1-pair", "4952e6ac"},
		{"era2-alone", "4952e6ac"},
		{"era2-board", "4952e6ac"},
		{"era2-factory", "a9dff30a"},
		{"era2-flight", "a68b5f29"},
		{"era2-foot", "4952e6ac"},
		{"era2-gears", "4952e6ac"},
		{"era2-interior", "2f169ace"},
		{"era2-landing", "03f4220d"},
		{"era2-nisan", "1d06f856"},
		{"era2-notice", "4952e6ac"},
		{"era2-return", "4952e6ac"},
		{"era2-sand", "4952e6ac"},
		{"era2-sea", "4952e6ac"},
		{"era2-tunnel", "4952e6ac"},
		{"era2-village", "29fbc992"},
		{"era3-foot", "4952e6ac"},
		{"era4-story", "4952e6ac"},
		{"era5-gears", "4952e6ac"},
		{"era6-foot", "4952e6ac"},
		{"era7-thames", "4952e6ac"},
		{"era8-foot", "4952e6ac"},
		{"scene10", "c3977d02"},
		{"scene11-look", "da39a3ee"},
		{"scene8-look", "c3aa0b0f"},
		{"scene8-script", "da39a3ee"},
		{"scene9", "d0de72fe"},
		{"scene12", "41636792"},
		{"scene14", "3429c0b3"},
		{"scene13", "09128ff0"},
		{"scene15-strike", "828011ce"},
		{"scene15-pass", "013dd6c7"},
		{"scene15-graze", "c47c83dd"},
		{"scene16", "a1ec03bc"},
		{"scene17", "c2a69413"},
		{"scene18", "86437055"},
	};

	/** The calls as the oracle writes them: the song each sequence was loaded from, the kept one across a battle. */
	class FRecordingWorldMapSound final : public IWorldMapSound
	{
	public:
		std::vector<std::string> Lines;
		int32 Tick = 0;

		void SetSceneGroup(int32) override {}
		void StopFieldSong() override { Add("music stop"); }
		void LoadBank() override { Add("load bank"); }
		void RegisterEffects() override { Add("register effects"); }
		void UnregisterEffects() override { Add("unregister effects"); }
		void LoadSong(EWorldMapSong Song) override
		{
			Current = Name(Song);
			Add("load " + Current);
		}
		void PlaySong(int32 Volume, int32 Ticks) override { Add("play " + Current + " " + std::to_string(Volume) + " " + std::to_string(Ticks)); }
		void ResumeSong(int32 Volume, int32 Ticks) override
		{
			Add("resume song " + Current + " " + std::to_string(Volume) + " " + std::to_string(Ticks));
		}
		void SetSongVolume(int32 Volume, int32 Ticks) override { Add("volume " + Current + " " + std::to_string(Volume) + " " + std::to_string(Ticks)); }
		void FreeSong() override { Add("free " + Current); }
		void StopAllSongs() override { Add("stop all"); }
		void KeepSong() override
		{
			Kept = Current;
			Current = "none";
		}
		void TakeKeptSong() override
		{
			Current = Kept;
			Kept = "none";
		}
		void PlayEffect(int32 Effect) override { Add("effect " + std::to_string(Effect)); }
		void PlayProtectedEffect(int32 Effect) override { Add("protected effect " + std::to_string(Effect)); }
		void StopAllEffects() override { Add("stop effects"); }
		void SetEffectVolume(int32 Effect, int32 Volume) override { Add("effect volume " + std::to_string(Effect) + " " + std::to_string(Volume & 0xFFFF)); }
		void FadeEffectVolume(int32 Effect, int32 Volume, int32 Ticks) override
		{
			Add("fade effect " + std::to_string(Effect) + " " + std::to_string(Volume & 0xFFFF) + " " + std::to_string(Ticks & 0xFFFF));
		}
		void Pause() override { Add("pause"); }
		void Resume() override { Add("resume"); }

		std::string Digest() const
		{
			std::string Joined;
			for (const std::string& Line : Lines)
			{
				Joined += (Joined.empty() ? "" : "\n") + Line;
			}
			Testing::FOracleDigest Hash;
			Hash.Raw(std::span<const uint8>(reinterpret_cast<const uint8*>(Joined.data()), Joined.size()));
			return Hash.Hex().substr(0, 8);
		}

	private:
		static std::string Name(EWorldMapSong Song)
		{
			switch (Song)
			{
			case EWorldMapSong::Vehicle:
				return "vehicle";
			case EWorldMapSong::Battle:
				return "battle";
			default:
				return "map";
			}
		}

		void Add(const std::string& Text) { Lines.push_back(std::to_string(Tick) + " " + Text); }

		std::string Current = "none";
		std::string Kept = "none";
	};
}

namespace
{
	/** The scenarios of one disc, each one's calls against the original's. */
	void CheckSounds(const FIso9660& Disc, EDiscRelease Release)
	{
		Testing::FWorldMapOracleFiles Files;
		const std::optional<FError> Read = Files.Read(Disc, Release);
		INFO((Read ? Read->Message : std::string()));
		REQUIRE(!Read);
		const std::span<const Testing::FWorldMapScenario> Scenarios = Testing::WorldMapScenarios();
		for (std::size_t Number = 0; Number < Scenarios.size(); ++Number)
		{
			const Testing::FWorldMapScenario& Scenario = Scenarios[Number];
			if (Scenario.Disc != Release)
			{
				continue;
			}
			INFO("scenario " << Scenario.Name);
			Testing::FWorldMapOracleSession Session(Scenario);
			FRecordingWorldMapSound Sound;
			Session.Sound = &Sound;
			Testing::FWorldMapOracleHooks Hooks;
			Hooks.Ticked = [&](FWorldMapRuntime&, int32 Tick) { Sound.Tick = Tick + 1; };
			const std::string Stopped = Testing::RunWorldMapScenario(Files, Number, Scenario.Ticks, Session, Hooks);
			INFO(Stopped);
			std::string Log;
			for (const std::string& Line : Sound.Lines)
			{
				Log += Line + "; ";
			}
			INFO(Log);
			CHECK(Sound.Digest() == OriginalDigests.at(Scenario.Name));
		}
	}
}

TEST_CASE("The world map's sound calls match the original's: its songs, the vehicle's, the battle's, its effects and the pause", "[Content][Oracle]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckSounds(*Disc, EDiscRelease::UsaDisc1);
}

TEST_CASE("The world map's sound calls match the original's in the second disc's cutscenes", "[Content][Oracle]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	CheckSounds(*Disc, EDiscRelease::UsaDisc2);
}
