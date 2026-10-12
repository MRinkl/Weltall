#include "EventOps.h"

#include "Game/GameState.h"

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint32 EntranceVariable = 0x002;
		/** Variable 0x002's place among the saved variables. */
		constexpr int32 SavedEntrance = 1;
		constexpr uint16 PlayerLocked = 0xFFFF;

		/** The menu's modes the scripts open. */
		namespace MenuMode
		{
			constexpr uint32 Normal = 0;
			constexpr uint32 MemberChange = 1;
			constexpr uint32 LoadGame = 2;
			constexpr uint32 EnterName = 3;
			constexpr uint32 Shop = 4;
			constexpr uint32 GearShop = 5;
			constexpr uint32 DiscChange = 6;
		}

		/** Asks for the menu in Mode, which the field opens at its next quiet tick; FE 87 waits until it has closed. */
		void RequestMenu(FEventContext& Context, uint32 Mode, int32 Length)
		{
			Context.MenuRequest = Mode;
			Context.Run.bYield = true;
			++Context.MenusPending;
			Advance(Context, Length);
		}

		/** FE 55: the Normal Menu, with the parameter the player's own would have (FE 99's). */
		void OpenMainMenu(FEventContext& Context)
		{
			Context.MenuProgramParam = Context.MenuParam;
			RequestMenu(Context, MenuMode::Normal, 1);
		}

		/**
		 * FE 56: Member Change. Its operand goes into variable 0x002 (whole, whatever the map's
		 * signedness), and the game state's copy of it and its entrance word.
		 */
		void OpenMemberChange(FEventContext& Context)
		{
			const uint16 Value = static_cast<uint16>(ReadValue(Context, 1));
			FGameState* Game = GameStateOf(Context);
			if (Game == nullptr)
			{
				return;
			}
			Context.Variables.Set(EntranceVariable, Value);
			Game->SetSavedEventVariable(SavedEntrance, Value);
			Game->SetFieldEntrance(Value);
			RequestMenu(Context, MenuMode::MemberChange, 3);
		}

		/** FE 57: Load Game, the title's menu. */
		void OpenLoadGame(FEventContext& Context)
		{
			RequestMenu(Context, MenuMode::LoadGame, 1);
		}

		/** FE 58, FE 59 and FE 5A: Enter Name, a shop and the gear shop, each with the parameter in its operand. */
		template <uint32 Mode>
		void OpenMenuWithParameter(FEventContext& Context)
		{
			Context.MenuProgramParam = static_cast<uint8>(ReadValue(Context, 1));
			RequestMenu(Context, Mode, 3);
		}

		/** FE CF: Member Change, then the map and entrance of its operands, recorded as a map change records them. */
		void MemberChangeThenMap(FEventContext& Context)
		{
			Context.PlayerLock = PlayerLocked;
			const int32 Entrance = ReadValue(Context, 3);
			const int32 Map = ReadValue(Context, 1);
			SaveMapExit(Context);
			SetVar(Context, EntranceVariable, Entrance);
			Context.MapNumber = Map;
			RequestMenu(Context, MenuMode::MemberChange, 5);
		}

		/** FE DA: the menu that asks for the other disc. */
		void OpenDiscChange(FEventContext& Context)
		{
			Context.MenuProgramParam = 1;
			RequestMenu(Context, MenuMode::DiscChange, 1);
		}

		/** FE 87: waits until every menu the scripts asked for has closed. */
		void WaitForMenus(FEventContext& Context)
		{
			Advance(Context, Context.MenusPending == 0 ? 1 : -1);
			Context.Run.bYield = true;
		}
	}

	void RegisterMenus(FBaseTable& Base, FExtendedTable& Extended)
	{
		(void)Base;
		Extended[0x55] = &OpenMainMenu;
		Extended[0x56] = &OpenMemberChange;
		Extended[0x57] = &OpenLoadGame;
		Extended[0x58] = &OpenMenuWithParameter<MenuMode::EnterName>;
		Extended[0x59] = &OpenMenuWithParameter<MenuMode::Shop>;
		Extended[0x5A] = &OpenMenuWithParameter<MenuMode::GearShop>;
		Extended[0x87] = &WaitForMenus;
		Extended[0xCF] = &MemberChangeThenMap;
		Extended[0xDA] = &OpenDiscChange;
	}
}
