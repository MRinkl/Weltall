#include "Field/EventMachine.h"
#include "Field/EventPlacement.h"
#include "Field/FieldDiscFiles.h"
#include "Field/FieldMap.h"
#include "Field/FieldPartySprites.h"
#include "Field/FieldSprites.h"
#include "Field/FieldSpriteFiles.h"
#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Graphics/GteMath.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/FieldSongsModel.h"
#include "Support/GameDiscs.h"
#include "Support/NewGameParty.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <map>
#include <string>

using namespace Weltall;

namespace
{
	/**
	 * What stopped an init, by the error: the opcode of an instruction not implemented; the opcode and
	 * "stale floor" for a placement on a layer the actor does not walk on (the camera's), which the port
	 * refuses (the original takes the floor from stale stack there); an empty string for any other error.
	 */
	std::string StopReason(const std::string& Message)
	{
		const std::string Tail = " is not implemented";
		const std::size_t End = Message.rfind(Tail);
		if (End != std::string::npos && End + Tail.size() == Message.size())
		{
			const std::size_t Start = Message.rfind(": opcode ", End);
			return Start == std::string::npos ? std::string{} : Message.substr(Start + 9, End - Start - 9);
		}
		const std::size_t Opcode = Message.find(", opcode ");
		const std::size_t Colon = Message.find(':', Opcode);
		if (Opcode != std::string::npos && Colon != std::string::npos && Message.find("which it does not walk on") != std::string::npos)
		{
			return Message.substr(Opcode + 9, Colon - Opcode - 9) + " stale floor";
		}
		return {};
	}

	struct FInitCensus
	{
		int32 Maps = 0;
		int32 Completed = 0;
		/** Each opcode that stopped an init, with the number of maps it stopped: "BC:333 A0:16". */
		std::string StoppedBy;
	};

	/**
	 * Runs the init scripts of maps First to End - 1 of the first disc, from zeroed variables with a
	 * new game's party, its sprite files loaded as the field enters its first map.
	 */
	FInitCensus RunInits(int32 First, int32 End)
	{
		REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
		const auto Executable = ReadBootExecutable(*Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		const auto Arctangents = FGteArctangents::Read(*Executable);
		REQUIRE(Arctangents);
		FGameState NewGame;
		REQUIRE(Testing::CopyNewGameParty(Disc->Image(), *Index, NewGame));
		FPartySpriteFiles EnteredParty(Disc->Image(), *Index);
		FFieldDiscFiles Files(&Disc->Image(), &*Index);
		std::array<uint8, 3> FieldParty{};
		REQUIRE_FALSE(EnteredParty.LoadForMap(false, NewGame, FieldParty));
		FInitCensus Census;
		std::map<std::string, int32> Stops;
		for (int32 Number = First; Number < End; ++Number)
		{
			INFO("map " << Number);
			auto Map = LoadFieldMap(Disc->Image(), *Index, Number);
			REQUIRE(Map);
			const auto Scripts = FEventScripts::Parse(Map->EventScripts);
			REQUIRE(Scripts);
			REQUIRE(static_cast<std::size_t>(Scripts->ActorCount()) <= Map->Actors.size());
			const auto SpriteFiles = ParseFieldSpriteFiles(Map->SpriteSets);
			REQUIRE(SpriteFiles);
			FPsxRandom Random;
			FGameState Game = NewGame;
			FPartySpriteFiles Party = EnteredParty;
			FEventContext Context;
			Context.Scripts = &*Scripts;
			Context.MapNumber = Number;
			Context.RunningMap = Number;
			Context.Zones = Map->Zones;
			Context.MapModels = Map->Models;
			Context.Walkmesh = &Map->Walkmesh;
			Context.Arctangents = &*Arctangents;
			Context.Files = &Files;
			Context.Random = &Random;
			Context.GameState = &Game;
			Context.Actors = CreateFieldActors(Map->Actors);
			Context.States.resize(static_cast<std::size_t>(Scripts->ActorCount()));
			for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
			{
				FEventActorState& State = Context.States[Actor];
				State.Reset(Random.Next());
				REQUIRE(PlaceActorOnLoad(State, Context.Actors[Actor], Map->Walkmesh));
			}
			Context.PartyCharacters = FieldParty;
			Context.PartySprites = &Party;
			const auto Messages = FMessageTable::Parse(Map->Messages);
			REQUIRE(Messages);
			Context.Messages.Table = *Messages;
			Context.Messages.EnterMap();
			FFieldSprites Sprites(*SpriteFiles, Map->SpriteSlots, Party, Context.States, Disc->Image(), *Index);
			Sprites.System().Random = &Random;
			Sprites.System().Arctangents = &*Arctangents;
			Context.Models = &Sprites;
			Testing::FFieldSongsModel Songs;
			Context.Songs = &Songs;
			++Census.Maps;
			const auto Error = FEventMachine::InitMap(Context);
			if (auto SpriteError = Sprites.System().FaultError())
			{
				FAIL(SpriteError->Message);
			}
			if (!Error)
			{
				++Census.Completed;
				continue;
			}
			const std::string Reason = StopReason(Error->Message);
			if (Reason.empty())
			{
				FAIL(Error->Message);
			}
			++Stops[Reason];
		}
		for (const auto& [Reason, Count] : Stops)
		{
			Census.StoppedBy += (Census.StoppedBy.empty() ? "" : " ") + Reason + ":" + std::to_string(Count);
		}
		return Census;
	}
}

// Every map's init runs from zeroed variables with a new game's party until it ends or reaches an
// instruction the port does not run yet; the census of those instructions follows the port's progress.
TEST_CASE("Every map's event scripts parse and their inits run to their end or an unimplemented opcode, maps 0 to 364", "[Field][Content]")
{
	const FInitCensus Census = RunInits(0, 365);
	CHECK(Census.Maps == 365);
	CHECK(Census.Completed == 365);
	CHECK(Census.StoppedBy.empty());
}

TEST_CASE("Every map's event scripts parse and their inits run to their end or an unimplemented opcode, maps 365 to 729", "[Field][Content]")
{
	const FInitCensus Census = RunInits(365, 730);
	CHECK(Census.Maps == 365);
	CHECK(Census.Completed == 365);
	CHECK(Census.StoppedBy.empty());
}

TEST_CASE("A map's entrance records are where an independent reading finds them", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Lahan = LoadFieldMap(Disc->Image(), *Index, 1);
	REQUIRE(Lahan);
	const auto Scripts = FEventScripts::Parse(Lahan->EventScripts);
	REQUIRE(Scripts);
	const auto First = Scripts->Entrance(0);
	REQUIRE(First);
	CHECK((First->X == -906 && First->Z == -863 && First->Layer == 0));
	CHECK(First->CameraDirection == FEventEntrance::KeepDirection);
	CHECK(First->Facing == 1);
	const auto Fifth = Scripts->Entrance(4);
	REQUIRE(Fifth);
	CHECK((Fifth->X == 535 && Fifth->Z == 1052 && Fifth->Facing == 1));
}
