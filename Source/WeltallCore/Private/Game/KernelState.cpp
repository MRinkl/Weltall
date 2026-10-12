#include "Game/KernelState.h"

#include "Game/GameState.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The opening's logos, the first disc's movie 16 and the second's movie 7 (the same file on each). */
		constexpr uint8 FirstDiscOpeningMovie = 16;
		constexpr uint8 SecondDiscOpeningMovie = 7;
		constexpr uint8 MovieWithSound = 1;
		/** The place words' mask the kernel tests a battle's return to the world map by, and the world map's first place. */
		constexpr uint16 PlaceMapBits = 0x7FF;
		constexpr uint16 FirstWorldMapPlace = 0x400;
		/** A loaded game's place as the title's map reads it: the field's gear-mode bits aside. */
		constexpr uint16 LoadedPlaceBits = 0x3FFF;
		constexpr std::array<uint8, 3> NewGameWindowColour = {0x88, 0x76, 0x54};
		constexpr uint8 NewGameBattleWindowBlend = 2;
	}

	const char* GameProgramName(EGameProgram Program)
	{
		switch (Program)
		{
		case EGameProgram::DebugMenu:
			return "the kernel's debug menu";
		case EGameProgram::Field:
			return "the Field program";
		case EGameProgram::Battle:
			return "the Battle program";
		case EGameProgram::WorldMap:
			return "the world map program";
		case EGameProgram::Battling:
			return "the Battling arena's program";
		case EGameProgram::Menu:
			return "the Menu program";
		case EGameProgram::Movie:
			return "the Movie program";
		}
		return "an unknown program";
	}

	EGameProgram LoadedGameProgram(const FGameState& Game)
	{
		return (Game.Place() & LoadedPlaceBits) < FirstWorldMapPlace ? EGameProgram::Field : EGameProgram::WorldMap;
	}

	FKernelState FKernelState::PowerOn(int32 DiscNumber)
	{
		FKernelState Kernel;
		Kernel.Disc = DiscNumber;
		Kernel.Movie.Type = MovieWithSound;
		Kernel.Movie.NextProgram = EGameProgram::Field;
		Kernel.Movie.NoSkip = 0;
		Kernel.Movie.Number = DiscNumber == 0 ? FirstDiscOpeningMovie : SecondDiscOpeningMovie;
		return Kernel;
	}

	void FKernelState::InitNewGame()
	{
		bBattleDebug = true;
		WindowColour = NewGameWindowColour;
		BattleWindowBlend = NewGameBattleWindowBlend;
	}

	void FKernelState::EndBattle(const FBattleProgramEnd& End, FGameState& Game)
	{
		if (End.Result == BattleWon || End.Result == BattleEscaped || End.Result == BattleResultUnused)
		{
			if (End.bMovie)
			{
				NextProgram = EGameProgram::Movie;
			}
			else if (ChainedBattle != 0)
			{
				NextProgram = EGameProgram::Battle;
			}
			else
			{
				NextProgram = (Game.Place() & PlaceMapBits) >= FirstWorldMapPlace ? EGameProgram::WorldMap : EGameProgram::Field;
			}
		}
		else if (End.Result == BattleLost)
		{
			FieldTrips = 0;
			NextProgram = EGameProgram::Field;
			Game.SetWorldMapPlace(TitleMap, 0, 0, 0);
		}
		if (ChainedBattle == 0)
		{
			bBattleDebug = true;
		}
	}

	std::optional<std::array<uint8, FKernelState::FormationBytes>> FKernelState::Formation(int32 Index) const
	{
		if (Index < 0 || Index >= FormationCount)
		{
			return std::nullopt;
		}
		std::array<uint8, FormationBytes> Record{};
		std::copy_n(Formations.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(Index) * FormationBytes), FormationBytes, Record.begin());
		return Record;
	}
}
