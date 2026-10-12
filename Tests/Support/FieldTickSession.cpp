#include "Support/FieldTickSession.h"

#include "Support/XorShift32.h"

#include <iterator>
#include <sstream>

namespace Weltall::Testing
{
	namespace
	{
		constexpr uint32 PadSeed = 0x71C00000;

		/** Physical buttons in the controller's report word (bit 15 Left to bit 0 L2). */
		constexpr uint16 L1 = 0x04, R1 = 0x08, Triangle = 0x10, Circle = 0x20, Cross = 0x40;
		constexpr uint16 Up = 0x1000, Right = 0x2000, Down = 0x4000, Left = 0x8000;
		constexpr uint16 Directions[] = {0, Up, Up | Right, Right, Right | Down, Down, Down | Left, Left, Left | Up};
		/** The jump, talk and turn buttons, never the menu's (physical Square), which stops the field. */
		constexpr uint16 Presses[] = {0, 0, 0, Triangle, Cross, L1, R1, 0};

		FPadState FromReport(uint16 Report)
		{
			return {static_cast<uint16>(Report >> 8 | (Report & 0xFF) << 8)};
		}
	}

	std::vector<FOraclePadStep> OraclePadScript(int32 Map, int32 Ticks)
	{
		FXorShift32 G(PadSeed + static_cast<uint32>(Map));
		std::vector<FOraclePadStep> Script;
		while (static_cast<int32>(Script.size()) < Ticks)
		{
			const uint32 Length = 4 + G.Next() % 20;
			uint16 Held = Directions[G.Next() % std::size(Directions)];
			if (G.Next() % 3 == 0)
			{
				Held |= Circle;
			}
			const uint16 Pressed = Presses[G.Next() % std::size(Presses)];
			Script.push_back({Held, Pressed});
			for (uint32 Tick = 1; Tick < Length; ++Tick)
			{
				Script.push_back({Held, 0});
			}
		}
		Script.resize(static_cast<std::size_t>(Ticks));
		return Script;
	}

	std::vector<FPadState> OracleVblanks(const FOraclePadStep& Step, int32 Vblanks)
	{
		std::vector<FPadState> Held(static_cast<std::size_t>(Vblanks), FromReport(Step.Held));
		Held[0] = FromReport(static_cast<uint16>(Step.Held | Step.Pressed));
		return Held;
	}

	int32 FirstDifference(const std::string& Actual, const std::string& Original)
	{
		std::istringstream A(Actual);
		std::istringstream B(Original);
		std::string Ours;
		std::string Theirs;
		for (int32 Tick = 0;; ++Tick)
		{
			const bool bOurs = static_cast<bool>(A >> Ours);
			const bool bTheirs = static_cast<bool>(B >> Theirs);
			if (!bOurs && !bTheirs)
			{
				return -1;
			}
			if (bOurs != bTheirs || Ours != Theirs)
			{
				return Tick;
			}
		}
	}

	std::vector<FOracleMapVariable> OracleMapVariables(int32 Map)
	{
		switch (Map)
		{
		case 305:
			return {{0x1FE, 0x0180}};
		case 306:
			return {{0x000, 149}, {0x1FE, 0x0180}};
		case 307:
			return {{0x000, 340}, {0x1FC, 0x8040}, {0x1FE, 0x0117}};
		default:
			return {};
		}
	}

	FOracleTickSession::FOracleTickSession(const FOracleFieldFiles& Files, int32 Map)
		: Game(Files.NewGame), Party(Files.Disc->Image(), Files.Index), CockpitHud(Files.CockpitHud)
	{
		for (const FOracleMapVariable& Variable : OracleMapVariables(Map))
		{
			Game.SetSavedEventVariable(static_cast<int32>(Variable.Address / 2), Variable.Value);
		}
	}

	FFieldServices FOracleTickSession::Services(const FOracleFieldFiles& Files, const FMesResident& Resident)
	{
		FFieldServices Result;
		Result.Image = &Files.Disc->Image();
		Result.Index = &Files.Index;
		Result.Arctangents = &Files.Arctangents;
		Result.Game = &Game;
		Result.Random = &Random;
		Result.Songs = &Songs;
		Result.PartySprites = &Party;
		Result.Pad = &Pad;
		Result.Clock = &Clock;
		Result.Text.Font = &Resident.Font;
		Result.Text.Tables = &Resident.NameTables;
		Result.Text.State = &Names;
		Result.Text.ButtonMap = MesKernelButtonMap;
		Result.Options = FEventOptions::Original();
		return Result;
	}
}
