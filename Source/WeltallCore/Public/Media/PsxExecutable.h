#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	/** A PlayStation executable (PS-X EXE) viewed as it lies in RAM once loaded. */
	class FPsxExecutable
	{
	public:
		static constexpr uint32 HeaderSize = 0x800;

		WELTALLCORE_API static TResult<FPsxExecutable> Parse(std::vector<uint8> File);

		uint32 InitialPc() const { return Pc; }
		uint32 InitialGp() const { return Gp; }
		uint32 LoadAddress() const { return Load; }
		std::span<const uint8> Body() const { return std::span<const uint8>(File).subspan(HeaderSize, BodySize); }

		WELTALLCORE_API bool Contains(uint32 Address, uint32 Size) const;

		/** Size bytes at a RAM address; empty when the range is not part of the executable. */
		WELTALLCORE_API std::span<const uint8> Read(uint32 Address, uint32 Size) const;
		WELTALLCORE_API uint32 ReadU32(uint32 Address) const;

	private:
		WELTALLCORE_API FPsxExecutable(std::vector<uint8> InFile, uint32 InPc, uint32 InGp, uint32 InLoad, uint32 InBodySize);

		std::vector<uint8> File;
		uint32 Pc;
		uint32 Gp;
		uint32 Load;
		uint32 BodySize;
	};

	/** The boot executable's path from SYSTEM.CNF ("BOOT = cdrom:\SLUS_006.64;1" gives "SLUS_006.64"). */
	WELTALLCORE_API TResult<std::string> ParseBootExecutable(std::string_view SystemCnf);

	class FIso9660;

	/** The path of the disc's boot executable, as SYSTEM.CNF names it. */
	WELTALLCORE_API TResult<std::string> FindBootExecutable(const FIso9660& Disc);

	WELTALLCORE_API TResult<FPsxExecutable> ReadBootExecutable(const FIso9660& Disc);
}
