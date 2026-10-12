#include "Support/WorldMapScenario.h"

#include "Foundation/LittleEndian.h"
#include "Media/Iso9660.h"
#include "Support/GameText.h"
#include "Support/XorShift32.h"
#include "Text/MesWindow.h"

#include <algorithm>
#include <memory>

namespace Weltall::Testing
{
	namespace
	{
		using namespace WorldMapButtons;

		constexpr uint32 PadSeed = 0x57A00000;
		/** Every place the program's debug start shows on the minimap. */
		constexpr uint32 AllMinimapDots = 0x07FFFFFF;
		/** The frames the script runs beyond the ticks, for the pause's frames of one vertical blank. */
		constexpr int32 PauseFrames = 64;

		/** Characters 0, 2 and 3 with gears 0, 2 and 3, as the oracle's scenarios give them; the rest without. */
		constexpr std::array<std::pair<uint8, uint8>, 3> Gears{{{0, 0}, {2, 2}, {3, 3}}};

		constexpr uint16 Directions[] = {0, Up, Up | Right, Right, Right | Down, Down, Down | Left, Left, Left | Up};
		constexpr uint16 ScriptPresses[] = {0, 0, 0, 0, Cross, Circle, Triangle, L1, R1, Select, L2 | R2, 0};

		constexpr std::array<FWorldMapScenario, 41> Scenarios{{
			{"era1-foot", 30, 1, 1},
			{"era2-foot", 60, 2, 2},
			{"era2-gears", 60, 3, 3, 0xC00, {0, 2, 3}, {1, 1, 1}},
			{"era1-pair", 40, 4, 4, 0xC00, {0, 2, 0xFF}},
			{"era2-alone", 100, 5, 5, 0x400, {0, 0xFF, 0xFF}},
			{"era0-foot", 10, 1, 1},
			{"era0-gears", 20, 2, 2, 0xC00, {0, 2, 3}, {1, 1, 1}},
			// A battle falls at tick 353; the world map then starts again from what it kept.
			{"era1-battle", 30, 1, 1, 0xC00, {0, 2, 3}, {0, 0, 0}, 512},
			// The menu asked for (and closed at once), then the pause from one Start to the next.
			{"era1-menu", 30, 1, 1, 0xC00, {0, 2, 3}, {0, 0, 0}, 160, {{{100, Square}, {130, Start}, {140, Start}}}},
			// The Yggdrasil: sailing the sand, the sea, flying (the throttle held), and boarded from the sand.
			{"era2-sand", 60, 6, 6, 0xC00, {0, 2, 3}, {0, 0, 0}, 256, {}, {26997, 0, 11946, 0x400}, 0x6001},
			{"era2-sea", 60, 8, 8, 0xC00, {0, 2, 3}, {0, 0, 0}, 256, {}, {26368, 0, 14080, 0x400}, 0x6002, {{{0, 40, Up}}}},
			{"era2-flight", 60, 1, 1, 0xC00, {0, 2, 3}, {0, 0, 0}, 256, {}, {29947, -480, 11075, 0xC00}, 0x6003, {{{70, 180, Triangle}, {200, 256, Triangle}}}, 0, 1,
				AllMinimapDots},
			{"era2-board", 60, 6, 6, 0xC00, {0, 2, 3}, {0, 0, 0}, 256, {{{70, Cross}}}, {27037, 0, 11946, 0}, 0x0001, {}, 70},
			// The flying ship landing, the sand ship's menu button (its interior), its way into the Road to Nisan, and the voyages.
			{"era2-landing", 60, 1, 1, 0xC00, {0, 2, 3}, {0, 0, 0}, 256, {{{70, Circle}}}, {29947, -480, 11075, 0xC00}, 0x6003, {}, 160},
			{"era2-interior", 60, 6, 6, 0xC00, {0, 2, 3}, {0, 0, 0}, 256, {{{70, Square}}}, {26997, 0, 11946, 0x400}, 0x6001, {}, 70},
			{"era2-tunnel", 60, 37, 37, 0xC00, {0, 2, 3}, {0, 0, 0}, 256, {{{70, Cross}}}, {18750, 0, 8260, 0}, 0x6001, {}, 70},
			// A notice's rectangle walked into, the pad otherwise still.
			{"era2-notice", 60, 12, 12, 0xC00, {0, 2, 3}, {0, 0, 0}, 256, {}, {}, 0, {{{70, 200, Left}}}, 256},
			{"era2-village", 60, 1, 1, 0xC00, {0, 2, 3}, {0, 0, 0}, 700, {}, {20200, 0, 11400, 0xC00}, 0x6001, {}, 0, 3},
			{"era2-factory", 60, 1, 1, 0xC00, {0, 2, 3}, {0, 0, 0}, 900, {}, {9200, 0, 19700, 0xC00}, 0x6002, {}, 0, 4},
			{"era2-nisan", 60, 1, 1, 0xC00, {0, 2, 3}, {0, 0, 0}, 700, {}, {17000, 0, 12000, 0x200}, 0x6002, {}, 0, 5},
			{"era2-return", 60, 1, 1, 0xC00, {0, 2, 3}, {0, 0, 0}, 400, {}, {19264, 0, 7168, 0}, 0x6001, {}, 0, 2},
			// The later eras' objects: Shevat drifting, Thames moored then cruising, the spinner pairs, the story's
			// landmark at its one value, the landmarks' colliders.
			{"era3-foot", 140, 1, 1},
			{"era4-story", 153, 1, 1},
			{"era5-gears", 190, 4, 4, 0xC00, {0, 2, 3}, {1, 1, 1}},
			{"era6-foot", 200, 5, 5},
			{"era7-thames", 210, 1, 1, 0xC00, {0, 2, 3}, {0, 0, 0}, 256, {}, {}, 0, {}, 0, 1, AllMinimapDots},
			{"era8-foot", 240, 4, 4},
			// The cutscene scenes, entered as the field's ExitToWorldMap hands them over and run to their own end. The
			// telescope (8, and 11, the same scene) looked through with the pad until Circle, and on the script's own
			// buttons; scenes 9 and 10 with a pause.
			{"scene8-look", 20, 3, 0, 0xC00, {0, 0xFF, 0xFF}, {0, 0, 0}, 400, {{{60, Select}, {120, Start}, {135, Start}, {300, Circle}}}, {}, 0,
				{{{10, 40, Up}, {20, 80, Left}, {90, 130, R1}, {100, 160, Down}, {170, 200, L1 | Up}, {210, 260, Right}, {265, 290, L1 | R1 | Down}}}, 400, 8},
			{"scene8-script", 20, 3, 0, 0xC00, {0, 0xFF, 0xFF}, {0, 0, 0}, 256, {}, {}, 0, {}, 0, 8},
			{"scene11-look", 20, 3, 0, 0xC00, {0, 0xFF, 0xFF}, {0, 0, 0}, 200, {{{150, Circle}}}, {}, 0, {{{5, 60, Up | Right}}}, 200, 11},
			{"scene9", 135, 0, 0, 0xC00, {0, 2, 3}, {0, 0, 0}, 720, {{{200, Start}, {230, Start}}}, {}, 0, {}, 0, 9},
			{"scene10", 138, 0, 0, 0xC00, {0, 3, 2}, {0, 0, 0}, 820, {{{400, Start}, {420, Start}}}, {}, 0, {}, 0, 10},
			// Scenes 12 and 14, the pad still but for Select and a pause.
			{"scene12", 0, 0, 0, 0x800, {0, 2, 3}, {0, 0, 0}, 1500, {{{300, Select}, {670, Start}, {700, Start}}}, {}, 0, {}, 1600, 12},
			{"scene14", 0, 0, 0, 0x800, {0, 2, 3}, {0, 0, 0}, 700, {{{240, Start}, {262, Start}, {500, Select}}}, {}, 0, {}, 800, 14},
			// Scenes 13, 15 (its three entrances) and 16, each with Select and a pause, the pad otherwise still.
			{"scene13", 200, 0, 0, 0xC00, {0, 2, 3}, {0, 0, 0}, 900, {{{150, Select}, {300, Start}, {320, Start}}}, {}, 0, {}, 964, 13},
			{"scene15-strike", 196, 0, 0, 0xC00, {0, 2, 3}, {0, 0, 0}, 870, {{{100, Select}, {386, Start}, {410, Start}}}, {}, 0, {}, 934, 15},
			{"scene15-pass", 196, 1, 0, 0xC00, {0, 2, 3}, {0, 0, 0}, 590, {{{250, Start}, {260, Start}}}, {}, 0, {}, 654, 15},
			{"scene15-graze", 196, 2, 0, 0xC00, {0, 2, 3}, {0, 0, 0}, 630, {}, {}, 0, {}, 694, 15},
			{"scene16", 150, 17, 0, 0xC00, {0, 2, 3}, {0, 0, 0}, 850, {{{200, Select}, {500, Start}, {530, Start}}}, {}, 0, {}, 914, 16},
			// Disc 2's scenes, which a script runs, the pad still but for the presses: 17 with a pause under its first shot,
			// a Start while its fade to black moves and a Select, 18 with a Select and a pause across a cut.
			{"scene17", 252, 0xFFF, 0, 0x800, {0, 1, 2}, {0, 0, 0}, 2400, {{{300, Start}, {330, Start}, {575, Start}, {700, Select}}}, {}, 0, {}, 2400, 17, 0,
				EDiscRelease::UsaDisc2},
			{"scene18", 285, 0xFFF, 0, 0x800, {0, 1, 2}, {0, 0, 0}, 2300, {{{100, Select}, {1045, Start}, {1060, Start}}}, {}, 0, {}, 2300, 18, 0,
				EDiscRelease::UsaDisc2},
		}};

		/**
		 * The pad's script, a step a frame: runs of 4 to 23 frames holding a direction or none, a button pressed on a
		 * run's first frame now and then, and the scenario's own presses.
		 */
		std::vector<FOraclePadStep> PadScript(uint32 Seed, int32 Count, const FWorldMapScenario& Scenario)
		{
			FXorShift32 G(Seed);
			std::vector<FOraclePadStep> Script;
			while (static_cast<int32>(Script.size()) < Count)
			{
				const uint32 Length = 4 + G.Next() % 20;
				const uint16 Held = Directions[G.Next() % std::size(Directions)];
				const uint16 Pressed = ScriptPresses[G.Next() % std::size(ScriptPresses)];
				Script.push_back({Held, Pressed});
				for (uint32 Tick = 1; Tick < Length; ++Tick)
				{
					Script.push_back({Held, 0});
				}
			}
			Script.resize(static_cast<std::size_t>(Count));
			std::fill_n(Script.begin(), Scenario.Quiet, FOraclePadStep{});
			for (const auto& [Frame, Buttons] : Scenario.Presses)
			{
				Script[static_cast<std::size_t>(Frame)].Pressed |= Buttons;
			}
			for (const auto& [First, End, Buttons] : Scenario.Holds)
			{
				for (int32 Frame = First; Frame < End; ++Frame)
				{
					Script[static_cast<std::size_t>(Frame)].Held |= static_cast<uint16>(Buttons);
				}
			}
			return Script;
		}

		FGameState ScenarioState(const FWorldMapScenario& Scenario)
		{
			FGameState Game;
			std::span<uint8> Block = Game.Block();
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				Block[0x1D34 + static_cast<std::size_t>(Slot)] = Scenario.Party[static_cast<std::size_t>(Slot)];
				Block[0x22B1 + static_cast<std::size_t>(Slot)] = Scenario.Riding[static_cast<std::size_t>(Slot)];
			}
			for (int32 Character = 0; Character < FGameState::CharacterCount; ++Character)
			{
				uint8 Gear = 0xFF;
				for (const auto& [Owner, Number] : Gears)
				{
					if (Owner == Character)
					{
						Gear = Number;
					}
				}
				Block[0x26C + 0xA4 * static_cast<std::size_t>(Character) + 0xA0] = Gear;
			}
			LittleEndian::WriteU16(Block, 0x1930, Scenario.Story);
			LittleEndian::WriteU32(Block, 0x1B2C, Scenario.MinimapDots);
			LittleEndian::WriteU16(Block, 0x231A, static_cast<uint16>(0x400 + Scenario.Location));
			LittleEndian::WriteU16(Block, 0x231C, Scenario.Yaw);
			LittleEndian::WriteU16(Block, 0x231E, Scenario.Entrance);
			LittleEndian::WriteU16(Block, 0x2320, Scenario.Scene);
			for (std::size_t Word = 0; Word < Scenario.Vehicle.size(); ++Word)
			{
				LittleEndian::WriteU16(Block, 0x182C + 2 * Word, static_cast<uint16>(Scenario.Vehicle[Word]));
			}
			LittleEndian::WriteU16(Block, 0x1834, Scenario.VehicleFlags);
			return Game;
		}
	}

	std::span<const FWorldMapScenario> WorldMapScenarios()
	{
		return Scenarios;
	}

	std::pair<const FWorldMapScenario*, std::size_t> FindWorldMapScenario(const std::string& Name)
	{
		for (std::size_t Number = 0; Number < Scenarios.size(); ++Number)
		{
			if (Name == Scenarios[Number].Name)
			{
				return {&Scenarios[Number], Number};
			}
		}
		return {nullptr, 0};
	}

	std::optional<FError> FWorldMapOracleFiles::Read(const FIso9660& InDisc, EDiscRelease Release)
	{
		Disc = &InDisc;
		auto ReadExecutable = ReadBootExecutable(InDisc);
		if (!ReadExecutable)
		{
			return ReadExecutable.Error();
		}
		Executable.emplace(std::move(*ReadExecutable));
		auto ReadIndex = FGameFileIndex::Read(*Executable);
		if (!ReadIndex)
		{
			return ReadIndex.Error();
		}
		Index.emplace(std::move(*ReadIndex));
		auto ReadArctangents = FGteArctangents::Read(*Executable);
		if (!ReadArctangents)
		{
			return ReadArctangents.Error();
		}
		Arctangents.emplace(*ReadArctangents);
		auto ReadTables = FWorldMapTables::Read(InDisc.Image(), *Index, Release);
		if (!ReadTables)
		{
			return ReadTables.Error();
		}
		Tables.emplace(std::move(*ReadTables));
		Resident.emplace(LoadMesResident(InDisc));
		return std::nullopt;
	}

	FWorldMapOracleSession::FWorldMapOracleSession(const FWorldMapScenario& Scenario) : Game(ScenarioState(Scenario))
	{
	}

	FWorldMapServices FWorldMapOracleSession::Services(const FWorldMapOracleFiles& Files)
	{
		FWorldMapServices Services;
		Services.Image = &Files.Disc->Image();
		Services.Index = &*Files.Index;
		Services.Tables = &*Files.Tables;
		Services.Arctangents = &*Files.Arctangents;
		Services.Game = &Game;
		Services.Random = &Random;
		Services.Pad = &Pad;
		Services.Clock = &Clock;
		Services.Text.Font = &Files.Resident->Font;
		Services.Text.Tables = &Files.Resident->NameTables;
		Services.Text.State = &Names;
		Services.Text.ButtonMap = MesKernelButtonMap;
		Services.Sound = Sound;
		Services.Options = Options;
		return Services;
	}

	std::string RunWorldMapScenario(const FWorldMapOracleFiles& Files, std::size_t Number, int32 Ticks, FWorldMapOracleSession& Session,
		FWorldMapOracleHooks& Hooks)
	{
		const FWorldMapScenario& Scenario = Scenarios[Number];
		const FWorldMapServices Services = Session.Services(Files);
		auto Runtime = std::make_unique<FWorldMapRuntime>(Services);
		if (const std::optional<FError> Entered = Runtime->Enter())
		{
			return "the entry: " + Entered->Message;
		}
		if (Hooks.Entered)
		{
			if (const std::optional<FError> Error = Hooks.Entered(*Runtime))
			{
				return "the entry's drawing: " + Error->Message;
			}
		}
		const std::vector<FOraclePadStep> Script = PadScript(PadSeed + static_cast<uint32>(Number), Scenario.Ticks + PauseFrames, Scenario);
		std::shared_ptr<const FWorldMapBattleKeep> Keep;
		int32 Tick = 0;
		for (int32 Frame = 0; Frame < static_cast<int32>(Script.size()) && Tick < Ticks; ++Frame)
		{
			if (Runtime->Request().Kind == EWorldMapRequest::Battle)
			{
				// The battle changes nothing the world map reads; it starts again from what it kept.
				Keep = Runtime->Request().Keep;
				FWorldMapServices Returning = Services;
				Returning.BattleKeep = Keep.get();
				Runtime = std::make_unique<FWorldMapRuntime>(Returning);
				if (const std::optional<FError> Error = Runtime->Enter())
				{
					return "the return at tick " + std::to_string(Tick) + ": " + Error->Message;
				}
				if (Hooks.Entered)
				{
					if (const std::optional<FError> Error = Hooks.Entered(*Runtime))
					{
						return "the return's drawing at tick " + std::to_string(Tick) + ": " + Error->Message;
					}
				}
			}
			const std::vector<FPadState> Held = OracleVblanks(Script[static_cast<std::size_t>(Frame)], Runtime->VblanksPerTick());
			const std::optional<FError> Error = Hooks.Frame ? Hooks.Frame(*Runtime, Held) : Runtime->Tick(Held);
			if (Error)
			{
				return "tick " + std::to_string(Tick) + ": " + Error->Message;
			}
			if (Runtime->Request().Kind == EWorldMapRequest::Menu)
			{
				// The menu changes nothing; the pass ends as it closes.
				if (const std::optional<FError> MenuError = Runtime->ResumeAfterMenu())
				{
					return "the menu at tick " + std::to_string(Tick) + ": " + MenuError->Message;
				}
			}
			if (Runtime->Request().Kind == EWorldMapRequest::None && Runtime->VblanksPerTick() != 2)
			{
				// A frame of the pause, inside the pass.
				continue;
			}
			if (Hooks.Ticked)
			{
				Hooks.Ticked(*Runtime, Tick);
			}
			++Tick;
			if (Runtime->Request().Kind != EWorldMapRequest::None && Runtime->Request().Kind != EWorldMapRequest::Battle)
			{
				break;
			}
		}
		return {};
	}
}
