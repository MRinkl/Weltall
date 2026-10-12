#include "Field/FieldPlay.h"

#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Text/MesWindow.h"

namespace Weltall
{
	FFieldPlay::FFieldPlay(const FCdImage& InImage, const FGameFileIndex& InIndex, FGteArctangents InArctangents, FMesResident InText, FGameState InGame,
		FFieldDrawingTables InTables)
		: Arctangents(std::move(InArctangents)), Text(std::move(InText)), GameState(std::move(InGame)), Names(GameState), PartySprites(InImage, InIndex),
		  Tables(std::move(InTables))
	{
	}

	FFieldPlay::~FFieldPlay() = default;

	TResult<std::unique_ptr<FFieldPlay>> FFieldPlay::NewGame(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Executable,
		EDiscRelease Release, IFieldSongs& Songs, IFieldSoundEffects* Effects)
	{
		TResult<FGteArctangents> Arctangents = FGteArctangents::Read(Executable);
		if (!Arctangents)
		{
			return Arctangents.Error();
		}
		TResult<FMesResident> Text = LoadMesResident(Image, Index, Executable);
		if (!Text)
		{
			return Text.Error();
		}
		TResult<FGameState> Game = LoadNewGameState(Image, Index, Text->NameTables);
		if (!Game)
		{
			return Game.Error();
		}
		TResult<FFieldDrawingTables> Tables = FFieldDrawingTables::Read(Image, Index, Executable, Release);
		if (!Tables)
		{
			return Tables.Error();
		}
		std::unique_ptr<FFieldPlay> Play(new FFieldPlay(Image, Index, std::move(*Arctangents), std::move(*Text), std::move(*Game), std::move(*Tables)));
		TResult<FFieldPresenter> Presenter = FFieldPresenter::Start(Play->Tables, Image, Index);
		if (!Presenter)
		{
			return Presenter.Error();
		}
		Play->Presenter.emplace(std::move(*Presenter));
		FFieldServices Services;
		Services.Image = &Image;
		Services.Index = &Index;
		Services.Arctangents = &Play->Arctangents;
		Services.Game = &Play->GameState;
		Services.Random = &Play->Random;
		Services.Songs = &Songs;
		Services.SoundEffects = Effects;
		Services.PartySprites = &Play->PartySprites;
		Services.Pad = &Play->Pad;
		Services.Clock = &Play->Clock;
		Services.Text.Font = &Play->Text.Font;
		Services.Text.Tables = &Play->Text.NameTables;
		Services.Text.State = &Play->Names;
		Services.Text.ButtonMap = MesKernelButtonMap;
		Play->Runtime = std::make_unique<FFieldRuntime>(Services);
		return Play;
	}

	std::optional<FError> FFieldPlay::EnterMap(int32 Number)
	{
		if (auto Error = Runtime->EnterMap(Number))
		{
			return Error;
		}
		return Presenter->EnterMap(*Runtime);
	}

	std::optional<FError> FFieldPlay::Tick(std::span<const FPadState> HeldPerVblank, EFieldCulling Culling, FFieldFrame& Frame)
	{
		return Presenter->Tick(*Runtime, HeldPerVblank, Culling, Frame);
	}
}
