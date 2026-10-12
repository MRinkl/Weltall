#include "Field/FieldArchive.h"

#include "Formats/Lzss.h"
#include "Foundation/LittleEndian.h"

#include <string>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		constexpr std::size_t SlotBytes = 8;
		constexpr std::size_t SectionSizesAt = 0x10C;
		constexpr std::size_t SectionOffsetsAt = 0x130;
		constexpr std::size_t LightsAt = 0x154;
		constexpr std::size_t LightBytes = 0x10;
		constexpr std::size_t AmbientAt = LightsAt + 0x30;
		constexpr std::size_t ActorCountAt = 0x18C;
		constexpr std::size_t ActorsAt = 0x190;
		constexpr std::size_t ActorBytes = 16;
		constexpr uint16 AlternateDrawModeFlag = 0x4000;

		FGteVector ReadVector(std::span<const uint8> Bytes, std::size_t At)
		{
			return {ReadS16(Bytes, At), ReadS16(Bytes, At + 2), ReadS16(Bytes, At + 4)};
		}
	}

	int32 FFieldActorDefinition::ModelDrawMode(bool bFog) const
	{
		return FieldModelDrawMode(Flags, bFog);
	}

	int32 FieldModelDrawMode(uint16 Flags, bool bFog)
	{
		const bool bFarthest = (Flags & FieldActorFlag::SortByFarthestVertex) != 0;
		if (bFog)
		{
			return bFarthest ? 5 : 4;
		}
		if ((Flags & FieldActorFlag::LightMode) != 0)
		{
			return 1;
		}
		if ((Flags & AlternateDrawModeFlag) != 0)
		{
			return 3;
		}
		return bFarthest ? 2 : 0;
	}

	TResult<FFieldArchive> ParseFieldArchive(std::span<const uint8> Sectors)
	{
		if (Sectors.size() < ActorsAt)
		{
			return MakeError("field archive is shorter than its header");
		}
		FFieldArchive Archive;
		for (std::size_t Slot = 0; Slot < Archive.SpriteSlots.size(); ++Slot)
		{
			const std::size_t At = Slot * SlotBytes;
			Archive.SpriteSlots[Slot] = {ReadS16(Sectors, At), ReadS16(Sectors, At + 2), ReadU16(Sectors, At + 6) == 0};
		}
		for (std::size_t Light = 0; Light < Archive.Lights.Directional.size(); ++Light)
		{
			const std::size_t At = LightsAt + Light * LightBytes;
			Archive.Lights.Directional[Light] = {ReadVector(Sectors, At), ReadVector(Sectors, At + 8)};
		}
		Archive.Lights.Ambient = ReadVector(Sectors, AmbientAt);

		const std::size_t ActorCount = ReadU16(Sectors, ActorCountAt);
		if (Sectors.size() < ActorsAt + ActorCount * ActorBytes)
		{
			return MakeError("field archive's actor definitions run past it");
		}
		Archive.Actors.resize(ActorCount);
		for (std::size_t Actor = 0; Actor < ActorCount; ++Actor)
		{
			const std::size_t At = ActorsAt + Actor * ActorBytes;
			Archive.Actors[Actor] = {ReadU16(Sectors, At), ReadVector(Sectors, At + 2), ReadVector(Sectors, At + 8), ReadU16(Sectors, At + 14)};
		}

		for (std::size_t Section = 0; Section < Archive.Sections.size(); ++Section)
		{
			const uint32 Size = ReadU32(Sectors, SectionSizesAt + Section * 4);
			const uint32 Offset = ReadU32(Sectors, SectionOffsetsAt + Section * 4);
			TResult<std::vector<uint8>> Unpacked = Offset < Sectors.size()
				? UnpackLzssPrefix(Sectors.subspan(Offset), Size)
				: TResult<std::vector<uint8>>(MakeError("it lies past the archive"));
			if (!Unpacked)
			{
				return MakeError("field archive section " + std::to_string(Section) + ": " + Unpacked.Error().Message);
			}
			Archive.Sections[Section] = std::move(*Unpacked);
		}
		return Archive;
	}
}
