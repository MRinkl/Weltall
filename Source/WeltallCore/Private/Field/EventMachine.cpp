#include "Field/EventMachine.h"

#include "EventOps.h"
#include "EventSprites.h"

#include <cstdio>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace EventOps;

		/** A run that neither yields nor ends in this many instructions is cut off. */
		constexpr int32 RunLimit = 0x401;
		constexpr int32 UnlimitedBudget = 0xFFFF;
		constexpr int32 InitScript = 0;
		constexpr uint8 EndOpcode = 0x00;
		constexpr uint8 ExtendedOpcode = 0xFE;
		constexpr uint32 FirstLocalAddress = 0x10;
		/** A map sprite's id: its file with bit 7. */
		constexpr uint8 DefaultSpriteId = 0x80;
		constexpr std::array<uint32, 3> PartyCharacterAddresses = {0x3E, 0x40, 0x42};

		std::string Hex(uint32 Value, int32 Digits)
		{
			char Text[16];
			std::snprintf(Text, sizeof(Text), "%0*X", Digits, Value);
			return Text;
		}

		std::string Describe(const FEventContext& Context, EEventFault Fault, uint32 Value)
		{
			switch (Fault)
			{
			case EEventFault::UnimplementedOpcode:
				return Value > 0xFF ? "opcode FE " + Hex(Value & 0xFF, 2) + " is not implemented" : "opcode " + Hex(Value, 2) + " is not implemented";
			case EEventFault::PcOutsideCode:
				return "the pc " + Hex(Value, 4) + " is outside the " + std::to_string(Context.Scripts->CodeSize()) + "-byte bytecode";
			case EEventFault::ActorWithoutScripts:
				return "actor " + std::to_string(Value) + " has no script state";
			case EEventFault::VariableOutOfRange:
				return "variable address " + Hex(Value, 4) + " is past the variables";
			case EEventFault::ZoneOutOfRange:
				return "zone " + std::to_string(Value) + " is past the map's " + std::to_string(Context.Zones.size()) + " zones";
			case EEventFault::ReadOutsideCode:
				return "it reads the bytecode at " + Hex(Value, 4) + ", past its end";
			case EEventFault::NoFloorTriangle:
				return "the map's walkmesh does not hold the actor's floor (layer or triangle " + std::to_string(static_cast<int16>(Value)) + ")";
			case EEventFault::SquareRootOutsideTable:
				return "the square root of " + Hex(Value, 8) + " lies outside libgte's table";
			case EEventFault::TableIndexOutOfRange:
				return "index " + std::to_string(static_cast<int32>(Value)) + " is past the table the instruction reads";
			case EEventFault::NoGameState:
				return "it reads the game's state, and none is attached";
			case EEventFault::NoActorModels:
				return "actor " + std::to_string(Value) + " needs a sprite, and no actor model system is attached";
			case EEventFault::EncountersNeverScheduled:
				return "the encounters asked for cannot all fall on different ticks of their period";
			case EEventFault::NoSongs:
				return "it asks for a song, and no field music is attached";
			case EEventFault::WindowPlaceUndefined:
				return "message " + std::to_string(Value) + "'s window is placed by no band and no x of the script's, where the original reads a stale stack word";
			case EEventFault::NoSpriteFile:
				return "there is no sprite for file or party slot " + std::to_string(Value);
			case EEventFault::GearModule:
				if (Context.Gears.Models && Context.Gears.Models->Fault)
				{
					return Context.Gears.Models->Fault->Message;
				}
				return "actor " + std::to_string(Value) + "'s gear has no model loaded, where the original works on the gears' module's memory as it is";
			case EEventFault::GearArraysFull:
				return "gear slot " + std::to_string(Value) + " is past the four the field's gear arrays hold, which the original overruns";
			case EEventFault::GearJointUnset:
				return "a point on a joint of gear slot " + std::to_string(Value) + ", past the gears' module's slots, whose table the original reads past";
			case EEventFault::PlatformModel:
				return "platform actor " + std::to_string(Value) + " has no model to stand on, or is attached to an actor the map does not have";
			case EEventFault::NoPartySprites:
				return "it loads the party's sprite files, and none are attached";
			case EEventFault::PartySpriteFileUnreadable:
				return Value != 0 ? "the party's sprite file 4/" + std::to_string(Value) + " cannot be read into its buffer" : "the party's sprite files cannot be read into their buffers";
			case EEventFault::ActorWithoutSprite:
				return "actor " + std::to_string(Value) + " has no sprite";
			case EEventFault::UnimplementedAfterInit:
				return "opcode FE " + Hex(Value & 0xFF, 2) + " is not implemented once the map's init is over";
			case EEventFault::UnusableLineScroll:
				return "a VRAM scroller of " + std::to_string(Value >> 16) + " bands " + std::to_string(Value & 0xFFFF)
					+ " texels wide, which the original divides by zero or cannot allocate";
			case EEventFault::ImageUnreadable:
				return Value != 0 ? "the image file 4/" + std::to_string(Value) + " cannot be read as a TIM" : "it reads an image, and no files are attached";
			case EEventFault::ImageNotLoaded:
				return "it uses an image or CLUT rows it has not read or has given up, or a row past them (" + std::to_string(static_cast<int32>(Value)) + ")";
			case EEventFault::ScreenMarker:
				return "screen marker " + std::to_string(static_cast<int32>(Value))
					+ " is used before the markers are made, after they are given up, past the 33 or twice in a tick, where the original writes memory it does not own or hangs";
			case EEventFault::ModelUvScroll:
				return "actor " + std::to_string(Value) + " has no model of flat-textured polygons alone, whose texture coordinates the original's scroll assumes";
			case EEventFault::FloorFromStaleStack:
				return "the actor is placed on walkmesh layer " + std::to_string(static_cast<int16>(Value))
					+ ", which the map does not have: the original takes its height and floor normal from stale stack";
			case EEventFault::SoundBankSlot:
				return "sound bank slot " + std::to_string(static_cast<int32>(Value)) + " is not the gear battles' bank's, where the original unloads no bank or one behind the music's back";
			case EEventFault::SoundBankUnreadable:
				return "sound bank file " + std::to_string(Value >> 16) + "/" + std::to_string(static_cast<int16>(Value)) + " cannot be read";
			case EEventFault::SoundBankNotLoaded:
				return "no sound bank was read for the upload, or it does not fit in sound RAM";
			case EEventFault::None:
				break;
			}
			return "no fault";
		}

		FError FaultError(const FEventContext& Context, uint16 Pc, EEventFault Fault, uint32 Value)
		{
			const uint8* Code = Context.Scripts->Code();
			std::string Instruction;
			if (Pc < Context.Scripts->CodeSize())
			{
				Instruction = ", opcode " + Hex(Code[Pc], 2) + (Code[Pc] == ExtendedOpcode ? " " + Hex(Code[Pc + 1], 2) : "");
			}
			return MakeError("field map " + std::to_string(Context.RunningMap) + ", actor " + std::to_string(Context.Run.Actor) + ", pc " + Hex(Pc, 4) + Instruction + ": " + Describe(Context, Fault, Value));
		}

		/**
		 * Runs the running actor's thread from its pc until its turn ends, the budget is spent or a fault
		 * stops it; then the pc of the instruction that faulted, the fault left in the run's registers.
		 */
		std::optional<uint16> RunThread(FEventContext& Context, int32 Budget)
		{
			FEventRun& Run = Context.Run;
			Run.bYield = false;
			Run.Budget = Budget;
			if (Budget <= 0)
			{
				return std::nullopt;
			}
			const FBaseTable& Handlers = BaseTable();
			const uint8* Code = Context.Scripts->Code();
			const std::size_t CodeSize = Context.Scripts->CodeSize();
			for (int32 Count = 0; Count < RunLimit; ++Count)
			{
				const uint16 Pc = Run.State->Pc;
				if (Pc >= CodeSize)
				{
					Fail(Context, EEventFault::PcOutsideCode, Pc);
					return Pc;
				}
				Handlers[Code[Pc]](Context);
				if (Run.Fault != EEventFault::None)
				{
					return Pc;
				}
				if (Run.bUnlimited)
				{
					Run.Budget = UnlimitedBudget;
				}
				if (Context.bInitDone && Context.Requests.StopScripts())
				{
					return std::nullopt;
				}
				if (Run.bYield && !Run.bUnlimited)
				{
					return std::nullopt;
				}
				if (Run.Budget <= Count + 1)
				{
					return std::nullopt;
				}
			}
			return std::nullopt;
		}

		/** RunThread, its fault made the error that names it. */
		std::optional<FError> RunActor(FEventContext& Context, int32 Budget)
		{
			const std::optional<uint16> FaultPc = RunThread(Context, Budget);
			if (!FaultPc)
			{
				return std::nullopt;
			}
			const EEventFault Fault = Context.Run.Fault;
			Context.Run.Fault = EEventFault::None;
			return FaultError(Context, *FaultPc, Fault, Context.Run.FaultValue);
		}

		bool IsPartyActor(const FEventContext& Context, int32 Actor)
		{
			for (const uint8 PartyActor : Context.PartyActors)
			{
				if (PartyActor != FEventContext::NoActor && PartyActor == Actor)
				{
					return true;
				}
			}
			return false;
		}

		void Enter(FEventContext& Context, int32 Actor)
		{
			Context.Run.Actor = Actor;
			Context.Run.State = &Context.States[static_cast<std::size_t>(Actor)];
		}
	}

	std::string EventOps::DescribeFault(const FEventContext& Context, EEventFault Fault, uint32 Value)
	{
		return Describe(Context, Fault, Value);
	}

	void EventOps::RunScriptNow(FEventContext& Context, int32 Actor)
	{
		Enter(Context, Actor);
		Context.Run.bUnlimited = true;
		RunThread(Context, UnlimitedBudget);
	}

	void FEventMachine::SetMapStartVariables(FEventContext& Context)
	{
		Context.Variables.Set(FirstLocalAddress, 0);
		for (std::size_t Slot = 0; Slot < PartyCharacterAddresses.size(); ++Slot)
		{
			Context.Variables.Set(PartyCharacterAddresses[Slot], Context.PartyCharacters[Slot]);
		}
	}

	std::optional<FError> FEventMachine::InitMap(FEventContext& Context)
	{
		Context.bInitDone = false;
		SetMapStartVariables(Context);
		const int32 ActorCount = Context.Scripts->ActorCount();
		for (int32 Actor = 0; Actor < ActorCount; ++Actor)
		{
			Enter(Context, Actor);
			FEventActorState& State = *Context.Run.State;
			State.Pc = Context.Scripts->EntryPoint(Actor, TalkScript);
			if (State.Pc >= Context.Scripts->CodeSize())
			{
				return FaultError(Context, State.Pc, EEventFault::PcOutsideCode, State.Pc);
			}
			if (Context.Scripts->Code()[State.Pc] == EndOpcode)
			{
				State.Flags1 |= EventActorFlags1::NoTalkScript;
			}
			State.Pc = Context.Scripts->EntryPoint(Actor, InitScript);
		}
		for (int32 Actor = 0; Actor < ActorCount; ++Actor)
		{
			Enter(Context, Actor);
			Context.ModelsCreated = 0;
			Context.Run.bUnlimited = true;
			if (auto Error = RunActor(Context, UnlimitedBudget))
			{
				return Error;
			}
			if (Context.ModelsCreated == 0)
			{
				// An actor whose init made no model or sprite gets the map's first sprite.
				if (!EventOps::LoadActorSprite(Context, Actor, EFieldSpriteKind::Map, 0, 0, DefaultSpriteId, false))
				{
					const EEventFault Fault = Context.Run.Fault;
					Context.Run.Fault = EEventFault::None;
					return FaultError(Context, Context.Run.State->Pc, Fault, Context.Run.FaultValue);
				}
				Context.Run.State->Flags1 |= EventActorFlags1::NotFloorAligned;
			}
		}
		Context.bInitDone = true;
		return std::nullopt;
	}

	std::optional<FError> FEventMachine::RunScriptAside(FEventContext& Context, int32 Script)
	{
		constexpr int32 Actor = 0;
		if (Context.States.empty())
		{
			return MakeError("field map " + std::to_string(Context.RunningMap) + ": the first actor has no script state to run its script " + std::to_string(Script) + " with");
		}
		Enter(Context, Actor);
		FEventActorState& State = *Context.Run.State;
		const FEventActorState Saved = State;
		for (FEventThread& Thread : State.Threads)
		{
			// Each slot freed: no script, no pc, the lowest priority, no wait, reservation, movement or stage.
			Thread.WaitCount = 0;
			Thread.Script = FEventThread::NoScript;
			Thread.Pc = 0xFFFF;
			Thread.Word = (Thread.Word & ~(FEventThread::RequestWaitMask | FEventThread::Reserved | FEventThread::MoveStageMask | FEventThread::MoveCounterMask))
				| FEventThread::PriorityMask | FEventThread::MoveCounterMask;
		}
		Context.bInitDone = false;
		Context.Run.bUnlimited = true;
		State.Pc = Context.Scripts->EntryPoint(Actor, Script);
		std::optional<FError> Error = RunActor(Context, UnlimitedBudget);
		Context.bInitDone = true;
		// The blocks the original keeps beside the state are not given back.
		FEventActorState& After = Context.States[static_cast<std::size_t>(Actor)];
		const FEventActorState::FRide Ride = After.Ride;
		const std::array<int16, 8> MoveBounds = After.MoveBounds;
		const std::array<int32, FEventActorState::PartValueCount> PartValues = After.PartValues;
		After = Saved;
		After.Ride = Ride;
		After.MoveBounds = MoveBounds;
		After.PartValues = PartValues;
		return Error;
	}

	std::optional<FError> FEventMachine::Update(FEventContext& Context)
	{
		const int32 ActorCount = Context.MovieView == EFieldMovieView::OverField ? 1 : Context.Scripts->ActorCount();
		Context.bPlayerHadControl = Context.bPlayerControlled;
		Context.bPlayerControlled = false;
		Context.MessagesOpened = 0;
		for (int32 Actor = 0; Actor < ActorCount; ++Actor)
		{
			FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
			if (!Context.Actors[static_cast<std::size_t>(Actor)].TakesPartInEvents() || (State.Flags1 & EventActorFlags1::Removed) != 0)
			{
				continue;
			}
			if (Context.bInitDone && Context.Requests.StopScripts())
			{
				return std::nullopt;
			}
			Enter(Context, Actor);
			State.Flags0 &= ~EventActorFlags0::PartyFollower;
			if (Context.bPartyWalking && IsPartyActor(Context, Actor))
			{
				continue;
			}
			// The most urgent thread runs; among equals, the last slot.
			uint32 Best = FEventThread::Free;
			for (int32 Slot = 0; Slot < FEventActorState::ThreadCount; ++Slot)
			{
				const uint32 Priority = State.Threads[static_cast<std::size_t>(Slot)].Priority();
				if (Priority <= Best)
				{
					State.Slot = static_cast<uint8>(Slot);
					Best = Priority;
				}
			}
			if (Best == FEventThread::Free)
			{
				State.Threads[0].Pc = Context.Scripts->EntryPoint(Actor, DefaultScript);
				State.Slot = 0;
				State.Threads[0].SetPriority(DefaultPriority);
			}
			Context.Run.bUnlimited = false;
			State.Pc = State.Running().Pc;
			if ((State.Flags0 & EventActorFlags0::Frozen) == 0)
			{
				if (auto Error = RunActor(Context, TickBudget))
				{
					return Error;
				}
			}
			State.Running().Pc = State.Pc;
		}
		return std::nullopt;
	}

	bool FEventMachine::StartScript(FEventContext& Context, int32 Actor, int32 Script, uint32 Priority)
	{
		FEventActorState* State = Context.StateOf(Actor);
		if (State == nullptr)
		{
			return false;
		}
		for (const FEventThread& Thread : State->Threads)
		{
			if (Thread.Script == Script)
			{
				return false;
			}
		}
		for (FEventThread& Thread : State->Threads)
		{
			if (Thread.IsAvailable())
			{
				Thread.Pc = Context.Scripts->EntryPoint(Actor, Script);
				Thread.Script = static_cast<uint8>(Script);
				Thread.SetPriority(Priority);
				return true;
			}
		}
		return false;
	}
}
