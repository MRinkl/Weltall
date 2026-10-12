#include "Menu/MenuPortFrames.h"

#include "Menu/MenuResources.h"
#include "Menu/MenuSpriteSheet.h"

#include <vector>

namespace Weltall
{
	namespace
	{
		/** The Gear screen's "Gear Option" label: its p, its "tion" and its O; and "Status", whose last letter is an s. */
		constexpr int32 GearOptionLabel = 0x13C;
		constexpr int32 StatusLabel = 0x12B;
		constexpr int32 StatusOrb = 0x10F;
		constexpr int32 SelectedOrbStep = 0xD;
		/** The s of "Status": its columns in the word, outlines included. */
		constexpr int32 StatusS = 26;
		constexpr int32 LetterSWidth = 6;
		/** Each piece of "Options" from the O's place: the p sits two rows lower, its stroke reaching down. */
		constexpr int32 PAfterO = 6;
		constexpr int32 PDrop = 2;
		constexpr int32 TionAfterO = 11;
		/** The s takes the column of the n's outline as its own. */
		constexpr int32 SAfterO = 30;
		/** The O of "Options" beside the title's Sound orb, where "Sound" starts. */
		constexpr int32 TitleOptionsX = -85;
		constexpr int32 TitleOptionsY = 49;
		/** The Normal Menu's eighth orb: Status's, a step further up the arc; its label beside it as Status's is. */
		constexpr int32 OrbStepX = -10;
		constexpr int32 OrbStepY = -16;
		constexpr int32 OptionsX = -145;
		constexpr int32 OptionsY = -71;
		/** The title's File row (its Continue's) and its Load, the third command up the inner arc from its first place. */
		constexpr std::size_t TitleFileRow = 8;
		constexpr std::size_t TitleLoad = 2;
		constexpr std::size_t InnerArcPlace = 7;

		FMenuSpritePart Placed(FMenuSpritePart Part, int32 X, int32 Y)
		{
			Part.X = static_cast<int16>(X);
			Part.Y = static_cast<int16>(Y);
			return Part;
		}

		/** "Options" with its O at (X, Y) from the frame's origin; none where the sheet lacks the pieces. */
		std::vector<FMenuSpritePart> OptionsWord(const FMenuSpriteSheet& Sheet, int32 X, int32 Y)
		{
			const std::span<const FMenuSpritePart> GearOption = Sheet.Frame(GearOptionLabel);
			const std::span<const FMenuSpritePart> Status = Sheet.Frame(StatusLabel);
			if (GearOption.size() < 3 || Status.empty())
			{
				return {};
			}
			FMenuSpritePart S = Status.front();
			S.U = static_cast<uint16>(S.U + StatusS);
			S.Width = LetterSWidth;
			return {Placed(GearOption[2], X, Y), Placed(GearOption[0], X + PAfterO, Y + PDrop), Placed(GearOption[1], X + TionAfterO, Y),
				Placed(S, X + SAfterO, Y)};
		}

		std::vector<FMenuSpritePart> Moved(std::span<const FMenuSpritePart> Parts, int32 X, int32 Y)
		{
			std::vector<FMenuSpritePart> Result(Parts.begin(), Parts.end());
			for (FMenuSpritePart& Part : Result)
			{
				Part.X = static_cast<int16>(Part.X + X);
				Part.Y = static_cast<int16>(Part.Y + Y);
			}
			return Result;
		}
	}

	void AddMenuPortFrames(FMenuSpriteSheet& Sheet, const FMenuMainTables& Tables)
	{
		std::vector<FMenuSpritePart> Title = OptionsWord(Sheet, TitleOptionsX, TitleOptionsY);
		std::vector<FMenuSpritePart> Normal = OptionsWord(Sheet, OptionsX, OptionsY);
		std::vector<FMenuSpritePart> Orb = Moved(Sheet.Frame(StatusOrb), OrbStepX, OrbStepY);
		std::vector<FMenuSpritePart> Selected = Moved(Sheet.Frame(StatusOrb + SelectedOrbStep), OrbStepX, OrbStepY);
		Sheet.SetFrame(MenuPortFrame::TitleOptions, std::move(Title));
		Sheet.SetFrame(MenuPortFrame::OptionsOrb, std::move(Orb));
		Sheet.SetFrame(MenuPortFrame::Options, std::move(Normal));
		Sheet.SetFrame(MenuPortFrame::SelectedOptionsOrb, std::move(Selected));
		for (std::size_t Command = 0; Command < Tables.TitleCommands.size(); ++Command)
		{
			const FMenuArcCommand& Shown = Tables.TitleCommands[Command];
			const int32 StepX = Tables.ArcX[Command + 1] - Tables.ArcX[Command];
			const int32 StepY = Tables.ArcY[Command + 1] - Tables.ArcY[Command];
			const int32 Raised = static_cast<int32>(Command);
			Sheet.SetFrame(MenuPortFrame::RaisedTitleOrbs + Raised, Moved(Sheet.Frame(Shown.Orb), StepX, StepY));
			Sheet.SetFrame(MenuPortFrame::RaisedTitleOrbs + Raised + SelectedOrbStep, Moved(Sheet.Frame(Shown.Orb + SelectedOrbStep), StepX, StepY));
			Sheet.SetFrame(MenuPortFrame::RaisedTitleLabels + Raised, Moved(Sheet.Frame(Shown.Label), StepX, StepY));
		}
		Sheet.SetFrame(MenuPortFrame::RaisedTitleOptions,
			OptionsWord(Sheet, TitleOptionsX + Tables.ArcX[1] - Tables.ArcX[0], TitleOptionsY + Tables.ArcY[1] - Tables.ArcY[0]));
		const FMenuArcCommand& Load = Tables.SubCommands[TitleFileRow][TitleLoad];
		const std::size_t LoadPlace = InnerArcPlace + TitleLoad;
		const int32 StepX = Tables.ArcX[LoadPlace + 1] - Tables.ArcX[LoadPlace];
		const int32 StepY = Tables.ArcY[LoadPlace + 1] - Tables.ArcY[LoadPlace];
		Sheet.SetFrame(MenuPortFrame::TopLoadOrb, Moved(Sheet.Frame(Load.Orb), StepX, StepY));
		Sheet.SetFrame(MenuPortFrame::TopLoadOrb + SelectedOrbStep, Moved(Sheet.Frame(Load.Orb + SelectedOrbStep), StepX, StepY));
		Sheet.SetFrame(MenuPortFrame::TopLoad, Moved(Sheet.Frame(Load.Label), StepX, StepY));
	}
}
