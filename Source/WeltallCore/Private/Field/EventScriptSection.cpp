#include "Field/EventAssembly.h"

#include "Foundation/LittleEndian.h"

#include <algorithm>

namespace Weltall
{
	std::optional<uint16> FEventAssembly::Label(std::string_view Name) const
	{
		const auto Found = std::find_if(Labels.begin(), Labels.end(), [Name](const std::pair<std::string, uint16>& Label) { return Label.first == Name; });
		return Found != Labels.end() ? std::optional<uint16>(Found->second) : std::nullopt;
	}

	TResult<std::vector<uint8>> BuildEventScriptSection(const FEventAssembly& Assembly)
	{
		constexpr std::size_t BitmapBytes = 0x80;
		constexpr std::size_t EntryPointsAt = BitmapBytes + 4;
		if (!Assembly.Actors || Assembly.Origin != 0)
		{
			return MakeError("a script section needs its actor count and bytecode from offset 0");
		}
		const std::size_t Actors = static_cast<std::size_t>(*Assembly.Actors);
		const std::size_t CodeAt = EntryPointsAt + Actors * FEventScripts::ScriptsPerActor * 2;
		std::vector<uint8> Section(CodeAt, 0);
		for (const uint16 Address : Assembly.UnsignedVariables)
		{
			// One bit a variable: its word by the address's high bits, its bit by the five above the lowest.
			const std::size_t Word = (Address >> 6) * 4;
			LittleEndian::WriteU32(Section, Word, LittleEndian::ReadU32(Section, Word) | uint32{1} << (Address >> 1 & 31));
		}
		LittleEndian::WriteU32(Section, BitmapBytes, static_cast<uint32>(Actors));
		for (const FEventEntryPoint& Entry : Assembly.EntryPoints)
		{
			if (static_cast<std::size_t>(Entry.Actor) >= Actors)
			{
				return MakeError("a script of actor " + std::to_string(Entry.Actor) + " among " + std::to_string(Actors) + " actors");
			}
			LittleEndian::WriteU16(Section, EntryPointsAt + static_cast<std::size_t>(Entry.Actor * FEventScripts::ScriptsPerActor + Entry.Script) * 2, Entry.Offset);
		}
		Section.insert(Section.end(), Assembly.Code.begin(), Assembly.Code.end());
		return Section;
	}

	TResult<std::vector<uint8>> AssembleEventScripts(std::string_view Text)
	{
		TResult<FEventAssembly> Assembly = AssembleEventText(Text);
		if (!Assembly)
		{
			return Assembly.Error();
		}
		return BuildEventScriptSection(*Assembly);
	}
}
