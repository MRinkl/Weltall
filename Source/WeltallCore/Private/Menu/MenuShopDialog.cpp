#include "Menu/MenuShopModule.h"

namespace Weltall
{
	namespace
	{
		constexpr int32 DialogWindow = 4;
		constexpr int16 DialogX = 0x42;
		constexpr int16 DialogY = 0x46;
		constexpr uint16 DialogHeight = 0x40;
		constexpr int32 PiecesEntry = 4;
		constexpr int32 LinesX = 0x50;
		constexpr int32 LinesY = 0x50;
		constexpr int32 PointerFrame = 0x108;
		constexpr int32 HalfScale = 0x800;
		/** The dialogs' pointers: the last two beside Yes and No. */
		constexpr std::size_t YesCursor = 2;
		constexpr std::size_t NoCursor = 3;
		constexpr uint8 Accepted = 2;
		/** A notice left alone ends after this many checks. */
		constexpr int32 NoticeChecks = 60;
		/** The dialog's total, drawn before it opens and shown once it is up. */
		constexpr uint8 TotalDrawn = 2;
		constexpr uint8 TotalShown = 1;
	}

	FMenuShopModule::FMenuShopModule(const FMenuServices& InServices, const FMenuRequest& InRequest, uint16 InDialogWidth)
		: FMenuModule(InServices, InRequest), DialogWidth(InDialogWidth)
	{
		DetailImage.Rect = {0x140, 0x4E, 0x3C, 13};
		DetailImage.Units.assign(0x3C * 13, 0);
	}

	TMenuTask<> FMenuShopModule::OpenDialog(uint8 FirstText)
	{
		OpenWindow(DialogWindow, DialogX, DialogY, DialogWidth, DialogHeight, true, true, PiecesEntry, false);
		while (!Growths[DialogWindow].bGrown)
		{
			co_await Frame();
		}
		RenderPromptLines(DialogLines, FirstText, LinesX, LinesY);
		bDialogShown = true;
		if (TotalState == TotalDrawn)
		{
			TotalState = TotalShown;
		}
		co_await Frame();
		co_await Frame();
	}

	TMenuTask<> FMenuShopModule::CloseDialog()
	{
		if (Flags[static_cast<std::size_t>(MenuFlag::Windows + DialogWindow)] != 0)
		{
			CloseWindow(DialogWindow);
			bDialogShown = false;
		}
		TotalState = 0;
		co_await Frame();
	}

	TMenuTask<bool> FMenuShopModule::RunYesNo(bool bWaits, bool bChoice)
	{
		bool bYes = false;
		bool bAsks = true;
		int32 Checks = NoticeChecks;
		while (bAsks)
		{
			if (!bWaits)
			{
				ChoiceShown[YesCursor] = false;
				ChoiceShown[NoCursor] = false;
				if (Code != EMenuCode::None || --Checks == 0)
				{
					break;
				}
			}
			co_await Frame();
			switch (Code)
			{
			case EMenuCode::Left:
				if (bChoice)
				{
					ChoiceShown[YesCursor] = true;
					ChoiceShown[NoCursor] = false;
					bYes = true;
				}
				break;
			case EMenuCode::Right:
				if (bChoice)
				{
					ChoiceShown[YesCursor] = false;
					ChoiceShown[NoCursor] = true;
					bYes = false;
				}
				break;
			case EMenuCode::Confirm:
				PlaySound(Accepted);
				bAsks = false;
				break;
			case EMenuCode::Cancel:
				bYes = false;
				bAsks = false;
				break;
			default:
				break;
			}
		}
		ChoiceShown[YesCursor] = false;
		ChoiceShown[NoCursor] = false;
		co_return bYes;
	}

	TMenuTask<bool> FMenuShopModule::AskYesNo(uint8 FirstText)
	{
		co_await OpenDialog(FirstText);
		ChoiceShown[NoCursor] = true;
		const bool bYes = co_await RunYesNo(true, true);
		co_await CloseDialog();
		co_return bYes;
	}

	void FMenuShopModule::OpenChoiceCursors()
	{
		bChoiceCursorsShown = true;
		ChoiceShown = {};
		for (std::size_t Cursor = 0; Cursor < ChoiceCursors.size(); ++Cursor)
		{
			ChoiceCursors[Cursor].clear();
			Resources.Sheet().Draw(PointerFrame, ChoiceX[Cursor], ChoiceY[Cursor], HalfScale, ChoiceCursors[Cursor]);
		}
	}

	TMenuTask<> FMenuShopModule::CloseChoiceCursors()
	{
		bChoiceCursorsShown = false;
		co_await Frame();
	}

	void FMenuShopModule::AddChoiceCursors()
	{
		if (!bChoiceCursorsShown)
		{
			return;
		}
		for (std::size_t Cursor = 0; Cursor < ChoiceCursors.size(); ++Cursor)
		{
			if (ChoiceShown[Cursor] && !ChoiceCursors[Cursor].empty())
			{
				Table.Add(PiecesEntry, ChoiceCursors[Cursor].front());
			}
		}
	}
}
