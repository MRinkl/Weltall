#pragma once

#include "Foundation/Types.h"
#include "Graphics/SpriteFile.h"

#include <array>

namespace Weltall
{
	class FSprite;
	struct FSpriteTask;

	/** What a sprite's owner hears from its animations. */
	class ISpriteListener
	{
	public:
		virtual ~ISpriteListener() = default;

		/** An animation ended, held, restarted or took a notifying jump. */
		virtual void OnAnimationEvent(FSprite& Sprite) = 0;
	};

	/** An anchor slot: the offset and rotations applied to the parts of one anchor group. */
	struct FSpriteAnchor
	{
		uint8 OffsetX = 0;
		uint8 OffsetY = 0;
		int16 RotationX = 0;
		int16 RotationY = 0;
		/** About the view axis; 4096 is a turn. */
		int16 RotationZ = 0;
	};

	/** A part as the frame builder leaves it: one textured quad, placed from the sprite's origin. */
	struct FSpritePart
	{
		int16 X = 0;
		int16 Y = 0;
		uint8 U = 0;
		uint8 V = 0;
		uint8 Width = 0;
		uint8 Height = 0;
		/** Added to the quad's size: the texture is stretched over it. */
		uint8 GrowWidth = 0;
		uint8 GrowHeight = 0;
		uint16 TexturePage = 0;
		uint16 Clut = 0;
		/** The RGB colour, and the GPU code in the top byte (bit 25: semi-transparent). */
		uint32 ColorCode = 0;
		/** Bits 0-2 the anchor group, 3 8-bit texels, 4 flipped horizontally, 5 flipped vertically. */
		uint32 Flags = 0;
	};

	/** Where a sprite's cells and palettes are in VRAM, and which file it draws from through which tables. */
	struct FSpriteResource
	{
		const FSpriteFile* File = nullptr;
		/** The file's tables as they were when the sprite took the file. */
		FSpriteTables Tables;
		int16 TextureX = 0;
		int16 TextureY = 0;
		int16 ClutX = 0;
		int16 ClutY = 0;
	};

	/** A place in VRAM a resident sprite's cells take their texture page and origin from. */
	struct FSpriteTextureSlot
	{
		int16 X = 0;
		int16 Y = 0;
	};

	/** The block a sprite shares with the mode that owns it. */
	struct FSpriteExtra
	{
		static constexpr int32 MaxTextureSlots = 8;

		/** Used instead of the scripts' own vertical speed and gravity when set. */
		int32 PresetVerticalSpeed = 0;
		int32 PresetGravity = 0;
		/** Set by the scripts for the owner to read. */
		uint16 Signal = 0;
		/** The owner's number for the sprite (the field's actor). */
		int16 Owner = 0;
		/**
		 * When the owner gives it some, the places a resident sprite's cells take by their page column
		 * (the first the sprite's own texture place) instead of the columns beside the sprite's own.
		 */
		std::array<FSpriteTextureSlot, MaxTextureSlots> TextureSlots{};
		int32 TextureSlotCount = 0;
		/** The status marks a mode shows over the sprite (Battle's combatants, at +0xC): the battle's word of them when last refreshed. */
		uint16 StatusMarks = 0;
		/**
		 * The texture place a mode keeps for the sprite in the block (Battle's combatants, at +0xE), which
		 * the kernel puts back into the sprite's resource when it takes a file in battle mode.
		 */
		FSpriteTextureSlot ModeTexture;
	};

	/**
	 * How a battle model attached to a sprite had its packets built and last tinted, which its drawing
	 * shows: the kernel keeps the packets, built once with the sprite's blend and raw-texture flag and
	 * tinted again only when a script adds to the model's colour.
	 */
	struct FSpriteModelPackets
	{
		uint8 Blend = 0;
		bool bRawTexture = false;
		/** Added to the model's own colours (0 when built). */
		std::array<int16, 3> Tint{};
		/** Which build of a battle's packets these are: another for every attach, so a frame's drawing knows the packets it linked. */
		uint32 Build = 0;
	};

	/** Bits of FSprite::DrawFlags. */
	namespace SpriteDrawFlags
	{
		constexpr uint32 LayoutMask = 0x3;
		constexpr uint32 LayoutParts = 0x1;
		constexpr uint32 LayoutTransform = 0x2;
		constexpr uint32 Mirrored = 0x8;
		constexpr uint32 FlippedVertically = 0x10;
		constexpr uint32 BlendShift = 5;
		constexpr uint32 BlendMask = 0x7u << BlendShift;
		constexpr uint32 HiddenGroupsShift = 8;
		constexpr uint32 PaletteSetShift = 20;
		constexpr uint32 SimplePhysics = 0x4000000;
		constexpr uint32 MatrixDirty = 0x10000000;
		constexpr uint32 PaletteUploadPending = 0x40000000;
	}

	/** Bits of FSprite::BuildFlags. */
	namespace SpriteBuildFlags
	{
		/** The transform's matrix is built scale first (the rotation times the scale's diagonal) rather than rows scaled after. */
		constexpr uint32 ScaleFirst = 0x1;
		/** A battle model sprite is lit by its own light (its transform's angles and colour). */
		constexpr uint32 OwnLight = 0x2;
		constexpr uint32 PartCountShift = 2;
		constexpr uint32 PartCountMask = 0x3Fu << PartCountShift;
		constexpr uint32 SizeShiftShift = 8;
		constexpr uint32 SizeShiftMask = 0x1Fu << SizeShiftShift;
		constexpr uint32 KindShift = 13;
		constexpr uint32 KindMask = 0xFu << KindShift;
		constexpr uint32 Queued = 0x20000;
		constexpr uint32 Fog = 0x40000;
		constexpr uint32 KeepQueuedAnchors = 0x80000;
		constexpr uint32 NewAnimation = 0x100000;
	}

	/** Bits of FSprite::AnimationFlags. */
	namespace SpriteAnimationFlags
	{
		constexpr uint32 HasExtra = 0x1;
		constexpr uint32 BounceShift = 1;
		constexpr uint32 BounceMask = 0x3FFu << BounceShift;
		constexpr uint32 ListStepShift = 11;
		constexpr uint32 ListStepMask = 0x3Fu << ListStepShift;
		constexpr uint32 FacingSlotShift = 17;
		constexpr uint32 FacingSlotMask = 0x7u << FacingSlotShift;
		constexpr uint32 DirectionModeShift = 20;
		constexpr uint32 DirectionModeMask = 0x3u << DirectionModeShift;
		constexpr uint32 StepsShift = 22;
		constexpr uint32 StepsMask = 0x3Fu << StepsShift;
		constexpr uint32 StateShift = 28;
		constexpr uint32 StateMask = 0x3u << StateShift;
		constexpr uint32 StateHeld = 1;
		constexpr uint32 StateRunning = 2;
	}

	/** Bits of FSprite::TimeFlags, whose top byte is the animation's number. */
	namespace SpriteTimeFlags
	{
		constexpr uint32 FacingMirrored = 0x4;
		constexpr uint32 ListEntryFlipped = 0x8;
		constexpr uint32 KillChildrenOnDestroy = 0x20;
		constexpr uint32 UpdateTwice = 0x40;
		constexpr uint32 TimeScaleShift = 7;
		constexpr uint32 TimeScaleMask = 0xFFFu << TimeScaleShift;
		constexpr uint32 AnimationShift = 24;
	}

	/** Bits of FSprite::NextWord, whose low byte is the animation to play after this one. */
	namespace SpriteNextFlags
	{
		constexpr uint32 KeepTaskMarks = 0x100;
		constexpr uint32 SizeFactorToChildren = 0x200;
		constexpr uint32 AlternateFile = 0x400;
		constexpr uint32 HasChildren = 0x800;
	}

	/**
	 * One of the kernel's animated sprites: a position and motion, the animation script that runs it,
	 * the facing direction that picks its frame lists, and the frame it shows, built into part records.
	 * The members are the kernel's sprite object, its transform, extra block and resource block; packed
	 * words stay whole where scripts and modes read and write them whole. The operations are
	 * FSpriteSystem's, which holds the kernel's state they share.
	 */
	class FSprite
	{
	public:
		static constexpr int32 AnchorCount = 8;
		static constexpr int32 StackSize = 16;
		static constexpr uint32 Stopped = 0;

		/** 16.16, y down. */
		std::array<int32, 3> Position{};
		std::array<int32, 3> Velocity{};
		int32 Speed = 0;
		int32 Gravity = 0;
		uint8 Red = 0;
		uint8 Green = 0;
		uint8 Blue = 0;
		/** The GPU code of its quads: bit 0 raw texture, bit 1 semi-transparent. */
		uint8 Code = 0;
		/** 4.12. */
		int16 Scale = 0;
		int16 OtIndex = 0;
		int16 OtBias = 0;
		/** The direction it moves in; 4096 is a turn. */
		int16 Heading = 0;
		uint16 Frame = 0;
		/** The frame's height above the origin and half its height, times the scale. */
		int16 FrameTop = 0;
		int16 FrameHalfHeight = 0;
		/** Scales motion and the matrix, 0 none. */
		uint16 SizeFactor = 0;
		uint32 DrawFlags = 0;
		uint32 BuildFlags = 0;

		const FSpriteFile* CurrentFile = nullptr;
		const FSpriteFile* PrimaryFile = nullptr;
		const FSpriteFile* AlternateFile = nullptr;
		/** Its own resource block, or its parent's for a spawned child. */
		FSpriteResource* Resource = nullptr;
		FSpriteResource OwnResource;

		/** The file the running animation's header, lists and script are in, and where in it. */
		const FSpriteFile* ScriptFile = nullptr;
		uint32 Header = 0;
		uint32 List = 0;
		uint32 SingleList = 0;
		/** Frames through the byte table: the primary file's, set at creation. */
		const FSpriteFile* ByteTableFile = nullptr;
		uint32 ByteTable = 0;
		/** The script's next instruction, Stopped when the script is over. */
		uint32 Pc = Stopped;
		/** Where bit-7 variables are, when a script has set it. */
		bool bHasVariables = false;
		uint32 Variables = 0;

		ISpriteListener* Listener = nullptr;
		/** The task that runs a spawned child; a sprite of a mode's own has none. */
		FSpriteTask* Task = nullptr;
		/**
		 * The sprite's own word for its task was cleared after the task was made (a battle event's object, reset once
		 * linked): the children it spawns are nobody's, and a script that reaches its task through it reads and writes
		 * the console's lowest memory instead.
		 */
		bool bTaskUnlinked = false;
		/** A spawned child's parent and its owner's target: sprites some instructions read. */
		FSprite* Parent = nullptr;
		FSprite* Target = nullptr;

		FSpriteExtra Extra;
		/**
		 * The sprite whose extra block this one reads instead of its own: a child spawned in battle mode
		 * takes its parent's block (the kernel copies the pointer). None for its own.
		 */
		const FSprite* ExtraOf = nullptr;
		/** The sound effect set its own sounds play from (Battle's: the set of the effect it runs), -1 none. */
		int32 SoundSet = -1;
		/** The floor triangle it stood on last, where Battle's floor searches for it start; a child starts from its parent's. */
		int32 FloorTriangle = 0;
		/** Relative to the view; picks the facing slot. */
		int16 Facing = 0;
		/** 0x1000 is 1: scales speeds and gravity. */
		int16 Weight = 0;
		/** The height it lands on. */
		int16 FloorY = 0;
		int8 StackPointer = StackSize;
		/** A child: the animation its parent played when it was spawned. */
		int8 ParentAnimation = 0;
		std::array<uint8, StackSize> Stack{};
		/** Animation ticks until the script's next step. */
		uint16 Wait = 0;
		std::array<int16, 3> SavedPoint{};
		uint32 AnimationFlags = 0;
		uint32 TimeFlags = 0;
		uint32 NextWord = 0;
		/**
		 * How many animations were started on it from outside its script (FSpriteSystem::SetAnimation), not counting the
		 * script's own loops and chains: a display that follows the sprite sees each start, the same animation's again included.
		 */
		uint32 Starts = 0;

		/** The transform: rotation and scale (4096 = 1), 4096 a turn. Sprites without one only move. */
		bool bHasTransform = false;
		std::array<int16, 3> Rotation{};
		std::array<int16, 3> TransformScale{};
		/** A transform-only sprite's colour (a battle model's), which SetColor and AddColor change. */
		std::array<int16, 3> TransformColor{};
		/** A battle model sprite's own light, when it is lit: the light's angles and colour (its transform's 44h and 4Ch). */
		std::array<int16, 3> LightAngles{};
		std::array<int16, 3> LightColor{};
		/** Whether its anchor slots exist; a child gets them with its first anchor. */
		bool bHasAnchors = false;
		std::array<FSpriteAnchor, AnchorCount> Anchors{};
		int8 PivotX = 0;
		int8 PivotY = 0;
		/** A model part attached by its script (its offset in the script's file), in place of anchor slots. */
		const FSpriteFile* ModelFile = nullptr;
		uint32 Model = 0;
		/** A battle model's packets, when one is attached. */
		FSpriteModelPackets ModelPackets;
		/** The texture page the model's packets were built on, when a script set one. */
		bool bModelTextureOverride = false;
		int16 ModelTextureX = 0;
		int16 ModelTextureY = 0;

		/** The part records, as many as its frames can have; the built frame's are the first PartCount(). */
		std::array<FSpritePart, 0x3F> Parts{};
		int32 PartCapacity = 0;

		int32 Layout() const { return static_cast<int32>(DrawFlags & SpriteDrawFlags::LayoutMask); }
		int32 PartCount() const { return static_cast<int32>((BuildFlags & SpriteBuildFlags::PartCountMask) >> SpriteBuildFlags::PartCountShift); }
		int32 SizeShift() const { return static_cast<int32>((BuildFlags & SpriteBuildFlags::SizeShiftMask) >> SpriteBuildFlags::SizeShiftShift); }
		int32 Kind() const { return static_cast<int32>((BuildFlags & SpriteBuildFlags::KindMask) >> SpriteBuildFlags::KindShift); }
		int32 Blend() const { return static_cast<int32>((DrawFlags & SpriteDrawFlags::BlendMask) >> SpriteDrawFlags::BlendShift); }
		int32 PaletteSet() const { return static_cast<int32>(DrawFlags >> SpriteDrawFlags::PaletteSetShift & 0xF); }
		uint32 TimeScale() const { return (TimeFlags & SpriteTimeFlags::TimeScaleMask) >> SpriteTimeFlags::TimeScaleShift; }
		int8 Animation() const { return static_cast<int8>(TimeFlags >> SpriteTimeFlags::AnimationShift); }
		/** Writes the animation's number without playing it. */
		void SetAnimationNumber(int32 Animation)
		{
			TimeFlags = (TimeFlags & ~(0xFFu << SpriteTimeFlags::AnimationShift)) | (static_cast<uint32>(Animation) & 0xFF) << SpriteTimeFlags::AnimationShift;
		}
		int8 NextAnimation() const { return static_cast<int8>(NextWord); }
		uint32 ListStep() const { return (AnimationFlags & SpriteAnimationFlags::ListStepMask) >> SpriteAnimationFlags::ListStepShift; }
		uint32 FacingSlot() const { return (AnimationFlags & SpriteAnimationFlags::FacingSlotMask) >> SpriteAnimationFlags::FacingSlotShift; }
		uint32 DirectionMode() const { return (AnimationFlags & SpriteAnimationFlags::DirectionModeMask) >> SpriteAnimationFlags::DirectionModeShift; }
		uint32 StepsRun() const { return (AnimationFlags & SpriteAnimationFlags::StepsMask) >> SpriteAnimationFlags::StepsShift; }
		uint32 State() const { return (AnimationFlags & SpriteAnimationFlags::StateMask) >> SpriteAnimationFlags::StateShift; }
		bool IsMirroredFacing() const { return (TimeFlags & SpriteTimeFlags::FacingMirrored) != 0; }

		/** How slowly its animation runs, 0x100 = 1.0: every wait is scaled by Rate / 256. */
		void SetTimeScale(int32 Rate)
		{
			TimeFlags = (TimeFlags & ~SpriteTimeFlags::TimeScaleMask) | (static_cast<uint32>(Rate) & 0xFFF) << SpriteTimeFlags::TimeScaleShift;
		}
		bool IsQueued() const { return (BuildFlags & SpriteBuildFlags::Queued) != 0; }
		/** The extra block it reads: its own, or the one it shares (ExtraOf). */
		const FSpriteExtra& ExtraBlock() const { return ExtraOf != nullptr ? ExtraOf->Extra : Extra; }
	};
}
