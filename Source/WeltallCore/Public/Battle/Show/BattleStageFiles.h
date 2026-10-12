#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Gears/GearFiles.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/** A corner of a stage's floor mesh. */
	struct FBattleFloorVertex
	{
		int16 X = 0;
		int16 Y = 0;
		int16 Z = 0;
	};

	/** A triangle of a stage's floor: its corners, the triangle beyond each of its edges (negative for none), and its kind of ground. */
	struct FBattleFloorTriangle
	{
		std::array<int16, 3> Corners{};
		std::array<int16, 3> Neighbours{};
		uint8 Attribute = 0;
	};

	/** A round obstacle on the stage that the performer's object is pushed out of. */
	struct FBattleStageObstacle
	{
		int16 X = 0;
		int16 Z = 0;
		int16 Radius = 0;
	};

	/** What one of a stage's six elements is. */
	namespace BattleStageElementKinds
	{
		inline constexpr int16 Panorama = 1;
		/** A panorama whose ground takes the stage's clear colour. */
		inline constexpr int16 PanoramaOverClear = 2;
		inline constexpr int16 FloorDome = 3;
		inline constexpr int16 ClearToColour = 5;
		inline constexpr int16 VramScroll = 7;
	}

	/**
	 * One of a stage's six elements: a point (the panorama's horizon, the dome's centre) and twelve
	 * words of parameters, the fifth its kind.
	 */
	struct FBattleStageElement
	{
		static constexpr int32 WordCount = 12;
		static constexpr int32 KindWord = 4;

		std::array<int32, 4> Point{};
		std::array<int16, WordCount> Words{};

		int16 Kind() const { return Words[KindWord]; }
	};

	/** Where a stage's camera looks from and to at one of its preset places. */
	struct FBattleCameraPreset
	{
		FGteVector Target{};
		FGteVector Eye{};
	};

	/**
	 * A battle stage's record as the Battle program keeps it (the file's bytes after its length word):
	 * the places and routes the rules read, then what the show reads, which is read from the bytes at
	 * the offsets the program reads so that an index the program takes past a table reads what it
	 * reads (a camera preset past the twelve reads the floor's offsets). The floor mesh, the obstacles,
	 * the VRAM scrolls' band speeds and the stage object's sequences are the record's own tables.
	 */
	class FBattleStageRecord
	{
	public:
		static constexpr int32 ElementCount = 6;
		static constexpr int32 CameraPresetCount = 12;
		static constexpr int32 ScrollSpeedTables = 3;

		/** From the stage's record file, as the kernel reads it: its length, then the record. */
		WELTALLCORE_API static TResult<FBattleStageRecord> Parse(std::span<const uint8> File);

		std::span<const uint8> Bytes() const { return Record; }

		/** The kinds of ground the rules read (the gears' agility on it). */
		WELTALLCORE_API std::array<uint8, 4> TerrainBytes() const;
		/** Every object's model scale, 4096 = 1. */
		WELTALLCORE_API int16 ObjectScale() const;
		/** The sizes of the objects' animation-track pool and of the trails' pool. */
		WELTALLCORE_API int16 TrackPoolSize() const;
		WELTALLCORE_API int16 TrailPoolSize() const;
		/** The camera's bounds on the ground: its eye stays inside (the largest x, the smallest x, the smallest z, the largest z). */
		WELTALLCORE_API std::array<int16, 4> CameraBounds() const;
		/**
		 * The flag the stage's build leaves: nonzero when the screen clears to the stage's clear colour rather than the stage
		 * drawing its background. The record's byte as the program keeps it, 1 when an element asks for the clear colour.
		 */
		WELTALLCORE_API uint8 ClearFlag() const;
		/** The ground's variant of the footstep and landing sounds. */
		WELTALLCORE_API uint8 GroundSound() const;
		WELTALLCORE_API FBattleStageElement Element(int32 Index) const;
		/** The floor dome's grid: its size across, its height, its columns and rows. */
		WELTALLCORE_API std::array<int16, 4> DomeGrid() const;
		WELTALLCORE_API std::array<uint8, 3> ClearColour() const;
		WELTALLCORE_API FGteVector LightDirection() const;
		WELTALLCORE_API FGteVector LightColour() const;
		WELTALLCORE_API std::array<uint8, 3> AmbientColour() const;
		WELTALLCORE_API std::array<uint8, 3> ShadowColour() const;
		/** Preset Index's target and eye, read where the program reads them; nothing past the record. */
		WELTALLCORE_API std::optional<FBattleCameraPreset> CameraPreset(int32 Index) const;
		/** The point a model sequence's target can be set to. */
		WELTALLCORE_API FGteVector GroundPoint() const;

		const std::vector<FBattleFloorVertex>& FloorVertices() const { return Vertices; }
		const std::vector<FBattleFloorTriangle>& FloorTriangles() const { return Triangles; }
		const std::vector<FBattleStageObstacle>& Obstacles() const { return StageObstacles; }
		/** The band speeds (a signed byte a band) of the VRAM scroll element Element makes; elements 0-2 have them. */
		std::span<const uint8> ScrollSpeeds(int32 Element) const { return std::span<const uint8>(Record).subspan(ScrollSpeedOffsets[static_cast<std::size_t>(Element)]); }
		/** The stage object's sequences: the record's offsets of each (the first runs as the stage is built). */
		const std::vector<uint32>& Sequences() const { return SequenceOffsets; }
		/** Where the archive of the obstacles, the scrolls and the sequences lies in the record. */
		WELTALLCORE_API uint32 ArchiveOffset() const;

	private:
		std::vector<uint8> Record;
		std::vector<FBattleFloorVertex> Vertices;
		std::vector<FBattleFloorTriangle> Triangles;
		std::vector<FBattleStageObstacle> StageObstacles;
		std::array<uint32, ScrollSpeedTables> ScrollSpeedOffsets{};
		std::vector<uint32> SequenceOffsets;
	};

	/** Where a stage bone stands in its model and how it is drawn. */
	struct FBattleStageBonePlace
	{
		FGteVector Translation{};
		/** 0 into the ordering table's farthest entry, 1 and 2 billboards, 4-7 the model draw modes 2-5. */
		int16 DrawKind = 0;
	};

	/** A stage's model: the gears' model pack layout, and a fifth section placing each bone. */
	struct FBattleStageModel
	{
		FGearModelPack Pack;
		/** Bones 1 on: one place each. */
		std::vector<FBattleStageBonePlace> BonePlaces;

		WELTALLCORE_API static TResult<FBattleStageModel> Parse(std::span<const uint8> File);
	};

	/** A battle stage's two files. */
	struct FBattleStageFiles
	{
		/** Stages are a group of directory 15: stage n is files 2n + 6 (the model) and 2n + 7 (the record). */
		static constexpr int32 Directory = 15;

		FBattleStageModel Model;
		FBattleStageRecord Record;

		/** How many stages the disc holds, as the kernel counts them from the group's record. */
		WELTALLCORE_API static int32 Count(const FGameFileIndex& Index);
		WELTALLCORE_API static TResult<FBattleStageFiles> Load(const FCdImage& Image, const FGameFileIndex& Index, int32 Number);
	};
}
