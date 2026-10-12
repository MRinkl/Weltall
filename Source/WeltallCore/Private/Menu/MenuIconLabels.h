#pragma once

#include <string>
#include <string_view>

namespace Weltall::MenuIconLabels
{
	/** An arrow's code in the labels' font ('u', 'd', 'l', 'r') for a direction's name, 0 for none. */
	char ArrowOf(std::string_view Direction);

	/**
	 * A keyboard key's label: its character, an arrow, or its name shortened as keyboards print it; several keys joined by
	 * MenuKeysSeparator, their labels one after another.
	 */
	std::string KeyLabel(std::string_view Icon);

	/** A controller button's label other than a face button's: its name as printed, a stick's direction with an arrow. */
	std::string PadLabel(std::string_view Icon);
}
