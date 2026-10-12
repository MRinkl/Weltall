#include "Support/BattleAiScenario.h"

#include "Support/BattleAiScripts.h"

#include <algorithm>
#include <array>

namespace Weltall::Testing
{
	namespace
	{
		constexpr std::size_t StageSize = 0x344;
		constexpr std::size_t Routes = 0x140;
	}

	void DrawBattleStage(FXorShift32& G, FBattleStage& Stage)
	{
		std::vector<uint8> File(4 + StageSize);
		for (std::size_t Byte = 0; Byte < StageSize; ++Byte)
		{
			File[4 + Byte] = static_cast<uint8>(G.Next());
		}
		for (std::size_t Route = 0; Route < 64; ++Route)
		{
			const std::size_t At = 4 + Routes + Route * 8;
			const uint32 Count = G.Next() % 8;
			for (uint32 Step = 1; Step <= Count; ++Step)
			{
				const uint32 Group = G.Next() % 8;
				File[At + Step] = static_cast<uint8>(Group | (G.Next() % 4 == 0 ? 0x80 : 0));
			}
			if (Count < 7)
			{
				File[At + Count + 1] = 0xFF;
			}
		}
		Stage = *FBattleStage::Parse(File);
	}

	void DrawBattleAiState(FXorShift32& G, FBattleContext& Context, FGameState& State, std::vector<uint8>& Scene)
	{
		for (std::size_t Slot = FBattleContext::PartySlotCount; Slot < FBattleContext::SlotCount; ++Slot)
		{
			Context.Combatants[Slot].GearPart.Buffs &= static_cast<uint16>(~0x2000);
		}
		Context.bEventWork = G.Next() % 2 != 0;
		Scene.clear();
		for (uint32 Enemy = 0; Enemy < FBattleContext::EnemySlotCount; ++Enemy)
		{
			for (std::size_t Script = 0; Script < 4; ++Script)
			{
				Context.AiRecords[Enemy].Scripts[Script] = static_cast<uint32>(Scene.size());
				DrawAiScript(G, Enemy, Context.bEventWork, Scene);
			}
		}
		Context.SceneFile = Scene;
		for (FBattleAiRecord& Record : Context.AiRecords)
		{
			for (uint8& Byte : Record.Variables)
			{
				Byte = static_cast<uint8>(G.Next());
			}
		}
		Context.AiQueue = {};
		for (uint32 Entry = 0, Count = G.Next() % 6; Entry < Count; ++Entry)
		{
			DrawAiQueueEntry(G, Context.AiQueue, Entry);
		}
		for (uint8& Name : Context.NameEntries)
		{
			Name = static_cast<uint8>(G.Next());
		}
		for (bool& bTargeted : Context.Targeted)
		{
			bTargeted = G.Next() % 2 != 0;
		}
		Context.NextMessageBox = static_cast<uint8>(G.Next() % 11);
		for (FBattleWaypoint& Waypoint : Context.Route)
		{
			Waypoint.X = static_cast<uint16>(G.Next());
			Waypoint.Z = static_cast<uint16>(G.Next());
			Waypoint.bJump = static_cast<uint8>(G.Next());
		}
		Context.EventVariables = {};
		if (Context.bEventWork)
		{
			for (uint16& Variable : Context.EventVariables)
			{
				Variable = static_cast<uint16>(G.Next());
			}
		}
		Context.CurrentAttack = static_cast<uint8>(G.Next());
		for (uint8& Target : Context.SlotTargets)
		{
			const uint32 Value = G.Next() % 12;
			Target = static_cast<uint8>(Value == 11 ? 0xFF : Value);
		}
		for (uint16& Global : State.BattleGlobals())
		{
			Global = static_cast<uint16>(G.Next());
		}
		DrawBattleStage(G, Context.Stage);
	}

	void DigestBattleAiState(FOracleDigest& Digest, const FBattleContext& Context, const FGameState& State)
	{
		for (const FBattleAiRecord& Record : Context.AiRecords)
		{
			for (const uint32 Script : Record.Scripts)
			{
				Digest.I32(Script);
			}
			Digest.Raw(Record.Variables);
		}
		Digest.Raw(Context.AiQueue);
		Digest.Raw(Context.NameEntries);
		for (const bool bTargeted : Context.Targeted)
		{
			Digest.U8(bTargeted ? 1 : 0);
		}
		Digest.U8(Context.NextMessageBox);
		for (const FBattleWaypoint& Waypoint : Context.Route)
		{
			Digest.I16(Waypoint.X, Waypoint.Z);
			Digest.U8(Waypoint.bJump);
		}
		Digest.U8(Context.bEventWork ? 1 : 0);
		if (Context.bEventWork)
		{
			for (const uint16 Variable : Context.EventVariables)
			{
				Digest.I16(Variable);
			}
		}
		Digest.U8(Context.CurrentAttack);
		Digest.Raw(Context.SlotTargets);
		for (const uint16 Global : State.BattleGlobals())
		{
			Digest.I16(Global);
		}
		for (int32 Byte = 0; Byte < static_cast<int32>(StageSize); ++Byte)
		{
			Digest.U8(Context.Stage.Bytes()[static_cast<std::size_t>(Byte)]);
		}
	}

	void FRecordingBattleShow::FrameCamera(uint16 Slots)
	{
		Calls.insert(Calls.end(), {1, static_cast<uint8>(Slots), static_cast<uint8>(Slots >> 8)});
		++Count;
	}

	void FRecordingBattleShow::SetMessageBox(uint8 Box, uint8 Entry)
	{
		Calls.insert(Calls.end(), {2, Box, Entry});
		++Count;
	}

	void FRecordingBattleShow::ShowNameBox(uint8 Entry)
	{
		Calls.insert(Calls.end(), {5, Entry});
		++Count;
	}

	void FRecordingBattleShow::BeginPlayback(bool bStays, uint8 Actor, uint8 Target, uint8 NextReady)
	{
		Calls.insert(Calls.end(), {3, static_cast<uint8>(bStays ? 1 : 0), Actor, Target, NextReady});
		++Count;
	}

	void FRecordingBattleShow::CancelPlayback()
	{
		Calls.push_back(6);
		++Count;
	}

	void FRecordingBattleShow::SetTargetMarkers(uint16 Slots)
	{
		Calls.insert(Calls.end(), {7, static_cast<uint8>(Slots), static_cast<uint8>(Slots >> 8)});
		++Count;
	}

	void FRecordingBattleShow::PlaySound(uint8 Effect)
	{
		Calls.insert(Calls.end(), {8, Effect});
		++Count;
	}

	void FRecordingBattleShow::BoardGear(uint8 Slot)
	{
		Calls.insert(Calls.end(), {9, Slot});
		++Count;
		BlockLeft = BlockFrames;
	}

	void FRecordingBattleShow::PlayEventCue(const FBattleEventCue& Cue)
	{
		Calls.insert(Calls.end(), {10, static_cast<uint8>(Cue.Cue)});
		for (const uint16 Arg : Cue.Args)
		{
			Calls.insert(Calls.end(), {static_cast<uint8>(Arg), static_cast<uint8>(Arg >> 8)});
		}
		++Count;
		Cues.push_back(Cue);
		switch (Cue.Cue)
		{
		case EBattleEventCue::FinishPerform:
		case EBattleEventCue::SpecialAttack:
		case EBattleEventCue::FinishLoads:
		case EBattleEventCue::PlayVoice:
			BlockLeft = BlockFrames;
			break;
		default:
			break;
		}
	}

	void FRecordingBattleShow::PlayTurnCue(EBattleTurnCue Cue, uint8 Arg)
	{
		Calls.insert(Calls.end(), {12, static_cast<uint8>(Cue), Arg});
		++Count;
	}

	void FRecordingBattleShow::PlayEndCue(EBattleEndCue Cue, uint8 Arg)
	{
		Calls.insert(Calls.end(), {13, static_cast<uint8>(Cue), Arg});
		++Count;
		if (Cue == EBattleEndCue::FinishLoads || Cue == EBattleEndCue::Pose)
		{
			BlockLeft = EndBlockFrames;
		}
	}

	void FRecordingBattleShow::PassFrame()
	{
		if (BlockLeft > 0)
		{
			--BlockLeft;
		}
	}

	std::vector<FBattleEventCue> FRecordingBattleShow::TakeEventCues()
	{
		std::vector<FBattleEventCue> Taken;
		Taken.swap(Cues);
		return Taken;
	}

	void FRecordingBattleShow::RecordEventHook()
	{
		Calls.push_back(4);
		++Count;
	}

	void FRecordingBattleShow::RecordResultScreen(uint32 Gold)
	{
		Calls.insert(Calls.end(), {11, static_cast<uint8>(Gold), static_cast<uint8>(Gold >> 8), static_cast<uint8>(Gold >> 16), static_cast<uint8>(Gold >> 24)});
		++Count;
	}

	void FRecordingBattleShow::Digest(FOracleDigest& Digest)
	{
		Digest.I16(Count);
		Digest.Raw(Calls);
		Calls.clear();
		Count = 0;
	}
}
