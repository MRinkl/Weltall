#include "Menu/MenuModuleTables.h"

#include "Foundation/LittleEndian.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** Where Member Change's tables lie in its file (the module loads at 801C5000). */
		namespace MemberOffsets
		{
			constexpr std::size_t PointerX = 0x61A0;
			constexpr std::size_t PointerY = 0x61C4;
			constexpr std::size_t ReserveX = 0x61E8;
			constexpr std::size_t PartyX = 0x622C;
			constexpr std::size_t ReserveY = 0x6274;
			constexpr std::size_t PartyY = 0x62B8;
			constexpr std::size_t NameStripX = 0x6344;
			constexpr std::size_t NameStripY = 0x6390;
			constexpr std::size_t PanelLabels = 0x63DC;
			constexpr std::size_t Labels = 0x6400;
			constexpr std::size_t End = 0x6404;
		}

		/** Where Enter Name's tables lie in its file. */
		namespace NameOffsets
		{
			constexpr std::size_t Grid = 0x6EC0;
			constexpr std::size_t Labels = 0x6F98;
			constexpr std::size_t End = 0x6F9C;
		}

		/** Where the Shop's tables lie in its file. */
		namespace ShopOffsets
		{
			constexpr std::size_t MainCommands = 0xCF54;
			constexpr std::size_t SubCommands = 0xCF6C;
			constexpr std::size_t MainLabels = 0xCFCC;
			constexpr std::size_t SubLabels = 0xCFD0;
			constexpr std::size_t ListLabels = 0xCFD4;
			constexpr std::size_t MainLabelOffsets = 0xCFD8;
			constexpr std::size_t SubLabelOffsets = 0xCFE8;
			constexpr std::size_t ChoiceX = 0xCFF8;
			constexpr std::size_t ChoiceY = 0xD008;
			constexpr std::size_t HelpLabels = 0xD018;
			constexpr std::size_t ArcX = 0xD194;
			constexpr std::size_t ArcY = 0xD1B0;
			constexpr std::size_t FaceX = 0xD1CC;
			constexpr std::size_t BuyFrames = 0xD210;
			constexpr std::size_t SellFrames = 0xD214;
			constexpr std::size_t BuyX = 0xD218;
			constexpr std::size_t SellX = 0xD228;
			constexpr std::size_t BuyY = 0xD230;
			constexpr std::size_t SellY = 0xD240;
			constexpr std::size_t Gold = 0xD248;
			constexpr std::size_t End = 0xD260;
		}

		/** Where the gear shop's tables lie in its file. */
		namespace GearShopOffsets
		{
			constexpr std::size_t MainCommands = 0x11980;
			constexpr std::size_t SubCommands = 0x119A0;
			constexpr std::size_t MainLabels = 0x11A20;
			constexpr std::size_t SubLabels = 0x11A24;
			constexpr std::size_t TuneLabels = 0x11A28;
			constexpr std::size_t ListLabels = 0x11A2C;
			constexpr std::size_t MainLabelOffsets = 0x11A30;
			constexpr std::size_t SubLabelOffsets = 0x11A40;
			constexpr std::size_t TuneLabelOffsets = 0x11A50;
			constexpr std::size_t ChoiceX = 0x11A60;
			constexpr std::size_t ChoiceY = 0x11A70;
			constexpr std::size_t HelpLabels = 0x11A80;
			constexpr std::size_t ArcX = 0x11BFC;
			constexpr std::size_t ArcY = 0x11C20;
			constexpr std::size_t FaceX = 0x11C44;
			/** Buy's four, then Tune up's four; Sell's two. */
			constexpr std::size_t StaticFrames = 0x11D08;
			constexpr std::size_t SellFrames = 0x11D10;
			constexpr std::size_t StaticX = 0x11D14;
			constexpr std::size_t SellX = 0x11D34;
			constexpr std::size_t StaticY = 0x11D3C;
			constexpr std::size_t SellY = 0x11D5C;
			constexpr std::size_t Gold = 0x11D64;
			constexpr std::size_t ZoomDistance = 0x11DFC;
			constexpr std::size_t ZoomHeight = 0x11E18;
			constexpr std::size_t ZoomTurn = 0x11FB0;
			constexpr std::size_t HintX = 0x11FD0;
			constexpr std::size_t BracketFrames = 0x11FE0;
			constexpr std::size_t ScanPlaces = 0x12030;
			constexpr std::size_t BracketX = 0x12040;
			constexpr std::size_t BracketY = 0x12044;
			constexpr std::size_t PanelX = 0x1205C;
			constexpr std::size_t PanelY = 0x12068;
			constexpr std::size_t FlickerX = 0x12074;
			constexpr std::size_t FlickerY = 0x12080;
			constexpr std::size_t StatusFrames = 0x1208C;
			constexpr std::size_t StatusX = 0x120A8;
			constexpr std::size_t StatusY = 0x120C4;
			constexpr std::size_t StatusNumbers = 0x120E0;
			constexpr std::size_t Pilots = 0x120F4;
			constexpr std::size_t End = 0x12108;
		}

		template <std::size_t Count>
		void ReadBytes(std::span<const uint8> Module, std::size_t At, std::array<uint8, Count>& Values)
		{
			std::copy_n(Module.begin() + static_cast<std::ptrdiff_t>(At), Count, Values.begin());
		}

		template <std::size_t Count>
		void ReadCommands(std::span<const uint8> Module, std::size_t At, std::array<FMenuArcCommand, Count>& Commands)
		{
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				Commands[Index] = {static_cast<int32>(LittleEndian::ReadU32(Module, At + 8 * Index)), static_cast<int32>(LittleEndian::ReadU32(Module, At + 8 * Index + 4))};
			}
		}

		template <std::size_t Count>
		void ReadWords(std::span<const uint8> Module, std::size_t At, std::array<int32, Count>& Values)
		{
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				Values[Index] = static_cast<int32>(LittleEndian::ReadU32(Module, At + 4 * Index));
			}
		}

		template <typename THalf, std::size_t Count>
		void ReadHalves(std::span<const uint8> Module, std::size_t At, std::array<THalf, Count>& Values)
		{
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				Values[Index] = static_cast<THalf>(LittleEndian::ReadU16(Module, At + 2 * Index));
			}
		}

		template <std::size_t Count>
		void ReadPlacedFrames(std::span<const uint8> Module, std::size_t Frames, std::size_t X, std::size_t Y, FMenuPlacedFrames<Count>& Placed)
		{
			ReadBytes(Module, Frames, Placed.Frames);
			ReadWords(Module, X, Placed.X);
			ReadWords(Module, Y, Placed.Y);
		}
	}

	TResult<FMenuMemberTables> FMenuMemberTables::Parse(std::span<const uint8> Module)
	{
		if (Module.size() < MemberOffsets::End)
		{
			return MakeError("Member Change's module is too short for its tables");
		}
		FMenuMemberTables Tables;
		ReadWords(Module, MemberOffsets::PointerX, Tables.PointerX);
		ReadWords(Module, MemberOffsets::PointerY, Tables.PointerY);
		ReadWords(Module, MemberOffsets::PartyX, Tables.Party.X);
		ReadWords(Module, MemberOffsets::PartyY, Tables.Party.Y);
		ReadWords(Module, MemberOffsets::ReserveX, Tables.Reserve.X);
		ReadWords(Module, MemberOffsets::ReserveY, Tables.Reserve.Y);
		ReadWords(Module, MemberOffsets::NameStripX, Tables.NameStripX);
		ReadWords(Module, MemberOffsets::NameStripY, Tables.NameStripY);
		ReadWords(Module, MemberOffsets::PanelLabels, Tables.PanelLabels);
		ReadBytes(Module, MemberOffsets::Labels, Tables.Labels);
		return Tables;
	}

	TResult<FMenuNameTables> FMenuNameTables::Parse(std::span<const uint8> Module)
	{
		if (Module.size() < NameOffsets::End)
		{
			return MakeError("Enter Name's module is too short for its tables");
		}
		FMenuNameTables Tables;
		Tables.Grid.assign(Module.begin() + static_cast<std::ptrdiff_t>(NameOffsets::Grid), Module.end());
		ReadBytes(Module, NameOffsets::Labels, Tables.Labels);
		return Tables;
	}

	TResult<FMenuShopTables> FMenuShopTables::Parse(std::span<const uint8> Module)
	{
		if (Module.size() < ShopOffsets::End)
		{
			return MakeError("the Shop's module is too short for its tables");
		}
		FMenuShopTables Tables;
		ReadWords(Module, ShopOffsets::ArcX, Tables.ArcX);
		ReadWords(Module, ShopOffsets::ArcY, Tables.ArcY);
		ReadCommands(Module, ShopOffsets::MainCommands, Tables.MainCommands);
		for (std::size_t Row = 0; Row < Tables.SubCommands.size(); ++Row)
		{
			ReadCommands(Module, ShopOffsets::SubCommands + Row * 0x20, Tables.SubCommands[Row]);
		}
		ReadBytes(Module, ShopOffsets::MainLabels, Tables.MainLabels);
		ReadBytes(Module, ShopOffsets::SubLabels, Tables.SubLabels);
		ReadBytes(Module, ShopOffsets::ListLabels, Tables.ListLabels);
		ReadWords(Module, ShopOffsets::MainLabelOffsets, Tables.MainLabelOffsets);
		ReadWords(Module, ShopOffsets::SubLabelOffsets, Tables.SubLabelOffsets);
		ReadWords(Module, ShopOffsets::ChoiceX, Tables.ChoiceX);
		ReadWords(Module, ShopOffsets::ChoiceY, Tables.ChoiceY);
		ReadBytes(Module, ShopOffsets::HelpLabels, Tables.HelpLabels);
		ReadWords(Module, ShopOffsets::FaceX, Tables.FaceX);
		ReadBytes(Module, ShopOffsets::BuyFrames, Tables.BuyStatics.Frames);
		ReadBytes(Module, ShopOffsets::SellFrames, Tables.SellStatics.Frames);
		ReadWords(Module, ShopOffsets::BuyX, Tables.BuyStatics.X);
		ReadWords(Module, ShopOffsets::SellX, Tables.SellStatics.X);
		ReadWords(Module, ShopOffsets::BuyY, Tables.BuyStatics.Y);
		ReadWords(Module, ShopOffsets::SellY, Tables.SellStatics.Y);
		for (std::size_t Line = 0; Line < Tables.GoldX.size(); ++Line)
		{
			Tables.GoldX[Line] = static_cast<int32>(LittleEndian::ReadU32(Module, ShopOffsets::Gold + Line * 8));
			Tables.GoldY[Line] = static_cast<int32>(LittleEndian::ReadU32(Module, ShopOffsets::Gold + Line * 8 + 4));
		}
		return Tables;
	}

	TResult<FMenuGearShopTables> FMenuGearShopTables::Parse(std::span<const uint8> Module)
	{
		if (Module.size() < GearShopOffsets::End)
		{
			return MakeError("the gear shop's module is too short for its tables");
		}
		FMenuGearShopTables Tables;
		ReadWords(Module, GearShopOffsets::ArcX, Tables.ArcX);
		ReadWords(Module, GearShopOffsets::ArcY, Tables.ArcY);
		ReadCommands(Module, GearShopOffsets::MainCommands, Tables.MainCommands);
		for (std::size_t Row = 0; Row < Tables.SubCommands.size(); ++Row)
		{
			ReadCommands(Module, GearShopOffsets::SubCommands + Row * 0x20, Tables.SubCommands[Row]);
		}
		ReadBytes(Module, GearShopOffsets::MainLabels, Tables.MainLabels);
		ReadBytes(Module, GearShopOffsets::SubLabels, Tables.SubLabels);
		ReadBytes(Module, GearShopOffsets::TuneLabels, Tables.TuneLabels);
		ReadBytes(Module, GearShopOffsets::ListLabels, Tables.ListLabels);
		ReadWords(Module, GearShopOffsets::MainLabelOffsets, Tables.MainLabelOffsets);
		ReadWords(Module, GearShopOffsets::SubLabelOffsets, Tables.SubLabelOffsets);
		ReadWords(Module, GearShopOffsets::TuneLabelOffsets, Tables.TuneLabelOffsets);
		ReadWords(Module, GearShopOffsets::ChoiceX, Tables.ChoiceX);
		ReadWords(Module, GearShopOffsets::ChoiceY, Tables.ChoiceY);
		ReadBytes(Module, GearShopOffsets::HelpLabels, Tables.HelpLabels);
		ReadWords(Module, GearShopOffsets::FaceX, Tables.FaceX);
		ReadPlacedFrames(Module, GearShopOffsets::StaticFrames, GearShopOffsets::StaticX, GearShopOffsets::StaticY, Tables.BuyStatics);
		ReadPlacedFrames(Module, GearShopOffsets::StaticFrames + 4, GearShopOffsets::StaticX + 0x10, GearShopOffsets::StaticY + 0x10, Tables.TuneStatics);
		ReadPlacedFrames(Module, GearShopOffsets::SellFrames, GearShopOffsets::SellX, GearShopOffsets::SellY, Tables.SellStatics);
		for (std::size_t Line = 0; Line < Tables.GoldX.size(); ++Line)
		{
			Tables.GoldX[Line] = static_cast<int32>(LittleEndian::ReadU32(Module, GearShopOffsets::Gold + Line * 8));
			Tables.GoldY[Line] = static_cast<int32>(LittleEndian::ReadU32(Module, GearShopOffsets::Gold + Line * 8 + 4));
		}
		ReadHalves(Module, GearShopOffsets::ZoomDistance, Tables.ZoomDistance);
		ReadHalves(Module, GearShopOffsets::ZoomHeight, Tables.ZoomHeight);
		ReadHalves(Module, GearShopOffsets::ZoomTurn, Tables.ZoomTurn);
		ReadWords(Module, GearShopOffsets::HintX, Tables.HintX);
		for (std::size_t Bracket = 0; Bracket < Tables.BracketFrames.size(); ++Bracket)
		{
			ReadHalves(Module, GearShopOffsets::BracketFrames + Bracket * 0x28, Tables.BracketFrames[Bracket]);
			ReadHalves(Module, GearShopOffsets::BracketY + Bracket * 0xC, Tables.BracketY[Bracket]);
		}
		ReadBytes(Module, GearShopOffsets::ScanPlaces, Tables.ScanPlaces);
		ReadHalves(Module, GearShopOffsets::BracketX, Tables.BracketX);
		ReadHalves(Module, GearShopOffsets::PanelX, Tables.PanelX);
		ReadHalves(Module, GearShopOffsets::PanelY, Tables.PanelY);
		ReadHalves(Module, GearShopOffsets::FlickerX, Tables.FlickerX);
		ReadHalves(Module, GearShopOffsets::FlickerY, Tables.FlickerY);
		ReadHalves(Module, GearShopOffsets::StatusFrames, Tables.StatusFrames);
		ReadHalves(Module, GearShopOffsets::StatusX, Tables.StatusX);
		ReadHalves(Module, GearShopOffsets::StatusY, Tables.StatusY);
		for (std::size_t Number = 0; Number < Tables.StatusNumbers.size(); ++Number)
		{
			ReadHalves(Module, GearShopOffsets::StatusNumbers + Number * 4, Tables.StatusNumbers[Number]);
		}
		ReadBytes(Module, GearShopOffsets::Pilots, Tables.Pilots);
		return Tables;
	}
}
