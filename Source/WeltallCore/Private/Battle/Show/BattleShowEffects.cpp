#include "Battle/BattleContext.h"
#include "Battle/Show/BattleShow.h"
#include "Battle/Show/BattleShowPlayback.h"
#include "Battle/Show/BattleShowVram.h"
#include "Graphics/SpriteFile.h"
#include "Graphics/VramStream.h"

namespace Weltall
{
	namespace
	{
		constexpr int32 ItemCastCode = 0xF5;
		/** Where an effect's resource puts its cells and its palettes. */
		constexpr int16 EffectTextureX = 0x380;
		constexpr int16 EffectTextureY = 0x100;
		constexpr int16 EffectClutY = 0x1F4;
		/** The music while an effect shows, and the steps it takes to fall to it. */
		constexpr int32 EffectMusicVolume = 0x60;
		constexpr int32 EffectMusicSteps = 0x78;
		/** The effect whose load first brings the gears' columns back over the effects' own. */
		constexpr int32 ColumnsEffect = 0xE3;
		/** No party slot keeps its gear. */
		constexpr int32 NoSlotKept = -1;
		/** The characters whose gears the reload makes again (the party's; 11h and past are none). */
		constexpr uint8 GearCharacters = 0x11;
	}

	// Records F4 and F5 ----------------------------------------------------------------------------------

	EShowFlow FBattlePlayback::RunCastRecord(FShowRoutine& Routine)
	{
		FSprite* Sprite = SlotSprite(Routine.Other);
		if (Sprite == nullptr)
		{
			Fail(static_cast<uint8>(EBattleShowFault::EndedTask));
			return EShowFlow::Done;
		}
		switch (Routine.Resume)
		{
		case 0:
		{
			// The effect's number and its animation within its file; the performer casts it from the plain pose.
			const int32 Parameter = Record().Parameter;
			FBattlePlayState& State = Show.Battle.Play;
			State.Actor = Sprite;
			State.EffectAnimation = Parameter >> 9 & 0x3F;
			Sprite->SetAnimationNumber(CastAnimation);
			return Call(Routine, 1, EPlayRoutine::EffectCast, Parameter & 0x1FF, Routine.Other);
		}
		case 1:
			// An item's effect starts at the next step, the performer in the item's pose.
			if (Routine.Value == ItemCastCode)
			{
				Show.Battle.Play.Caster = Sprite;
				Show.Sprites.SetAnimation(*Sprite, ItemAnimation);
				bItemCast = true;
			}
			return EShowFlow::Done;
		default:
			return EShowFlow::Done;
		}
	}

	// An effect's cast and load --------------------------------------------------------------------------

	EShowFlow FBattlePlayback::RunEffectCast(FShowRoutine& Routine)
	{
		FSprite* Sprite = SlotSprite(Routine.Other);
		if (Sprite == nullptr)
		{
			Fail(static_cast<uint8>(EBattleShowFault::EndedTask));
			return EShowFlow::Done;
		}
		switch (Routine.Resume)
		{
		case 0:
			if (Sprite->PrimaryFile == nullptr)
			{
				return Call(Routine, 3, EPlayRoutine::EffectLoad, Routine.Value);
			}
			// A performer with a file of its own plays its cast pose, the pose's ends counted, while the effect loads.
			CastPoseEnds = 0;
			if (Sprite->Animation() != 0)
			{
				Sprite->Listener = &CastPoseEnd;
				Show.Sprites.SetAnimation(*Sprite, Sprite->Animation());
			}
			return Call(Routine, 1, EPlayRoutine::EffectLoad, Routine.Value);
		case 1:
			if (Sprite->Animation() == 0)
			{
				return EShowFlow::Done;
			}
			Routine.Resume = 2;
			[[fallthrough]];
		case 2:
			// Frames until the pose has ended once.
			if (CastPoseEnds == 0)
			{
				return EShowFlow::Frame;
			}
			Sprite->Listener = nullptr;
			return EShowFlow::Done;
		default:
			return EShowFlow::Done;
		}
	}

	EShowFlow FBattlePlayback::RunEffectLoad(FShowRoutine& Routine)
	{
		const int32 Effect = Routine.Value;
		switch (Routine.Resume)
		{
		case 0:
			if (Effect == ColumnsEffect)
			{
				BringBackGearColumns();
			}
			// Aborting the loads: the port's reads are never under way, so it is waiting for them.
			return Call(Routine, 1, EPlayRoutine::WaitLoads);
		case 1:
		{
			TResult<FBattleSpriteFile> File = Show.SpriteFiles->ReadEffect(Effect);
			if (!File)
			{
				Fail(static_cast<uint8>(EBattleShowFault::File));
				return EShowFlow::Done;
			}
			LoadingEffect = std::make_unique<FBattleSpriteFile>(std::move(*File));
			const uint8 Drop = static_cast<uint8>(LoadingEffect->DropsGears());
			if (Drop != 0)
			{
				// The party gears leave to give the effect their room: every one, or all but the actor's.
				GearDrop = Drop;
				Show.Camera.Frame(0, Show.Battle.CameraSubjects(), Show.Gte);
				int32 Kept = NoSlotKept;
				if (Drop == DropsEveryGear)
				{
					Show.Objects.GearPlacesTaken = 0;
				}
				else
				{
					const FSprite* Actor = Show.Battle.Play.Actor;
					if (Actor == nullptr)
					{
						Fail(static_cast<uint8>(EBattleShowFault::EndedTask));
						return EShowFlow::Done;
					}
					// The places' bits are cleared by the actor's slot, not by its gear's place.
					Kept = FBattleSprites::SlotOf(*Actor);
					Show.Objects.GearPlacesTaken = static_cast<uint16>(Show.Objects.GearPlacesTaken & ~(1u << Kept));
				}
				return Call(Routine, 2, EPlayRoutine::RemoveOtherParty, Kept);
			}
			[[fallthrough]];
		}
		case 2:
		{
			// Its images and palettes, streamed to VRAM as the drive delivers them.
			TResult<std::vector<uint8>> Stream = Show.SpriteFiles->ReadEffectStream(Effect);
			TResult<std::vector<FVramImage>> Images = Stream ? DecodeVramStream(*Stream) : TResult<std::vector<FVramImage>>(Stream.Error());
			if (!Images)
			{
				Fail(static_cast<uint8>(EBattleShowFault::File));
				return EShowFlow::Done;
			}
			for (const FVramImage& Image : *Images)
			{
				Show.Vram.Upload(Image);
			}
			// The effect is the action's file from now on.
			ActionFileData = std::move(LoadingEffect);
			++FileReads;
			AddWritable(*ActionFileData);
			bEffectInMemory = true;
			return EShowFlow::Done;
		}
		default:
			return EShowFlow::Done;
		}
	}

	void FBattlePlayback::BringBackGearColumns()
	{
		if (Show.Objects.Tables == nullptr)
		{
			Fail(static_cast<uint8>(EBattleShowFault::File));
			return;
		}
		CopyGearColumns(Show.Vram, Show.Objects.Tables->GearPlaces, true, Show.MoveScratch);
	}

	// The party gears an effect drops and brings back ------------------------------------------------------

	EShowFlow FBattlePlayback::RunRemoveOtherParty(FShowRoutine& Routine)
	{
		const int32 Kept = Routine.Value;
		switch (Routine.Resume)
		{
		case 0:
			// The other party slots' proxies end at once (a combatant's sprite has a file of its own and stays).
			for (int32 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				FSpriteTask* Task = Show.Battle.SlotTask(Slot);
				if (Slot != Kept && Task != nullptr && Task->Sprite.PrimaryFile == nullptr)
				{
					Show.Sprites.EndTask(*Task);
				}
			}
			Show.Objects.StartRemovingOtherParty(Kept);
			Routine.Resume = 1;
			[[fallthrough]];
		case 1:
			// A frame after each look at them while any still shows or plays, then one more, then they are freed.
			Routine.Other = Show.Objects.OtherPartyLeaving(Kept) ? 1 : 0;
			Routine.Resume = 2;
			return EShowFlow::Frame;
		case 2:
			Routine.Resume = Routine.Other != 0 ? 1 : 3;
			return Routine.Other != 0 ? EShowFlow::Again : EShowFlow::Frame;
		case 3:
			Show.Objects.FreeOtherParty(Kept);
			return EShowFlow::Done;
		default:
			return EShowFlow::Done;
		}
	}

	EShowFlow FBattlePlayback::RunReloadGearModels(FShowRoutine& Routine)
	{
		switch (Routine.Resume)
		{
		case 0:
			if (GearDrop == 0)
			{
				return EShowFlow::Done;
			}
			return Call(Routine, 1, EPlayRoutine::WaitLoads);
		case 1:
			Routine.Resume = 2;
			return EShowFlow::Frame;
		case 2:
			Routine.Resume = 3;
			return EShowFlow::Frame;
		case 3:
		{
			// The dropped gears are read and made again: every party member in a gear, or all but the actor.
			int32 ActorSlot = NoSlotKept;
			if (GearDrop != DropsEveryGear)
			{
				const FSprite* Actor = Show.Battle.Play.Actor;
				if (Actor == nullptr)
				{
					Fail(static_cast<uint8>(EBattleShowFault::EndedTask));
					return EShowFlow::Done;
				}
				ActorSlot = FBattleSprites::SlotOf(*Actor);
			}
			for (int32 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				const FBattleSlot& Record = Show.Context->Slots[static_cast<std::size_t>(Slot)];
				if (Record.InGear != 0 && Slot != ActorSlot && Record.Occupant < GearCharacters)
				{
					Show.Objects.QueueGearModel(Slot);
				}
			}
			Routine.Resume = 4;
			[[fallthrough]];
		}
		case 4:
			if (Show.Objects.PendingGearModels != 0)
			{
				return EShowFlow::Frame;
			}
			GearDrop = 0;
			Show.Objects.PendingGearModels = 0;
			return EShowFlow::Done;
		default:
			return EShowFlow::Done;
		}
	}

	// The effect the objects ask for ---------------------------------------------------------------------------

	EShowFlow FBattlePlayback::RunLoadRequested(FShowRoutine& Routine)
	{
		switch (Routine.Resume)
		{
		case 0:
			return Call(Routine, 1, EPlayRoutine::EffectLoad, Routine.Value);
		case 1:
			StartEffect();
			bAskedEffectLoaded = true;
			return EShowFlow::Done;
		default:
			return EShowFlow::Done;
		}
	}

	// An effect's start -----------------------------------------------------------------------------------

	bool FBattlePlayback::StartEffect()
	{
		FBattlePlayState& State = Show.Battle.Play;
		FSprite* Actor = State.Actor;
		if (!bEffectInMemory || !ActionFileData)
		{
			// The last effect loaded was freed since (or none was loaded): the original parses whatever its pointer reaches.
			Fail(static_cast<uint8>(EBattleShowFault::Effect));
			return false;
		}
		if (Actor == nullptr)
		{
			Fail(static_cast<uint8>(EBattleShowFault::EndedTask));
			return false;
		}
		FBattleSpriteFile& Effect = *ActionFileData;
		State.bCastEnded = false;
		FSpriteResource& Resource = Show.Battle.EffectResource;
		Show.Sprites.ParseResource(Resource, Effect.Sprite(), EffectTextureX, EffectTextureY, 0, EffectClutY);
		Show.LoadAnimExtension();
		const bool bResident = Resource.File->IsResident(Resource.Tables);
		FSprite* Played = Actor;
		if (!bResident)
		{
			Actor->AlternateFile = &Effect.Sprite();
			Show.Sprites.SetAnimation(*Actor, -1);
		}
		else
		{
			// Its sprite a child of the actor, running the cast's animation of the resource (which a section may replace).
			Played = Show.Sprites.SpawnChild(*Actor, *Resource.File, Resource.File->AnimationHeader(Resource.Tables, State.EffectAnimation), &Resource);
			if (Played == nullptr)
			{
				Fail(static_cast<uint8>(EBattleShowFault::Sprites));
				return false;
			}
		}
		const int32 SoundSet = Show.Battle.Install(Effect);
		Played->SoundSet = SoundSet;
		Actor->SoundSet = SoundSet;
		bAttackFileInstalled = true;
		State.bMusicLowered = true;
		Show.SetMusicVolume(EffectMusicVolume, EffectMusicSteps);
		return !bResident;
	}
}
