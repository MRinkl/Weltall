#pragma once

#include "Input/ButtonGlyphs.h"

#include <string>
#include <utility>
#include <vector>

namespace Weltall::Testing
{
	/** Glyphs of one icon set as its controllers print their buttons, each PlayStation button shown as the icon it is bound to. */
	class FBoundGlyphs final : public IButtonGlyphs
	{
	public:
		explicit FBoundGlyphs(EButtonIconSet InSet) : Set(InSet) {}

		FButtonGlyph Glyph(EPadButton Button) const override
		{
			for (const auto& [Bound, Icon] : Icons)
			{
				if (Bound == Button)
				{
					return {Set, "Key", Icon};
				}
			}
			return {Set, "", ""};
		}

		uint32 Revision() const override { return Changes; }

		/** New bindings: a new revision. */
		void Bind(std::vector<std::pair<EPadButton, std::string>> InIcons)
		{
			Icons = std::move(InIcons);
			++Changes;
		}

		EButtonIconSet Set;
		std::vector<std::pair<EPadButton, std::string>> Icons;
		uint32 Changes = 1;
	};
}
