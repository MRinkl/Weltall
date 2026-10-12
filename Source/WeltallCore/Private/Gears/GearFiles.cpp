#include "Gears/GearFiles.h"

#include "Foundation/LittleEndian.h"

#include <string>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		constexpr uint32 ImageBlock = 0x1100;
		constexpr uint32 ClutBlock = 0x1101;
		constexpr std::size_t TextureBlockHeader = 0x10;
		constexpr std::size_t HeaderBytes = 0x14;
		constexpr std::size_t ClothSpecWords = 18;
		constexpr std::size_t SphereWords = 5;
		constexpr uint16 NoPart = 0xFFFF;
		/** Beyond this a section container's count is not one. */
		constexpr uint32 MaxEntries = 0x400;

		bool Has(std::span<const uint8> Bytes, std::size_t Offset, std::size_t Count)
		{
			return Offset <= Bytes.size() && Count <= Bytes.size() - Offset;
		}

		/** A section container's entries as offsets from the start of Bytes (the load relocates them so). */
		TResult<std::vector<uint32>> ReadContainer(std::span<const uint8> Bytes, std::size_t Base, const char* What)
		{
			if (!Has(Bytes, Base, 4))
			{
				return MakeError(std::string("the gear's ") + What + " lies past its file");
			}
			const uint32 Count = ReadU32(Bytes, Base);
			if (Count == 0 || Count >= MaxEntries || !Has(Bytes, Base + 4, 4 * std::size_t{Count}))
			{
				return MakeError(std::string("the gear's ") + What + " is not a section container");
			}
			std::vector<uint32> Entries(Count);
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				Entries[Index] = static_cast<uint32>(Base) + ReadU32(Bytes, Base + 4 + 4 * std::size_t{Index});
				if (Entries[Index] > Bytes.size())
				{
					return MakeError(std::string("the gear's ") + What + " points past its file");
				}
			}
			return Entries;
		}
	}

	TResult<std::vector<FGearTextureBlock>> ReadGearImageList(std::span<const uint8> Bytes, std::size_t Offset)
	{
		if (!Has(Bytes, Offset, 4))
		{
			return MakeError("the gear's texture list lies past its file");
		}
		const uint32 Count = ReadU32(Bytes, Offset);
		std::size_t Cursor = Offset + 4 + 4 * std::size_t{Count};
		std::vector<FGearTextureBlock> Blocks;
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			if (!Has(Bytes, Cursor, TextureBlockHeader))
			{
				return MakeError("the gear's texture list runs past its file");
			}
			const uint32 Kind = ReadU32(Bytes, Cursor);
			if (Kind != ImageBlock && Kind != ClutBlock)
			{
				break;
			}
			FGearTextureBlock Block;
			Block.bClut = Kind == ClutBlock;
			Block.X = ReadS16(Bytes, Cursor + 4);
			Block.Y = ReadS16(Bytes, Cursor + 6);
			Block.OffsetX = ReadS16(Bytes, Cursor + 8);
			Block.OffsetY = ReadS16(Bytes, Cursor + 10);
			Block.Width = ReadS16(Bytes, Cursor + 12);
			Block.Height = ReadS16(Bytes, Cursor + 14);
			const std::size_t Texels = Block.Width > 0 && Block.Height > 0 ? std::size_t(Block.Width) * std::size_t(Block.Height) : 0;
			Cursor += TextureBlockHeader;
			if (!Has(Bytes, Cursor, 2 * Texels))
			{
				return MakeError("a gear's texture block runs past its file");
			}
			Block.Pixels.resize(Texels);
			for (std::size_t Texel = 0; Texel < Texels; ++Texel)
			{
				Block.Pixels[Texel] = ReadU16(Bytes, Cursor + 2 * Texel);
			}
			Cursor += 2 * Texels;
			Blocks.push_back(std::move(Block));
		}
		return Blocks;
	}

	namespace
	{
		TResult<std::vector<FGearSkeletonEntry>> ReadSkeleton(std::span<const uint8> Bytes, std::size_t Offset, std::size_t PartCount)
		{
			std::vector<FGearSkeletonEntry> Skeleton;
			for (std::size_t Cursor = Offset;; Cursor += 4)
			{
				if (!Has(Bytes, Cursor, 4))
				{
					return MakeError("the gear's skeleton runs past its file");
				}
				const FGearSkeletonEntry Entry{ReadU16(Bytes, Cursor), ReadU16(Bytes, Cursor + 2)};
				if (Entry.Part != NoPart && Entry.Part >= PartCount)
				{
					break;
				}
				if (Entry.Parent != NoPart && Entry.Parent >= Skeleton.size())
				{
					return MakeError("a gear's bone comes before its parent");
				}
				Skeleton.push_back(Entry);
			}
			if (Skeleton.empty())
			{
				return MakeError("the gear has no bones");
			}
			return Skeleton;
		}

		TResult<FGearClothMesh> ReadClothMesh(std::span<const uint8> Bytes, std::size_t Offset)
		{
			if (!Has(Bytes, Offset, 4))
			{
				return MakeError("a gear's cloth lies past its file");
			}
			FGearClothMesh Mesh;
			const uint16 Columns = ReadU16(Bytes, Offset);
			Mesh.Rows = ReadU16(Bytes, Offset + 2);
			std::size_t Cursor = Offset + 4;
			if (!Has(Bytes, Cursor, 8 * std::size_t{Columns} + 2))
			{
				return MakeError("a gear's cloth runs past its file");
			}
			for (uint16 Column = 0; Column < Columns; ++Column, Cursor += 6)
			{
				Mesh.Anchors.push_back({ReadU16(Bytes, Cursor), ReadU16(Bytes, Cursor + 2), ReadU16(Bytes, Cursor + 4)});
			}
			std::size_t Links = 0;
			for (uint16 Column = 0; Column < Columns; ++Column, Cursor += 2)
			{
				Mesh.PointCounts.push_back(ReadU16(Bytes, Cursor));
				Links += Mesh.PointCounts.back();
			}
			const uint16 Total = ReadU16(Bytes, Cursor);
			Cursor += 2;
			if (Total != Links || !Has(Bytes, Cursor, 3 * std::size_t{Total}))
			{
				return MakeError("a gear's cloth has a link count its columns do not add up to");
			}
			for (uint16 Link = 0; Link < Total; ++Link, Cursor += 2)
			{
				Mesh.LinkLengths.push_back(ReadU16(Bytes, Cursor));
			}
			Mesh.Pulls.assign(Bytes.begin() + static_cast<std::ptrdiff_t>(Cursor), Bytes.begin() + static_cast<std::ptrdiff_t>(Cursor + Total));
			return Mesh;
		}

		/** The header, then each cloth's 18 words and its spheres, as the load reads them after it. */
		std::optional<FError> ReadHeader(std::span<const uint8> Bytes, std::size_t Offset, FGearModelPack& Pack)
		{
			if (!Has(Bytes, Offset, HeaderBytes))
			{
				return MakeError("the gear's header runs past its file");
			}
			FGearModelHeader& Header = Pack.Header;
			Header.Height = ReadS16(Bytes, Offset + 2);
			Header.ShadowHalfWidth = ReadS16(Bytes, Offset + 4);
			Header.ShadowHalfDepth = ReadS16(Bytes, Offset + 6);
			Header.Scale = ReadS16(Bytes, Offset + 8);
			Header.DefaultRecord = Bytes[Offset + 0xA];
			Header.Flags = ReadU16(Bytes, Offset + 0xC);
			Header.Trails = Bytes[Offset + 0xE];
			Header.TextureAnimations = Bytes[Offset + 0x10];
			Header.Cloths = Bytes[Offset + 0x12];
			std::size_t Cursor = Offset + HeaderBytes;
			for (uint8 Cloth = 0; Cloth < Header.Cloths; ++Cloth)
			{
				if (!Has(Bytes, Cursor, 2 * ClothSpecWords))
				{
					return MakeError("a gear's cloth description runs past its file");
				}
				const auto Word = [&](std::size_t Index) { return ReadS16(Bytes, Cursor + 2 * Index); };
				FGearClothSpec Spec;
				Spec.Bone = Word(0);
				Spec.PullBase = Word(1);
				Spec.Scale = Word(2);
				Spec.Offset = {Word(3), Word(4), Word(5)};
				Spec.TextureX = Word(6);
				Spec.TextureY = Word(7);
				Spec.USpan = Word(8);
				Spec.VSpan = Word(9);
				Spec.ClutX = Word(10);
				Spec.Front = {static_cast<uint8>(Word(11)), static_cast<uint8>(Word(12)), static_cast<uint8>(Word(13))};
				Spec.Back = {static_cast<uint8>(Word(14)), static_cast<uint8>(Word(15)), static_cast<uint8>(Word(16))};
				const int16 Spheres = Word(17);
				Cursor += 2 * ClothSpecWords;
				for (int16 Sphere = 0; Sphere < Spheres; ++Sphere, Cursor += 2 * SphereWords)
				{
					if (!Has(Bytes, Cursor, 2 * SphereWords))
					{
						return MakeError("a gear's cloth spheres run past its file");
					}
					Spec.Spheres.push_back({ReadS16(Bytes, Cursor), ReadS16(Bytes, Cursor + 2), {ReadS16(Bytes, Cursor + 4), ReadS16(Bytes, Cursor + 6), ReadS16(Bytes, Cursor + 8)}});
				}
				Pack.Cloths.push_back(std::move(Spec));
			}
			return std::nullopt;
		}
	}

	TResult<FGearModelPack> ParseGearModelPack(std::span<const uint8> Bytes)
	{
		auto Sections = ReadContainer(Bytes, 0, "model pack");
		if (!Sections)
		{
			return Sections.Error();
		}
		if (Sections->size() < 4 || (*Sections)[2] < (*Sections)[1])
		{
			return MakeError("a gear's model pack lacks its textures, parts, skeleton or header");
		}
		FGearModelPack Pack;
		auto Textures = ReadGearImageList(Bytes, (*Sections)[0]);
		if (!Textures)
		{
			return Textures.Error();
		}
		Pack.Textures = std::move(*Textures);
		auto Model = ParseMdlModel(Bytes.subspan((*Sections)[1], (*Sections)[2] - (*Sections)[1]));
		if (!Model)
		{
			return Model.Error();
		}
		Pack.Model = std::move(*Model);
		auto Skeleton = ReadSkeleton(Bytes, (*Sections)[2], Pack.Model.Parts.size());
		if (!Skeleton)
		{
			return Skeleton.Error();
		}
		Pack.Skeleton = std::move(*Skeleton);
		auto Inner = ReadContainer(Bytes, (*Sections)[3], "header container");
		if (!Inner)
		{
			return Inner.Error();
		}
		if (auto Error = ReadHeader(Bytes, (*Inner)[0], Pack))
		{
			return *Error;
		}
		if (Inner->size() < 1 + std::size_t{Pack.Header.Cloths})
		{
			return MakeError("a gear's header container lacks a mesh for each cloth");
		}
		for (uint8 Cloth = 0; Cloth < Pack.Header.Cloths; ++Cloth)
		{
			auto Mesh = ReadClothMesh(Bytes, (*Inner)[1 + std::size_t{Cloth}]);
			if (!Mesh)
			{
				return Mesh.Error();
			}
			Pack.ClothMeshes.push_back(std::move(*Mesh));
		}
		return Pack;
	}

	std::optional<uint32> FGearMotionPack::Script(int32 Number) const
	{
		if (Number < 0 || static_cast<uint32>(Number) + 2 > ScriptEntries)
		{
			return std::nullopt;
		}
		return Scripts + ReadU32(Bytes, Scripts + 4 * (static_cast<std::size_t>(Number) + 2));
	}

	std::optional<uint32> FGearMotionPack::Record(int32 Number) const
	{
		if (Number < 0 || static_cast<uint32>(Number) + 1 > BankEntries)
		{
			return std::nullopt;
		}
		return Bank + ReadU32(Bytes, Bank + 4 * (static_cast<std::size_t>(Number) + 1));
	}

	TResult<FGearFiles> ReadGearFiles(const FCdImage& Image, const FGameFileIndex& Index, int32 GearModel)
	{
		if (GearModel < 0 || GearModel >= FGearFileNumbers::ModelCount)
		{
			return MakeError("there is no gear model " + std::to_string(GearModel));
		}
		const FGearFileNumbers Numbers = FGearFileNumbers::Of(GearModel);
		const FIndexRecord* MotionRecord = Index.Find(FGearFileNumbers::Directory, Numbers.Motion);
		const FIndexRecord* ModelRecord = Index.Find(FGearFileNumbers::Directory, Numbers.Model);
		if (MotionRecord == nullptr || ModelRecord == nullptr)
		{
			return MakeError("gear model " + std::to_string(GearModel) + "'s files are not on the disc");
		}
		auto MotionBytes = ReadFileSectors(Image, *MotionRecord);
		auto ModelBytes = ReadFileSectors(Image, *ModelRecord);
		if (!MotionBytes || !ModelBytes)
		{
			return MakeError("gear model " + std::to_string(GearModel) + "'s files cannot be read");
		}
		auto Model = ParseGearModelPack(*ModelBytes);
		if (!Model)
		{
			return MakeError("gear model " + std::to_string(GearModel) + ": " + Model.Error().Message);
		}
		auto Motion = ParseGearMotionPack(std::move(*MotionBytes));
		if (!Motion)
		{
			return MakeError("gear model " + std::to_string(GearModel) + ": " + Motion.Error().Message);
		}
		return FGearFiles{std::move(*Model), std::move(*Motion)};
	}

	TResult<FGearMotionPack> ParseGearMotionPack(std::vector<uint8> Bytes)
	{
		auto Sections = ReadContainer(Bytes, 0, "motion pack");
		if (!Sections)
		{
			return Sections.Error();
		}
		if (Sections->size() != 2)
		{
			return MakeError("a gear's motion pack has other sections than its scripts and its sounds");
		}
		auto Scripts = ReadContainer(Bytes, (*Sections)[0], "script table");
		if (!Scripts)
		{
			return Scripts.Error();
		}
		auto Bank = ReadContainer(Bytes, (*Scripts)[0], "animation bank");
		auto Sounds = ReadContainer(Bytes, (*Sections)[1], "sound container");
		if (!Bank || !Sounds)
		{
			return MakeError("a gear's animation bank or sound container cannot be read");
		}
		if (Sounds->size() < 3)
		{
			return MakeError("a gear's sound container lacks the entries the load compares");
		}
		FGearMotionPack Pack;
		Pack.Scripts = (*Sections)[0];
		Pack.ScriptEntries = static_cast<uint32>(Scripts->size());
		Pack.Bank = (*Scripts)[0];
		Pack.BankEntries = static_cast<uint32>(Bank->size());
		Pack.bHasSoundEffects = (*Sounds)[1] != (*Sounds)[2];
		Pack.SoundEffects = (*Sounds)[1];
		Pack.SoundEffectsEnd = (*Sounds)[2];
		Pack.Bytes = std::move(Bytes);
		return Pack;
	}
}
