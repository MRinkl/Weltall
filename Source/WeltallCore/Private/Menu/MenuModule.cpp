#include "Menu/MenuModule.h"

#include "Formats/Tim.h"
#include "Game/GameState.h"
#include "Input/KernelPad.h"

namespace Weltall
{
	namespace
	{
		/** The steps' frames nest as deep as the screens do: a few kilobytes each at most. */
		constexpr std::size_t TaskArenaBytes = 64 * 1024;
		/** The layer's depth before anything opens it: far, a quarter of the screen's scale. */
		constexpr int32 LayerRestingDepth = 0x800;
		/** The layer's steps: opening from far while it turns about x, closing away while it turns about y. */
		constexpr int32 OpeningDepth = 0x800;
		constexpr int32 OpenDepth = 0x200;
		constexpr int32 OpeningStep = 0x30;
		constexpr int16 OpeningTurn = 0x7C;
		constexpr int32 ClosingStep = 0x40;
		constexpr int16 ClosingTurn = 0x60;
		constexpr int32 ClosedDepth = 0xDFF;
		/** The ordering table's entries: the pieces, and under them the backdrop. */
		constexpr int32 PiecesEntry = 4;
		constexpr int32 BackdropEntry = 8;
		/** The backdrop's subtractive grey over the whole screen, and its drawing mode's page. */
		constexpr uint8 BackdropGrey = 0x80;
		constexpr int32 BackdropPageX = 0x180;
		constexpr int16 ScreenWidth = 320;
		constexpr int16 ScreenHeight = 224;
		/** The large portraits' frames in the sprite sheet, one per party slot, whose texture places the portraits' TIMs. */
		constexpr int32 FirstPortraitFrame = 0x14B;
		/** The second slot's portrait lies this many units right of where its frame's texture says. */
		constexpr int32 SecondPortraitShift = 0xC;
		constexpr uint8 Blank = 0xFF;

		namespace Effect
		{
			constexpr uint8 LayerOpens = 0x5B;
			constexpr uint8 LayerCloses = 0x5C;
		}
	}

	FMenuModule::FMenuModule(const FMenuServices& InServices, const FMenuRequest& InRequest)
		: Services(InServices), Request(InRequest), Resources(*InServices.Resources), Arena(TaskArenaBytes)
	{
		Layer.Translation = {0, 0, LayerRestingDepth};
		FlatGte = MenuProjection(MenuFlatTransform);
		LabelImage.Rect = {0, 0, 28, 13};
		LabelImage.Units.assign(28 * 13, 0);
		NameImage.Rect = {0, 0, 0x28, 13};
		NameImage.Units.assign(0x28 * 13, 0);
		PromptImage.Rect = {0, 0, 0x3A, 13};
		PromptImage.Units.assign(0x3A * 13, 0);
	}

	void FMenuModule::DrawOrWaitForPad()
	{
		if (Services.Pad->IsConnected())
		{
			DrawFrame();
			return;
		}
		bPadWait = true;
		PadWaitPlayTime = Services.Game->PlayTime();
		if (Services.Sound != nullptr)
		{
			Services.Sound->Pause();
		}
	}

	bool FMenuModule::StepPadWait()
	{
		if (!bPadWait)
		{
			return false;
		}
		if (!Services.Pad->IsConnected())
		{
			return true;
		}
		// The time without a controller is no play time; the frame the read held goes on.
		bPadWait = false;
		if (Services.Sound != nullptr)
		{
			Services.Sound->Resume();
		}
		Services.Game->SetPlayTime(PadWaitPlayTime);
		DrawFrame();
		return true;
	}

	void FMenuModule::ReadInput(std::span<const FMenuInputTest> Tests)
	{
		FKernelPad& Pad = *Services.Pad;
		Code = EMenuCode::None;
		if (Pad.Overflowed())
		{
			Pad.Reset();
			PadHeld = 0;
			return;
		}
		// The queued vertical blanks are taken one at a time until one yields a code; the rest wait.
		while (const std::optional<FKernelPadWords> Words = Pad.Pop())
		{
			PadHeld = Words->Held;
			for (const FMenuInputTest& Test : Tests)
			{
				if (((Test.bRepeated ? Words->Repeated : Words->Pressed) & Test.Bits) != 0)
				{
					Code = Test.Code;
					if (Test.Sound != 0)
					{
						PlaySound(Test.Sound);
					}
					return;
				}
			}
		}
	}

	void FMenuModule::UpdateTransition()
	{
		switch (LayerState)
		{
		case EMenuLayer::Closing:
			Layer.Translation[2] += ClosingStep;
			Layer.Angles[1] = static_cast<int16>(Layer.Angles[1] - ClosingTurn);
			if (Layer.Translation[2] > ClosedDepth)
			{
				LayerState = EMenuLayer::Still;
			}
			break;
		case EMenuLayer::Opening:
			Layer.Translation[2] -= OpeningStep;
			Layer.Angles[0] = static_cast<int16>(Layer.Angles[0] + OpeningTurn);
			if (Layer.Translation[2] < OpenDepth)
			{
				Layer.Translation[2] = OpenDepth;
				Layer.Angles = {};
				LayerState = EMenuLayer::Still;
			}
			break;
		case EMenuLayer::OpenAsked:
		case EMenuLayer::CloseAsked:
			Layer.Translation = {0, 0, LayerState == EMenuLayer::OpenAsked ? OpeningDepth : OpenDepth};
			Layer.Angles = {};
			LayerState = LayerState == EMenuLayer::OpenAsked ? EMenuLayer::Opening : EMenuLayer::Closing;
			break;
		case EMenuLayer::Still:
			break;
		}
	}

	void FMenuModule::OpenLayer()
	{
		LayerState = EMenuLayer::OpenAsked;
		PlaySound(Effect::LayerOpens);
	}

	void FMenuModule::CloseLayer()
	{
		LayerState = EMenuLayer::CloseAsked;
		PlaySound(Effect::LayerCloses);
	}

	void FMenuModule::AddScreen(const std::vector<FMenuPrimitive>& Primitives)
	{
		for (const FMenuPrimitive& Primitive : Primitives)
		{
			Table.Add(PiecesEntry, Primitive);
		}
	}

	void FMenuModule::Upload(const FVramImage& Image)
	{
		MenuVram.Upload(Image);
		if (UploadObserver != nullptr)
		{
			UploadObserver->Upload(Image);
		}
	}

	void FMenuModule::PlaySound(uint8 Sound)
	{
		if (bSoundOn && Services.Sound != nullptr)
		{
			Services.Sound->PlaySystemEffect(Sound);
		}
	}

	void FMenuModule::SplitNumber(uint32 Value)
	{
		uint32 Power = 100000000;
		for (uint8& Digit : NumberDigits)
		{
			Digit = static_cast<uint8>(Value / Power);
			Value %= Power;
			Power /= 10;
		}
		// Leading zeros are blank; the last digit always shows.
		for (std::size_t Digit = 1; Digit < NumberDigits.size(); ++Digit)
		{
			if (NumberDigits[Digit] != 0)
			{
				if (NumberDigits[Digit - 1] == 0)
				{
					NumberDigits[Digit - 1] = Blank;
				}
				return;
			}
			NumberDigits[Digit - 1] = Blank;
		}
	}

	void FMenuModule::InitBackdrop()
	{
		Background.Code = MenuGpu::FlatQuad | MenuGpu::SemiTransparent;
		const FDrawColor Grey{BackdropGrey, BackdropGrey, BackdropGrey};
		Background.Corners = {FMenuCorner{0, 0, 0, 0, Grey}, FMenuCorner{ScreenWidth, 0, 0, 0, Grey}, FMenuCorner{0, ScreenHeight, 0, 0, Grey},
			FMenuCorner{ScreenWidth, ScreenHeight, 0, 0, Grey}};
		Background.bWholeScreen = true;
		Background.BlendKey = MenuBlendKey::Background;
		BackgroundMode = FMenuPrimitive::MakeDrawMode(MenuGpu::TexturePage(0, 2, BackdropPageX, 0));
	}

	void FMenuModule::AddBackdrop()
	{
		Table.Add(BackdropEntry, Background);
		Table.Add(BackdropEntry, BackgroundMode);
	}

	void FMenuModule::UploadPortrait(int32 Slot, uint8 Character)
	{
		TResult<FTim> Portrait = ParseTim(Resources.Portrait(Character));
		if (!Portrait || !Portrait->Clut)
		{
			return;
		}
		const FMenuSpriteTexture Place = Resources.Sheet().Texture(FirstPortraitFrame + Slot);
		Portrait->Clut->Rect.X = static_cast<int16>(Place.ClutX);
		Portrait->Clut->Rect.Y = static_cast<int16>(Place.ClutY);
		Portrait->Image.Rect.X = static_cast<int16>(Place.VramX + (Slot == 1 ? SecondPortraitShift : 0));
		Portrait->Image.Rect.Y = static_cast<int16>(Place.VramY);
		Upload(*Portrait->Clut);
		Upload(Portrait->Image);
	}
}
