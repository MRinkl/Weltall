#include "WorldMap/WorldMapTables.h"

#include "Formats/Lzss.h"
#include "Foundation/LittleEndian.h"
#include "Media/GameFileIndex.h"

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		/** Where a release's Worldmap program (directory 1, unpacked) keeps each table, as offsets in the program. */
		struct FWorldMapTablesLayout
		{
			EDiscRelease Release;
			int32 ProgramFile;
			std::size_t EraThresholds;
			std::size_t BandThresholds;
			std::size_t Scenes;
			std::size_t WalkTable;
			std::size_t SplitNormals;
			std::size_t SplitPoints;
			std::size_t SlideDirections;
			std::size_t DiagonalWeights;
			std::size_t ForestRegions;
			std::size_t ColliderPasses;
			std::size_t GearSprites;
			std::size_t CameraDistances;
			std::size_t CameraPitches;
			std::size_t FlyingCameraPitches;
			std::size_t CameraClearances;
			std::size_t FlyingCameraClearances;
			std::size_t DockingPlaces;
			std::size_t CloudPlaces;
			std::size_t EmptySlotGear;
			std::size_t VillageVoyage;
			std::size_t FactoryVoyage;
			std::size_t SpinnerPairObjects;
			std::size_t FastSpinnerPairObjects;
			std::size_t ThamesObjects;
			std::size_t ShevatObjects;
			std::size_t LandmarkAObjects;
			std::size_t LandmarkCObjects;
			std::size_t ThamesWaypointsX;
			std::size_t ThamesWaypointsZ;
			std::size_t ShevatAttachments;
			std::size_t StoryObjects;
		};

		constexpr int32 ProgramDirectory = 1;
		constexpr std::array<FWorldMapTablesLayout, 2> Layouts{{
			{EDiscRelease::UsaDisc1, 15, 0x2BA76, 0x2BA8A, 0x2BA8C, 0x2BFD8, 0x2B874, 0x2B974, 0x2B774, 0x2B754, 0x2A8B0, 0x2B690, 0x2B69C, 0x2B724,
				0x2B734, 0x2B73C, 0x2B744, 0x2B74C, 0x2BBD4, 0x2B440, 0x81AC, 0x2B6BC, 0x2B6E4, 0x2BB34, 0x2BB5C, 0x2BB84, 0x2BB98, 0x2BBAC, 0x2BBC0, 0x2B490, 0x2B4A0,
				0x2B4B0, 0x2B4EC},
			{EDiscRelease::UsaDisc2, 15, 0x2BA76, 0x2BA8A, 0x2BA8C, 0x2BFD8, 0x2B874, 0x2B974, 0x2B774, 0x2B754, 0x2A8B0, 0x2B690, 0x2B69C, 0x2B724,
				0x2B734, 0x2B73C, 0x2B744, 0x2B74C, 0x2BBD4, 0x2B440, 0x81AC, 0x2B6BC, 0x2B6E4, 0x2BB34, 0x2BB5C, 0x2BB84, 0x2BB98, 0x2BBAC, 0x2BBC0, 0x2B490, 0x2B4A0,
				0x2B4B0, 0x2B4EC},
		}};

		const FWorldMapTablesLayout* FindLayout(EDiscRelease Release)
		{
			for (const FWorldMapTablesLayout& Candidate : Layouts)
			{
				if (Candidate.Release == Release)
				{
					return &Candidate;
				}
			}
			return nullptr;
		}

		/** A threshold list ends at FFFFh, which no story value passes. */
		constexpr uint16 ThresholdEnd = 0xFFFF;
		constexpr std::size_t ThresholdLimit = 16;
		/** The gear sprite places' four columns of three: palette x, palette y, texture x, texture y. */
		constexpr std::size_t GearSpriteColumn = 8;
		constexpr std::size_t TriggerSize = 16;
		constexpr std::size_t CloudPlaceSize = 16;
		/** The model lists end at -1; none is longer than this. */
		constexpr int16 ListEnd = -1;
		constexpr std::size_t ListLimit = 32;
		/** Past the scene table the program would read its next tables as scenes. */
		constexpr int32 FirstCutsceneScene = 8;
		constexpr int32 LastScene = 18;

		std::vector<uint16> ReadThresholds(std::span<const uint8> Program, std::size_t Offset)
		{
			std::vector<uint16> Values;
			for (std::size_t Index = 0; Index < ThresholdLimit; ++Index)
			{
				const uint16 Value = ReadU16(Program, Offset + 2 * Index);
				if (Value == ThresholdEnd)
				{
					break;
				}
				Values.push_back(Value);
			}
			return Values;
		}

		FWorldMapTrigger ReadTrigger(std::span<const uint8> Program, std::size_t Offset)
		{
			return {ReadS16(Program, Offset), ReadS16(Program, Offset + 2), ReadS16(Program, Offset + 4), ReadS16(Program, Offset + 6),
				ReadS16(Program, Offset + 8), ReadS16(Program, Offset + 10), ReadS16(Program, Offset + 12), ReadS16(Program, Offset + 14)};
		}

		template <std::size_t Count>
		void ReadS16s(std::span<const uint8> Program, std::size_t Offset, std::array<int16, Count>& Values)
		{
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				Values[Index] = ReadS16(Program, Offset + 2 * Index);
			}
		}

		int32 ReadS32(std::span<const uint8> Program, std::size_t Offset)
		{
			return static_cast<int32>(ReadU32(Program, Offset));
		}

		/** Waypoints of x and z, each a word of 8 bytes' record. */
		template <std::size_t Count>
		void ReadWaypoints(std::span<const uint8> Program, std::size_t Offset, std::array<std::array<int16, 2>, Count>& Waypoints)
		{
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				Waypoints[Index] = {ReadS16(Program, Offset + Index * 8), ReadS16(Program, Offset + Index * 8 + 4)};
			}
		}
	}

	TResult<std::vector<uint8>> ReadWorldMapProgram(const FCdImage& Image, const FGameFileIndex& Index, EDiscRelease Release)
	{
		const FWorldMapTablesLayout* Layout = FindLayout(Release);
		if (Layout == nullptr)
		{
			return MakeError("no world map tables are known for this release");
		}
		const FIndexRecord* Record = Index.Find(ProgramDirectory, Layout->ProgramFile);
		if (Record == nullptr)
		{
			return MakeError("the disc lacks the Worldmap program");
		}
		const TResult<std::vector<uint8>> Sectors = ReadFileSectors(Image, *Record);
		if (!Sectors)
		{
			return Sectors.Error();
		}
		return UnpackLzss(*Sectors);
	}

	TResult<FWorldMapTables> FWorldMapTables::Read(const FCdImage& Image, const FGameFileIndex& Index, EDiscRelease Release)
	{
		const TResult<std::vector<uint8>> Unpacked = ReadWorldMapProgram(Image, Index, Release);
		if (!Unpacked)
		{
			return Unpacked.Error();
		}
		const FWorldMapTablesLayout* Layout = FindLayout(Release);
		const std::span<const uint8> Program = *Unpacked;
		if (Program.size() < Layout->WalkTable + TravelModeCount * TerrainTypeCount * 2)
		{
			return MakeError("the Worldmap program is too short to hold its tables");
		}

		FWorldMapTables Tables;
		Tables.EraThresholds = ReadThresholds(Program, Layout->EraThresholds);
		Tables.BandThresholds = ReadThresholds(Program, Layout->BandThresholds);
		if (Tables.EraThresholds.size() + 1 != EraCount)
		{
			return MakeError("the Worldmap program's era thresholds are not the eight known");
		}
		for (std::size_t Entry = 0; Entry < Tables.Scenes.size(); ++Entry)
		{
			const std::size_t At = Layout->Scenes + Entry * 8;
			Tables.Scenes[Entry] = {ReadS16(Program, At), ReadS16(Program, At + 2), ReadS16(Program, At + 4), ReadS16(Program, At + 6)};
		}
		for (std::size_t Mode = 0; Mode < TravelModeCount; ++Mode)
		{
			ReadS16s(Program, Layout->WalkTable + Mode * TerrainTypeCount * 2, Tables.WalkTable[Mode]);
		}
		for (std::size_t Kind = 0; Kind < SplitKindCount; ++Kind)
		{
			const std::size_t Normal = Layout->SplitNormals + Kind * 16;
			const std::size_t Point = Layout->SplitPoints + Kind * 16;
			Tables.SplitLines[Kind] = {ReadS32(Program, Normal), ReadS32(Program, Normal + 8), ReadS32(Program, Point), ReadS32(Program, Point + 8)};
			const std::size_t Slide = Layout->SlideDirections + Kind * 16;
			Tables.SlideDirections[Kind] = {ReadS32(Program, Slide), ReadS32(Program, Slide + 8)};
		}
		for (std::size_t Weight = 0; Weight < Tables.DiagonalWeights.size(); ++Weight)
		{
			Tables.DiagonalWeights[Weight] = ReadS32(Program, Layout->DiagonalWeights + Weight * 8);
		}
		ReadS16s(Program, Layout->ForestRegions, Tables.ForestRegions);
		ReadS16s(Program, Layout->ColliderPasses, Tables.ColliderPasses);
		for (std::size_t Slot = 0; Slot < Tables.GearSprites.size(); ++Slot)
		{
			const std::size_t At = Layout->GearSprites + Slot * 2;
			Tables.GearSprites[Slot] = {ReadS16(Program, At), ReadS16(Program, At + GearSpriteColumn), ReadS16(Program, At + 2 * GearSpriteColumn),
				ReadS16(Program, At + 3 * GearSpriteColumn)};
		}
		for (std::size_t Preset = 0; Preset < Tables.Camera.Distances.size(); ++Preset)
		{
			Tables.Camera.Distances[Preset] = ReadS32(Program, Layout->CameraDistances + Preset * 4);
		}
		ReadS16s(Program, Layout->CameraPitches, Tables.Camera.Pitches);
		ReadS16s(Program, Layout->FlyingCameraPitches, Tables.Camera.FlyingPitches);
		ReadS16s(Program, Layout->CameraClearances, Tables.Camera.Clearances);
		ReadS16s(Program, Layout->FlyingCameraClearances, Tables.Camera.FlyingClearances);
		Tables.ThamesPlace = ReadTrigger(Program, Layout->DockingPlaces);
		Tables.ShevatPlace = ReadTrigger(Program, Layout->DockingPlaces + TriggerSize);
		Tables.InteriorPlace = ReadTrigger(Program, Layout->DockingPlaces + 2 * TriggerSize);
		for (std::size_t Place = 0; Place < Tables.CloudPlaces.size(); ++Place)
		{
			const std::size_t At = Layout->CloudPlaces + Place * CloudPlaceSize;
			Tables.CloudPlaces[Place] = {ReadS32(Program, At), ReadS32(Program, At + 8)};
		}
		Tables.EmptySlotGear = Program[Layout->EmptySlotGear];
		ReadWaypoints(Program, Layout->VillageVoyage, Tables.VillageVoyage);
		ReadWaypoints(Program, Layout->FactoryVoyage, Tables.FactoryVoyage);
		for (std::size_t Era = 0; Era < EraCount; ++Era)
		{
			for (std::size_t Part = 0; Part < 2; ++Part)
			{
				Tables.SpinnerPairObjects[Era][Part] = ReadU16(Program, Layout->SpinnerPairObjects + Era * 4 + Part * 2);
				Tables.FastSpinnerPairObjects[Era][Part] = ReadU16(Program, Layout->FastSpinnerPairObjects + Era * 4 + Part * 2);
			}
			Tables.ThamesObjects[Era] = ReadU16(Program, Layout->ThamesObjects + Era * 2);
			Tables.ShevatObjects[Era] = ReadU16(Program, Layout->ShevatObjects + Era * 2);
			Tables.LandmarkAObjects[Era] = ReadU16(Program, Layout->LandmarkAObjects + Era * 2);
			Tables.LandmarkCObjects[Era] = ReadU16(Program, Layout->LandmarkCObjects + Era * 2);
		}
		for (std::size_t Waypoint = 0; Waypoint < Tables.ThamesWaypoints.size(); ++Waypoint)
		{
			Tables.ThamesWaypoints[Waypoint] = {ReadU16(Program, Layout->ThamesWaypointsX + Waypoint * 2), ReadU16(Program, Layout->ThamesWaypointsZ + Waypoint * 2)};
		}
		for (std::size_t At = Layout->ShevatAttachments; ReadS16(Program, At) != ListEnd && Tables.ShevatAttachments.size() < ListLimit; At += 4)
		{
			Tables.ShevatAttachments.push_back({ReadS16(Program, At), ReadS16(Program, At + 2)});
		}
		for (std::size_t At = Layout->StoryObjects; ReadS16(Program, At) != ListEnd && Tables.StoryObjects.size() < ListLimit; At += 2)
		{
			Tables.StoryObjects.push_back(ReadS16(Program, At));
		}
		TResult<FWorldMapCutsceneTables> Cutscenes = ReadWorldMapCutsceneTables(Program, Release);
		if (!Cutscenes)
		{
			return Cutscenes.Error();
		}
		Tables.Cutscenes = std::move(*Cutscenes);
		return Tables;
	}

	TResult<FWorldMapSceneChoice> ChooseWorldMapScene(const FWorldMapTables& Tables, int32 Scene, uint16 StoryVariable)
	{
		FWorldMapSceneChoice Choice;
		if (Scene < 0 || Scene > LastScene)
		{
			return MakeError("world map scene " + std::to_string(Scene) + " lies past the program's scene table");
		}
		if (Scene < FirstCutsceneScene)
		{
			// The count stops at the first threshold above the value; the list's end, FFFFh, stops every value but itself.
			if (StoryVariable == ThresholdEnd)
			{
				return MakeError("story value FFFFh passes every world map era threshold");
			}
			int32 Era = 0;
			while (Era < static_cast<int32>(Tables.EraThresholds.size()) && Tables.EraThresholds[static_cast<std::size_t>(Era)] <= StoryVariable)
			{
				++Era;
			}
			Choice.Era = Era;
			Choice.Entry = Era + 1;
		}
		else
		{
			Choice.Entry = Scene + 2;
		}
		Choice.Scene = Tables.Scenes[static_cast<std::size_t>(Choice.Entry)];
		return Choice;
	}
}
