#include "Field/FieldMoonGlow.h"

#include "Graphics/Vram.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace Weltall
{
	namespace
	{
		/** Where a map's moon lies in VRAM: its picture of 8-bit texels, and its CLUT. */
		struct FMoon
		{
			int32 Map = 0;
			int16 X = 0;
			int16 Y = 0;
			int16 ClutX = 0;
			int16 ClutY = 0;
		};

		constexpr std::array<FMoon, 2> Moons = {{{278, 512, 0, 0, 504}, {283, 320, 256, 0, 505}}};

		/** The picture is this many texels square, two to a unit of VRAM. */
		constexpr int32 Side = 128;
		constexpr double Middle = (Side - 1) / 2.0;
		/** How far from the middle the glow is kept as painted, how far it is painted out to, and the picture's edge. */
		constexpr double KeptOut = 58.0;
		constexpr double PaintedOut = 80.0;
		constexpr double Edge = Side / 2.0;

		/** A CLUT colour's red, which the moon has and its navy glow has not. */
		constexpr uint16 Red = 0x1F;
	}

	void FadeMoonGlow(int32 Map, FVram& Vram)
	{
		const auto Moon = std::find_if(Moons.begin(), Moons.end(), [Map](const FMoon& Each) { return Each.Map == Map; });
		if (Moon == Moons.end())
		{
			return;
		}
		std::vector<uint8> Texels(static_cast<std::size_t>(Side * Side));
		for (int32 V = 0; V < Side; ++V)
		{
			for (int32 U = 0; U < Side; ++U)
			{
				Texels[static_cast<std::size_t>(V * Side + U)] = static_cast<uint8>(Vram.At(Moon->X + U / 2, Moon->Y + V) >> (U % 2 * 8));
			}
		}
		const auto Colour = [&](uint8 Texel) { return Vram.At(Moon->ClutX + Texel, Moon->ClutY); };
		const auto IsGlow = [&](uint8 Texel) { return (Colour(Texel) & Red) == 0; };
		std::optional<uint8> Clear;
		for (int32 Texel = 0; Texel < 256 && !Clear; ++Texel)
		{
			if (Colour(static_cast<uint8>(Texel)) == 0)
			{
				Clear = static_cast<uint8>(Texel);
			}
		}
		// The moon's picture as the disc has it: a moon in the middle, nothing drawn in the corners.
		const uint8 InTheMiddle = Texels[static_cast<std::size_t>(Side / 2 * Side + Side / 2)];
		if (!Clear || IsGlow(InTheMiddle) || Colour(Texels[0]) != 0)
		{
			return;
		}

		std::vector<uint8> Faded = Texels;
		const double Stretch = (PaintedOut - KeptOut) / (Edge - KeptOut);
		for (int32 V = 0; V < Side; ++V)
		{
			for (int32 U = 0; U < Side; ++U)
			{
				const std::size_t At = static_cast<std::size_t>(V * Side + U);
				const double Distance = std::hypot(U - Middle, V - Middle);
				if (Distance < KeptOut || !IsGlow(Texels[At]))
				{
					continue;
				}
				// The texel the painted glow has as far out as this one's place is drawn to: the glow from KeptOut to
				// PaintedOut drawn between KeptOut and the edge.
				const double From = KeptOut + (Distance - KeptOut) * Stretch;
				const int32 FromU = static_cast<int32>(std::lround(Middle + (U - Middle) * From / Distance));
				const int32 FromV = static_cast<int32>(std::lround(Middle + (V - Middle) * From / Distance));
				const bool bInside = From < PaintedOut && FromU >= 0 && FromU < Side && FromV >= 0 && FromV < Side;
				const uint8 Painted = bInside ? Texels[static_cast<std::size_t>(FromV * Side + FromU)] : *Clear;
				Faded[At] = IsGlow(Painted) ? Painted : *Clear;
			}
		}

		FVramImage Picture{{Moon->X, Moon->Y, Side / 2, Side}, std::vector<uint16>(static_cast<std::size_t>(Side / 2 * Side))};
		for (std::size_t Unit = 0; Unit < Picture.Units.size(); ++Unit)
		{
			Picture.Units[Unit] = static_cast<uint16>(Faded[Unit * 2] | Faded[Unit * 2 + 1] << 8);
		}
		Vram.Upload(Picture);
	}
}
