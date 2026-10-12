#include "Media/CueSheet.h"

#include <cctype>
#include <charconv>
#include <optional>

namespace Weltall
{
	namespace
	{
		constexpr uint32 FramesPerSecond = 75;

		std::string_view Trim(std::string_view Text)
		{
			while (!Text.empty() && std::isspace(static_cast<unsigned char>(Text.front())))
			{
				Text.remove_prefix(1);
			}
			while (!Text.empty() && std::isspace(static_cast<unsigned char>(Text.back())))
			{
				Text.remove_suffix(1);
			}
			return Text;
		}

		std::string_view NextWord(std::string_view& Line)
		{
			Line = Trim(Line);
			const auto End = Line.find_first_of(" \t");
			const auto Word = Line.substr(0, End);
			Line = End == std::string_view::npos ? std::string_view() : Line.substr(End);
			return Word;
		}

		std::optional<uint32> ParseNumber(std::string_view Text)
		{
			uint32 Value = 0;
			const auto [End, Error] = std::from_chars(Text.data(), Text.data() + Text.size(), Value);
			if (Error != std::errc() || End != Text.data() + Text.size())
			{
				return std::nullopt;
			}
			return Value;
		}

		/** "mm:ss:ff" to a frame count. */
		std::optional<uint32> ParseTimestamp(std::string_view Text)
		{
			const auto FirstColon = Text.find(':');
			const auto SecondColon = Text.find(':', FirstColon + 1);
			if (FirstColon == std::string_view::npos || SecondColon == std::string_view::npos)
			{
				return std::nullopt;
			}
			const auto Minutes = ParseNumber(Text.substr(0, FirstColon));
			const auto Seconds = ParseNumber(Text.substr(FirstColon + 1, SecondColon - FirstColon - 1));
			const auto Frames = ParseNumber(Text.substr(SecondColon + 1));
			if (!Minutes || !Seconds || !Frames)
			{
				return std::nullopt;
			}
			return (*Minutes * 60 + *Seconds) * FramesPerSecond + *Frames;
		}

		/** A FILE name is quoted when it contains spaces. */
		std::string_view ParseFileName(std::string_view Rest)
		{
			Rest = Trim(Rest);
			if (!Rest.empty() && Rest.front() == '"')
			{
				const auto Close = Rest.find('"', 1);
				return Close == std::string_view::npos ? Rest.substr(1) : Rest.substr(1, Close - 1);
			}
			return Rest.substr(0, Rest.find_first_of(" \t"));
		}

		uint32 SectorSizeOf(std::string_view Mode)
		{
			const auto Slash = Mode.find('/');
			if (Slash != std::string_view::npos)
			{
				return ParseNumber(Mode.substr(Slash + 1)).value_or(0);
			}
			return Mode == "AUDIO" ? 2352 : 0;
		}
	}

	TResult<std::vector<FCueTrack>> ParseCueSheet(std::string_view Text, const std::filesystem::path& SheetDirectory)
	{
		std::vector<FCueTrack> Tracks;
		std::filesystem::path CurrentFile;
		std::size_t LineNumber = 0;
		while (!Text.empty())
		{
			const auto LineEnd = Text.find('\n');
			std::string_view Line = Text.substr(0, LineEnd);
			Text = LineEnd == std::string_view::npos ? std::string_view() : Text.substr(LineEnd + 1);
			++LineNumber;

			const std::string_view Keyword = NextWord(Line);
			if (Keyword == "FILE")
			{
				CurrentFile = SheetDirectory / std::filesystem::path(std::string(ParseFileName(Line)));
			}
			else if (Keyword == "TRACK")
			{
				const auto Number = ParseNumber(NextWord(Line));
				const std::string_view Mode = NextWord(Line);
				if (!Number || Mode.empty() || CurrentFile.empty())
				{
					return MakeError("malformed TRACK on line " + std::to_string(LineNumber));
				}
				Tracks.push_back({static_cast<int32>(*Number), std::string(Mode), CurrentFile, SectorSizeOf(Mode), 0});
			}
			else if (Keyword == "INDEX")
			{
				const auto Index = ParseNumber(NextWord(Line));
				const auto Frame = ParseTimestamp(NextWord(Line));
				if (!Index || !Frame || Tracks.empty())
				{
					return MakeError("malformed INDEX on line " + std::to_string(LineNumber));
				}
				if (*Index == 1)
				{
					Tracks.back().StartFrame = *Frame;
				}
			}
		}
		if (Tracks.empty())
		{
			return MakeError("the sheet has no tracks");
		}
		return Tracks;
	}
}
