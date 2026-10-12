#include "Game/GameNames.h"

#include "Foundation/LittleEndian.h"
#include "Text/MessageTable.h"

#include <algorithm>
#include <string>

namespace Weltall::GameNames
{
	namespace
	{
		/** The glyph map entries a name's code is looked up in. */
		constexpr int32 SearchedGlyphs = 0x144;
		constexpr uint16 GlyphNotFound = 0x8000;

		uint16 FindGlyph(uint8 Lead, uint8 Code, const FMesNameTables& NameTables)
		{
			for (int32 Glyph = 0; Glyph < SearchedGlyphs; ++Glyph)
			{
				const std::span<const uint8> Entry = NameTables.GlyphMapEntry(Glyph);
				if (Entry.size() == 2 && Entry[0] == Lead && Entry[1] == Code)
				{
					return static_cast<uint16>(Glyph);
				}
			}
			return GlyphNotFound;
		}
	}

	TResult<FNameSlot> FromGlyphIndices(std::span<const uint8> Slot, uint16 End, const FMesNameTables& NameTables)
	{
		std::array<uint8, FGameState::NameBytes + 1> Text{};
		std::size_t Length = 0;
		for (std::size_t Offset = 0; Offset < FGameState::NameBytes; Offset += 2)
		{
			const uint16 Glyph = LittleEndian::ReadU16(Slot, Offset);
			if (Glyph == End)
			{
				break;
			}
			const std::span<const uint8> Entry = NameTables.GlyphMapEntry(Glyph);
			if (Entry.empty())
			{
				return MakeError("a name uses glyph " + std::to_string(Glyph) + ", which the glyph map lacks");
			}
			if (Entry[0] != 0)
			{
				Text[Length++] = Entry[0];
			}
			Text[Length++] = Entry[1];
		}
		FNameSlot Converted;
		std::copy_n(Text.begin(), Converted.size(), Converted.begin());
		return Converted;
	}

	FNameSlot ToGlyphIndices(std::span<const uint8> Name, uint8 LeadByte, const FMesNameTables& NameTables)
	{
		FNameSlot Stored{};
		std::size_t In = 0;
		for (std::size_t Out = 0; Out < Stored.size() && In < Name.size() && Name[In] != 0; Out += 2)
		{
			uint8 Lead = 0;
			uint8 Code = Name[In++];
			if (Code >= LeadByte)
			{
				if (In == Name.size())
				{
					break;
				}
				Lead = Code;
				Code = Name[In++];
			}
			const uint16 Glyph = FindGlyph(Lead, Code, NameTables);
			LittleEndian::WriteU16(Stored, Out, Glyph);
			if (Glyph == GlyphNotFound)
			{
				break;
			}
		}
		return Stored;
	}
}
