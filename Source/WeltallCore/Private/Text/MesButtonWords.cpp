#include "Text/MesButtonWords.h"

#include "Text/MesGlyphs.h"

#include <array>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace Weltall
{
	namespace
	{
		struct FButtonName
		{
			std::string_view Name;
			EPadButton Button;
			bool bDirections = false;
		};

		constexpr std::array<FButtonName, 7> ButtonNames{{
			{"L1", EPadButton::L1},
			{"L2", EPadButton::L2},
			{"R1", EPadButton::R1},
			{"R2", EPadButton::R2},
			{"Start", EPadButton::Start},
			{"Select", EPadButton::Select},
			{"Directional", EPadButton::Up, true},
		}};

		/** A glyph that is no plain character: it ends a word and matches nothing. */
		constexpr char NotPlain = '\x7F';
		/** A line's end read on past (MesLineEnd): a gap between words, as a space is. */
		constexpr char LineBreak = '\n';

		/** The plain character of each single-byte code of the message font, NotPlain for the others. */
		const std::array<char, 256>& PlainOfCodes()
		{
			static const std::array<char, 256> Table = []
			{
				std::array<char, 256> Made{};
				Made.fill(NotPlain);
				for (int32 Character = ' '; Character < 0x7F; ++Character)
				{
					if (const std::optional<FMesCode> Code = MesCodeOfCharacter(static_cast<char>(Character)); Code && Code->Lead == 0)
					{
						Made[Code->Code] = static_cast<char>(Character);
					}
				}
				return Made;
			}();
			return Table;
		}

		bool IsWordCharacter(char Character)
		{
			return std::isalnum(static_cast<unsigned char>(Character)) != 0;
		}

		/** The name of ButtonNames at At of Plain as a whole word, or none. */
		const FButtonName* NameAt(std::string_view Plain, std::size_t At)
		{
			if (At > 0 && IsWordCharacter(Plain[At - 1]))
			{
				return nullptr;
			}
			for (const FButtonName& Name : ButtonNames)
			{
				const std::size_t End = At + Name.Name.size();
				if (Plain.substr(At, Name.Name.size()) == Name.Name && (End == Plain.size() || !IsWordCharacter(Plain[End])))
				{
					return &Name;
				}
			}
			return nullptr;
		}

		/** The end of the gap of spaces and line ends from From of Plain: From itself where there is none. */
		std::size_t AfterGap(std::string_view Plain, std::size_t From)
		{
			while (From < Plain.size() && (Plain[From] == ' ' || Plain[From] == LineBreak))
			{
				++From;
			}
			return From;
		}

		/** Whether a gap from At of Plain is followed by "button" or "buttons", in either case, as a whole word. */
		bool IsButtonWord(std::string_view Plain, std::size_t At)
		{
			constexpr std::string_view Word = "button";
			const std::size_t From = AfterGap(Plain, At);
			if (From == At || Plain.size() - From < Word.size())
			{
				return false;
			}
			for (std::size_t Letter = 0; Letter < Word.size(); ++Letter)
			{
				if (std::tolower(static_cast<unsigned char>(Plain[From + Letter])) != Word[Letter])
				{
					return false;
				}
			}
			const std::size_t End = From + Word.size();
			return End == Plain.size() || !IsWordCharacter(Plain[End]) || (Plain[End] == 's' && (End + 1 == Plain.size() || !IsWordCharacter(Plain[End + 1])));
		}

		/**
		 * Whether the name at At of Plain, Length long, is followed by "button(s)", or by "and" another name and those;
		 * the gaps between the words may cross a line's end.
		 */
		bool NamesAButton(std::string_view Plain, std::size_t At, std::size_t Length)
		{
			const std::size_t After = At + Length;
			if (IsButtonWord(Plain, After))
			{
				return true;
			}
			constexpr std::string_view And = "and";
			const std::size_t AndAt = AfterGap(Plain, After);
			if (AndAt == After || Plain.substr(AndAt, And.size()) != And)
			{
				return false;
			}
			const std::size_t Next = AfterGap(Plain, AndAt + And.size());
			const FButtonName* Other = Next != AndAt + And.size() ? NameAt(Plain, Next) : nullptr;
			return Other != nullptr && IsButtonWord(Plain, Next + Other->Name.size());
		}
	}

	void FindMesButtonWords(std::span<const FMesPlacedGlyph> Glyphs, std::vector<FMesButtonWord>& Out)
	{
		Out.clear();
		const std::array<char, 256>& Plains = PlainOfCodes();
		std::string Plain(Glyphs.size(), NotPlain);
		for (std::size_t Index = 0; Index < Glyphs.size(); ++Index)
		{
			if (Glyphs[Index].Code == MesLineEnd)
			{
				Plain[Index] = LineBreak;
			}
			else if (Glyphs[Index].Code.Lead == 0)
			{
				Plain[Index] = Plains[Glyphs[Index].Code.Code];
			}
		}
		for (std::size_t At = 0; At < Plain.size(); ++At)
		{
			const FButtonName* Name = NameAt(Plain, At);
			if (Name == nullptr || !NamesAButton(Plain, At, Name->Name.size()))
			{
				continue;
			}
			// The name ends where the space after it starts.
			const std::size_t After = At + Name->Name.size();
			Out.push_back({At, Name->Name.size(), Glyphs[After].XUnits, Name->Button, Name->bDirections});
			At = After;
		}
	}
}
