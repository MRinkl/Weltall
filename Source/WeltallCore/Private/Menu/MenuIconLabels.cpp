#include "MenuIconLabels.h"

#include "Menu/MenuButtonIcons.h"

#include <cctype>
#include <iterator>
#include <utility>

namespace Weltall::MenuIconLabels
{
	namespace
	{
		std::string Upper(std::string_view Text)
		{
			std::string Upper;
			for (const char Letter : Text)
			{
				if (std::isalnum(static_cast<unsigned char>(Letter)) != 0)
				{
					Upper += static_cast<char>(std::toupper(static_cast<unsigned char>(Letter)));
				}
			}
			return Upper;
		}
	}

	/** An arrow's code in the labels' font for a direction's name, 0 for none. */
	char ArrowOf(std::string_view Direction)
	{
		if (Direction == "Up")
		{
			return 'u';
		}
		if (Direction == "Down")
		{
			return 'd';
		}
		if (Direction == "Left")
		{
			return 'l';
		}
		if (Direction == "Right")
		{
			return 'r';
		}
		return 0;
	}

	/** A keyboard key's label: its character, an arrow, the symbol keyboards mark it with, or its name shortened as they print it. */
	std::string KeyLabel(std::string_view Icon)
	{
		if (const std::size_t Separator = Icon.find(MenuKeysSeparator); Separator != std::string_view::npos)
		{
			return KeyLabel(Icon.substr(0, Separator)) + KeyLabel(Icon.substr(Separator + 1));
		}
		static constexpr std::pair<std::string_view, std::string_view> Labels[] = {
			{"Space", "s"}, {"Enter", "e"}, {"Esc", "ESC"}, {"Tab", "t"}, {"Backspace", "b"}, {"CapsLock", "CAPS"},
			{"LeftShift", "h"}, {"RightShift", "h"}, {"LeftControl", "CTRL"}, {"RightControl", "CTRL"}, {"LeftAlt", "ALT"},
			{"RightAlt", "ALT"}, {"LeftCommand", "k"}, {"RightCommand", "k"}, {"PageUp", "PGUP"}, {"PageDown", "PGDN"},
			{"Home", "HOME"}, {"End", "END"}, {"Insert", "INS"}, {"Delete", "DEL"}, {"Pause", "PAUSE"}, {"ScrollLock", "SCRL"},
			{"NumLock", "NUML"}, {"Multiply", "*"}, {"Add", "+"}, {"Subtract", "-"}, {"Divide", "/"}, {"Decimal", "."},
			{"Semicolon", ";"}, {"Equals", "="}, {"Comma", ","}, {"Hyphen", "-"}, {"Underscore", "-"}, {"Period", "."}, {"Slash", "/"},
			{"Tilde", "`"}, {"LeftBracket", "["}, {"RightBracket", "]"}, {"Backslash", "\\"}, {"Apostrophe", "'"},
			{"MouseLeft", "x"}, {"MouseRight", "y"}, {"MouseMiddle", "z"}, {"MouseWheel", "zud"}, {"Mouse", "m"},
			{"ThumbMouseButton", "M4"}, {"ThumbMouseButton2", "M5"}, {"MouseScrollUp", "zu"}, {"MouseScrollDown", "zd"},
		};
		for (const auto& [Name, Label] : Labels)
		{
			if (Name == Icon)
			{
				return std::string(Label);
			}
		}
		if (const char Arrow = ArrowOf(Icon))
		{
			return std::string(1, Arrow);
		}
		constexpr std::string_view NumPad = "NumPad";
		if (Icon.starts_with(NumPad))
		{
			static constexpr std::string_view Digits[] = {"Zero", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine"};
			for (std::size_t Digit = 0; Digit < std::size(Digits); ++Digit)
			{
				if (Icon.substr(NumPad.size()) == Digits[Digit])
				{
					return "NUM" + std::to_string(Digit);
				}
			}
		}
		return Upper(Icon).substr(0, 6);
	}

	/** A controller button's label other than a face button's: its name as printed, a stick's direction with an arrow. */
	std::string PadLabel(std::string_view Icon)
	{
		static constexpr std::pair<std::string_view, std::string_view> Labels[] = {
			{"Minus", "-"}, {"Plus", "+"}, {"LeftStick", "LS"}, {"RightStick", "RS"}, {"Create", "CREATE"}, {"Options", "OPTIONS"},
			{"Special_Left", "SELECT"}, {"Special_Right", "START"},
		};
		for (const auto& [Name, Label] : Labels)
		{
			if (Name == Icon)
			{
				return std::string(Label);
			}
		}
		for (const auto& [Stick, Short] : {std::pair<std::string_view, std::string_view>{"LeftStick_", "LS"}, {"RightStick_", "RS"}})
		{
			if (Icon.starts_with(Stick))
			{
				const char Arrow = ArrowOf(Icon.substr(Stick.size()));
				return Arrow != 0 ? std::string(Short) + Arrow : std::string(Short);
			}
		}
		return Upper(Icon).substr(0, 7);
	}
}
