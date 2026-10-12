#include "Support/BattleProgramDisc.h"

#include "Field/FieldMap.h"
#include "Formats/Lzss.h"
#include "Media/PsxExecutable.h"
#include "Support/GameText.h"
#include "WorldMap/WorldMapScene.h"
#include "WorldMap/WorldMapTables.h"

namespace Weltall::Testing
{
	namespace
	{
		/** A world map scene group's header file, and how far apart the groups' headers lie in their directory. */
		constexpr int32 FirstWorldMapGroupHeader = 43;
		constexpr int32 WorldMapGroupFiles = 11;

		/** The sixteen formations of a world map encounter region, as the kernel's table takes them. */
		std::optional<std::vector<uint8>> LoadWorldMapRegion(const FCdImage& Image, const FGameFileIndex& Index, int32 Group, int32 Region)
		{
			const FIndexRecord* Record = Index.Find(WorldMapFiles::Directory, WorldMapFiles::Archive(FirstWorldMapGroupHeader + WorldMapGroupFiles * Group));
			if (Record == nullptr)
			{
				return std::nullopt;
			}
			const auto Packed = ReadFileSectors(Image, *Record);
			const auto Archive = Packed ? UnpackLzss(*Packed) : TResult<std::vector<uint8>>(Packed.Error());
			const auto Scene = Archive ? ParseWorldMapArchive(*Archive) : TResult<FWorldMapScene>(Archive.Error());
			if (!Scene || Region < 0 || static_cast<std::size_t>(Region) >= Scene->Regions.size())
			{
				return std::nullopt;
			}
			std::vector<uint8> Formations;
			for (const auto& Formation : Scene->Regions[static_cast<std::size_t>(Region)].Formations)
			{
				Formations.insert(Formations.end(), Formation.begin(), Formation.end());
			}
			return Formations;
		}
	}

	FBattlePresentationFiles FBattleProgramDisc::PresentationFiles() const
	{
		FBattlePresentationFiles Files;
		Files.Image = &Disc->Image();
		Files.Index = &*Index;
		Files.Arctangents = &*Arctangents;
		Files.SpriteFiles = &*SpriteFiles;
		Files.Sheet = &*Sheet;
		Files.MarkerModel = &*MarkerModel;
		Files.ObjectFiles = &*ObjectFiles;
		Files.EventFiles = &*EventFiles;
		Files.StartSounds = &*StartSounds;
		return Files;
	}

	bool LoadBattleProgramDisc(const FIso9660& Iso, std::span<const int32> Maps, FBattleProgramDisc& Loaded)
	{
		Loaded.Disc = &Iso;
		const FCdImage& Image = Iso.Image();
		const auto Executable = ReadBootExecutable(Iso);
		if (!Executable)
		{
			return false;
		}
		auto Index = FGameFileIndex::Read(*Executable);
		if (!Index)
		{
			return false;
		}
		Loaded.Index = std::move(*Index);
		auto Tables = FBattleTables::Load(Image, *Loaded.Index);
		auto Program = FBattleProgramTables::Load(Image, *Loaded.Index);
		auto Growth = FBattleGrowthTable::Load(Image, *Loaded.Index);
		auto Events = FBattleEventArchive::Load(Image, *Loaded.Index);
		auto Arctangents = FGteArctangents::Read(*Executable);
		auto SpriteFiles = FBattleSpriteFiles::Load(Image, *Loaded.Index);
		auto Sheet = FBattleHudSheet::Load(Image, *Loaded.Index);
		auto HudResources = FBattleHudResources::Load(Image, *Loaded.Index, *Executable);
		auto MarkerModel = FBattleMarkerModel::Read(*Executable);
		auto ObjectFiles = FBattleObjectDisc::Load(Image, *Loaded.Index);
		auto StartSounds = FBattleStartSounds::Read(*Executable);
		if (!Tables || !Program || !Growth || !Events || !Arctangents || !SpriteFiles || !Sheet || !HudResources || !MarkerModel || !ObjectFiles
			|| !StartSounds)
		{
			return false;
		}
		auto EventFiles = FBattleEventFiles::Load(Image, *Loaded.Index, Program->VoiceSets);
		if (!EventFiles)
		{
			return false;
		}
		Loaded.EventFiles = std::move(*EventFiles);
		Loaded.Tables = std::move(*Tables);
		Loaded.Program = std::move(*Program);
		Loaded.Growth = std::move(*Growth);
		Loaded.Events = std::move(*Events);
		Loaded.Arctangents = *Arctangents;
		Loaded.SpriteFiles = std::move(*SpriteFiles);
		Loaded.Sheet = std::move(*Sheet);
		Loaded.HudResources = std::move(*HudResources);
		Loaded.MarkerModel = std::move(*MarkerModel);
		Loaded.ObjectFiles = std::move(*ObjectFiles);
		Loaded.StartSounds = *StartSounds;
		Loaded.Resident = LoadMesResident(Iso);
		auto NewGame = LoadNewGameState(Image, *Loaded.Index, Loaded.Resident.NameTables);
		if (!NewGame)
		{
			return false;
		}
		Loaded.NewGame = std::move(*NewGame);
		for (const int32 Map : Maps)
		{
			if (Map >= WorldMapRegionBase && !Loaded.Encounters.contains(Map))
			{
				const int32 Number = Map - WorldMapRegionBase;
				auto Formations = LoadWorldMapRegion(Image, *Loaded.Index, Number / WorldMapRegionsPerGroup, Number % WorldMapRegionsPerGroup);
				if (!Formations)
				{
					return false;
				}
				Loaded.Encounters[Map] = std::move(*Formations);
			}
			if (!Loaded.Encounters.contains(Map))
			{
				auto Loads = LoadFieldMap(Image, *Loaded.Index, Map);
				if (!Loads)
				{
					return false;
				}
				Loaded.Encounters[Map] = Loads->Encounters;
			}
		}
		return true;
	}
}
