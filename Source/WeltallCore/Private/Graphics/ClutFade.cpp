#include "Graphics/ClutFade.h"

#include <algorithm>
#include <array>

namespace Weltall
{
	namespace
	{
		constexpr int32 FullFactor = 32;
		constexpr uint16 SemiTransparent = 0x8000;
		constexpr uint16 Red = 0x001F;
		constexpr uint16 Green = 0x03E0;
		constexpr uint16 Blue = 0x7C00;

		/**
		 * GPF with the fraction shift: IR0 times a channel, shifted down 12 bits and saturated to the
		 * IR register's 16 signed bits. The channels go in unshifted, so each keeps its place.
		 */
		int32 Scale(int32 Ir0, int32 Channel)
		{
			const int64 Mac = static_cast<int64>(Ir0) * Channel >> 12;
			return static_cast<int32>(std::clamp<int64>(Mac, -0x8000, 0x7FFF));
		}
	}

	void FadeClutColors(std::span<const uint16> Colors, int32 Factor, std::span<uint16> Faded)
	{
		// The GTE's IR0 keeps the low 16 bits of the scaled factor, sign-extended.
		const int32 Ir0 = static_cast<int16>(static_cast<uint32>(std::min(Factor, FullFactor)) << 7);
		for (std::size_t Index = 0; Index < Colors.size(); ++Index)
		{
			const uint16 Color = Colors[Index];
			uint16 Scaled = static_cast<uint16>((Scale(Ir0, Color & Blue) & Blue) | (Scale(Ir0, Color & Red) & Red) | (Scale(Ir0, Color & Green) & Green));
			if (Color != 0 && Scaled == 0)
			{
				Scaled = 1;
			}
			Faded[Index] = static_cast<uint16>((Color & SemiTransparent) | Scaled);
		}
	}

	void LerpClutColors(std::span<const uint16> From, std::span<const uint16> To, int32 T, std::span<uint16> Blended)
	{
		const int32 Ir0 = static_cast<int16>(static_cast<uint32>(std::min(T, FullFactor)) << 7);
		for (std::size_t Index = 0; Index < From.size(); ++Index)
		{
			const int32 Start = From[Index];
			const int32 End = To[Index];
			const int32 R = (Start & Red) + (Scale(Ir0, (End & Red) - (Start & Red)) & Red);
			const int32 G = (Start & Green) + (Scale(Ir0, (End & Green) - (Start & Green)) & Green);
			const int32 B = (Start & Blue) + (Scale(Ir0, (End & Blue) - (Start & Blue)) & Blue);
			Blended[Index] = static_cast<uint16>(R | G | B);
		}
	}

	void TintClutColors(std::span<const uint16> Colors, std::span<const uint16> Targets, int32 AddRed, int32 AddGreen, int32 AddBlue, int32 Mode,
		int32 T, std::span<uint16> Tinted)
	{
		const int32 Ir0 = static_cast<int16>(static_cast<uint32>(std::min(T, FullFactor)) << 7);
		// Each sum is tested as the 16 bits it keeps: negative clears it, past its channel's top clamps it.
		const auto Clamp = [](int32 Sum, int32 Top) { return std::min<int32>(std::max<int32>(static_cast<int16>(Sum), 0), Top); };
		for (std::size_t Index = 0; Index < Colors.size(); ++Index)
		{
			const int32 Color = Colors[Index];
			std::array<int32, 3> Base{Color & Red, Color & Green, Color & Blue};
			if (Mode == 1)
			{
				Base = {(Color & 0x001E) >> 1, (Color & 0x03C0) >> 1, (Color & 0x7800) >> 1};
			}
			else if (Mode == 2)
			{
				Base = {(Color & 0x001C) >> 2, (Color & 0x0380) >> 2, (Color & 0x7000) >> 2};
			}
			else if (Mode == 3)
			{
				const int32 Grey = ((Color & Red) + ((Color & Green) >> 5) + ((Color & Blue) >> 10)) / 3;
				Base = {Grey, Grey << 5, Grey << 10};
			}
			const int32 R = Clamp(Base[0] + AddRed, Red);
			const int32 G = Clamp(Base[1] + (AddGreen << 5), Green);
			const int32 B = Clamp(Base[2] + (AddBlue << 10), Blue);
			if (Color == 0)
			{
				continue;
			}
			const int32 Target = Targets[Index];
			Tinted[Index] = static_cast<uint16>((R + (Scale(Ir0, (Target & Red) - R) & Red)) | SemiTransparent | (G + (Scale(Ir0, (Target & Green) - G) & Green))
				| (B + (Scale(Ir0, (Target & Blue) - B) & Blue)));
		}
	}
}
