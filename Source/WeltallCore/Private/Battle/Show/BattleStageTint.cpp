#include "Battle/Show/BattleStageTint.h"

#include "Battle/Show/BattleShowObjects.h"
#include "Battle/Show/BattleShowScreen.h"
#include "Battle/Show/BattleStageBackdrop.h"
#include "Gears/GearFiles.h"
#include "Graphics/ClutFade.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The layers' modes past the kernel's tint's four: the palettes kept aside faded toward black. */
		constexpr uint8 FadeToBlack = 4;
		/** Where the palettes' bounds start, beyond VRAM on every side. */
		constexpr int32 FarCorner = 0x800;

		/** Where each colour is kept: the dome's three bands and its glow's two ends, the first panorama's three bands, the clear colour. */
		constexpr std::size_t DomeBands = 0;
		constexpr std::size_t DomeGlow = 3;
		constexpr std::size_t PanoramaBands = 5;
		constexpr std::size_t ClearColour = 8;
	}

	void FBattleStageTint::Measure(std::span<const FGearTextureBlock> Images)
	{
		uint32 Left = FarCorner;
		uint32 Top = FarCorner;
		int32 Right = -FarCorner;
		int32 Bottom = -FarCorner;
		for (const FGearTextureBlock& Image : Images)
		{
			if (!Image.bClut)
			{
				continue;
			}
			const uint32 X = uint32{static_cast<uint16>(Image.X)} + static_cast<uint16>(Image.OffsetX);
			const uint32 Y = uint32{static_cast<uint16>(Image.Y)} + static_cast<uint16>(Image.OffsetY);
			Left = std::min(Left, X);
			Top = std::min(Top, Y);
			Right = std::max(Right, static_cast<int32>(X + static_cast<uint16>(Image.Width)));
			Bottom = std::max(Bottom, static_cast<int32>(Y + static_cast<uint16>(Image.Height)));
		}
		PaletteRect = {static_cast<int16>(Left), static_cast<int16>(Top), static_cast<int16>(Right - static_cast<int32>(Left)),
			static_cast<int16>(Bottom - static_cast<int32>(Top))};
	}

	void FBattleStageTint::Begin(FBattleTintLayers& Layers, FBattleObjects& Objects, const FBattleStageBackdrop& Backdrop, std::span<const uint8, 3> Clear)
	{
		if (!Objects.StageTintPalette.bActive)
		{
			Layers.bChanged = false;
		}
		Objects.BeginStageTint(PaletteRect);
		Kept = Gather(Backdrop, Clear);
	}

	void FBattleStageTint::Apply(FBattleTintLayers& Layers, FBattleObjects& Objects, FBattleStageBackdrop& Backdrop, std::span<uint8, 3> Clear)
	{
		FBattleClutAnimation& Palette = Objects.StageTintPalette;
		if (!(Layers.bChanged || Palette.bFed) || !Palette.bActive)
		{
			return;
		}
		Layers.bChanged = false;
		Palette.bFed = false;
		const std::size_t Count = static_cast<std::size_t>(std::clamp(PaletteRect.Width * PaletteRect.Height, 0, static_cast<int32>(Palette.Source.size())));
		const std::span<const uint16> KeptPalettes = std::span<const uint16>(Palette.Source).first(Count);
		const std::span<uint16> Tinted = std::span<uint16>(Palette.Second).first(Count);
		std::copy(KeptPalettes.begin(), KeptPalettes.end(), Tinted.begin());
		FColours Colours = Kept;
		bool bRecoloured = false;
		for (int32 Layer = 0; Layer < FBattleTintLayers::LayerCount; ++Layer)
		{
			const FBattleTintLayers::FLayer& Tint = Layers.Layers[static_cast<std::size_t>(Layer)];
			if (Tint.bOn == 0)
			{
				continue;
			}
			if (Tint.Mode < FadeToBlack)
			{
				TintClutColors(Tinted, Tinted, Tint.Color[0], Tint.Color[1], Tint.Color[2], Tint.Mode, Tint.Level, Tinted);
			}
			else if (Tint.Mode == FadeToBlack)
			{
				FadeClutColors(KeptPalettes, Tint.Level, Tinted);
			}
			for (std::array<uint8, 3>& Colour : Colours)
			{
				Layers.Tint(Layer, Colour);
			}
			bRecoloured = true;
		}
		if (bRecoloured)
		{
			Scatter(Colours, Backdrop, Clear);
		}
		Objects.Services->LoadImage(Palette.Rect, Palette.Second);
	}

	void FBattleStageTint::End(FBattleObjects& Objects, FBattleStageBackdrop& Backdrop, std::span<uint8, 3> Clear)
	{
		Objects.EndStageTint();
		Scatter(Kept, Backdrop, Clear);
	}

	FBattleStageTint::FColours FBattleStageTint::Gather(const FBattleStageBackdrop& Backdrop, std::span<const uint8, 3> Clear)
	{
		FColours Colours{};
		if (Backdrop.Dome)
		{
			std::copy(Backdrop.Dome->Colors.begin(), Backdrop.Dome->Colors.end(), Colours.begin() + DomeBands);
			std::copy(Backdrop.Dome->Glow.begin(), Backdrop.Dome->Glow.end(), Colours.begin() + DomeGlow);
		}
		if (Backdrop.Panoramas[0])
		{
			std::copy(Backdrop.Panoramas[0]->Colors.begin(), Backdrop.Panoramas[0]->Colors.end(), Colours.begin() + PanoramaBands);
		}
		std::copy(Clear.begin(), Clear.end(), Colours[ClearColour].begin());
		return Colours;
	}

	void FBattleStageTint::Scatter(const FColours& Colours, FBattleStageBackdrop& Backdrop, std::span<uint8, 3> Clear)
	{
		if (Backdrop.Dome)
		{
			FBattleFloorDome& Dome = *Backdrop.Dome;
			std::copy_n(Colours.begin() + DomeBands, Dome.Colors.size(), Dome.Colors.begin());
			std::copy_n(Colours.begin() + DomeGlow, Dome.Glow.size(), Dome.Glow.begin());
		}
		if (Backdrop.Panoramas[0])
		{
			FFieldPanorama& Panorama = *Backdrop.Panoramas[0];
			std::copy_n(Colours.begin() + PanoramaBands, Panorama.Colors.size(), Panorama.Colors.begin());
		}
		std::copy(Colours[ClearColour].begin(), Colours[ClearColour].end(), Clear.begin());
	}
}
