#include "Support/BattleEventScenario.h"

#include "Foundation/LittleEndian.h"
#include "Support/BattleAiScenario.h"

#include <catch_amalgamated.hpp>

#include <cstddef>
#include <utility>

namespace Weltall::Testing
{
	namespace
	{
		constexpr uint32 ScriptCount = 48;
		constexpr std::size_t PartyOffset = 0x1D34;
		constexpr uint32 ObjectEntries = 4;
		constexpr std::array<uint16, 7> Story = {0, 1, 2, 3, 4, 0xFD, 0xFE};
		constexpr std::array<uint16, 12> WindowStyles = {0x1A, 0x41B, 0x1A05, 0x1B04, 0x1C02, 0x1E01, 0x1F00, 0x1806, 0x1E, 0x1B, 0x104, 0x1};
		constexpr std::array<uint8, 10> MessageStyles = {0, 0, 0, 1, 2, 4, 8, 0x18, 0x0A, 0x05};
		constexpr std::array<uint16, 4> WindowX = {0x8000, 0x8010, 0x8070, 0x80A0};
		constexpr std::array<uint16, 4> WindowY = {0x8000, 0x8010, 0x8098, 0x80A8};
		constexpr std::array<uint8, 19> Weighted = {0x02, 0x02, 0x05, 0x05, 0x06, 0x09, 0x0D, 0x18, 0x18, 0x1A, 0x1C, 0x1C, 0x22, 0x22, 0x23, 0x2B, 0x31, 0x3E, 0x45};
		constexpr uint32 OpcodeChoices = BattleEventScript::OpcodeCount + static_cast<uint32>(Weighted.size());

		uint8 PickOpcode(uint32 Index)
		{
			return Index < BattleEventScript::OpcodeCount ? static_cast<uint8>(Index) : Weighted[Index - BattleEventScript::OpcodeCount];
		}

		uint32 MessageCount(std::span<const uint8> Table)
		{
			const uint16 Last = LittleEndian::ReadU16(Table, 0);
			return Last == 0xFFFF ? 0 : Last + 1u;
		}

		void Pad(std::vector<uint8>& Bytes)
		{
			Bytes.resize((Bytes.size() + 7) / 8 * 8, 0);
		}

		/** One instruction's bytes, and for Jump and If the instruction number it jumps to. */
		struct FInstruction
		{
			std::vector<uint8> Bytes;
			bool bJumps = false;
			uint32 Target = 0;
		};

		/** battle_event_scenario.Writer, draw for draw. */
		class FWriter
		{
		public:
			FWriter(FXorShift32& InG, uint32 InActors, uint32 InMessages) : G(InG), Actors(InActors), Messages(InMessages) {}

			std::vector<FInstruction> Script(uint32 Number)
			{
				std::vector<FInstruction> Parts;
				const uint32 Count = N(7);
				for (uint32 Index = 0; Index < Count; ++Index)
				{
					Parts.push_back(Instruction(PickOpcode(N(OpcodeChoices))));
				}
				bool bReturns = false;
				if (Number == 1)
				{
					bReturns = N(4) != 0;
				}
				else if (Number == 0)
				{
					bReturns = N(2) == 1;
				}
				if (bReturns)
				{
					Parts.push_back({{0x22}});
				}
				Parts.push_back({{0x00}});
				return Parts;
			}

		private:
			uint32 N(uint32 Modulus) { return G.Next() % Modulus; }

			static void Put(std::vector<uint8>& Bytes, uint16 Word)
			{
				Bytes.push_back(static_cast<uint8>(Word));
				Bytes.push_back(static_cast<uint8>(Word >> 8));
			}

			static uint16 Number(uint32 Value) { return static_cast<uint16>(0x8000 | (Value & 0x7FFF)); }
			uint16 Variable() { return static_cast<uint16>(2 * N(32)); }

			uint16 Word(uint32 Value)
			{
				if (N(4) == 0)
				{
					return Variable();
				}
				return Number(Value);
			}

			uint16 Flagged(uint8 Flags, uint8 Bit)
			{
				if ((Flags & Bit) != 0)
				{
					return static_cast<uint16>(G.Next());
				}
				return Variable();
			}

			uint16 Combatant()
			{
				if (N(2) != 0)
				{
					return Number(N(16));
				}
				return Number(0x10 + N(8));
			}

			uint16 Performer() { return Number(0xF3 + N(11)); }
			uint16 Object() { return Number(0xF3 + N(4)); }

			/** Word operands, each drawn after the value before it. */
			template <typename... TValues>
			void Words(std::vector<uint8>& Bytes, TValues... Moduli)
			{
				(Put(Bytes, Word(N(Moduli))), ...);
			}

			FInstruction Instruction(uint8 Op)
			{
				static constexpr std::array<uint8, 4> FlagChoices = {0, 0x40, 0x80, 0xC0};
				FInstruction Out;
				std::vector<uint8>& B = Out.Bytes;
				B.push_back(Op);
				switch (Op)
				{
				case 0x01:
					Put(B, 0);
					Out.bJumps = true;
					Out.Target = N(8);
					return Out;
				case 0x02:
				{
					const uint32 Test = N(16);
					const uint8 Flags = static_cast<uint8>(Test | FlagChoices[N(4)]);
					const uint16 First = Flagged(Flags, 0x80);
					const uint16 Second = Flagged(Flags, 0x40);
					Put(B, First);
					Put(B, Second);
					B.push_back(Flags);
					Put(B, 0);
					Out.bJumps = true;
					Out.Target = N(8);
					return Out;
				}
				case 0x03:
				case 0x04:
				case 0x05:
				{
					const uint32 Actor = N(Actors);
					const uint32 Priority = N(8);
					const uint32 Script = 2 + N(6);
					B.push_back(static_cast<uint8>(Actor));
					B.push_back(static_cast<uint8>(Priority << 5 | Script));
					return Out;
				}
				case 0x06:
				case 0x09:
				case 0x0A:
				case 0x0B:
				case 0x0C:
				case 0x0F:
				case 0x10:
				case 0x11:
				case 0x16:
				case 0x17:
				{
					const uint8 Flags = FlagChoices[N(4)];
					const uint16 Destination = Variable();
					const uint16 Source = Flagged(Flags, 0x40);
					Put(B, Destination);
					Put(B, Source);
					B.push_back(Flags);
					return Out;
				}
				case 0x07:
				case 0x08:
				case 0x0D:
				case 0x0E:
				case 0x14:
					Put(B, Variable());
					return Out;
				case 0x12:
				case 0x13:
				{
					const uint16 Destination = Variable();
					Put(B, Destination);
					Put(B, Variable());
					return Out;
				}
				case 0x15:
				{
					const uint32 Small = N(100);
					const std::array<uint16, 3> Highs = {0, 0xFFFF, static_cast<uint16>(Small)};
					Put(B, Highs[N(3)]);
					Put(B, Variable());
					return Out;
				}
				case 0x18:
				case 0x19:
				{
					if (Messages == 0)
					{
						return {{0x32}};
					}
					const uint16 Text = static_cast<uint16>(N(Messages));
					if (Op == 0x19)
					{
						const uint8 Portrait = N(4) == 0 ? uint8{0xFF} : static_cast<uint8>(N(0x5A));
						B.push_back(Portrait);
					}
					Put(B, Text);
					B.push_back(MessageStyles[N(static_cast<uint32>(MessageStyles.size()))]);
					return Out;
				}
				case 0x1A:
				{
					Put(B, WindowX[N(4)]);
					Put(B, WindowY[N(4)]);
					const uint32 Width = N(2) != 0 ? 0 : 4 + N(13);
					Put(B, Number(Width));
					const uint32 Lines = N(2) != 0 ? 0 : 1 + N(4);
					Put(B, Number(Lines));
					Put(B, Number(WindowStyles[N(static_cast<uint32>(WindowStyles.size()))]));
					return Out;
				}
				case 0x1B:
					B.push_back(static_cast<uint8>(N(2) != 0 ? 0xF3 + N(3) : N(0x5A)));
					return Out;
				case 0x1E:
				case 0x1F:
					Words(B, 20);
					return Out;
				case 0x23:
				case 0x38:
				{
					const uint16 Who = Performer();
					const uint16 Target = Performer();
					Put(B, Who);
					Put(B, Target);
					Put(B, Number(N(0x60)));
					return Out;
				}
				case 0x24:
					Words(B, 32, 6);
					return Out;
				case 0x26:
					for (int32 Index = 0; Index < 4; ++Index)
					{
						const uint32 Value = G.Next();
						Put(B, Word(Value));
					}
					return Out;
				case 0x27:
					Words(B, 3, 20, 2, 500);
					return Out;
				case 0x28:
					for (const uint32 Modulus : {4u, 256u, 256u, 256u, 30u})
					{
						B.push_back(static_cast<uint8>(N(Modulus)));
					}
					return Out;
				case 0x29:
					Words(B, 64, 64, 64, 20);
					return Out;
				case 0x2A:
					Put(B, Object());
					Words(B, 12);
					return Out;
				case 0x2B:
					Words(B, 12);
					return Out;
				case 0x2C:
					B.push_back(static_cast<uint8>(N(2) != 0 ? 0xFE : N(256)));
					B.push_back(0x32);
					return Out;
				case 0x2D:
				case 0x2E:
					Words(B, 4);
					return Out;
				case 0x2F:
					Words(B, 128, 60);
					return Out;
				case 0x30:
					Words(B, 2);
					return Out;
				case 0x31:
					Words(B, 40, 128, 128, 2);
					return Out;
				case 0x35:
					Put(B, Object());
					Put(B, Number(N(ObjectEntries)));
					return Out;
				case 0x36:
				case 0x40:
					Put(B, Object());
					return Out;
				case 0x3A:
					Put(B, Combatant());
					Words(B, 24);
					return Out;
				case 0x3B:
				case 0x3C:
				case 0x3D:
				case 0x4B:
					Put(B, Combatant());
					return Out;
				case 0x3E:
				case 0x3F:
					Put(B, Combatant());
					Words(B, 400, 400, 400);
					return Out;
				case 0x41:
					Words(B, 40, 128, 2);
					return Out;
				case 0x45:
				{
					const uint16 Who = Combatant();
					Put(B, Who);
					Put(B, Combatant());
					Words(B, 24, 12);
					return Out;
				}
				case 0x46:
				{
					const uint16 Who = Combatant();
					Put(B, Who);
					Put(B, Combatant());
					Words(B, 40);
					return Out;
				}
				case 0x48:
					Words(B, 9, 4);
					return Out;
				case 0x49:
					Words(B, 3);
					return Out;
				default:
					return Out;
				}
			}

			FXorShift32& G;
			uint32 Actors;
			uint32 Messages;
		};

		std::vector<uint8> RandomScript(FXorShift32& G, uint32 Messages)
		{
			const uint32 Actors = 1 + G.Next() % 5;
			FWriter Writer(G, Actors, Messages);
			std::vector<std::array<uint16, 8>> Entries(Actors);
			std::vector<uint8> Code;
			for (uint32 Actor = 0; Actor < Actors; ++Actor)
			{
				const uint32 Count = 2 + G.Next() % 4;
				for (uint32 Number = 0; Number < Count; ++Number)
				{
					Entries[Actor][Number] = static_cast<uint16>(Code.size());
					const std::vector<FInstruction> Parts = Writer.Script(Number);
					std::vector<std::size_t> Starts;
					std::vector<std::pair<std::size_t, uint32>> Fixups;
					for (const FInstruction& Part : Parts)
					{
						Starts.push_back(Code.size());
						if (Part.bJumps)
						{
							Fixups.emplace_back(Code.size() + Part.Bytes.size() - 2, Part.Target);
						}
						Code.insert(Code.end(), Part.Bytes.begin(), Part.Bytes.end());
					}
					for (const auto& [At, Target] : Fixups)
					{
						const std::size_t To = Starts[std::min<std::size_t>(Target, Starts.size() - 1)];
						Code[At] = static_cast<uint8>(To);
						Code[At + 1] = static_cast<uint8>(To >> 8);
					}
				}
			}
			std::vector<uint8> Script(BattleEventScript::ActorCountOffset, 0);
			for (int32 Shift = 0; Shift < 32; Shift += 8)
			{
				Script.push_back(static_cast<uint8>(Actors >> Shift));
			}
			for (const std::array<uint16, 8>& Row : Entries)
			{
				for (const uint16 Entry : Row)
				{
					Script.push_back(static_cast<uint8>(Entry));
					Script.push_back(static_cast<uint8>(Entry >> 8));
				}
			}
			Script.insert(Script.end(), Code.begin(), Code.end());
			Pad(Script);
			return Script;
		}
	}

	FBattleEventCase DrawBattleEventState(FXorShift32& G, FBattleContext& Context, FGameState& State, const FBattleEventArchive& Archive)
	{
		std::span<uint8, FGameState::BlockSize> Block = State.Block();
		for (std::size_t Name = 0; Name < 31; ++Name)
		{
			const uint32 Length = 1 + G.Next() % 8;
			for (uint32 Glyph = 0; Glyph < 20; ++Glyph)
			{
				Block[20 * Name + Glyph] = Glyph < Length ? static_cast<uint8>(0x20 + G.Next() % 0x37) : uint8{0};
			}
		}
		for (std::size_t Slot = 0; Slot < 3; ++Slot)
		{
			Block[PartyOffset + Slot] = G.Next() % 4 == 0 ? uint8{0xFF} : static_cast<uint8>(G.Next() % 12);
		}
		DrawBattleStage(G, Context.Stage);
		const std::size_t Free = 16 + G.Next() % 8;
		Context.Occupancy[Free].Count = 0;
		FBattleEventCase Case;
		const bool bReal = G.Next() % 3 != 0;
		const int32 Number = static_cast<int32>(G.Next() % ScriptCount);
		const std::span<const uint8> Messages = Archive.Messages(Number);
		Case.Messages.assign(Messages.begin(), Messages.end());
		if (bReal)
		{
			const std::span<const uint8> Script = Archive.Script(Number);
			Case.Script.assign(Script.begin(), Script.end());
		}
		else
		{
			Pad(Case.Messages);
			Case.Script = RandomScript(G, MessageCount(Case.Messages));
		}
		Case.MotionFrames = static_cast<uint8>(1 + G.Next() % 6);
		Case.PerformFrames = static_cast<uint8>(1 + G.Next() % 8);
		Case.BlockFrames = static_cast<uint8>(G.Next() % 4);
		Case.PressPeriod = static_cast<uint8>(1 + G.Next() % 5);
		Case.PressPhase = static_cast<uint8>(G.Next() % Case.PressPeriod);
		Case.Runs.push_back({});
		const uint32 Count = 1 + G.Next() % 4;
		for (uint32 Index = 1; Index < Count; ++Index)
		{
			FBattleEventRunCase Run;
			const uint32 Pick = G.Next() % 8;
			Run.Story = Pick < Story.size() ? Story[Pick] : static_cast<uint16>(G.Next());
			if (Index == Count - 1 && G.Next() % 3 == 0)
			{
				std::array<uint16, 3> KnockedOut{};
				for (uint16& Bits : KnockedOut)
				{
					Bits = G.Next() % 2 != 0 ? uint16{0x8000} : uint16{0};
				}
				Run.KnockedOut = KnockedOut;
			}
			Case.Runs.push_back(Run);
		}
		Case.bShutdown = G.Next() % 2 == 1;
		return Case;
	}

	void DigestBattleEventState(FOracleDigest& Digest, const FBattleContext& Context, std::optional<bool> ListCursorShown)
	{
		constexpr uint8 Unset = 0xEE;
		const FBattleEventWork& Event = Context.Event;
		if (!Context.bEventWork)
		{
			Digest.U8(0);
			return;
		}
		Digest.U8(1);
		for (const FBattleEventRecord& Record : Event.Records)
		{
			for (const uint16 Pc : Record.Pcs)
			{
				Digest.I16(Pc);
			}
			Digest.Raw(Record.Priorities);
			Digest.Raw(Record.Scripts);
			Digest.U8(Record.Running, Record.RunningScript, Record.OrderArgument, Record.StartedScript, Record.Portrait);
			Digest.I16(static_cast<uint16>(Record.WaitCount));
			Digest.U8(Record.bWaiting ? 1 : 0, Record.PerformState, Record.bObjectLoaded ? 1 : 0);
		}
		for (const uint16 Operand : Event.Operands)
		{
			Digest.I16(Operand);
		}
		for (const uint16 Variable : Context.EventVariables)
		{
			Digest.I16(Variable);
		}
		Digest.Raw(Event.Order);
		Digest.I16(Event.Window.X, Event.Window.Y, Event.Window.Width, Event.Window.Lines, Event.Window.Style);
		Digest.U8(Event.bDone ? 1 : 0, Event.ReturnCountdown, Event.bMessageOpen ? 1 : 0);
		for (const bool bMoving : Event.Moving)
		{
			Digest.U8(bMoving ? 1 : 0);
		}
		for (const bool bStarted : Event.MotionStarted)
		{
			Digest.U8(bStarted ? 1 : 0);
		}
		Digest.U8(Event.bMusicLoaded ? 1 : 0);
		Digest.I16(Event.MusicFile);
		Digest.U8(Event.MusicVolume, Event.bSoundsRegistered ? 1 : 0);
		Digest.I32(static_cast<uint32>(Event.ArrowFrame), Event.TextX, Event.TextY);
		Digest.I16(static_cast<uint16>(Event.ArrowX), static_cast<uint16>(Event.ArrowY));
		Digest.U8(ListCursorShown.value_or(Event.bArrowShown) ? 1 : 0, Event.bPortraitShown ? 1 : 0, Event.bMessageShown ? 1 : 0, Event.bWaitCounters ? 1 : 0,
			Event.bKeyWait ? 1 : 0, Context.Party.bFirstWindowGrown ? 1 : 0);
		Digest.U8(Context.Party.InputMode, Context.Party.Event);
		if (Event.bMessageShown && Event.Text)
		{
			const FMesWindow::FState State = Event.Text->State();
			Digest.I16(static_cast<uint16>(State.X), static_cast<uint16>(State.Line), State.Flags, static_cast<uint16>(State.TopLine),
				static_cast<uint16>(State.LinesTyped));
			Digest.U8(State.BaseSpeed, State.Speed, State.SavedSpeed, State.Reason, State.bClosing ? 1 : 0);
			Digest.I16(static_cast<uint16>(State.Queued), static_cast<uint16>(State.WaitTimer), static_cast<uint16>(State.DelayCounter),
				static_cast<uint16>(State.DelayReload));
			Digest.U8(static_cast<uint32>(Event.Text->HighlightedRow()));
			for (const int16 Width : State.Widths)
			{
				Digest.I16(static_cast<uint16>(Width));
			}
			Digest.U8(State.Next);
		}
		const FBattleEventRequests& Requests = Event.Requests;
		Digest.U8(Requests.bRunsAfterBattle ? 1 : 0, Requests.bNoResults ? 1 : 0, Requests.bEndedBattle ? 1 : 0, Requests.bDefeatScripted ? 1 : 0,
			Context.End);
		Digest.U8(Requests.Chain ? Requests.Chain->Formation : 0, Requests.Chain ? Requests.Chain->Kind : Unset, Requests.Movie ? 1 : 0);
		if (Requests.Movie)
		{
			Digest.U8(Requests.Movie->Type, Requests.Movie->Number, Requests.Movie->NoSkip);
			Digest.I16(Requests.Movie->EndFrame);
			Digest.U8(1);
		}
		else
		{
			Digest.U8(Unset, Unset, Unset);
			Digest.I16(0xEEEE);
			Digest.U8(Unset);
		}
		Digest.U8(Requests.bFieldStartsFresh ? 0 : 1, Requests.FieldReturn ? *Requests.FieldReturn : Unset);
	}

	namespace
	{
		constexpr uint8 MessageInputMode = 2;

		void Fire(std::map<uint8, int32>& Due, int32 Frame, auto&& Report)
		{
			for (auto It = Due.begin(); It != Due.end();)
			{
				if (It->second <= Frame)
				{
					Report(It->first);
					It = Due.erase(It);
				}
				else
				{
					++It;
				}
			}
		}
	}

	bool FBattleEventFrames::Run()
	{
		return Run([this] { return StepBattleEvent(Context); });
	}

	void FBattleEventFrames::DigestEvent()
	{
		DigestBattleEventState(Digest, Context);
		Show.Digest(Digest);
		if (Extra)
		{
			Extra(Digest);
		}
	}

	void FBattleEventFrames::TakeCues()
	{
		for (const FBattleEventCue& Cue : Show.TakeEventCues())
		{
			const uint8 Slot = static_cast<uint8>(Cue.Args[0]);
			switch (Cue.Cue)
			{
			case EBattleEventCue::PerformSignalled:
				PerformDue[Slot] = Frames + Case.PerformFrames;
				break;
			case EBattleEventCue::CancelPerformSignals:
				PerformDue.clear();
				break;
			case EBattleEventCue::SetAnim:
			case EBattleEventCue::Attack:
			case EBattleEventCue::Walk:
			case EBattleEventCue::Jump:
				MotionDue[Slot] = Frames + Case.MotionFrames;
				break;
			default:
				break;
			}
		}
	}

	bool FBattleEventFrames::Frame()
	{
		if (Frames >= FrameCap)
		{
			return false;
		}
		++Frames;
		Show.PassFrame();
		Fire(PerformDue, Frames, [this](uint8 Slot) { FinishBattleEventPerform(Context, Slot); });
		Fire(MotionDue, Frames, [this](uint8 Slot) { FinishBattleEventMotion(Context, Slot); });
		if (Context.Party.InputMode == MessageInputMode)
		{
			TickBattleEventMessage(Context, Text);
			TakeBattleMessageInput(Context, (Frames + Case.PressPhase) % Case.PressPeriod == 0);
		}
		DigestEvent();
		return true;
	}

	bool FBattleEventFrames::Finish()
	{
		TakeCues();
		if (Context.Fault == EBattleFault::EventHangs)
		{
			return false;
		}
		REQUIRE(Context.Fault == EBattleFault::None);
		return true;
	}
}
