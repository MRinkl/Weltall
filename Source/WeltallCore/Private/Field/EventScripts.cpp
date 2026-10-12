#include "Field/EventScripts.h"

#include "Foundation/LittleEndian.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		constexpr std::size_t BitmapBytes = 0x80;
		constexpr std::size_t ActorCountAt = BitmapBytes;
		constexpr std::size_t EntryPointsAt = ActorCountAt + 4;
		constexpr std::size_t EntranceBytes = 7;
		constexpr uint8 EntranceTableMark = 0xFF;
		/** A map has at most a few dozen script actors; a count past this is not a script section. */
		constexpr uint32 MaxActors = 0x100;
	}

	TResult<FEventScripts> FEventScripts::Parse(std::span<const uint8> Section)
	{
		if (Section.size() < EntryPointsAt)
		{
			return MakeError("event script section is shorter than its header");
		}
		const uint32 Actors = ReadU32(Section, ActorCountAt);
		const std::size_t CodeAt = EntryPointsAt + std::size_t{Actors} * ScriptsPerActor * 2;
		if (Actors > MaxActors || CodeAt > Section.size())
		{
			return MakeError("event script section names " + std::to_string(Actors) + " actors its " + std::to_string(Section.size()) + " bytes cannot hold");
		}
		FEventScripts Scripts;
		for (std::size_t Word = 0; Word < Scripts.UnsignedBits.size(); ++Word)
		{
			Scripts.UnsignedBits[Word] = ReadU32(Section, Word * 4);
		}
		Scripts.EntryPoints.resize(std::size_t{Actors} * ScriptsPerActor);
		for (std::size_t Entry = 0; Entry < Scripts.EntryPoints.size(); ++Entry)
		{
			Scripts.EntryPoints[Entry] = ReadU16(Section, EntryPointsAt + Entry * 2);
		}
		Scripts.Size = Section.size() - CodeAt;
		Scripts.Bytes.assign(Section.begin() + static_cast<std::ptrdiff_t>(CodeAt), Section.end());
		Scripts.Bytes.resize(Scripts.Size + ReadSlack, 0);
		return Scripts;
	}

	std::optional<FEventEntrance> FEventScripts::Entrance(int32 Index) const
	{
		const std::size_t At = 1 + static_cast<std::size_t>(Index) * EntranceBytes;
		if (Size == 0 || Bytes[0] != EntranceTableMark || Index < 0 || At + EntranceBytes > Size)
		{
			return std::nullopt;
		}
		const std::span<const uint8> Code(Bytes.data(), Size);
		return FEventEntrance{ReadS16(Code, At), ReadS16(Code, At + 2), Code[At + 4], Code[At + 5], Code[At + 6]};
	}

	bool FEventScripts::Replace(std::size_t Offset, std::span<const uint8> Was, std::span<const uint8> Now)
	{
		if (Was.size() != Now.size() || Offset > Size || Size - Offset < Was.size()
			|| !std::equal(Was.begin(), Was.end(), Bytes.begin() + static_cast<std::ptrdiff_t>(Offset)))
		{
			return false;
		}
		std::copy(Now.begin(), Now.end(), Bytes.begin() + static_cast<std::ptrdiff_t>(Offset));
		return true;
	}

	std::optional<uint16> FEventScripts::Append(std::span<const uint8> Code)
	{
		constexpr std::size_t Reach = 0x10000;
		if (Size + Code.size() > Reach)
		{
			return std::nullopt;
		}
		const uint16 Offset = static_cast<uint16>(Size);
		Bytes.resize(Size);
		Bytes.insert(Bytes.end(), Code.begin(), Code.end());
		Size = Bytes.size();
		Bytes.resize(Size + ReadSlack, 0);
		return Offset;
	}
}
