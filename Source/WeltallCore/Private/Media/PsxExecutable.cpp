#include "Media/PsxExecutable.h"

#include "Foundation/LittleEndian.h"
#include "Media/Iso9660.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr std::string_view Magic = "PS-X EXE";
		constexpr std::size_t PcOffset = 0x10;
		constexpr std::size_t GpOffset = 0x14;
		constexpr std::size_t LoadAddressOffset = 0x18;
		constexpr std::size_t LoadSizeOffset = 0x1C;

		std::string_view Trim(std::string_view Text)
		{
			const auto First = Text.find_first_not_of(" \t\r");
			const auto Last = Text.find_last_not_of(" \t\r");
			return First == std::string_view::npos ? std::string_view() : Text.substr(First, Last - First + 1);
		}
	}

	TResult<FPsxExecutable> FPsxExecutable::Parse(std::vector<uint8> File)
	{
		if (File.size() < HeaderSize || std::string_view(reinterpret_cast<const char*>(File.data()), Magic.size()) != Magic)
		{
			return MakeError("not a PS-X EXE");
		}
		const std::span<const uint8> Header(File.data(), HeaderSize);
		const uint32 LoadSize = LittleEndian::ReadU32(Header, LoadSizeOffset);
		const uint32 BodySize = std::min<uint32>(LoadSize, static_cast<uint32>(File.size() - HeaderSize));
		const uint32 Pc = LittleEndian::ReadU32(Header, PcOffset);
		const uint32 Gp = LittleEndian::ReadU32(Header, GpOffset);
		const uint32 Load = LittleEndian::ReadU32(Header, LoadAddressOffset);
		return FPsxExecutable(std::move(File), Pc, Gp, Load, BodySize);
	}

	FPsxExecutable::FPsxExecutable(std::vector<uint8> InFile, uint32 InPc, uint32 InGp, uint32 InLoad, uint32 InBodySize)
		: File(std::move(InFile))
		, Pc(InPc)
		, Gp(InGp)
		, Load(InLoad)
		, BodySize(InBodySize)
	{
	}

	bool FPsxExecutable::Contains(uint32 Address, uint32 Size) const
	{
		return Address >= Load && Size <= BodySize && Address - Load <= BodySize - Size;
	}

	std::span<const uint8> FPsxExecutable::Read(uint32 Address, uint32 Size) const
	{
		return Contains(Address, Size) ? Body().subspan(Address - Load, Size) : std::span<const uint8>();
	}

	uint32 FPsxExecutable::ReadU32(uint32 Address) const
	{
		const auto Bytes = Read(Address, 4);
		return Bytes.empty() ? 0 : LittleEndian::ReadU32(Bytes, 0);
	}

	TResult<std::string> ParseBootExecutable(std::string_view SystemCnf)
	{
		while (!SystemCnf.empty())
		{
			const auto LineEnd = SystemCnf.find('\n');
			const std::string_view Line = SystemCnf.substr(0, LineEnd);
			SystemCnf = LineEnd == std::string_view::npos ? std::string_view() : SystemCnf.substr(LineEnd + 1);
			const auto Equals = Line.find('=');
			if (Equals == std::string_view::npos || Trim(Line.substr(0, Equals)) != "BOOT")
			{
				continue;
			}
			std::string_view Value = Trim(Line.substr(Equals + 1));
			if (const auto Colon = Value.find(':'); Colon != std::string_view::npos)
			{
				Value = Value.substr(Colon + 1);
			}
			Value = Value.substr(0, Value.find(';'));
			std::string Path;
			for (const char C : Value)
			{
				if (C == '\\' || C == '/')
				{
					if (!Path.empty())
					{
						Path.push_back('/');
					}
					continue;
				}
				Path.push_back(C);
			}
			if (!Path.empty())
			{
				return Path;
			}
		}
		return MakeError("SYSTEM.CNF names no boot executable");
	}

	TResult<std::string> FindBootExecutable(const FIso9660& Disc)
	{
		auto SystemCnf = Disc.ReadFile("SYSTEM.CNF");
		if (!SystemCnf)
		{
			return MakeError("not a PlayStation disc: " + SystemCnf.Error().Message);
		}
		return ParseBootExecutable(std::string_view(reinterpret_cast<const char*>(SystemCnf->data()), SystemCnf->size()));
	}

	TResult<FPsxExecutable> ReadBootExecutable(const FIso9660& Disc)
	{
		auto Path = FindBootExecutable(Disc);
		if (!Path)
		{
			return Path.Error();
		}
		auto File = Disc.ReadFile(*Path);
		if (!File)
		{
			return File.Error();
		}
		return FPsxExecutable::Parse(std::move(*File));
	}
}
