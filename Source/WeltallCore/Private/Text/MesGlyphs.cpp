#include "Text/MesGlyphs.h"

#include <string_view>

namespace Weltall
{
	namespace
	{
		constexpr uint8 Space = 0x10;
		constexpr uint8 Digits = 0x16;
		constexpr uint8 Capitals = 0x20;
		constexpr uint8 Smalls = 0x3D;
		/** The marks from code 11h, and those from 57h. */
		constexpr std::string_view FirstMarks = "+,-./";
		constexpr uint8 FirstMark = 0x11;
		constexpr std::string_view LaterMarks = "!\"#%&'():?";
		constexpr uint8 LaterMark = 0x57;
		/** The brackets and the equals sign between the capitals and the small letters. */
		constexpr std::string_view Brackets = "[]=";
		constexpr uint8 FirstBracket = 0x3A;
	}

	std::optional<FMesCode> MesCodeOfCharacter(char Character)
	{
		if (Character == ' ')
		{
			return FMesCode{0, Space};
		}
		if (Character >= '0' && Character <= '9')
		{
			return FMesCode{0, static_cast<uint8>(Digits + Character - '0')};
		}
		if (Character >= 'A' && Character <= 'Z')
		{
			return FMesCode{0, static_cast<uint8>(Capitals + Character - 'A')};
		}
		if (Character >= 'a' && Character <= 'z')
		{
			return FMesCode{0, static_cast<uint8>(Smalls + Character - 'a')};
		}
		if (const std::size_t Mark = FirstMarks.find(Character); Mark != std::string_view::npos)
		{
			return FMesCode{0, static_cast<uint8>(FirstMark + Mark)};
		}
		if (const std::size_t Mark = LaterMarks.find(Character); Mark != std::string_view::npos)
		{
			return FMesCode{0, static_cast<uint8>(LaterMark + Mark)};
		}
		if (const std::size_t Mark = Brackets.find(Character); Mark != std::string_view::npos)
		{
			return FMesCode{0, static_cast<uint8>(FirstBracket + Mark)};
		}
		return std::nullopt;
	}

	std::optional<char> PlainOfFullWidth(uint8 Lead, uint8 Second)
	{
		const uint16 Character = static_cast<uint16>(Lead << 8 | Second);
		if (Character >= 0x824F && Character <= 0x8258)
		{
			return static_cast<char>('0' + Character - 0x824F);
		}
		if (Character >= 0x8260 && Character <= 0x8279)
		{
			return static_cast<char>('A' + Character - 0x8260);
		}
		if (Character >= 0x8281 && Character <= 0x829A)
		{
			return static_cast<char>('a' + Character - 0x8281);
		}
		switch (Character)
		{
		case 0x8140:
			return ' ';
		case 0x8144:
			return '.';
		case 0x8149:
			return '!';
		case 0x8166:
			return '\'';
		case 0x815E:
			return '/';
		case 0x817C:
			return '-';
		default:
			return std::nullopt;
		}
	}

	std::optional<FMesCode> MesCodeOfFullWidth(uint8 Lead, uint8 Second)
	{
		const std::optional<char> Plain = PlainOfFullWidth(Lead, Second);
		return Plain ? MesCodeOfCharacter(*Plain) : std::nullopt;
	}
}
