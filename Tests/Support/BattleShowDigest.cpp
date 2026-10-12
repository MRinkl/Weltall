#include "Support/BattleShowDigest.h"

#include "Battle/BattleContext.h"
#include "Battle/Show/BattleShow.h"
#include "Game/PsxRandom.h"
#include "Support/BattleObjectDigest.h"
#include "Support/BattleSoundRecorder.h"

#include <algorithm>
#include <vector>

namespace Weltall::Testing
{
	namespace
	{
		/** The files numbered past the sprite files: a sequence extension's places by its pack's number, an event object's by the event's record of it. */
		constexpr uint32 ExtensionPlaces = 0xE00;
		constexpr uint32 EventObjectPlaces = 0xD00;
	}

	FBattleShowDigester::FBattleShowDigester(FBattleShow& InShow, const FBattleContext& InContext, const FPsxRandom& InRandom, FBattleSoundRecorder& InSound)
		: Show(InShow)
		, Context(InContext)
		, Random(InRandom)
		, Sound(InSound)
	{
		Digester.Sprites = &Show.Sprites;
		Digester.Battle = &Show.Battle;
		Digester.Markers = &Show.Markers;
		Digester.Extensions = &Show.Extensions;
		Digester.Place = [this](const FSpriteFile* File, uint32 Offset) -> std::pair<uint32, uint32> {
			if (Show.Playback.IsFreed(File))
			{
				return {FreedFile, 0};
			}
			// A sequence extension's sprite resource: its place in the extension, whose pack the objects number.
			if (const FBattleSequenceExtension* Extension = Show.ExtensionOf(File))
			{
				if (Show.Objects.IsReleased(&Extension->Pack))
				{
					return {FreedFile, 0};
				}
				return {ExtensionPlaces + static_cast<uint32>(Extension->Pack.Slot), Extension->Sprite.Offset + Offset};
			}
			if (const int32 Record = Show.Playback.EventObjectOf(File); Record >= 0)
			{
				return {EventObjectPlaces + static_cast<uint32>(Record), Offset};
			}
			const FBattleSpriteFile* Action = Show.Playback.ActionFile();
			std::vector<FSpriteFile*> Kept = Show.Battle.WritableFiles;
			Kept.erase(std::remove_if(Kept.begin(), Kept.end(),
						   [this](const FSpriteFile* Held) { return Show.ExtensionOf(Held) != nullptr || Show.Playback.EventObjectOf(Held) >= 0; }),
				Kept.end());
			if (Action != nullptr)
			{
				// The action's file, its sprite resources' places counted from the file's start (they lie inside it).
				const std::size_t Sections = Action->Sections().size();
				const auto InAction = [Action, Sections](const FSpriteFile* Held) {
					for (std::size_t Section = 0; Section < Sections; ++Section)
					{
						if (Held == Action->SectionSprite(Section))
						{
							return true;
						}
					}
					return Held == &Action->Sprite();
				};
				Kept.erase(std::remove_if(Kept.begin(), Kept.end(), InAction), Kept.end());
				const uint32 Number = static_cast<uint32>(Kept.size() + static_cast<std::size_t>(Show.Playback.ActionFileReads()) - 1);
				if (File == &Action->Sprite())
				{
					return {Number, Offset};
				}
				for (std::size_t Section = 0; Section < Sections; ++Section)
				{
					if (File == Action->SectionSprite(Section))
					{
						return {Number, Action->Sections()[Section].Offset + Offset};
					}
				}
			}
			const auto Found = std::find(Kept.begin(), Kept.end(), File);
			return Found == Kept.end() ? std::pair<uint32, uint32>{Elsewhere, 0} : std::pair<uint32, uint32>{static_cast<uint32>(Found - Kept.begin()), Offset};
		};
	}

	void FBattleShowDigester::DigestState(FOracleDigest& One)
	{
		const FBattleCamera& Camera = Show.Camera;
		One.I16(Camera.Eye[0], Camera.Eye[1], Camera.Eye[2], Camera.Target[0], Camera.Target[1], Camera.Target[2]);
		One.I16(Camera.DesiredEye[0], Camera.DesiredEye[1], Camera.DesiredEye[2], Camera.DesiredTarget[0], Camera.DesiredTarget[1], Camera.DesiredTarget[2]);
		One.I32(Camera.Mode, Camera.FramedSlots, Camera.FramingDistance, Camera.Distance);
		One.I16(Camera.SpriteDistance, Camera.ViewPitch, Camera.ViewYaw, 0);
		One.I32(Camera.EyePoint[0], Camera.EyePoint[1], Camera.EyePoint[2], Camera.TargetPoint[0], Camera.TargetPoint[1], Camera.TargetPoint[2]);
		const std::array<uint8, 4>& Clear = Show.Battle.Clear;
		One.U8(Clear[0], Clear[1], Clear[2], Clear[3], Show.HasStarted(), Show.ExtensionModule());
		One.I32(Show.Sprites.CountedTasks, Random.Seed());
		Digester.DigestResource(One, Show.Battle.CommonEffects);
		Sound.Digest(One);
	}

	void FBattleShowDigester::DigestObjects(FOracleDigest& One)
	{
		FBattleObjectWords Words;
		Words.RequestedEffect = Show.Objects.RequestedEffect;
		Words.Seed = Random.Seed();
		Words.TasksMade = static_cast<uint32>(Show.Sprites.CountedTasks);
		Words.TasksCounted = Show.Sprites.bCountingTasks ? 1 : 0;
		Words.Record = Show.Battle.Play.Record;
		One.U8(0xFB);
		DigestBattleObjects(One, Show.Objects, Show.Camera, Words);
	}

	void FBattleShowDigester::DigestVram(FOracleDigest& One)
	{
		const FVram& Vram = Show.Vram;
		if (!VramHash || HashedRevision != Vram.Revision())
		{
			FSha1 Hash;
			for (const uint16 Unit : Vram.ConsoleUnits())
			{
				const uint8 Bytes[2] = {static_cast<uint8>(Unit), static_cast<uint8>(Unit >> 8)};
				Hash.Update(Bytes);
			}
			VramHash = Hash.Finish();
			HashedRevision = Vram.Revision();
		}
		One.Raw(*VramHash);
	}

	void FBattleShowDigester::DigestTasks(FOracleDigest& One)
	{
		Digester.DigestTasks(One, Show.Fade, Show.Shake, Show.TintFade, Show.Numbers);
	}

	void FBattleShowDigester::DigestPlayback(FOracleDigest& One)
	{
		const FBattlePlayback& Playback = Show.Playback;
		const FBattlePlayState& State = Show.Battle.Play;
		One.U8(0xEC, Playback.HasController());
		if (Playback.HasController())
		{
			const FBattlePlayController& Play = Playback.Controller();
			One.I32(Play.State, Play.Actor, Play.PerformerSlot, Play.FirstTargetSlot, Play.RouteIndex, Play.PendingModelRecords);
			One.I32(Play.NextReady, Play.LastDistance);
			One.U8(Play.bReady, Play.Event, Play.bModelRecordRan);
			One.I16(Digester.IndexOf(Show.Battle.SlotSprites[static_cast<std::size_t>(Play.PerformerSlot)]), Digester.IndexOf(State.Targets[0]));
		}
		One.I32(State.Record);
		One.I16(Playback.WaitingRecords());
		One.I32(Playback.ActionFileSlot());
		const FBattleSpriteFile* Action = Playback.ActionFile();
		Digester.DigestPlace(One, Action != nullptr ? &Action->Sprite() : nullptr, 0);
		One.U8(Playback.IsAttackFileInstalled(), State.bHitVoices, Playback.IsItemCast(), State.bCastEnded);
		One.U8(Playback.DroppedGears());
		One.I16(Playback.Stayed(), Playback.Collapsed(), State.HitVoiced);
		One.U8(State.bReloadEffectImages, State.bMusicLowered, Context.bActionsPlayed);
		One.I16(Digester.IndexOf(State.Actor), Digester.IndexOf(State.Caster));
		One.I16(State.TargetCount);
		for (int32 Target = 0; Target < std::max(State.TargetCount, 1); ++Target)
		{
			One.I16(Digester.IndexOf(State.Targets[static_cast<std::size_t>(Target)]));
		}
		One.I16(State.TargetMask);
		One.I32(Show.Camera.ModeAfter);
		One.I16(Show.Camera.Pitch, State.EffectAnimation);
		for (const FSprite* Sprite : Show.Battle.SlotSprites)
		{
			One.U8(Sprite != nullptr ? Playback.ListenerKind(*Sprite) : 0);
			One.I16(Sprite != nullptr ? Sprite->Extra.StatusMarks : 0);
		}
	}

	void FBattleShowDigester::DigestPlayed(FOracleDigest& One)
	{
		DigestTasks(One);
		DigestPlayback(One);
		DigestState(One);
		DigestObjects(One);
		DigestVram(One);
	}
}
