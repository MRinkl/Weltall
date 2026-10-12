#include "Battle/BattleContext.h"
#include "Battle/Show/BattleObjectFiles.h"
#include "Battle/Show/BattleShow.h"
#include "Graphics/GteVectors.h"
#include "Graphics/SpriteFile.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** Where the effect's resource an extension gives puts its cells and palettes. */
		constexpr int16 ExtensionTextureX = 0x380;
		constexpr int16 ExtensionTextureY = 0x100;
		constexpr int16 ExtensionClutY = 0x1D0;
	}

	void FBattleShow::SpawnEffect(const FBattleBoneEffect& Effect)
	{
		if (!Battle.SpawnBoneEffect(Effect))
		{
			ReportFault(EBattleShowFault::Sprites);
		}
	}

	void FBattleShow::ObjectsSwapped(int32 First, int32 Second)
	{
		Battle.FollowSwappedObjects(First, Second);
	}

	bool FBattleShow::BonePlace(int32 Slot, int32 Bone, const FGteVector& Offset, bool bOnFloor, std::array<int32, 3>& Place)
	{
		const FBattleObject* Object = Objects.Object(Slot);
		if (Object == nullptr || Bone < 0 || static_cast<std::size_t>(Bone) >= Object->Bones.size())
		{
			return false;
		}
		const FGteMatrix Frame = Bone == 0 ? Object->Bones[0].Local : CompMatrix(Object->Bones[0].Local, Object->Bones[static_cast<std::size_t>(Bone)].World);
		const FGteLongVector Placed = RotTrans(Frame, Offset);
		Place = {Placed[0], bOnFloor ? int32{Object->FloorY} : Placed[1], Placed[2]};
		return true;
	}

	void FBattleShow::RequestEffect(int16 Effect)
	{
		Playback.RequestEffect(Effect);
	}

	bool FBattleShow::EffectLoaded()
	{
		return Playback.IsAskedEffectLoaded();
	}

	void FBattleShow::TakeEffectResource(const FBattleSequenceExtension& Extension)
	{
		// The extension's sprite resource becomes the effect's own, its palettes on row 1D0h; its scripts may write into it.
		const std::span<const uint8> Bytes(Extension.Pack.Bytes);
		TResult<FSpriteFile> Sprite = FSpriteFile::Parse(Bytes.subspan(Extension.Sprite.Offset, Extension.Sprite.End - Extension.Sprite.Offset));
		if (!Sprite)
		{
			ReportFault(EBattleShowFault::File);
			return;
		}
		ExtensionSprites.push_back({std::make_unique<FSpriteFile>(std::move(*Sprite)), &Extension});
		FSpriteFile& Resource = *ExtensionSprites.back().Sprite;
		Battle.WritableFiles.push_back(&Resource);
		Sprites.ParseResource(Battle.EffectResource, Resource, ExtensionTextureX, ExtensionTextureY, 0, ExtensionClutY);
	}

	const FBattleSequenceExtension* FBattleShow::ExtensionOf(const FSpriteFile* File) const
	{
		const auto Found = std::find_if(ExtensionSprites.begin(), ExtensionSprites.end(), [File](const FExtensionSprite& Kept) { return Kept.Sprite.get() == File; });
		return Found != ExtensionSprites.end() ? Found->Source : nullptr;
	}

	uint32 FBattleShow::EventSoundBase()
	{
		// The battle event's sound effects, which its load registers. Without them the original reads the pointer's cleared
		// word and then low memory.
		const int32 Set = Playback.EventSoundSet();
		if (Set < 0)
		{
			ReportFault(EBattleShowFault::EventSounds);
			return 0;
		}
		return static_cast<uint32>(Set) << 16;
	}
}
