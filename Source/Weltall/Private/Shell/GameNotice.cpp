#include "Shell/GameNotice.h"

#include "Foundation/Result.h"
#include "GenericPlatform/GenericPlatformOutputDevices.h"
#include "Menu/OptionsText.h"
#include "Misc/Paths.h"
#include "Shell/GameVersion.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#include <cctype>
#include <filesystem>
#include <string>

namespace
{
	/** The widest the text runs before it wraps, in Slate's units: a comfortable line on any display. */
	constexpr float NoticeWidth = 1100.0f;
}

FGameNotice MakeGameNotice(const FText& Text, bool bOverPicture)
{
	const TSharedRef<STextBlock> Lines = SNew(STextBlock)
		.Text(Text)
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
		.ColorAndOpacity(FLinearColor::White)
		.AutoWrapText(true);
	const TSharedRef<SBorder> Widget = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, bOverPicture ? 0.85f : 1.0f))
		.Padding(FMargin(48.0f, 40.0f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.MaxDesiredWidth(NoticeWidth)
			[
				Lines
			]
		];
	return {Widget, Lines};
}

FText GameStoppedText(const Weltall::FError& Error, bool bStarted)
{
	namespace Text = Weltall::OptionsText::Stop;
	// The reason as a sentence of its own.
	std::string Reason = Error.Message;
	if (!Reason.empty())
	{
		Reason[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(Reason[0])));
		if (Reason.back() != '.')
		{
			Reason += '.';
		}
	}
	const FString LogFile = FPaths::ConvertRelativePathToFull(FGenericPlatformOutputDevices::GetAbsoluteLogFilename());
	const std::u8string LogName = std::filesystem::path(*LogFile).make_preferred().u8string();
	std::string Message = std::string(bStarted ? Text::Stopped : Text::CannotStart) + "\n\n" + Reason + "\n\n"
		+ Weltall::OptionsText::Fill(Text::Log, {std::string(LogName.begin(), LogName.end())});
	if (bStarted)
	{
		Message += "\n\n" + std::string(Text::SavesKept);
	}
	Message += "\n\n" + Weltall::OptionsText::Fill(Text::Version, {std::string(TCHAR_TO_UTF8(*GameVersion()))});
	return FText::FromString(FString(UTF8_TO_TCHAR(Message.c_str())));
}
