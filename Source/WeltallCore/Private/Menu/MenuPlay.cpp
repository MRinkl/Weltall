#include "Menu/MenuPlay.h"

#include "Media/PsxExecutable.h"

#include <utility>

namespace Weltall
{
	FMenuPlay::FMenuPlay(FGameFileIndex InIndex, FMesResident InText, FMenuResources InResources, FGameState InGame)
		: Index(std::move(InIndex)), Text(std::move(InText)), Resources(std::move(InResources)), GameState(std::move(InGame)), Names(GameState)
	{
	}

	FMenuPlay::~FMenuPlay() = default;

	TResult<std::unique_ptr<FMenuPlay>> FMenuPlay::NewGame(const FCdImage& Image, const FPsxExecutable& Executable, int32 Disc, const FMenuRequest& Request,
		IMenuSound* Sound, IMenuSettings* Settings, IMenuKeys* Keys, std::function<void()> Quit, bool bArrangesParty)
	{
		TResult<FGameFileIndex> Index = FGameFileIndex::Read(Executable);
		if (!Index)
		{
			return Index.Error();
		}
		TResult<FMesResident> Text = LoadMesResident(Image, *Index, Executable);
		if (!Text)
		{
			return Text.Error();
		}
		TResult<FMenuResources> Resources = FMenuResources::Load(Image, *Index, Executable);
		if (!Resources)
		{
			return Resources.Error();
		}
		TResult<FGameState> Game = LoadNewGameState(Image, *Index, Text->NameTables);
		if (!Game)
		{
			return Game.Error();
		}
		std::unique_ptr<FMenuPlay> Play(new FMenuPlay(std::move(*Index), std::move(*Text), std::move(*Resources), std::move(*Game)));
		FMenuServices Services;
		Services.Image = &Image;
		Services.Index = &Play->Index;
		Services.Resources = &Play->Resources;
		Services.Game = &Play->GameState;
		Services.Random = &Play->Random;
		Services.Pad = &Play->Pad;
		Services.Clock = &Play->Clock;
		Services.Sound = Sound;
		Services.Settings = Settings;
		Services.Keys = Keys;
		Services.Quit = std::move(Quit);
		Services.bArrangesParty = bArrangesParty;
		Services.Text.Font = &Play->Text.Font;
		Services.Text.Tables = &Play->Text.NameTables;
		Services.Text.State = &Play->Names;
		Services.Text.ButtonMap = MesKernelButtonMap;
		Services.Disc = Disc;
		Play->Program = std::make_unique<FMenuProgram>(Services, Request);
		return Play;
	}

	TResult<bool> FMenuPlay::Frame(FPadState Held)
	{
		return Program->Frame(Held);
	}
}
