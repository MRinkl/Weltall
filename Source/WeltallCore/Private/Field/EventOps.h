#pragma once

#include "Field/EventContext.h"
#include "Foundation/ProcessorDivide.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

// What every instruction handler uses: the running actor, its operands and the variables. A handler
// runs one instruction of the running thread: it moves the pc (or leaves it, to run the instruction
// again), and may end the actor's turn, grant it more instructions or report a fault.
namespace Weltall::EventOps
{
	using FHandler = void (*)(FEventContext& Context);
	using FBaseTable = std::array<FHandler, 256>;
	/** The extended opcodes (FE xx) the game defines; a larger xx has no handler. */
	using FExtendedTable = std::array<FHandler, 0xE3>;

	inline constexpr uint8 PartySlot0 = 0xFF;
	inline constexpr uint8 PartySlot1 = 0xFE;
	inline constexpr uint8 PartySlot2 = 0xFD;
	inline constexpr uint8 RunningActor = 0xFB;

	inline FEventActorState& Self(FEventContext& Context)
	{
		return *Context.Run.State;
	}

	/** What a fault means, for the error that reports it. */
	std::string DescribeFault(const FEventContext& Context, EEventFault Fault, uint32 Value);

	inline void Fail(FEventContext& Context, EEventFault Fault, uint32 Value)
	{
		if (Context.Run.Fault == EEventFault::None)
		{
			Context.Run.Fault = Fault;
			Context.Run.FaultValue = Value;
		}
	}

	/** A case no script reaches: whether the scripts carry on past it (noted once), or else fail with the fault. */
	inline bool Recovers(FEventContext& Context, std::string_view Case, EEventFault Fault, uint32 Value)
	{
		if (RecoverUnreached(Context.Unreached, Case))
		{
			return true;
		}
		Fail(Context, Fault, Value);
		return false;
	}

	/** The byte at Offset from the running instruction (from its second byte for FE xx). */
	inline uint8 CodeByte(FEventContext& Context, int32 Offset)
	{
		return Context.Scripts->Code()[Self(Context).Pc + Offset];
	}

	inline uint16 ReadU16(FEventContext& Context, int32 Offset)
	{
		const uint8* At = Context.Scripts->Code() + Self(Context).Pc + Offset;
		return static_cast<uint16>(At[0] | At[1] << 8);
	}

	inline int32 ReadS16(FEventContext& Context, int32 Offset)
	{
		return static_cast<int16>(ReadU16(Context, Offset));
	}

	inline int32 GetVar(FEventContext& Context, uint32 Address)
	{
		if (Address >= FEventVariables::AddressEnd)
		{
			Fail(Context, EEventFault::VariableOutOfRange, Address);
			return 0;
		}
		return Context.Variables.Get(Address, *Context.Scripts);
	}

	inline void SetVar(FEventContext& Context, uint32 Address, int32 Value)
	{
		if (Address >= FEventVariables::AddressEnd)
		{
			Fail(Context, EEventFault::VariableOutOfRange, Address);
			return;
		}
		Context.Variables.Set(Address, Value);
	}

	inline bool IsVarUnsigned(FEventContext& Context, uint32 Address)
	{
		if (Address >= FEventVariables::AddressEnd)
		{
			Fail(Context, EEventFault::VariableOutOfRange, Address);
			return false;
		}
		return Context.Scripts->IsVariableUnsigned(Address);
	}

	/** A u16 operand: with bit 15 the constant in its other bits, else the variable it addresses. */
	inline int32 ReadValue(FEventContext& Context, int32 Offset)
	{
		const uint16 Operand = ReadU16(Context, Offset);
		return (Operand & 0x8000) != 0 ? Operand & 0x7FFF : GetVar(Context, Operand);
	}

	/** An operand whose bit in the instruction's flags byte makes it an s16 constant, else a variable's address. */
	inline int32 ReadOperand(FEventContext& Context, int32 Offset, uint8 Flags, uint8 Bit)
	{
		return (Flags & Bit) != 0 ? ReadS16(Context, Offset) : GetVar(Context, ReadU16(Context, Offset));
	}

	/** An actor operand: FF, FE and FD name the party's slots, FB the running actor; NoActor for an empty slot. */
	inline uint32 ReadActor(FEventContext& Context, int32 Offset)
	{
		const uint8 Operand = CodeByte(Context, Offset);
		switch (Operand)
		{
		case PartySlot0:
			return Context.PartyActors[0];
		case PartySlot1:
			return Context.PartyActors[1];
		case PartySlot2:
			return Context.PartyActors[2];
		case RunningActor:
			return static_cast<uint32>(Context.Run.Actor);
		default:
			return Operand;
		}
	}

	/** An actor's record, or a fault when the map has no such actor. */
	inline FFieldActor* RecordOf(FEventContext& Context, uint32 Actor)
	{
		if (Actor >= Context.Actors.size())
		{
			Fail(Context, EEventFault::ActorWithoutScripts, Actor);
			return nullptr;
		}
		return &Context.Actors[Actor];
	}

	/** An actor's script state, or a fault when the actor has none. */
	inline FEventActorState* StateOf(FEventContext& Context, uint32 Actor)
	{
		FEventActorState* State = Context.StateOf(static_cast<int32>(Actor));
		if (State == nullptr)
		{
			Fail(Context, EEventFault::ActorWithoutScripts, Actor);
		}
		return State;
	}

	inline void Advance(FEventContext& Context, int32 Length)
	{
		Self(Context).Pc = static_cast<uint16>(Self(Context).Pc + Length);
	}

	/** A conditional jump's two ways: past the instruction when the test holds, else to its target. */
	inline void Branch(FEventContext& Context, bool bTaken, int32 Length, int32 TargetOffset)
	{
		Self(Context).Pc = bTaken ? static_cast<uint16>(Self(Context).Pc + Length) : ReadU16(Context, TargetOffset);
	}

	/** Frees the running thread's slot, as a thread's end does. */
	inline void EndThread(FEventContext& Context)
	{
		FEventThread& Thread = Self(Context).Running();
		Thread.Word |= FEventThread::PriorityMask;
		Thread.Script = FEventThread::NoScript;
	}

	/** The game's state, or a fault when none is attached. */
	inline FGameState* GameStateOf(FEventContext& Context)
	{
		if (Context.GameState == nullptr)
		{
			Fail(Context, EEventFault::NoGameState, 0);
		}
		return Context.GameState;
	}

	/** A character operand: FF, FE and FD name the characters of party slots 2, 1 and 0, FC none (FF). */
	inline int32 ResolveCharacter(const FEventContext& Context, int32 Character)
	{
		switch (Character)
		{
		case 0xFF:
			return Context.PartyCharacters[2];
		case 0xFE:
			return Context.PartyCharacters[1];
		case 0xFD:
			return Context.PartyCharacters[0];
		case 0xFC:
			return 0xFF;
		default:
			return Character;
		}
	}

	/**
	 * Opens a window for the running actor's message instruction (its message number at operand 1,
	 * its style at 3), placed at Speaker by Mode, and moves the pc past it; false when it waits,
	 * the pc left where it was.
	 */
	bool OpenMessage(FEventContext& Context, uint32 Speaker, EFieldMessageMode Mode);

	/**
	 * The angle of direction Index as the field's table holds it, ((Index - 2) & 7) * 512, marked as
	 * set. The table has the eight twice in a row, so Index 8-15 give the same angles; past them it
	 * reads other tables, which no script does (a fault).
	 */
	std::optional<uint16> DirectionAngle(FEventContext& Context, int32 Index);

	/**
	 * Runs Actor's thread from its pc at once, as a map's init runs it: without a budget, until an
	 * instruction ends the unlimited run and the actor's turn. The running actor and its registers are
	 * the caller's to keep; a fault stops the run and stays in the run's registers.
	 */
	void RunScriptNow(FEventContext& Context, int32 Actor);

	/** Pushes the return pc and jumps, unless the call stack is full; false when it is. */
	bool Call(FEventContext& Context, int32 Length, int32 TargetOffset);

	/**
	 * The integer length of a vector as the field measures distances: the GTE squares each
	 * component's low 16 bits and libgte takes the root. False, with a fault, when the sum passes 31
	 * bits, which makes libgte read outside its table.
	 */
	bool Length(FEventContext& Context, int32 X, int32 Y, int32 Z, int32& Out);

	/** Builds the actor's model matrix again: the rotation matrix of its angles, its columns scaled by the actor's scale. */
	void BuildModelMatrix(FEventContext& Context, uint32 Actor);

	/** A sound effect at full volume, centred, on one of the scripts' eight channels; effect 0 stops the channel instead. */
	void PlayScriptSound(FEventContext& Context, int32 Effect, int32 Channel);

	/**
	 * The field is taking up a battle or the world map, or loading a song or an actor's extra
	 * animations: the instructions that ask it for another program or map wait.
	 */
	inline bool IsFieldBusy(const FEventContext& Context)
	{
		return Context.bBattleRequested || Context.Requests.bWorldMap || Context.bExtraAnimationsLoading || (Context.Songs != nullptr && Context.Songs->IsLoading());
	}

	/** Where the party leaves the map from, for the next: the map, the leader's and the camera's directions, one change more. */
	void SaveMapExit(FEventContext& Context);

	void RegisterFlow(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterThreads(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterVariables(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterMath(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterActorFlags(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterActors(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterFacing(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterItems(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterCharacters(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterPlacement(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterSprites(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterPartySprites(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterParty(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterCamera(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterCameraControl(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterCameraOrbits(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterActorControl(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterFieldState(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterScreen(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterModels(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterCameraPath(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterPlayerControl(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterMessages(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterWalks(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterGlides(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterScriptMoves(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterMapChanges(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterSoundEffects(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterSoundBanks(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterParticles(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterMusicControls(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterDoors(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterScrolls(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterPartyWalks(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterJumps(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterGears(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterImages(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterMarkers(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterPrograms(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterMenus(FBaseTable& Base, FExtendedTable& Extended);
	void RegisterMovies(FBaseTable& Base, FExtendedTable& Extended);

	const FBaseTable& BaseTable();
	const FExtendedTable& ExtendedTable();
}
