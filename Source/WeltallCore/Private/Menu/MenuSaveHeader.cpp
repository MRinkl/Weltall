#include "Menu/MenuSaveHeader.h"

#include "Foundation/LittleEndian.h"
#include "Menu/MenuResources.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr std::size_t IconKind = 2;
		constexpr std::size_t Blocks = 3;
		constexpr std::size_t Title = 4;
		constexpr std::size_t Colours = 0x60;
		constexpr std::size_t Pixels = 0x80;
		constexpr uint8 OneFrame = 0x11;
		constexpr std::array<uint8, 26> TitlePrefix = {0x82, 0x77, 0x82, 0x64, 0x82, 0x6D, 0x82, 0x6E, 0x82, 0x66, 0x82, 0x64, 0x82,
			0x60, 0x82, 0x71, 0x82, 0x72, 0x81, 0x5E, 0x82, 0x6D, 0x82, 0x8F, 0x81, 0x44};
		constexpr uint8 FullWidthLead = 0x82;
		constexpr uint8 FullWidthZero = 0x4F;
		constexpr std::array<uint8, 2> FullWidthSpace = {0x81, 0x40};
		/** A place's name: the first 30 bytes of its line. */
		constexpr std::size_t PlaceBytes = 30;
		/** The story's progress in a save's data. */
		constexpr std::size_t StoryProgress = 0x1134;

		/** The bytes after the line Progress names, its two-byte characters passed whole; zeros past the file's end. */
		std::array<uint8, PlaceBytes> PlaceName(std::span<const uint8> Places, uint16 Progress)
		{
			std::size_t At = 0;
			for (uint16 Line = 0; Line < Progress && At < Places.size(); ++Line)
			{
				while (At < Places.size() && Places[At] != '\n')
				{
					At += Places[At] > 0x7F ? 2 : 1;
				}
				++At;
			}
			std::array<uint8, PlaceBytes> Name{};
			for (std::size_t Byte = 0; Byte < PlaceBytes && At + Byte < Places.size(); ++Byte)
			{
				Name[Byte] = Places[At + Byte];
			}
			return Name;
		}
	}

	std::array<uint8, SaveHeaderSize> MenuSaveHeader(const FMenuResources& Resources, int32 Slot, std::span<const uint8> Data)
	{
		std::array<uint8, SaveHeaderSize> Header{};
		Header[0] = 'S';
		Header[1] = 'C';
		Header[IconKind] = OneFrame;
		Header[Blocks] = 1;
		std::size_t At = Title;
		std::copy(TitlePrefix.begin(), TitlePrefix.end(), Header.begin() + static_cast<std::ptrdiff_t>(At));
		At += TitlePrefix.size();
		const int32 Number = Slot + 1;
		Header[At++] = FullWidthLead;
		Header[At++] = static_cast<uint8>(FullWidthZero + Number / 10);
		Header[At++] = FullWidthLead;
		Header[At++] = static_cast<uint8>(FullWidthZero + Number % 10);
		Header[At++] = FullWidthSpace[0];
		Header[At++] = FullWidthSpace[1];
		const uint16 Progress = Data.size() >= StoryProgress + 2 ? LittleEndian::ReadU16(Data, StoryProgress) : 0;
		for (const uint8 Byte : PlaceName(Resources.PlaceNames(), Progress))
		{
			// The place is added as a string: a zero ends it.
			if (Byte == 0)
			{
				break;
			}
			Header[At++] = Byte;
		}
		std::copy(Resources.SaveIconColours().begin(), Resources.SaveIconColours().end(), Header.begin() + Colours);
		std::copy(Resources.SaveIconPixels().begin(), Resources.SaveIconPixels().end(), Header.begin() + Pixels);
		return Header;
	}
}
