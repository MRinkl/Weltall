#include "EventOps.h"

#include "Field/FieldFrameVariables.h"
#include "Game/GameState.h"

#include <array>

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint32 EntranceVariable = 0x002;
		constexpr uint16 PlayerLocked = 0xFFFF;
		/** FE 84's map operand that names no map: the field comes back to this one. */
		constexpr int32 NoNextMap = 0x7FFF;
		/** 56's camera yaw that means the field camera's own. */
		constexpr uint16 FieldCameraYaw = 0xFFFF;
		/** The world map keeps its camera half a turn from the field's. */
		constexpr int32 HalfTurn = 0x800;
		constexpr int32 AngleMask = 0xFFF;

		/** Whether the per-tick update finds the actors it reads: the leader, and each party slot's riding its gear; a fault names one without a script state. */
		bool HasFrameVariableActors(FEventContext& Context)
		{
			if (StateOf(Context, Context.LeaderActor) == nullptr)
			{
				return false;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const uint8 Actor = Context.PartyActors[static_cast<std::size_t>(Slot)];
				if (Context.GameState->PartySlotInGear(Slot) && Actor != FEventContext::NoActor && StateOf(Context, Actor) == nullptr)
				{
					return false;
				}
			}
			return true;
		}

		/**
		 * 71 and FE 84's start: the battle with the formation at operand 1, started as the scripted
		 * battles start. It stops the scripts, and FE 7F waits until the field is left.
		 */
		void RequestBattle(FEventContext& Context)
		{
			Context.Requests.BattleKind = Context.ScriptedBattleKind;
			Context.Encounters.Formation = static_cast<uint8>(ReadValue(Context, 1));
			Context.bBattleRequested = true;
			Context.Requests.bScriptedBattle = true;
			Context.Requests.bLeavingField = true;
		}

		/** 71: a battle, once the field is free for it. */
		void StartBattle(FEventContext& Context)
		{
			Context.Run.bYield = true;
			if (IsFieldBusy(Context) || Context.Requests.bMapChange)
			{
				return;
			}
			RequestBattle(Context);
			Advance(Context, 3);
		}

		/**
		 * FE 84: a battle as 71 asks for it, and unless its map operand is 7FFFh, the map and entrance
		 * the field goes on with after it, recorded as a map change records them (the field then
		 * starts that map afresh).
		 */
		void StartBattleThenMap(FEventContext& Context)
		{
			Context.Run.bYield = true;
			if (IsFieldBusy(Context) || Context.Requests.bMapChange)
			{
				Advance(Context, -1);
				return;
			}
			RequestBattle(Context);
			const int32 Map = ReadValue(Context, 5);
			if (Map != NoNextMap)
			{
				const int32 Entrance = ReadValue(Context, 7);
				SaveMapExit(Context);
				SetVar(Context, EntranceVariable, Entrance);
				Context.Requests.bBattleNamesNextMap = true;
				Context.MapNumber = Map;
			}
			Advance(Context, 9);
		}

		/**
		 * 56: the world map, once the field is free for it. The field's per-tick update runs once more,
		 * then stops writing the game state, which takes where the world map starts (the flags at
		 * operand 9 pick constants or variables): the place, the entrance, the camera's yaw (FFFFh for
		 * the field camera's own), the scene. The scripts stop.
		 */
		void ExitToWorldMap(FEventContext& Context)
		{
			Context.Run.bYield = true;
			if (IsFieldBusy(Context))
			{
				return;
			}
			FGameState* Game = GameStateOf(Context);
			if (Game == nullptr || !HasFrameVariableActors(Context))
			{
				return;
			}
			if (UpdateFieldFrameVariables(Context, Context.PlayClock))
			{
				return;
			}
			Context.PlayerLock = PlayerLocked;
			Context.Requests.bWorldMap = true;
			const uint8 Flags = CodeByte(Context, 9);
			const uint16 Place = static_cast<uint16>(ReadOperand(Context, 1, Flags, 0x80));
			const uint16 Entrance = static_cast<uint16>(ReadOperand(Context, 3, Flags, 0x40));
			int32 Yaw = ReadOperand(Context, 5, Flags, 0x20);
			if (static_cast<uint16>(Yaw) == FieldCameraYaw)
			{
				Yaw = Context.Camera.Rotation[1];
			}
			const uint16 Scene = static_cast<uint16>(ReadOperand(Context, 7, Flags, 0x10));
			Game->SetWorldMapPlace(Place, Entrance, static_cast<uint16>((Yaw + HalfTurn) & AngleMask), Scene);
			Context.bFrameVariablesFrozen = true;
			Advance(Context, 10);
		}

		/**
		 * FE BF: the Battling arena's match with the six bytes of its operands, once no battle, world
		 * map or song is on its way. The scripts go on (FE 7F waits until the field is left).
		 */
		void StartBattling(FEventContext& Context)
		{
			if (Context.bBattleRequested || Context.Requests.bWorldMap || (Context.Songs != nullptr && Context.Songs->IsLoading()))
			{
				Context.Run.bYield = true;
				Advance(Context, -1);
				return;
			}
			for (std::size_t Byte = 0; Byte < Context.Requests.BattlingParameters.size(); ++Byte)
			{
				Context.Requests.BattlingParameters[Byte] = static_cast<uint8>(ReadValue(Context, 1 + 2 * static_cast<int32>(Byte)));
			}
			Context.Requests.bLeavingField = true;
			Context.Requests.bBattling = true;
			Advance(Context, 13);
		}

		/** FE 83: another program (bit 7: a new game's state first), unless one is asked for already; the scripts go on. */
		void RunProgram(FEventContext& Context)
		{
			if (!Context.Requests.bProgram)
			{
				Context.PlayerLock = PlayerLocked;
				Context.Requests.bProgram = true;
				Context.Requests.Program = ReadValue(Context, 1);
			}
			Advance(Context, 3);
		}

		/** FE 7F: waits for good once a battle or the arena is asked for, as the field is left. */
		void WaitToLeave(FEventContext& Context)
		{
			Advance(Context, Context.Requests.bLeavingField ? -1 : 1);
			Context.Run.bYield = true;
		}

		/** FE B9: where the world map keeps the party (x, y, z, direction) into four variables. */
		void GetWorldMapPosition(FEventContext& Context)
		{
			if (const FGameState* Game = GameStateOf(Context))
			{
				const std::array<uint16, 4> Position = Game->WorldMapPosition();
				for (std::size_t Word = 0; Word < Position.size(); ++Word)
				{
					SetVar(Context, ReadU16(Context, 1 + 2 * static_cast<int32>(Word)), Position[Word]);
				}
			}
			Advance(Context, 9);
		}

		/** FE D5: where the world map keeps its sea object (x, z) into two variables. */
		void GetSeaObjectPlace(FEventContext& Context)
		{
			if (const FGameState* Game = GameStateOf(Context))
			{
				const std::array<uint16, 2> Place = Game->SeaObjectPlace();
				SetVar(Context, ReadU16(Context, 1), Place[0]);
				SetVar(Context, ReadU16(Context, 3), Place[1]);
			}
			Advance(Context, 5);
		}

		/** FE D6: where the world map keeps its air object (x, z) into two variables. */
		void GetAirObjectPlace(FEventContext& Context)
		{
			if (const FGameState* Game = GameStateOf(Context))
			{
				const std::array<uint16, 2> Place = Game->AirObjectPlace();
				SetVar(Context, ReadU16(Context, 1), Place[0]);
				SetVar(Context, ReadU16(Context, 3), Place[1]);
			}
			Advance(Context, 5);
		}

		/** FE C0: the arena's result of its last match into a variable. */
		void GetBattlingResult(FEventContext& Context)
		{
			SetVar(Context, ReadU16(Context, 1), Context.BattlingResult);
			Advance(Context, 3);
		}

		/** FE BA: sets where the world map keeps the party (the flags byte at operand 9 picks constants or variables). */
		void SetWorldMapPosition(FEventContext& Context)
		{
			constexpr std::array<uint8, 4> ConstantBits = {0x80, 0x40, 0x20, 0x10};
			const uint8 Flags = CodeByte(Context, 9);
			std::array<uint16, 4> Position{};
			for (std::size_t Word = 0; Word < Position.size(); ++Word)
			{
				Position[Word] = static_cast<uint16>(ReadOperand(Context, 1 + 2 * static_cast<int32>(Word), Flags, ConstantBits[Word]));
			}
			if (FGameState* Game = GameStateOf(Context))
			{
				Game->SetWorldMapPosition(Position);
			}
			Advance(Context, 10);
		}

		/** FE BB: the world map's flags into a variable. */
		void GetWorldMapFlags(FEventContext& Context)
		{
			const uint32 Address = ReadU16(Context, 1);
			if (const FGameState* Game = GameStateOf(Context))
			{
				SetVar(Context, Address, Game->WorldMapFlags());
			}
			Advance(Context, 3);
		}

		/** FE BC: sets the world map's flags (the flags byte at operand 3 picks a constant or a variable). */
		void SetWorldMapFlags(FEventContext& Context)
		{
			const uint16 Flags = static_cast<uint16>(ReadOperand(Context, 1, CodeByte(Context, 3), 0x80));
			if (FGameState* Game = GameStateOf(Context))
			{
				Game->SetWorldMapFlags(Flags);
			}
			Advance(Context, 4);
		}

		/** FE CD: the number of the disc the game plays from (1 or 2) into a variable. */
		void GetDiscNumber(FEventContext& Context)
		{
			SetVar(Context, ReadU16(Context, 1), Context.Disc);
			Advance(Context, 3);
		}

		/** FE E0: whether Start pauses the field. */
		void SetPauseDisabled(FEventContext& Context)
		{
			Context.PauseDisabled = CodeByte(Context, 1);
			Advance(Context, 2);
		}
	}

	void RegisterPrograms(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0x56] = &ExitToWorldMap;
		Base[0x71] = &StartBattle;
		Extended[0x7F] = &WaitToLeave;
		Extended[0x83] = &RunProgram;
		Extended[0x84] = &StartBattleThenMap;
		Extended[0xB9] = &GetWorldMapPosition;
		Extended[0xBA] = &SetWorldMapPosition;
		Extended[0xBB] = &GetWorldMapFlags;
		Extended[0xBC] = &SetWorldMapFlags;
		Extended[0xBF] = &StartBattling;
		Extended[0xCD] = &GetDiscNumber;
		Extended[0xC0] = &GetBattlingResult;
		Extended[0xD5] = &GetSeaObjectPlace;
		Extended[0xD6] = &GetAirObjectPlace;
		Extended[0xE0] = &SetPauseDisabled;
	}
}
