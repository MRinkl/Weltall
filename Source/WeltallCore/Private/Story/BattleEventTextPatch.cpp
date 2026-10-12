#include "Story/BattleEventTextPatch.h"

#include "Field/EventAssemblyText.h"
#include "Foundation/NumberText.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <algorithm>
#include <limits>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace EventAssemblyText;

		/** The most event scripts a formation's byte can name, and the most messages or pages a text can. */
		constexpr int64 LastEvent = 0xFF;
		constexpr int64 LastMessage = 0xFFFF;
		constexpr int32 EveryPage = std::numeric_limits<int32>::max();
		constexpr std::string_view BeginningShape = "a message's beginning reads 'message K from page P', the pages counted from 1, or with 'after' and the text told first";

		FError LineError(int32 Line, std::string_view Words)
		{
			std::string Message = "line ";
			NumberText::Append(Message, Line);
			Message += ": ";
			Message += Words;
			return MakeError(std::move(Message));
		}
	}

	TResult<FBattleEventTextPatch> FBattleEventTextPatch::Parse(std::string_view Text)
	{
		enum class EReading : uint8
		{
			Lines,
			TextBrace,
			TextLines,
		};

		FBattleEventTextPatch Patch;
		EReading Reading = EReading::Lines;
		bool bEvent = false;
		std::string Told;
		bool bToldBegun = false;
		for (int32 Number = 1; !Text.empty(); ++Number)
		{
			const std::size_t End = std::min(Text.find('\n'), Text.size());
			std::string_view Line = Text.substr(0, End);
			Text.remove_prefix(std::min(End + 1, Text.size()));
			if (Line.ends_with('\r'))
			{
				Line.remove_suffix(1);
			}
			if (Reading == EReading::TextLines)
			{
				if (Line != "}")
				{
					if (!MesCodesOfText(Line))
					{
						return LineError(Number, "a character or a mark the messages lack");
					}
					Told += bToldBegun ? "\n" : "";
					Told += Line;
					bToldBegun = true;
				}
				else
				{
					// Its lines were read one by one, so the whole has codes.
					Patch.Beginnings.back().Before = MesCodesOfText(Told).value_or(std::vector<uint8>());
					Reading = EReading::Lines;
				}
				continue;
			}
			const std::vector<std::string_view> Words = WordsOf(Line);
			if (Reading == EReading::TextBrace)
			{
				if (Words.size() != 1 || Words.front() != "{")
				{
					return LineError(Number, "the text told first begins after a line of '{'");
				}
				Reading = EReading::TextLines;
			}
			else if (Words.empty())
			{
			}
			else if (Words.front() == "battle")
			{
				const std::optional<int64> Event = Words.size() == 3 && Words[1] == "event" ? NumberText::NumberWithin(Words[2], 0, LastEvent) : std::nullopt;
				if (!Event || bEvent)
				{
					return LineError(Number, "'battle event N' takes the event's number, once");
				}
				Patch.EventNumber = static_cast<int32>(*Event);
				bEvent = true;
			}
			else if (Words.front() == "message")
			{
				// "message K from page P", or "message K from page P after" and the text told first.
				const bool bAfter = Words.size() == 6 && Words[5] == "after";
				const bool bFromPage = (Words.size() == 5 || bAfter) && Words[2] == "from" && Words[3] == "page";
				const std::optional<int64> Message = bFromPage ? NumberText::NumberWithin(Words[1], 0, LastMessage) : std::nullopt;
				const std::optional<int64> Page = bFromPage ? NumberText::NumberWithin(Words[4], 1, LastMessage) : std::nullopt;
				if (!Message || !Page)
				{
					return LineError(Number, BeginningShape);
				}
				if (std::any_of(Patch.Beginnings.begin(), Patch.Beginnings.end(), [&Message](const FBeginning& Other) { return Other.Message == *Message; }))
				{
					return LineError(Number, "a message given two beginnings");
				}
				Patch.Beginnings.push_back({static_cast<int32>(*Message), static_cast<int32>(*Page), {}});
				if (bAfter)
				{
					Told.clear();
					bToldBegun = false;
					Reading = EReading::TextBrace;
				}
			}
			else
			{
				return LineError(Number, "a battle event's text takes 'battle event N' and its messages' beginnings");
			}
		}
		if (Reading != EReading::Lines)
		{
			return MakeError("the text told first does not end with a line of '}'");
		}
		if (!bEvent)
		{
			return MakeError("the text names no battle event");
		}
		return Patch;
	}

	std::optional<std::vector<uint8>> FBattleEventTextPatch::Apply(std::span<const uint8> Messages) const
	{
		const TResult<FMessageTable> Table = FMessageTable::Parse(Messages);
		if (!Table || std::any_of(Beginnings.begin(), Beginnings.end(), [&Table](const FBeginning& Beginning) { return Beginning.Message >= Table->Count(); }))
		{
			return std::nullopt;
		}
		std::vector<FAddedMessage> Changed;
		for (int32 Message = 0; Message < Table->Count(); ++Message)
		{
			const auto Beginning = std::find_if(Beginnings.begin(), Beginnings.end(), [Message](const FBeginning& Other) { return Other.Message == Message; });
			std::optional<std::vector<uint8>> Codes = MesPagesOfCodes(Table->String(Message), Beginning != Beginnings.end() ? Beginning->Page : 1, EveryPage);
			if (!Codes)
			{
				return std::nullopt;
			}
			if (Beginning != Beginnings.end() && !Beginning->Before.empty())
			{
				// What is told first is typed in the message's own window.
				const FMesExtent Extent = MesExtentOfCodes(Beginning->Before);
				if (Extent.WidthUnits > Table->WidthUnits(Message) || Extent.Lines > Table->Lines(Message))
				{
					return std::nullopt;
				}
				Codes = MesPagesJoined(Beginning->Before, *Codes);
			}
			Changed.push_back({std::move(*Codes), static_cast<uint8>(Table->WidthUnits(Message)), static_cast<uint8>(Table->Lines(Message))});
		}
		return BuildMessageTable(Changed);
	}
}
