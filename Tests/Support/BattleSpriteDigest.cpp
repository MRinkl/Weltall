#include "Support/BattleSpriteDigest.h"

#include "Battle/Show/BattleShowExtensions.h"
#include "Battle/Show/BattleShowMarkers.h"
#include "Battle/Show/BattleShowNumbers.h"
#include "Battle/Show/BattleShowScreen.h"
#include "Battle/Show/BattleShowTasks.h"
#include "Battle/Show/BattleShowSprites.h"
#include "Graphics/Sprite.h"
#include "Graphics/SpriteSystem.h"

#include <algorithm>

namespace Weltall::Testing
{
	namespace
	{
		constexpr uint8 UnknownTask = 0xEF;
	}

	void FBattleSpriteDigester::DigestTasks(FOracleDigest& Out, const FBattleFadeTask& Fade, const FBattleShakeTask& Shake, const FBattleTintFadeTask& TintFade,
		const FBattleNumbers& Numbers) const
	{
		for (const FKernelTaskEntry& Entry : Sprites->Tasks())
		{
			if (Entry.Sprite)
			{
				const FSpriteTask& Task = *Entry.Sprite;
				Out.U8(1, static_cast<uint8>(Battle->UpdateRoutine(Task)), static_cast<uint8>(Battle->EndRoutine(Task)), Task.DrawRole, Task.bDrawn);
				Out.I16(IndexOf(Task.Owner));
				Out.U8((Task.Marks >> 29 & 3) | (Entry.bCounted ? 4u : 0u));
				DigestSprite(Out, Task.Sprite);
				continue;
			}
			const IKernelTask* Task = Entry.Task;
			const FBattleSprites::FScriptTaskView View = Battle->ScriptTask(Task);
			if (View.Kind != 0)
			{
				// The task's own fields come through the view the test reads them by.
				const FBattleSprites::FScriptTaskState State = Battle->ScriptTaskState(Task);
				switch (View.Kind)
				{
				case 1:
					Out.U8(2);
					Out.I16(IndexOf(View.Sprite));
					Out.I32(State.Distance, State.Minimum);
					DigestPlace(Out, View.Sprite->ScriptFile, State.Jump);
					Out.U8(State.Animation);
					break;
				case 2:
					Out.U8(3);
					Out.I16(IndexOf(View.Sprite));
					Out.U8(State.OwnSlot, State.TargetSlot, State.Animation);
					break;
				case 3:
					Out.U8(4);
					Out.I16(IndexOf(View.Sprite));
					Out.I32(State.Frame, State.Animation, State.Blend, State.Width);
					for (const std::array<FGteVector, 5>* Points : {&State.Read, &State.Drawn})
					{
						for (const FGteVector& Point : *Points)
						{
							Out.I16(Point[0], Point[1], Point[2], 0);
						}
					}
					break;
				case 5:
					// An event's walk watch: its sprite, the animation it began in, the distance last measured, the least.
					Out.U8(0x10);
					Out.I16(IndexOf(View.Sprite));
					Out.I32(State.Animation, State.Distance, State.Minimum);
					break;
				default:
					Out.U8(UnknownTask);
					break;
				}
			}
			else if (Task == &Fade)
			{
				Out.U8(5);
				Out.I32(Fade.Fade.Total, Fade.Fade.Remaining);
				Out.U8(Fade.Fade.Mode, Fade.Fade.Target[0], Fade.Fade.Target[1], Fade.Fade.Target[2], Fade.Fade.From[0], Fade.Fade.From[1], Fade.Fade.From[2],
					Fade.Fade.Shown[0], Fade.Fade.Shown[1], Fade.Fade.Shown[2]);
			}
			else if (Task == &Shake)
			{
				const FBattleShake& Moving = Shake.Shake;
				Out.U8(6);
				Out.I16(Moving.Amplitude[0], Moving.Amplitude[1], Moving.Amplitude[2], Moving.From[0], Moving.From[1], Moving.From[2], Moving.Target[0], Moving.Target[1],
					Moving.Target[2]);
				Out.I32(Moving.Count, Moving.Remaining, Moving.Total);
			}
			else if (Task == &TintFade)
			{
				const FBattleTintFade& Tint = TintFade.Fade;
				Out.U8(7);
				Out.I32(Tint.Total, Tint.Remaining);
				Out.I16(Tint.From, Tint.Target, Tint.Drawn, Tint.Level, Tint.Mode, Tint.Color[0], Tint.Color[1], Tint.Color[2]);
			}
			else if (Task == &Numbers.Total)
			{
				Out.U8(10);
			}
			else if (const auto Number = std::find_if(Numbers.Numbers.begin(), Numbers.Numbers.end(), [Task](const FBattleNumber& Each) { return &Each == Task; });
					 Number != Numbers.Numbers.end())
			{
				Out.U8(8, static_cast<uint8>(Number->Phase));
				Out.I16(IndexOf(Number->Over));
				Out.I32(Number->Count, Number->Position[0], Number->Position[1], Number->Position[2]);
				Out.I16(Number->Rotation[0], Number->Rotation[1], Number->Rotation[2]);
				Out.I32(Number->Scale[0], Number->Scale[1], Number->Scale[2]);
				Out.U8(Number->bRight, Number->Color[0], Number->Color[1], Number->Color[2], Number->Code);
			}
			else if (const auto Burst = std::find_if(Numbers.Bursts.begin(), Numbers.Bursts.end(), [Task](const FBattleTotalBurst& Each) { return &Each == Task; });
					 Burst != Numbers.Bursts.end())
			{
				Out.U8(9);
				Out.I32(Burst->Count, Burst->Spin);
				Out.I16(Burst->Rotation[0], Burst->Rotation[1], Burst->Rotation[2]);
				Out.I32(Burst->Scale[0], Burst->Scale[1], Burst->Scale[2]);
				Out.U8(Burst->Color[0], Burst->Color[1], Burst->Color[2], Burst->Code);
			}
			else if (const FBattleMarker* Marker = Markers != nullptr ? Markers->Find(Task) : nullptr)
			{
				Out.U8(11);
				Out.I16(IndexOf(Marker->Target));
				Out.I32(Marker->Position[0], Marker->Position[1], Marker->Position[2]);
				Out.I16(Marker->Rotation[0], Marker->Rotation[1], Marker->Rotation[2]);
				Out.I32(Marker->Height, Marker->Count);
			}
			else if (Markers != nullptr && Task == &Markers->Glow)
			{
				Out.U8(12);
				Out.I16(IndexOf(Markers->Glow.Sprite));
				Out.I32(Markers->Glow.Count, Markers->Glow.Actor);
			}
			else if ((Extensions == nullptr || !DigestExtensionTask(Out, Task)) && !(OtherTask && OtherTask(Out, Task)))
			{
				Out.U8(UnknownTask);
			}
		}
	}

	bool FBattleSpriteDigester::DigestExtensionTask(FOracleDigest& Out, const IKernelTask* Task) const
	{
		if (const FBattleWave* Wave = Extensions->Wave(Task))
		{
			Out.U8(13, 2);
			Out.I16(IndexOf(Wave->Sprite.Sprite));
			Out.I32(Wave->Amplitude, Wave->AmplitudeStep, Wave->Frequency, Wave->FrequencyStep, Wave->Phase, Wave->Speed);
			return true;
		}
		if (const FBattleShards* Shards = Extensions->Shards(Task))
		{
			Out.U8(13, 3);
			Out.I32(static_cast<uint32>(Shards->Shards.size()), Shards->StepsLeft);
			for (const FBattleShard& Shard : Shards->Shards)
			{
				Out.I16(Shard.Rotation[0], Shard.Rotation[1], Shard.Rotation[2], Shard.Spin[0], Shard.Spin[1], Shard.Spin[2]);
				Out.I32(Shard.Place[0], Shard.Place[1], Shard.Place[2], Shard.Velocity[0], Shard.Velocity[1], Shard.Velocity[2], Shard.Fall);
				for (const FGteVector& Corner : Shard.Corners)
				{
					Out.I16(Corner[0], Corner[1], Corner[2]);
				}
			}
			return true;
		}
		if (const FBattleScroll* Scroll = Extensions->Scroll(Task))
		{
			Out.U8(13, Scroll->Module);
			Out.I16(IndexOf(Scroll->Sprite.Sprite));
			Out.I32(Scroll->Scroll[0], Scroll->Scroll[1], Scroll->Scroll[2]);
			if (Scroll->Module == 5)
			{
				Out.I32(Scroll->Held[0], Scroll->Held[1], Scroll->Held[2]);
			}
			return true;
		}
		return false;
	}

	int32 FBattleSpriteDigester::IndexOf(const FSprite* Sprite) const
	{
		if (Sprite == nullptr)
		{
			return NoSprite;
		}
		int32 Position = 0;
		for (const FKernelTaskEntry& Entry : Sprites->Tasks())
		{
			if (Entry.Sprite && &Entry.Sprite->Sprite == Sprite)
			{
				return Position;
			}
			++Position;
		}
		return NoSprite;
	}

	int32 FBattleSpriteDigester::IndexOfTask(const FSpriteTask* Task) const
	{
		if (Task == nullptr)
		{
			return NoSprite;
		}
		int32 Position = 0;
		for (const FKernelTaskEntry& Entry : Sprites->Tasks())
		{
			if (Entry.Sprite.get() == Task)
			{
				return Position;
			}
			++Position;
		}
		return Unlisted;
	}

	void FBattleSpriteDigester::DigestPlace(FOracleDigest& Out, const FSpriteFile* File, uint32 Offset) const
	{
		const auto [Number, At] = File == nullptr ? std::pair<uint32, uint32>{NoFile, 0} : Place(File, Offset);
		Out.I32(Number, At);
	}

	void FBattleSpriteDigester::DigestResource(FOracleDigest& Out, const FSpriteResource& Resource) const
	{
		if (Resource.File == nullptr)
		{
			Out.I32(NoFile, 0, NoFile, 0);
		}
		else
		{
			DigestPlace(Out, Resource.File, Resource.Tables.Frames);
			DigestPlace(Out, Resource.File, Resource.Tables.Animations);
		}
		Out.I16(Resource.TextureX, Resource.TextureY, Resource.ClutX, Resource.ClutY);
	}

	void FBattleSpriteDigester::DigestSprite(FOracleDigest& Out, const FSprite& Sprite) const
	{
		Out.I32(Sprite.Position[0], Sprite.Position[1], Sprite.Position[2], Sprite.Velocity[0], Sprite.Velocity[1], Sprite.Velocity[2], Sprite.Speed, Sprite.Gravity);
		Out.U8(Sprite.Red, Sprite.Green, Sprite.Blue, Sprite.Code);
		Out.I16(Sprite.Scale, Sprite.OtIndex, Sprite.OtBias, Sprite.Heading, Sprite.Frame, Sprite.FrameTop, Sprite.FrameHalfHeight, Sprite.SizeFactor);
		Out.I32(Sprite.DrawFlags, Sprite.BuildFlags);
		DigestPlace(Out, Sprite.CurrentFile, 0);
		DigestPlace(Out, Sprite.PrimaryFile, 0);
		DigestPlace(Out, Sprite.AlternateFile, 0);
		DigestPlace(Out, Sprite.ScriptFile, Sprite.List);
		DigestPlace(Out, Sprite.ScriptFile, Sprite.Header);
		DigestPlace(Out, Sprite.ByteTableFile, Sprite.ByteTable);
		if (Sprite.Pc == FSprite::Stopped)
		{
			Out.I32(NoFile, 0);
		}
		else
		{
			DigestPlace(Out, Sprite.ScriptFile, Sprite.Pc);
		}
		const bool bCommon = Sprite.Resource == &Battle->CommonEffects;
		const bool bEffect = Sprite.Resource == &Battle->EffectResource;
		Out.U8(bCommon ? 2 : bEffect ? 3 : 1);
		if (!bCommon && !bEffect)
		{
			DigestResource(Out, *Sprite.Resource);
		}
		Out.I16(IndexOf(Sprite.Parent), IndexOf(Sprite.Target));
		Out.I32(Sprite.SoundSet >= 0 ? static_cast<uint32>(Sprite.SoundSet) : 0xFFFFFFFFu);
		Out.I32(Sprite.FloorTriangle);
		Out.I16(Sprite.Facing, Sprite.Weight, Sprite.FloorY);
		Out.U8(Sprite.StackPointer, Sprite.ParentAnimation);
		for (const uint8 Byte : Sprite.Stack)
		{
			Out.U8(Byte);
		}
		Out.I16(Sprite.Wait, Sprite.SavedPoint[0], Sprite.SavedPoint[1], Sprite.SavedPoint[2]);
		Out.I32(Sprite.AnimationFlags, Sprite.TimeFlags, Sprite.NextWord);
		const bool bModel = Sprite.ModelFile != nullptr;
		if (Sprite.bHasTransform)
		{
			Out.I16(Sprite.Rotation[0], Sprite.Rotation[1], Sprite.Rotation[2], Sprite.TransformScale[0], Sprite.TransformScale[1], Sprite.TransformScale[2]);
			if (Sprite.Layout() == SpriteDrawFlags::LayoutParts)
			{
				Out.U8(Sprite.PivotX, Sprite.PivotY);
				if (bModel)
				{
					Out.U8(2);
					DigestPlace(Out, Sprite.ModelFile, Sprite.Model);
				}
				else if (Sprite.bHasAnchors)
				{
					Out.U8(1);
					for (const FSpriteAnchor& Anchor : Sprite.Anchors)
					{
						Out.U8(Anchor.OffsetX, Anchor.OffsetY);
						Out.I16(Anchor.RotationX, Anchor.RotationY, Anchor.RotationZ);
					}
				}
				else
				{
					Out.U8(0);
				}
			}
			else if (Sprite.Layout() == SpriteDrawFlags::LayoutTransform)
			{
				if (bModel)
				{
					DigestPlace(Out, Sprite.ModelFile, Sprite.Model);
				}
				else
				{
					Out.I32(NoFile, 0);
				}
				Out.I16(Sprite.TransformColor[0], Sprite.TransformColor[1], Sprite.TransformColor[2], Sprite.LightAngles[0], Sprite.LightAngles[1], Sprite.LightAngles[2],
					Sprite.LightColor[0], Sprite.LightColor[1], Sprite.LightColor[2]);
			}
		}
		if (Sprite.bHasTransform && Sprite.Layout() == SpriteDrawFlags::LayoutParts && !bModel)
		{
			for (int32 Index = 0; Index < Sprite.PartCount(); ++Index)
			{
				const FSpritePart& Part = Sprite.Parts[static_cast<std::size_t>(Index)];
				Out.I16(Part.X, Part.Y);
				Out.U8(Part.U, Part.V, Part.Width, Part.Height, Part.GrowWidth, Part.GrowHeight);
				Out.I16(Part.TexturePage, Part.Clut);
				Out.I32(Part.ColorCode);
				Out.U8(Part.Flags & 0x3F);
			}
		}
	}
}
