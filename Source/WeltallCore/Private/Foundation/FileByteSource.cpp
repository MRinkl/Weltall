#include "Foundation/FileByteSource.h"

#include <system_error>

namespace Weltall
{
	namespace
	{
		bool Seek(std::FILE* File, uint64 Offset)
		{
#if defined(_WIN32)
			return _fseeki64(File, static_cast<long long>(Offset), SEEK_SET) == 0;
#else
			return fseeko(File, static_cast<off_t>(Offset), SEEK_SET) == 0;
#endif
		}
	}

	TResult<std::unique_ptr<FFileByteSource>> FFileByteSource::Open(const std::filesystem::path& Path)
	{
		std::error_code SizeError;
		const auto FileSize = std::filesystem::file_size(Path, SizeError);
		if (SizeError)
		{
			return MakeError("cannot read the size of " + Path.string() + ": " + SizeError.message());
		}
#if defined(_WIN32)
		std::FILE* File = nullptr;
		if (_wfopen_s(&File, Path.c_str(), L"rb") != 0)
		{
			File = nullptr;
		}
#else
		std::FILE* File = std::fopen(Path.c_str(), "rb");
#endif
		if (File == nullptr)
		{
			return MakeError("cannot open " + Path.string());
		}
		return std::unique_ptr<FFileByteSource>(new FFileByteSource(File, FileSize));
	}

	FFileByteSource::~FFileByteSource()
	{
		std::fclose(File);
	}

	bool FFileByteSource::Read(uint64 Offset, std::span<uint8> Destination) const
	{
		if (Offset > FileSize || Destination.size() > FileSize - Offset)
		{
			return false;
		}
		const std::lock_guard<std::mutex> Lock(Guard);
		return Seek(File, Offset) && std::fread(Destination.data(), 1, Destination.size(), File) == Destination.size();
	}
}
