#pragma once

#include "Foundation/Types.h"

#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>

/** Numbers written into text and read out of it, for the formats the game's own content is authored in. */
namespace Weltall::NumberText
{
	/** Adds Value in Base, capitals for the digits past 9, zeros before it up to MinDigits digits. */
	inline void Append(std::string& Out, int64 Value, int32 Base = 10, std::size_t MinDigits = 1)
	{
		std::array<char, 24> Digits{};
		const bool bNegative = Value < 0;
		const auto Converted = std::to_chars(Digits.data(), Digits.data() + Digits.size(), static_cast<uint64>(bNegative ? -Value : Value), Base);
		if (bNegative)
		{
			Out += '-';
		}
		const std::size_t Count = static_cast<std::size_t>(Converted.ptr - Digits.data());
		Out.append(Count < MinDigits ? MinDigits - Count : 0, '0');
		for (std::size_t Digit = 0; Digit < Count; ++Digit)
		{
			const char Character = Digits[Digit];
			Out += Character >= 'a' ? static_cast<char>(Character - 'a' + 'A') : Character;
		}
	}

	inline void AppendHex(std::string& Out, uint32 Value, std::size_t MinDigits)
	{
		Append(Out, Value, 16, MinDigits);
	}

	/** The number Digits spell whole in hexadecimal, without a prefix; none for anything else. */
	inline std::optional<uint32> Hex(std::string_view Digits)
	{
		uint32 Value = 0;
		const auto Read = std::from_chars(Digits.data(), Digits.data() + Digits.size(), Value, 16);
		return !Digits.empty() && Read.ec == std::errc() && Read.ptr == Digits.data() + Digits.size() ? std::optional<uint32>(Value) : std::nullopt;
	}

	/** The number Token spells whole: decimal, or hexadecimal after 0x, with a minus sign or none; none for anything else or past 32 bits. */
	inline std::optional<int64> Parse(std::string_view Token)
	{
		const bool bNegative = Token.starts_with('-');
		Token.remove_prefix(bNegative ? 1 : 0);
		const bool bHex = Token.starts_with("0x") || Token.starts_with("0X");
		Token.remove_prefix(bHex ? 2 : 0);
		uint64 Value = 0;
		const auto Read = std::from_chars(Token.data(), Token.data() + Token.size(), Value, bHex ? 16 : 10);
		if (Token.empty() || Read.ec != std::errc() || Read.ptr != Token.data() + Token.size() || Value > 0xFFFFFFFFull)
		{
			return std::nullopt;
		}
		return bNegative ? -static_cast<int64>(Value) : static_cast<int64>(Value);
	}

	inline std::optional<int64> NumberWithin(std::string_view Token, int64 Low, int64 High)
	{
		const std::optional<int64> Value = Parse(Token);
		return Value && *Value >= Low && *Value <= High ? Value : std::nullopt;
	}
}
