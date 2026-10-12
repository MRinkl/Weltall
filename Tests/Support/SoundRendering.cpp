#include "Support/SoundRendering.h"

#include "Foundation/Sha1.h"

#include <algorithm>

namespace Weltall::Testing
{
	std::vector<int16> RenderSound(IGameSound& Sound, int32 Frames, int32 BufferFrames)
	{
		std::vector<int16> Samples(static_cast<std::size_t>(Frames) * 2);
		for (int32 Made = 0; Made < Frames; Made += BufferFrames)
		{
			const int32 Count = std::min(BufferFrames, Frames - Made);
			Sound.Render(std::span<int16>(Samples).subspan(static_cast<std::size_t>(Made) * 2, static_cast<std::size_t>(Count) * 2));
		}
		return Samples;
	}

	int32 Peak(const std::vector<int16>& Samples)
	{
		int32 Loudest = 0;
		for (const int16 Sample : Samples)
		{
			Loudest = std::max(Loudest, Sample < 0 ? -static_cast<int32>(Sample) : static_cast<int32>(Sample));
		}
		return Loudest;
	}

	std::string DigestSamples(const std::vector<int16>& Samples)
	{
		FSha1 Hasher;
		for (const int16 Sample : Samples)
		{
			const uint16 Bits = static_cast<uint16>(Sample);
			const uint8 Bytes[2] = {static_cast<uint8>(Bits), static_cast<uint8>(Bits >> 8)};
			Hasher.Update(Bytes);
		}
		return FSha1::ToHex(Hasher.Finish());
	}
}
