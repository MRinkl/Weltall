#include "Graphics/SpriteSystem.h"

#include "Game/PsxRandom.h"
#include "Graphics/Gte.h"
#include "Graphics/SpriteFile.h"
#include "SpriteRules.h"

#include <algorithm>
#include <array>
#include <string>
#include <type_traits>

namespace Weltall
{
	using namespace SpriteRules;

	namespace
	{
		constexpr uint8 DefaultCode = 0x2D;
		constexpr int16 DefaultScale = 0x1000;
		constexpr uint32 DefaultTimeScale = 0x100;
		constexpr uint32 HeaderKeepVelocity = 0x800;
		constexpr uint32 HeaderKeepRotation = 0x1000;
		constexpr uint32 HeaderKeepScale = 0x2000;

		/** A register's absolute value: the negation wraps, as the CPU's does. */
		int32 WrappedAbs(int32 Value)
		{
			return Value < 0 ? static_cast<int32>(0u - static_cast<uint32>(Value)) : Value;
		}

		/** The frame list of facing direction Index, as the header's offsets point to them. */
		uint32 DirectionList(const FSprite& Sprite, uint32 Index)
		{
			const uint32 At = Sprite.Header + 4 + Index * 2;
			return At + Sprite.ScriptFile->Word(At);
		}

		void SetBit(uint32& Word, uint32 Bit, bool bSet)
		{
			Word = bSet ? Word | Bit : Word & ~Bit;
		}

		/** The mirrored bit the sprite is drawn with: the facing's mirror against the list entry's flip. */
		void ApplyMirror(FSprite& Sprite)
		{
			const bool bFacing = (Sprite.TimeFlags & SpriteTimeFlags::FacingMirrored) != 0;
			const bool bEntry = (Sprite.TimeFlags & SpriteTimeFlags::ListEntryFlipped) != 0;
			SetBit(Sprite.DrawFlags, SpriteDrawFlags::Mirrored, bFacing != bEntry);
		}
	}

	void FSpriteSystem::Create(FSprite& Sprite, const FSpriteFile& File, int32 ClutX, int32 ClutY, int32 TextureX, int32 TextureY, int32 PaletteSet)
	{
		Sprite = FSprite{};
		Sprite.Code = DefaultCode;
		Sprite.TimeFlags = DefaultTimeScale << SpriteTimeFlags::TimeScaleShift;
		const int32 Ticks = TickExtraFrames + 1;
		Sprite.Gravity = DivideByPower(int64{Ticks} * Ticks * 0x4000 * Sprite.Weight, 12);
		Sprite.Resource = &Sprite.OwnResource;
		Sprite.bHasTransform = true;
		Sprite.bHasAnchors = true;
		SetScale(Sprite, DefaultScale);
		Sprite.DrawFlags = (Sprite.DrawFlags & ~SpriteDrawFlags::LayoutMask) | SpriteDrawFlags::LayoutParts;
		// Battle keeps a block of its own in the extra block's place, which the kernel's instructions leave alone.
		if (BattleMode == nullptr)
		{
			Sprite.AnimationFlags |= SpriteAnimationFlags::HasExtra;
		}
		Sprite.DrawFlags = (Sprite.DrawFlags & 0xFF00FFFF) | static_cast<uint32>(PaletteSet & 0xF) << 20 | static_cast<uint32>(PaletteSet & 0xF) << 16;
		Sprite.PartCapacity = File.MaxParts(File.Tables());
		Sprite.OwnResource.TextureX = static_cast<int16>(TextureX);
		Sprite.OwnResource.TextureY = static_cast<int16>(TextureY);
		Sprite.OwnResource.ClutX = static_cast<int16>(ClutX);
		Sprite.OwnResource.ClutY = static_cast<int16>(ClutY);
		Sprite.PrimaryFile = &File;
		SetResource(Sprite, &File);
		Sprite.ByteTableFile = &File;
		Sprite.ByteTable = File.ByteTable(Sprite.Resource->Tables);
		SetAnimation(Sprite, 0);
	}

	void FSpriteSystem::SetPartCapacity(FSprite& Sprite, int32 Count)
	{
		Sprite.PartCapacity = Count;
	}

	void FSpriteSystem::SetSizeShift(FSprite& Sprite, int32 Shift)
	{
		Sprite.BuildFlags = (Sprite.BuildFlags & ~SpriteBuildFlags::SizeShiftMask) | (static_cast<uint32>(Shift) & 0x1F) << SpriteBuildFlags::SizeShiftShift;
	}

	FSpriteFrameSize FSpriteSystem::FirstFrameSize(const FSprite& Sprite) const
	{
		const FSpriteFile& File = *Sprite.Resource->File;
		const FSpriteTables& Tables = Sprite.Resource->Tables;
		const uint32 Header = File.AnimationHeader(Tables, 0);
		// The low byte of the first list entry; the kernel's bounds test compares the wrong word and never refuses a frame.
		uint32 Frame = File.Byte(Header + 4 + File.Word(Header + 4));
		uint32 Tested = 0;
		if (Frame != 0)
		{
			--Frame;
			Tested = Frame * 2;
		}
		const uint32 Index = File.Word(Tables.Frames + Tested) < Frame ? 0 : Frame * 2;
		const uint32 At = Tables.Frames + File.Word(Tables.Frames + Index + 2);
		return {DivideByPower(int64{File.Byte(At + 2)} * Sprite.Scale, 12), DivideByPower(int64{File.Byte(At + 3)} * Sprite.Scale, 12),
			DivideByPower(int64{File.Byte(At + 1)} * Sprite.Scale, 12)};
	}

	void FSpriteSystem::Free(FSprite& Sprite)
	{
		Unqueue(Sprite);
	}

	void FSpriteSystem::AllocTextureSlots(FSprite& Sprite, int32 Count)
	{
		Sprite.Extra.TextureSlots = {};
		Sprite.Extra.TextureSlotCount = Count;
		Sprite.Extra.TextureSlots[0] = {Sprite.Resource->TextureX, Sprite.Resource->TextureY};
	}

	void FSpriteSystem::SetResource(FSprite& Sprite, const FSpriteFile* File)
	{
		if (File == nullptr)
		{
			return;
		}
		if (File != Sprite.CurrentFile)
		{
			FSpriteResource& Resource = *Sprite.Resource;
			ParseResource(Resource, *File, Resource.TextureX, Resource.TextureY, Resource.ClutX, Resource.ClutY);
			Sprite.CurrentFile = File;
			Sprite.DrawFlags |= SpriteDrawFlags::PaletteUploadPending;
		}
		if (BattleMode != nullptr)
		{
			ApplyBattleTexture(Sprite, Sprite.Resource->File->IsResident(Sprite.Resource->Tables));
		}
	}

	void FSpriteSystem::ParseResource(FSpriteResource& Resource, const FSpriteFile& File, int16 TextureX, int16 TextureY, int16 ClutX, int16 ClutY)
	{
		Resource.File = &File;
		Resource.Tables = File.Tables();
		Resource.TextureX = TextureX;
		Resource.TextureY = TextureY;
		Resource.ClutX = ClutX;
		Resource.ClutY = ClutY;
		if (BattleMode != nullptr)
		{
			// The animations table's first word names the extension module the file's animations need.
			const int32 Extension = File.Word(Resource.Tables.Animations) >> 6 & 0x3F;
			if (Extension != 0)
			{
				RequestedExtension = Extension;
			}
		}
	}

	void FSpriteSystem::ApplyBattleTexture(FSprite& Sprite, bool bFromBlock)
	{
		if (!bFromBlock)
		{
			Sprite.Resource->TextureX = BattleStreamX;
			Sprite.Resource->TextureY = BattleStreamY;
			return;
		}
		const FSpriteTextureSlot& Place = Sprite.ExtraBlock().ModeTexture;
		Sprite.Resource->TextureX = Place.X;
		Sprite.Resource->TextureY = Place.Y;
	}

	void FSpriteSystem::SetAnimation(FSprite& Sprite, int32 Animation)
	{
		++Sprite.Starts;
		PlayAnimation(Sprite, Animation);
	}

	void FSpriteSystem::PlayAnimation(FSprite& Sprite, int32 Animation)
	{
		if (Sprite.PrimaryFile == nullptr)
		{
			Sprite.Pc = FSprite::Stopped;
			return;
		}
		SetBit(Sprite.NextWord, SpriteNextFlags::AlternateFile, Sprite.CurrentFile != Sprite.PrimaryFile);
		SetResource(Sprite, Animation < 0 ? Sprite.AlternateFile : Sprite.PrimaryFile);
		if (BattleMode != nullptr)
		{
			// The primary file's animations take the sprite's own place; the alternate file's stream
			// theirs to the battle's place unless they are resident.
			if (Animation >= 0)
			{
				ApplyBattleTexture(Sprite, true);
			}
			else if (Sprite.Resource->File != nullptr && !Sprite.Resource->File->IsResident(Sprite.Resource->Tables))
			{
				ApplyBattleTexture(Sprite, false);
			}
		}
		Sprite.TimeFlags = (Sprite.TimeFlags & 0x00FFFFFF) | static_cast<uint32>(Animation & 0xFF) << SpriteTimeFlags::AnimationShift;
		const int32 Index = Animation < 0 ? ~Animation : Animation;
		const FSpriteFile& File = *Sprite.Resource->File;
		const FSpriteTables& Tables = Sprite.Resource->Tables;
		// The kernel does not test the table's count: an animation past it takes the bytes after the
		// offsets as its offset, which is how a sprite without a ladder's animation climbs one.
		const uint32 Entry = Tables.Animations + 2 + 2 * static_cast<uint32>(Index);
		const uint32 Header = File.AnimationHeader(Tables, Index);
		if (Entry + 2 > File.Size() || Header + 4 > File.Size())
		{
			Report(ESpriteFault::PcOutsideFile, Entry + 2 > File.Size() ? Entry : Header);
			Sprite.Pc = FSprite::Stopped;
			return;
		}
		Sprite.BuildFlags |= SpriteBuildFlags::NewAnimation;
		Start(Sprite, File, Header);
		SetFacing(Sprite, Sprite.Facing);
	}

	void FSpriteSystem::Start(FSprite& Sprite, const FSpriteFile& File, uint32 Header)
	{
		Sprite.ScriptFile = &File;
		Sprite.Header = Header;
		const uint32 Flags = File.Word(Header);
		Sprite.Pc = Header + 2 + File.Word(Header + 2);
		Sprite.AnimationFlags = (Sprite.AnimationFlags & 0xFFCFFFFF) | (Flags & 3) << SpriteAnimationFlags::DirectionModeShift;
		Sprite.List = DirectionList(Sprite, 0);

		// The header's gravity, in the mode's ticks and the sprite's weight and time scale.
		int32 Gravity = static_cast<int32>(Flags >> 2 & 0x3F);
		if ((Gravity & 0x20) != 0)
		{
			Gravity |= ~0x3F;
		}
		const int32 Ticks = TickExtraFrames + 1;
		Gravity = Multiply(int64{Gravity} * 0x400, DivideByPower(int64{Ticks} * Ticks * Sprite.Weight, 12));
		if (Sprite.TimeScale() == 0)
		{
			Report(ESpriteFault::UnsupportedInstruction, 0);
			return;
		}
		const uint32 PerTick = 0x10000u / Sprite.TimeScale();
		Gravity = Multiply(Gravity, DivideByPower(int64{PerTick} * PerTick, 8));
		Sprite.Gravity = DivideByPower(Gravity, 8);

		if ((Flags & HeaderKeepVelocity) == 0)
		{
			Sprite.Velocity = {};
			Sprite.Speed = 0;
		}
		if (Sprite.bHasTransform)
		{
			if ((Flags & HeaderKeepRotation) == 0)
			{
				Sprite.Rotation = {};
			}
			if (BattleMode != nullptr && (Flags & HeaderKeepScale) == 0)
			{
				SetScale(Sprite, AnimationScale);
			}
			if (Sprite.Layout() == SpriteDrawFlags::LayoutParts)
			{
				Sprite.PivotX = 0;
				Sprite.PivotY = 0;
				if ((Sprite.BuildFlags & SpriteBuildFlags::NewAnimation) == 0 && Sprite.bHasAnchors)
				{
					Sprite.Anchors = {};
				}
			}
		}
		Sprite.StackPointer = FSprite::StackSize;
		Sprite.OtBias = 0;
		Sprite.Wait = 1;
		Sprite.AnimationFlags = (Sprite.AnimationFlags & 0xC03FF801) | 0x2001F800;
		if ((Sprite.AnimationFlags & SpriteAnimationFlags::HasExtra) != 0)
		{
			Sprite.Extra.PresetVerticalSpeed = 0;
			Sprite.Extra.PresetGravity = 0;
			Sprite.Extra.Signal = 0;
		}
	}

	void FSpriteSystem::SetFacing(FSprite& Sprite, int32 Angle)
	{
		const int32 Facing = static_cast<int16>(Angle);
		Sprite.Facing = static_cast<int16>(Facing);
		const uint32 OldSlot = Sprite.FacingSlot();
		// The kernel tests bit 0 here; every direction mode but the unused fourth sets the bit again below.
		SetBit(Sprite.TimeFlags, SpriteTimeFlags::FacingMirrored, ((Facing + 0x400) & 1) != 0);
		if (Sprite.PrimaryFile == nullptr)
		{
			return;
		}
		const uint32 Mode = Sprite.DirectionMode();
		bool bSetSlot = true;
		uint32 Slot = 0;
		if (Mode == 1)
		{
			Slot = static_cast<uint32>((Facing + 0x600) >> 10) & 3;
			const bool bMirror = Slot == 3;
			SetBit(Sprite.TimeFlags, SpriteTimeFlags::FacingMirrored, bMirror);
			Sprite.List = DirectionList(Sprite, bMirror ? 1 : Slot);
		}
		else if (Mode == 2)
		{
			Slot = static_cast<uint32>((Facing + 0x500) >> 9) & 7;
			const bool bMirror = Slot > 4;
			if (bMirror)
			{
				Slot = (Slot - 5) ^ 3;
			}
			SetBit(Sprite.TimeFlags, SpriteTimeFlags::FacingMirrored, bMirror);
			Sprite.List = DirectionList(Sprite, Slot);
		}
		else
		{
			bSetSlot = false;
			if (Mode == 0)
			{
				SetBit(Sprite.TimeFlags, SpriteTimeFlags::FacingMirrored, ((Angle + 0x400) & 0xFFF) > 0x800);
				Sprite.AnimationFlags &= ~SpriteAnimationFlags::FacingSlotMask;
				Sprite.SingleList = Sprite.Header + 6;
				Sprite.List = DirectionList(Sprite, 0);
			}
		}
		if (bSetSlot)
		{
			Sprite.AnimationFlags = (Sprite.AnimationFlags & ~SpriteAnimationFlags::FacingSlotMask) | (Slot & 7) << SpriteAnimationFlags::FacingSlotShift;
		}
		if (Sprite.FacingSlot() != OldSlot)
		{
			// Replays the script from its start to where it was, so the new direction shows the same moment.
			const uint16 Wait = Sprite.Wait;
			const uint32 Steps = Sprite.StepsRun();
			const uint32 Pc = Sprite.Pc;
			Sprite.AnimationFlags = (Sprite.AnimationFlags & 0xF03FFFFF) | 0x1F800;
			Sprite.Pc = Sprite.Header + 2 + Sprite.ScriptFile->Word(Sprite.Header + 2);
			Seek(Sprite, Pc, Steps);
			Sprite.Wait = Wait;
		}
		ApplyMirror(Sprite);
	}

	void FSpriteSystem::Seek(FSprite& Sprite, uint32 Pc, uint32 Steps)
	{
		const FSpriteFile& File = *Sprite.ScriptFile;
		int32 Wait = 0;
		for (;;)
		{
			const uint32 At = Sprite.Pc;
			if (At == Pc && Sprite.StepsRun() == Steps)
			{
				return;
			}
			if (At >= File.Size())
			{
				Report(ESpriteFault::PcOutsideFile, At);
				return;
			}
			const uint8 Opcode = File.Byte(At);
			if (Opcode < 0x80)
			{
				Sprite.Pc = At + 1;
				if (Opcode < 0x10)
				{
					SetFrame(Sprite, Sprite.Frame + 1);
				}
				else if (Opcode < 0x20)
				{
					Sprite.AnimationFlags = (Sprite.AnimationFlags & ~SpriteAnimationFlags::ListStepMask) | ((Sprite.ListStep() + 1) & 0x3F) << SpriteAnimationFlags::ListStepShift;
					ApplyDirectionFrame(Sprite);
				}
				else if (Opcode < 0x30)
				{
					SetFrame(Sprite, Sprite.Frame - 1);
				}
				if (Opcode < 0x40)
				{
					Wait = (Opcode & 0xF) + 1;
				}
				Sprite.Wait = static_cast<uint16>(Sprite.Wait + Wait);
				CountStep(Sprite);
				continue;
			}
			switch (Opcode)
			{
			case 0x80:
			case 0x81:
			case 0x82:
				return;
			case 0x86:
			case 0x87:
			case 0x97:
				if (At == Pc)
				{
					return;
				}
				break;
			case 0xB3:
				Sprite.AnimationFlags = (Sprite.AnimationFlags & ~SpriteAnimationFlags::ListStepMask) | (static_cast<uint32>(static_cast<int8>(File.Byte(At + 1))) & 0x3F) << SpriteAnimationFlags::ListStepShift;
				break;
			case 0xBE:
			{
				const uint16 Word = File.Word(At + 1);
				SetBit(Sprite.TimeFlags, SpriteTimeFlags::ListEntryFlipped, (Word & 0x200) != 0);
				ApplyMirror(Sprite);
				if (Sprite.Frame != (Word & 0x1FF))
				{
					SetFrame(Sprite, Word & 0x1FF);
				}
				Sprite.Wait = static_cast<uint16>(Sprite.Wait + 1 + (Word >> 11 & 0xF));
				break;
			}
			case 0xE2:
				if (!PushAddress(Sprite, At + 3))
				{
					return;
				}
				Sprite.Pc = At + static_cast<uint32>(static_cast<int16>(File.Word(At + 1)));
				continue;
			default:
				break;
			}
			// The kernel steps over SetFrame by its length table's two bytes, one short of the instruction.
			Sprite.Pc = At + TableLength(Opcode);
		}
	}

	void FSpriteSystem::ApplyDirectionFrame(FSprite& Sprite)
	{
		const uint32 At = Sprite.List + Sprite.ListStep() * 2;
		if (At + 1 >= Sprite.ScriptFile->Size())
		{
			Report(ESpriteFault::ListOutsideFile, At);
			return;
		}
		const uint16 Entry = Sprite.ScriptFile->Word(At);
		SetBit(Sprite.TimeFlags, SpriteTimeFlags::ListEntryFlipped, (Entry & 0x200) != 0);
		ApplyMirror(Sprite);
		SetFrame(Sprite, Entry & 0x1FF);
	}

	void FSpriteSystem::SetFrame(FSprite& Sprite, int32 Frame)
	{
		if (Sprite.Layout() != SpriteDrawFlags::LayoutParts)
		{
			Sprite.Frame = 0;
			return;
		}
		if ((Sprite.BuildFlags & SpriteBuildFlags::NewAnimation) != 0)
		{
			Sprite.BuildFlags &= ~SpriteBuildFlags::NewAnimation;
			if (Sprite.bHasAnchors)
			{
				Sprite.Anchors = {};
			}
		}
		if (Sprite.IsQueued() && std::find(Pending.begin(), Pending.end(), &Sprite) != Pending.end())
		{
			// A frame replaced before it was built still leaves its anchors.
			if ((Sprite.BuildFlags & SpriteBuildFlags::KeepQueuedAnchors) == 0)
			{
				ReadAnchors(Sprite, Sprite.Frame);
			}
			Sprite.Frame = static_cast<uint16>(Frame);
			return;
		}
		Sprite.Frame = static_cast<uint16>(Frame);
		Pending.push_back(&Sprite);
		Sprite.BuildFlags |= SpriteBuildFlags::Queued;
	}

	void FSpriteSystem::Unqueue(FSprite& Sprite)
	{
		Pending.erase(std::remove(Pending.begin(), Pending.end(), &Sprite), Pending.end());
	}

	void FSpriteSystem::SetDirection(FSprite& Sprite, int32 Direction)
	{
		Sprite.Heading = static_cast<int16>(Direction);
		UpdateVelocity(Sprite);
	}

	void FSpriteSystem::UpdateVelocity(FSprite& Sprite)
	{
		const int32 TimeScale = static_cast<int32>(Sprite.TimeScale());
		if (TimeScale == 0)
		{
			Report(ESpriteFault::UnsupportedInstruction, 0);
			return;
		}
		const int32 Speed = Multiply(Sprite.Speed >> 4, 0x100) / TimeScale;
		Sprite.Velocity[0] = Multiply(GteCos(Sprite.Heading) >> 2, Speed) >> 6;
		Sprite.Velocity[2] = Wrap(-int64{Multiply(GteSin(Sprite.Heading) >> 2, Speed)}) >> 6;
	}

	void FSpriteSystem::SetScale(FSprite& Sprite, int32 Scale)
	{
		if (!Sprite.bHasTransform)
		{
			return;
		}
		Sprite.Scale = static_cast<int16>(Scale);
		Sprite.TransformScale = {Sprite.Scale, Sprite.Scale, Sprite.Scale};
		Sprite.DrawFlags |= SpriteDrawFlags::MatrixDirty;
	}

	void FSpriteSystem::SetTint(FSprite& Sprite, int32 Red, int32 Green, int32 Blue)
	{
		Sprite.Red = static_cast<uint8>(Red);
		Sprite.Green = static_cast<uint8>(Green);
		Sprite.Blue = static_cast<uint8>(Blue);
		Sprite.Code &= 0xFE;
		ApplyColorBlend(Sprite);
	}

	void FSpriteSystem::SetBlend(FSprite& Sprite, int32 Mode)
	{
		const uint32 Blend = static_cast<uint32>(Mode) & 7;
		Sprite.DrawFlags = (Sprite.DrawFlags & ~SpriteDrawFlags::BlendMask) | Blend << SpriteDrawFlags::BlendShift;
		Sprite.Code = Blend == 0 ? Sprite.Code & 0xFD : Sprite.Code | 2;
		if (static_cast<uint32>(Sprite.Kind()) - 8 < 2)
		{
			if (Blend != 0)
			{
				Sprite.DrawFlags = (Sprite.DrawFlags & ~SpriteDrawFlags::BlendMask) | ((Blend - 1) & 7) << SpriteDrawFlags::BlendShift;
			}
			return;
		}
		ApplyColorBlend(Sprite);
	}

	void FSpriteSystem::TickAnimation(FSprite& Sprite)
	{
		for (int32 Frame = 0; Frame <= TickExtraFrames; ++Frame)
		{
			const uint16 Wait = Sprite.Wait;
			if (Wait != 0)
			{
				Sprite.Wait = static_cast<uint16>(Wait - 1);
				if (Wait == 1)
				{
					Step(Sprite);
				}
			}
		}
	}

	void FSpriteSystem::Move(FSprite& Sprite)
	{
		Sprite.Position[0] = Wrap(Multiply(ScaleBySize(Sprite, Sprite.Velocity[0] >> 4), 16) + int64{Sprite.Position[0]});
		Sprite.Position[2] = Wrap(Multiply(ScaleBySize(Sprite, Sprite.Velocity[2] >> 4), 16) + int64{Sprite.Position[2]});
		ApplyGravity(Sprite);
	}

	void FSpriteSystem::ApplyGravity(FSprite& Sprite)
	{
		if ((Sprite.DrawFlags & SpriteDrawFlags::SimplePhysics) != 0)
		{
			Sprite.Position[1] = Wrap(Multiply(ScaleBySize(Sprite, Sprite.Velocity[1] >> 4), 16) + int64{Sprite.Position[1]});
			Sprite.Velocity[1] = Wrap(int64{Sprite.Velocity[1]} + Sprite.Gravity);
			return;
		}
		if (BattleMode == nullptr)
		{
			// Landing and bouncing start with a routine of the battle program's.
			Report(ESpriteFault::UnsupportedInstruction, 0xC5);
			return;
		}
		// Down onto the floor: it lands on it, and bounces back by its bounce (256ths) while the bounce
		// is still more than the gravity, else it stops.
		BattleMode->SnapToFloor(Sprite);
		const int32 Fall = ScaleBySize(Sprite, Sprite.Velocity[1] >> 4);
		if (Sprite.Velocity[1] < 1 || Sprite.Gravity < 1)
		{
			Sprite.Position[1] = Wrap(int64{Sprite.Position[1]} + Multiply(Fall, 16));
			if (Sprite.FloorY <= Sprite.Position[1] >> 16)
			{
				Sprite.Position[1] = Multiply(Sprite.FloorY, 0x10000);
			}
		}
		else
		{
			if (static_cast<int16>(Sprite.Position[1] >> 16) == Sprite.FloorY)
			{
				return;
			}
			Sprite.Position[1] = Wrap(int64{Sprite.Position[1]} + Multiply(Fall, 16));
			if (Sprite.FloorY <= Sprite.Position[1] >> 16)
			{
				const int32 Bounce = static_cast<int32>((Sprite.AnimationFlags & SpriteAnimationFlags::BounceMask) >> SpriteAnimationFlags::BounceShift);
				Sprite.Position[1] = Multiply(Sprite.FloorY, 0x10000);
				const int32 Back = DivideByPower(Multiply(-int64{Sprite.Velocity[1]}, Bounce), 8);
				Sprite.Velocity[1] = Back;
				if (WrappedAbs(Sprite.Gravity) <= WrappedAbs(Back))
				{
					return;
				}
				Sprite.Velocity[1] = 0;
				return;
			}
		}
		Sprite.Velocity[1] = Wrap(int64{Sprite.Velocity[1]} + Sprite.Gravity);
	}

	namespace
	{
		/** The bytes the kernel's stack pointer reaches beside the stack: 2 before it, 8 after it. */
		constexpr int32 BytesBefore = 2;
		constexpr int32 BytesAfter = 8;

		/** A byte of a 16-bit word, little-endian. */
		uint8 WordByte(uint16 Word, int32 Byte)
		{
			return static_cast<uint8>(Word >> (8 * Byte));
		}

		void SetWordByte(auto& Word, int32 Byte, uint8 Value)
		{
			const uint16 Old = static_cast<uint16>(Word);
			const uint16 New = static_cast<uint16>(Byte == 0 ? (Old & 0xFF00) | Value : (Old & 0x00FF) | Value << 8);
			Word = static_cast<std::remove_reference_t<decltype(Word)>>(New);
		}
	}

	bool FSpriteSystem::ReadStackByte(FSprite& Sprite, int32 At, uint8& Value)
	{
		if (At < -BytesBefore || At >= FSprite::StackSize + BytesAfter)
		{
			Report(ESpriteFault::StackOutOfRange, static_cast<uint32>(At));
			return false;
		}
		if (At == -2)
		{
			Value = static_cast<uint8>(Sprite.StackPointer);
		}
		else if (At == -1)
		{
			Value = static_cast<uint8>(Sprite.ParentAnimation);
		}
		else if (At < FSprite::StackSize)
		{
			Value = Sprite.Stack[static_cast<std::size_t>(At)];
		}
		else if (At < FSprite::StackSize + 2)
		{
			Value = WordByte(Sprite.Wait, At - FSprite::StackSize);
		}
		else
		{
			const int32 Byte = At - FSprite::StackSize - 2;
			Value = WordByte(static_cast<uint16>(Sprite.SavedPoint[static_cast<std::size_t>(Byte / 2)]), Byte % 2);
		}
		return true;
	}

	bool FSpriteSystem::WriteStackByte(FSprite& Sprite, int32 At, uint8 Value)
	{
		if (At < -BytesBefore || At >= FSprite::StackSize + BytesAfter)
		{
			Report(ESpriteFault::StackOutOfRange, static_cast<uint32>(At));
			return false;
		}
		if (At == -2)
		{
			Sprite.StackPointer = static_cast<int8>(Value);
		}
		else if (At == -1)
		{
			Sprite.ParentAnimation = static_cast<int8>(Value);
		}
		else if (At < FSprite::StackSize)
		{
			Sprite.Stack[static_cast<std::size_t>(At)] = Value;
		}
		else if (At < FSprite::StackSize + 2)
		{
			SetWordByte(Sprite.Wait, At - FSprite::StackSize, Value);
		}
		else
		{
			const int32 Byte = At - FSprite::StackSize - 2;
			SetWordByte(Sprite.SavedPoint[static_cast<std::size_t>(Byte / 2)], Byte % 2, Value);
		}
		return true;
	}

	bool FSpriteSystem::PushByte(FSprite& Sprite, uint8 Value)
	{
		Sprite.StackPointer = static_cast<int8>(Sprite.StackPointer - 1);
		return WriteStackByte(Sprite, Sprite.StackPointer, Value);
	}

	bool FSpriteSystem::PopByte(FSprite& Sprite, uint8& Value)
	{
		const int32 At = Sprite.StackPointer;
		if (!ReadStackByte(Sprite, At, Value))
		{
			return false;
		}
		Sprite.StackPointer = static_cast<int8>(Sprite.StackPointer + 1);
		return true;
	}

	bool FSpriteSystem::PushAddress(FSprite& Sprite, uint32 Address)
	{
		Sprite.StackPointer = static_cast<int8>(Sprite.StackPointer - 3);
		// Each byte at the pointer as it reads it again: the first may have moved it.
		for (int32 Byte = 0; Byte < 3; ++Byte)
		{
			if (!WriteStackByte(Sprite, Sprite.StackPointer + Byte, static_cast<uint8>(Address >> (8 * Byte))))
			{
				return false;
			}
		}
		return true;
	}

	bool FSpriteSystem::PopAddress(FSprite& Sprite, uint32& Address)
	{
		const int32 At = Sprite.StackPointer;
		std::array<uint8, 3> Bytes{};
		for (int32 Byte = 0; Byte < 3; ++Byte)
		{
			if (!ReadStackByte(Sprite, At + Byte, Bytes[static_cast<std::size_t>(Byte)]))
			{
				return false;
			}
		}
		Address = Bytes[0] | Bytes[1] << 8 | static_cast<uint32>(Bytes[2]) << 16;
		Sprite.StackPointer = static_cast<int8>(Sprite.StackPointer + 3);
		return true;
	}

	void FSpriteSystem::Report(ESpriteFault Fault, uint32 Value)
	{
		if (FirstFault == ESpriteFault::None)
		{
			FirstFault = Fault;
			FirstFaultValue = Value;
		}
	}

	bool FSpriteSystem::ReachesTask(const FSprite& Sprite, uint32 Opcode)
	{
		if (Sprite.bTaskUnlinked)
		{
			Report(ESpriteFault::TaskUnlinked, Opcode);
			return false;
		}
		return true;
	}

	int32 FSpriteSystem::NextRandom()
	{
		return Random != nullptr ? Random->Next() : 0;
	}

	std::optional<FError> FSpriteSystem::FaultError() const
	{
		switch (FirstFault)
		{
		case ESpriteFault::None:
			return std::nullopt;
		case ESpriteFault::UnsupportedInstruction:
			return MakeError("sprite script: instruction " + std::to_string(FirstFaultValue) + " is not supported");
		case ESpriteFault::PcOutsideFile:
			return MakeError("sprite script: a read at " + std::to_string(FirstFaultValue) + " lies outside its file");
		case ESpriteFault::StackOutOfRange:
			return MakeError("sprite script: its stack goes past its 16 bytes");
		case ESpriteFault::NoVariables:
			return MakeError("sprite script: it changes its file's own data");
		case ESpriteFault::TooManyParts:
			return MakeError("sprite: a frame has more parts than the sprite has records for");
		case ESpriteFault::AnchorsHoldAModel:
			return MakeError("sprite: its anchors were replaced by a model");
		case ESpriteFault::TextureSlotOutsideTable:
			return MakeError("sprite: a cell takes texture slot " + std::to_string(FirstFaultValue) + ", past the sprite's slots");
		case ESpriteFault::SavedStepsNeverReached:
			return MakeError("sprite: its animation stops before the " + std::to_string(FirstFaultValue) + " steps it had run when it was saved");
		case ESpriteFault::TaskUnlinked:
			return MakeError("sprite script: instruction " + std::to_string(FirstFaultValue) + " reaches the task of a sprite that no longer names it");
		case ESpriteFault::ListOutsideFile:
			return MakeError("sprite: a frame is taken from a direction list at " + std::to_string(FirstFaultValue) + ", outside its file");
		}
		return std::nullopt;
	}
}
