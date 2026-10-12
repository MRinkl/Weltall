#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/MdlModel.h"
#include "Media/GameFileIndex.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	/** The gears' skeletal models are files of directory 4: model n is its motion pack and its model pack. */
	struct FGearFileNumbers
	{
		static constexpr int32 Directory = 4;
		static constexpr int32 ModelCount = 72;

		int32 Motion = 0;
		int32 Model = 0;

		static constexpr FGearFileNumbers Of(int32 GearModel) { return {0x6BA + 2 * GearModel, 0x6BB + 2 * GearModel}; }
	};

	/** An image or a CLUT the load places in VRAM, relative to the load's texture place unless the header says absolute. */
	struct FGearTextureBlock
	{
		bool bClut = false;
		int16 X = 0;
		int16 Y = 0;
		int16 OffsetX = 0;
		int16 OffsetY = 0;
		int16 Width = 0;
		int16 Height = 0;
		std::vector<uint16> Pixels;
	};

	namespace GearModelFlags
	{
		constexpr uint16 NoShadow = 0x0001;
		/** The texture blocks and the parts' pages and CLUTs are where the files say, not at the load's place. */
		constexpr uint16 AbsoluteTextures = 0x0004;
		constexpr uint16 RadiusFromX = 0x0008;
		/** An attached model keeps its own shown byte instead of its host's. */
		constexpr uint16 KeepsShownWhenAttached = 0x0010;
		constexpr uint16 EnvironmentMapped = 0x0200;
	}

	struct FGearModelHeader
	{
		/** How tall the model stands (Battle's objects read it; the field's module does not). */
		int16 Height = 0;
		int16 ShadowHalfWidth = 0;
		int16 ShadowHalfDepth = 0;
		int16 Scale = 0;
		/** The record numbers FEh and FFh stand for (low 7 bits), and a flag the record player obeys (bit 7). */
		uint8 DefaultRecord = 0;
		uint16 Flags = 0;
		uint8 Trails = 0;
		uint8 TextureAnimations = 0;
		uint8 Cloths = 0;
	};

	/** A sphere a cloth's points stay out of, in its bone's frame. */
	struct FGearClothSphere
	{
		int16 Bone = 0;
		int16 Radius = 0;
		FGteVector Center{};
	};

	/** How a cloth hangs: its bone, its points' pull and scale, its anchors' offset, its texture and colours, its spheres. */
	struct FGearClothSpec
	{
		int16 Bone = 0;
		int16 PullBase = 0;
		int16 Scale = 0;
		std::array<int16, 3> Offset{};
		int16 TextureX = 0;
		int16 TextureY = 0;
		int16 USpan = 0;
		int16 VSpan = 0;
		int16 ClutX = 0;
		std::array<uint8, 3> Front{};
		std::array<uint8, 3> Back{};
		std::vector<FGearClothSphere> Spheres;
	};

	/** A cloth's columns of linked points: each column's anchor, point count, and every link's length and pull. */
	struct FGearClothMesh
	{
		std::vector<std::array<uint16, 3>> Anchors;
		std::vector<uint16> PointCounts;
		uint16 Rows = 0;
		std::vector<uint16> LinkLengths;
		std::vector<uint8> Pulls;
	};

	/** Bone k + 1's MDL part (FFFFh none) and parent (FFFFh none, else bone Parent + 1). */
	struct FGearSkeletonEntry
	{
		uint16 Part = 0;
		uint16 Parent = 0;
	};

	/** What a gear looks like: textures, parts, skeleton, header and cloths (sections past these are their reader's). */
	struct FGearModelPack
	{
		std::vector<FGearTextureBlock> Textures;
		FMdlModel Model;
		std::vector<FGearSkeletonEntry> Skeleton;
		FGearModelHeader Header;
		std::vector<FGearClothSpec> Cloths;
		std::vector<FGearClothMesh> ClothMeshes;
	};

	WELTALLCORE_API TResult<FGearModelPack> ParseGearModelPack(std::span<const uint8> Bytes);

	/** The images and CLUTs of the image list at Offset, as the kernel's list loader reads them: it stops at a block of another kind. */
	WELTALLCORE_API TResult<std::vector<FGearTextureBlock>> ReadGearImageList(std::span<const uint8> Bytes, std::size_t Offset);

	/**
	 * How a gear moves: its bytes as the model keeps them (its scripts' loops count in them), with its
	 * scripts' table, its animation bank and its sound container found. Script n is the scripts'
	 * container's entry n + 2; record n the bank's entry n + 1.
	 */
	struct FGearMotionPack
	{
		std::vector<uint8> Bytes;
		uint32 Scripts = 0;
		uint32 ScriptEntries = 0;
		uint32 Bank = 0;
		uint32 BankEntries = 0;
		/** The sound container holds an effect file, which the load registers with the sound driver, and where it lies. */
		bool bHasSoundEffects = false;
		uint32 SoundEffects = 0;
		uint32 SoundEffectsEnd = 0;
		/** The slot of the model it belongs to: what the scripts and channels that read it are digested by. */
		int32 Slot = -1;

		/** Where script Number starts; nothing where the original reads past the table. */
		std::optional<uint32> Script(int32 Number) const;
		/** Where record Number starts; nothing where the original reads past the bank. */
		std::optional<uint32> Record(int32 Number) const;
		bool Has(uint32 Offset, uint32 Count) const { return Offset <= Bytes.size() && Count <= Bytes.size() - Offset; }
	};

	WELTALLCORE_API TResult<FGearMotionPack> ParseGearMotionPack(std::vector<uint8> Bytes);

	/** A gear's two packs, as a screen reads them to load the gear. */
	struct FGearFiles
	{
		FGearModelPack Model;
		FGearMotionPack Motion;
	};

	/** Reads and parses gear model GearModel's two files (whole sectors, as the game reads them). */
	WELTALLCORE_API TResult<FGearFiles> ReadGearFiles(const FCdImage& Image, const FGameFileIndex& Index, int32 GearModel);
}
