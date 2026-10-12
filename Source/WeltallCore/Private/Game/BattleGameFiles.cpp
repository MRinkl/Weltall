#include "Game/BattleGameFiles.h"

#include "Graphics/GteMath.h"

namespace Weltall
{
	TResult<std::unique_ptr<FBattleGameFiles>> FBattleGameFiles::Load(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Executable)
	{
		auto Files = std::make_unique<FBattleGameFiles>();
		TResult<FBattleTables> Tables = FBattleTables::Load(Image, Index);
		if (!Tables)
		{
			return Tables.Error();
		}
		Files->Tables = std::move(*Tables);
		TResult<FBattleProgramTables> Program = FBattleProgramTables::Load(Image, Index);
		if (!Program)
		{
			return Program.Error();
		}
		Files->Program = std::move(*Program);
		TResult<FBattleGrowthTable> Growth = FBattleGrowthTable::Load(Image, Index);
		if (!Growth)
		{
			return Growth.Error();
		}
		Files->Growth = std::move(*Growth);
		TResult<FBattleEventArchive> Events = FBattleEventArchive::Load(Image, Index);
		if (!Events)
		{
			return Events.Error();
		}
		Files->Events = std::move(*Events);
		TResult<FBattleSpriteFiles> SpriteFiles = FBattleSpriteFiles::Load(Image, Index);
		if (!SpriteFiles)
		{
			return SpriteFiles.Error();
		}
		Files->SpriteFiles = std::move(*SpriteFiles);
		TResult<FBattleHudSheet> Sheet = FBattleHudSheet::Load(Image, Index);
		if (!Sheet)
		{
			return Sheet.Error();
		}
		Files->Sheet = std::move(*Sheet);
		TResult<FBattleHudResources> HudResources = FBattleHudResources::Load(Image, Index, Executable);
		if (!HudResources)
		{
			return HudResources.Error();
		}
		Files->HudResources = std::move(*HudResources);
		TResult<FBattleMarkerModel> MarkerModel = FBattleMarkerModel::Read(Executable);
		if (!MarkerModel)
		{
			return MarkerModel.Error();
		}
		Files->MarkerModel = std::move(*MarkerModel);
		TResult<FBattleObjectDisc> ObjectFiles = FBattleObjectDisc::Load(Image, Index);
		if (!ObjectFiles)
		{
			return ObjectFiles.Error();
		}
		Files->ObjectFiles = std::move(*ObjectFiles);
		TResult<FBattleEventFiles> EventFiles = FBattleEventFiles::Load(Image, Index, Files->Program.VoiceSets);
		if (!EventFiles)
		{
			return EventFiles.Error();
		}
		Files->EventFiles = std::move(*EventFiles);
		TResult<FBattleStartSounds> StartSounds = FBattleStartSounds::Read(Executable);
		if (!StartSounds)
		{
			return StartSounds.Error();
		}
		Files->StartSounds = *StartSounds;
		return Files;
	}

	FBattlePresentationFiles FBattleGameFiles::PresentationFiles(const FCdImage& Image, const FGameFileIndex& Index, const FGteArctangents& Arctangents) const
	{
		FBattlePresentationFiles Files;
		Files.Image = &Image;
		Files.Index = &Index;
		Files.Arctangents = &Arctangents;
		Files.SpriteFiles = &SpriteFiles;
		Files.Sheet = &Sheet;
		Files.MarkerModel = &MarkerModel;
		Files.ObjectFiles = &ObjectFiles;
		Files.EventFiles = &EventFiles;
		Files.StartSounds = &StartSounds;
		return Files;
	}
}
