#include "Menu/MenuModule.h"
#include "Menu/MenuResources.h"

namespace Weltall
{
	namespace
	{
		/** Every orb and label of an arc is drawn from the same origin: its frame carries its place. */
		constexpr int32 ArcOriginX = 0xA0;
		constexpr int32 ArcOriginY = 0x96;
		constexpr int32 FullScale = 0x1000;
		/** The red orb of the command selected is the grey one this many frames on. */
		constexpr int32 SelectedOrb = 0xD;
		/** The red pointer over the selected orb. */
		constexpr int32 PointerFrame = 0x108;
		/** An arc is revealed a step every two frames. */
		constexpr int32 RevealFrames = 2;
		constexpr int32 PiecesEntry = 4;
		/** A row of the inner arc ends before its fourth command at this orb. */
		constexpr int32 NoCommand = 0xFFFF;
		/** A label beside an arc's orb: right of the orb's place and above it. */
		constexpr int32 ArcLabelX = 0x16;
		constexpr int32 ArcLabelY = -0x22;
		constexpr int32 LabelRows = 13;
	}

	std::span<const FMenuArcCommand> FMenuModule::SubCommandRow(int32 Row) const
	{
		return SubArcRows[static_cast<std::size_t>(Row + Command)];
	}

	TMenuTask<> FMenuModule::RevealCommands(std::span<const FMenuArcCommand> Commands)
	{
		const FMenuSpriteSheet& Sheet = Resources.Sheet();
		// One step more than there are orbs: the last one adds the last label.
		const int32 Steps = static_cast<int32>(Commands.size()) + 1;
		Arc.bDimmed = false;
		Arc.bDimmedApplied = false;
		Flags[MenuFlag::Commands] = 1;
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			if (Step != Steps)
			{
				Arc.Orbs.clear();
				for (int32 Orb = 0; Orb < Step; ++Orb)
				{
					Sheet.Draw(Commands[static_cast<std::size_t>(Orb)].Orb, ArcOriginX, ArcOriginY, FullScale, Arc.Orbs);
				}
			}
			Arc.Labels.clear();
			for (int32 Label = 0; Label < Step - 1; ++Label)
			{
				Sheet.Draw(Commands[static_cast<std::size_t>(Label)].Label, ArcOriginX, ArcOriginY, FullScale, Arc.Labels);
			}
			for (int32 Frame = 0; Frame < RevealFrames; ++Frame)
			{
				co_await this->Frame();
			}
		}
	}

	void FMenuModule::DrawCommands(std::span<const FMenuArcCommand> Commands, int32 Selected, int32 Place)
	{
		const FMenuSpriteSheet& Sheet = Resources.Sheet();
		Arc.Orbs.clear();
		Arc.Labels.clear();
		for (int32 Index = 0; Index < static_cast<int32>(Commands.size()); ++Index)
		{
			const FMenuArcCommand& Shown = Commands[static_cast<std::size_t>(Index)];
			Sheet.Draw(Shown.Orb + (Index == Selected ? SelectedOrb : 0), ArcOriginX, ArcOriginY, FullScale, Arc.Orbs);
			Sheet.Draw(Shown.Label, ArcOriginX, ArcOriginY, FullScale, Arc.Labels);
		}
		DrawSelection(Place < 0 ? Selected : Place);
		Flags[MenuFlag::Pointer] = 1;
	}

	TMenuTask<> FMenuModule::RevealSubCommands(int32 Row)
	{
		const FMenuSpriteSheet& Sheet = Resources.Sheet();
		bool bWaits = true;
		SubArc.Orbs.clear();
		SubArc.Labels.clear();
		Flags[MenuFlag::SubCommands] = 1;
		for (int32 Step = 1; Step < static_cast<int32>(SubArcRows.empty() ? 0 : SubArcRows.front().size()) + 1; ++Step)
		{
			const std::span<const FMenuArcCommand> Commands = SubCommandRow(Row);
			SubArc.Orbs.clear();
			SubCommandCount = 0;
			for (int32 Index = 0; Index < Step; ++Index)
			{
				const FMenuArcCommand& Revealed = Commands[static_cast<std::size_t>(Index)];
				if (Revealed.Orb == NoCommand)
				{
					bWaits = false;
				}
				else
				{
					Sheet.Draw(Revealed.Orb, ArcOriginX, ArcOriginY, FullScale, SubArc.Orbs);
					++SubCommandCount;
				}
			}
			if (bWaits)
			{
				for (int32 Frame = 0; Frame < RevealFrames; ++Frame)
				{
					co_await this->Frame();
				}
			}
			SubArc.Labels.clear();
			for (int32 Index = 0; Index < Step; ++Index)
			{
				const FMenuArcCommand& Revealed = SubCommandRow(Row)[static_cast<std::size_t>(Index)];
				if (Revealed.Orb != NoCommand)
				{
					Sheet.Draw(Revealed.Label, ArcOriginX, ArcOriginY, FullScale, SubArc.Labels);
				}
			}
			if (bWaits)
			{
				for (int32 Frame = 0; Frame < RevealFrames; ++Frame)
				{
					co_await this->Frame();
				}
			}
		}
	}

	void FMenuModule::DrawSubCommands(int32 Row)
	{
		const FMenuSpriteSheet& Sheet = Resources.Sheet();
		SubArc.Orbs.clear();
		SubArc.Labels.clear();
		const std::span<const FMenuArcCommand> Commands = SubCommandRow(Row);
		for (int32 Index = 0; Index < SubCommandCount; ++Index)
		{
			const FMenuArcCommand& Shown = Commands[static_cast<std::size_t>(Index)];
			Sheet.Draw(Shown.Orb + (Index == SubCommand ? SelectedOrb : 0), ArcOriginX, ArcOriginY, FullScale, SubArc.Orbs);
			Sheet.Draw(Shown.Label, ArcOriginX, ArcOriginY, FullScale, SubArc.Labels);
		}
		DrawSelection(SubCommand + SubArcFirstPlace);
		Flags[MenuFlag::Pointer] = 1;
	}

	void FMenuModule::DrawSelection(int32 Place)
	{
		Pointer.clear();
		Resources.Sheet().Draw(PointerFrame, ArcX[static_cast<std::size_t>(Place)], ArcY[static_cast<std::size_t>(Place)], FullScale, Pointer);
		Flags[MenuFlag::Selection] = 1;
	}

	void FMenuModule::HideSelection()
	{
		Flags[MenuFlag::Pointer] = 0;
		Flags[MenuFlag::Selection] = 0;
	}

	void FMenuModule::PlaceArcLabel(std::span<FMenuLabel> Labels, std::span<const int32> Offsets, int32 FirstFlag, int32 Cleared, int32 Index, int32 ArcOffset)
	{
		ClearFlags(FirstFlag, Cleared);
		FMenuLabel& Label = Labels[static_cast<std::size_t>(Index)];
		RecordLabel(Label);
		const int16 Left = static_cast<int16>(ArcX[static_cast<std::size_t>(Index + ArcOffset)] + Offsets[static_cast<std::size_t>(Index)] + ArcLabelX);
		const int16 Top = static_cast<int16>(ArcY[static_cast<std::size_t>(Index + ArcOffset)] + ArcLabelY);
		const int16 Right = static_cast<int16>(Left + Label.Width);
		const int16 Bottom = static_cast<int16>(Top + LabelRows);
		Label.Quad.Corners[0].X = Left;
		Label.Quad.Corners[0].Y = Top;
		Label.Quad.Corners[1].X = Right;
		Label.Quad.Corners[1].Y = Top;
		Label.Quad.Corners[2].X = Left;
		Label.Quad.Corners[2].Y = Bottom;
		Label.Quad.Corners[3].X = Right;
		Label.Quad.Corners[3].Y = Bottom;
		Flags[static_cast<std::size_t>(FirstFlag + Index)] = 1;
	}

	void FMenuModule::AddSelection()
	{
		Table.Add(PiecesEntry, SelectionMode);
		if (Flags[MenuFlag::Pointer] != 0 && !Pointer.empty())
		{
			Table.Add(PiecesEntry, Pointer.front());
		}
	}

	void FMenuModule::AddArc(FMenuArc& Drawing, bool bApplyDim)
	{
		// The arc behind a screen is dimmed: its quads become semi-transparent, their colour a quarter.
		if (bApplyDim && Drawing.bDimmed != Drawing.bDimmedApplied)
		{
			const uint8 Grey = Drawing.bDimmed ? 0x20 : 0x80;
			for (std::vector<FMenuPrimitive>* Quads : {&Drawing.Labels, &Drawing.Orbs})
			{
				for (FMenuPrimitive& Quad : *Quads)
				{
					Quad.Code = static_cast<uint8>(MenuGpu::TexturedQuad | (Drawing.bDimmed ? MenuGpu::SemiTransparent : 0));
					Quad.Page |= MenuGpu::PageBlendAdd;
					Quad.Corners[0].Color = {Grey, Grey, Grey};
				}
			}
			Drawing.bDimmedApplied = Drawing.bDimmed;
		}
		AddScreen(Drawing.Labels);
		AddScreen(Drawing.Orbs);
	}
}
