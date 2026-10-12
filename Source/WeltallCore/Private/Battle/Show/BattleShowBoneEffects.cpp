#include "Battle/Show/BattleShowObjects.h"
#include "Battle/Show/BattleShowSprites.h"

#include <algorithm>

namespace Weltall
{
	bool FBattleSprites::SpawnBoneEffect(const FBattleBoneEffect& Effect)
	{
		FSpriteResource& Resource = Effect.bEffectResource ? EffectResource : CommonEffects;
		FSpriteTask* Task = Sprites->SpawnEffect(Resource, Effect.Animation, {Effect.Point[0], Effect.Point[1], Effect.Point[2]});
		if (Task == nullptr)
		{
			return false;
		}
		FSprite& Sprite = Task->Sprite;
		Sprites->SetDirection(Sprite, Effect.Direction);
		Sprites->SetFacing(Sprite, Effect.Direction);
		Sprites->SetScale(Sprite, Effect.Scale);
		if (!Effect.bFollows)
		{
			return true;
		}
		// The follows of sprites that have ended since are let go before this one is kept.
		const std::vector<FKernelTaskEntry>& Tasks = Sprites->Tasks();
		BoneFollows.erase(std::remove_if(BoneFollows.begin(), BoneFollows.end(),
							  [&Tasks](const FBoneFollow& Follow) {
								  return std::none_of(Tasks.begin(), Tasks.end(), [&Follow](const FKernelTaskEntry& Entry) {
									  return Entry.Sprite.get() == Follow.Task && Entry.Sprite->Id == Follow.TaskId;
								  });
							  }),
			BoneFollows.end());
		BoneFollows.push_back({Task, Task->Id, Effect.Slot, Effect.Bone, Effect.FollowOffset, Effect.bOnFloor, Task->UpdateRole, std::nullopt});
		Task->UpdateRole = BoneFollowRole.get();
		return true;
	}

	void FBattleSprites::FollowSwappedObjects(int32 First, int32 Second)
	{
		for (FBoneFollow& Follow : BoneFollows)
		{
			Follow.Slot = Follow.Slot == First ? Second : Follow.Slot == Second ? First : Follow.Slot;
		}
	}

	void FBattleSprites::FollowBone(FSpriteTask& Task)
	{
		const auto Found = std::find_if(BoneFollows.begin(), BoneFollows.end(), [&Task](const FBoneFollow& Follow) {
			return Follow.Task == &Task && Follow.TaskId == Task.Id;
		});
		std::array<int32, 3> Place{};
		if (Found != BoneFollows.end() && Services->BonePlace(Found->Slot, Found->Bone, Found->Offset, Found->bOnFloor, Place))
		{
			Found->LastPlace = Place;
		}
		else if (Found != BoneFollows.end() && Found->LastPlace)
		{
			// The object the sprite follows was freed: the original reads the bone in the memory it left, where it last stood.
			Place = *Found->LastPlace;
		}
		else
		{
			ReportFault(0xC0FB);
			return;
		}
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Task.Sprite.Position[Axis] = static_cast<int32>(static_cast<uint32>(Place[Axis]) << 16);
		}
		if (ISpriteTaskRole* Update = Found->Update)
		{
			Update->Update(Task);
		}
		else
		{
			Sprites->UpdateSpriteTask(Task);
		}
	}
}
