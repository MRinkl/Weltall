#include "Graphics/SpriteFile.h"
#include "Graphics/SpriteSystem.h"
#include "SpriteRules.h"

#include <algorithm>

namespace Weltall
{
	using namespace SpriteRules;

	namespace
	{
		constexpr uint32 NumberMask = 0x1FFFFFFF;
		constexpr uint32 DetachedWithOwner = 0x20000000;
		constexpr uint32 OutlivesOwner = 0x40000000;
		constexpr uint8 DefaultCode = 0x2D;
		constexpr uint32 DefaultTimeScale = 0x100;
		constexpr int32 CountingKind = 7;
		constexpr int32 LineKind = 8;
		constexpr int32 TileKind = 9;
		constexpr int32 EyeControllerKind = 10;
		constexpr int32 LastControllerKind = 13;
		constexpr int32 CommonEffectsKind = 5;
		constexpr int32 EffectKind = 6;

		/** A kind's layout: 1 parts, 0 none, 2 a transform only. */
		int32 KindLayout(int32 Kind)
		{
			switch (Kind)
			{
			case 1:
			case 4:
			case 8:
			case 9:
				return 0;
			case 2:
			case 7:
			case 15:
				return 2;
			default:
				return 1;
			}
		}
	}

	uint32 FSpriteSystem::OwnerNumber(const FSprite& Owner)
	{
		// A sprite of a mode's own is its own owner for the kernel, which then reads the word where a
		// task keeps its number: the sprite's y velocity.
		return (Owner.Task != nullptr ? Owner.Task->Id : static_cast<uint32>(Owner.Velocity[1])) & NumberMask;
	}

	FSpriteTask& FSpriteSystem::NewTask(const FSprite* Owner)
	{
		// Tasks ended earlier are made again, as the kernel's heap gives their memory back.
		std::unique_ptr<FSpriteTask> Created;
		if (FreeTasks.empty())
		{
			Created = std::make_unique<FSpriteTask>();
		}
		else
		{
			Created = std::move(FreeTasks.back());
			FreeTasks.pop_back();
			*Created = FSpriteTask{};
		}
		FSpriteTask& Task = *Created;
		Task.Owner = Owner;
		Task.Id = NextTaskId & NumberMask;
		// The kernel numbers the update and the draw of a task apart.
		NextTaskId += 2;
		Task.Marks = Owner != nullptr ? OwnerNumber(*Owner) : 0;
		Insert(FKernelTaskEntry{std::move(Created)});
		Task.Sprite.Task = &Task;
		return Task;
	}

	FSpriteTask& FSpriteSystem::CreateTask()
	{
		return NewTask(nullptr);
	}

	FSprite* FSpriteSystem::SpawnChild(FSprite& Parent, const FSpriteFile& File, uint32 Header, FSpriteResource* Resource)
	{
		const bool bWasCounting = bCountingTasks;
		const bool bUncounted = (Parent.NextWord & SpriteNextFlags::KeepTaskMarks) != 0;
		Parent.NextWord |= SpriteNextFlags::HasChildren;
		if (bUncounted)
		{
			bCountingTasks = false;
		}
		int32 Kind = (File.Word(Header) >> 8 & 7) + ((File.Word(Header) & 0x4000) != 0 ? 8 : 0);
		if (Kind == 3)
		{
			Kind = Parent.Kind();
		}
		const int32 Layout = KindLayout(Kind);
		const bool bBattleKind = Kind == CommonEffectsKind || Kind == EffectKind || Kind == CountingKind || Kind >= LineKind;
		if (bBattleKind && BattleMode == nullptr)
		{
			// Kinds of the battle program's, with its resources and set-ups.
			Report(ESpriteFault::UnsupportedInstruction, 0xE0);
			bCountingTasks = bWasCounting;
			return nullptr;
		}
		if (Layout == 1 && (Kind == CommonEffectsKind || Kind == EffectKind))
		{
			Resource = &BattleMode->KindResource(Kind);
		}

		// The child is its parent's task's, which the kernel reads from the parent: nobody's when that word was cleared.
		FSpriteTask& Task = NewTask(Parent.bTaskUnlinked ? nullptr : &Parent);
		Task.Marks |= DetachedWithOwner;

		FSprite& Sprite = Task.Sprite;
		Sprite.Code = DefaultCode;
		Sprite.TimeFlags = DefaultTimeScale << SpriteTimeFlags::TimeScaleShift;
		Sprite.Resource = Resource;
		Sprite.bHasTransform = Layout != 0;
		Sprite.PartCapacity = Layout == 1 && Resource->File != nullptr ? Resource->File->MaxParts(Resource->Tables) : 0;
		Sprite.BuildFlags = static_cast<uint32>(Kind) << SpriteBuildFlags::KindShift | (Parent.BuildFlags & (SpriteBuildFlags::SizeShiftMask | SpriteBuildFlags::Fog));
		Sprite.DrawFlags = static_cast<uint32>(Layout) | (Parent.DrawFlags & (SpriteDrawFlags::Mirrored | SpriteDrawFlags::FlippedVertically))
			| (Parent.DrawFlags & 0xFF00) | SpriteDrawFlags::SimplePhysics;
		Sprite.Speed = Parent.Speed;
		Sprite.Heading = Parent.Heading;
		Sprite.Scale = Parent.Scale;
		Sprite.Frame = Parent.Frame;
		Sprite.NextWord = Parent.NextWord & (SpriteNextFlags::KeepTaskMarks | SpriteNextFlags::SizeFactorToChildren);
		if ((Parent.NextWord & SpriteNextFlags::SizeFactorToChildren) != 0)
		{
			Sprite.SizeFactor = Parent.SizeFactor;
			Sprite.BuildFlags = (Sprite.BuildFlags & ~SpriteBuildFlags::SizeShiftMask) | 3u << SpriteBuildFlags::SizeShiftShift;
		}
		Sprite.AnimationFlags = Parent.AnimationFlags & 0xC0000000;
		Sprite.TimeFlags = Parent.TimeFlags & (0x7 | SpriteTimeFlags::UpdateTwice | SpriteTimeFlags::TimeScaleMask);
		// A child reads its parent's extra block when the parent does not keep one of the kernel's own.
		if ((Parent.AnimationFlags & SpriteAnimationFlags::HasExtra) == 0)
		{
			Sprite.ExtraOf = Parent.ExtraOf != nullptr ? Parent.ExtraOf : &Parent;
		}
		Sprite.Parent = &Parent;
		Sprite.CurrentFile = Parent.CurrentFile;
		Sprite.PrimaryFile = Parent.PrimaryFile;
		Sprite.Target = Parent.Target;
		Sprite.Weight = Parent.Weight;
		Sprite.SoundSet = Parent.SoundSet;
		Sprite.ParentAnimation = Parent.Animation();
		Sprite.FloorTriangle = Parent.FloorTriangle;
		Sprite.Position = Parent.Position;
		Sprite.Velocity = Parent.Velocity;
		if (Layout != 0)
		{
			Sprite.Rotation = Parent.Rotation;
			Sprite.TransformScale = Parent.TransformScale;
		}
		Start(Sprite, File, Header);
		SetUpKind(Task);
		bCountingTasks = bWasCounting;
		return &Sprite;
	}

	FSpriteTask* FSpriteSystem::SpawnEffect(FSpriteResource& Resource, int32 Animation, const std::array<int16, 3>& Point)
	{
		if (Resource.File == nullptr)
		{
			Report(ESpriteFault::UnsupportedInstruction, 0xE0);
			return nullptr;
		}
		const FSpriteFile& File = *Resource.File;
		const uint32 Entry = Resource.Tables.Animations + 2 + 2 * static_cast<uint32>(Animation);
		const uint32 Header = File.AnimationHeader(Resource.Tables, Animation);
		if (Animation < 0 || Entry + 2 > File.Size() || Header + 4 > File.Size())
		{
			Report(ESpriteFault::PcOutsideFile, Entry + 2 > File.Size() ? Entry : Header);
			return nullptr;
		}
		const int32 Kind = (File.Word(Header) >> 8 & 7) + ((File.Word(Header) & 0x4000) != 0 ? 8 : 0);
		const bool bBattleKind = Kind == CommonEffectsKind || Kind == EffectKind || Kind == CountingKind || Kind >= LineKind;
		// Kind 3 takes a spawning sprite's kind, which an effect has none of: the kernel's layout is then a stray register.
		if (Kind == 3 || (bBattleKind && BattleMode == nullptr))
		{
			Report(ESpriteFault::UnsupportedInstruction, 0xE0);
			return nullptr;
		}
		const int32 Layout = KindLayout(Kind);
		FSpriteResource* Drawn = &Resource;
		if (Layout == 1 && (Kind == CommonEffectsKind || Kind == EffectKind))
		{
			Drawn = &BattleMode->KindResource(Kind);
		}

		FSpriteTask& Task = NewTask(nullptr);
		Task.Marks |= DetachedWithOwner;
		FSprite& Sprite = Task.Sprite;
		Sprite.Code = DefaultCode;
		Sprite.TimeFlags = DefaultTimeScale << SpriteTimeFlags::TimeScaleShift;
		Sprite.Resource = Drawn;
		Sprite.bHasTransform = Layout != 0;
		Sprite.PartCapacity = Layout == 1 && Drawn->File != nullptr ? Drawn->File->MaxParts(Drawn->Tables) : 0;
		Sprite.BuildFlags = static_cast<uint32>(Kind) << SpriteBuildFlags::KindShift;
		Sprite.DrawFlags = static_cast<uint32>(Layout);
		// In battle it takes the actor's slot, target, motion, look, time scale, extra block and sounds (not its files).
		const FSprite* Actor = BattleMode != nullptr ? BattleMode->Actor() : nullptr;
		if (Actor != nullptr)
		{
			Sprite.Target = Actor->Target;
			Sprite.Speed = Actor->Speed;
			Sprite.Heading = Actor->Heading;
			Sprite.Scale = Actor->Scale;
			Sprite.BuildFlags |= Actor->BuildFlags & SpriteBuildFlags::SizeShiftMask;
			Sprite.DrawFlags |= (Actor->DrawFlags & (SpriteDrawFlags::Mirrored | SpriteDrawFlags::FlippedVertically | 0xFF00)) | SpriteDrawFlags::SimplePhysics;
			Sprite.TimeFlags = Actor->TimeFlags & (0x7 | SpriteTimeFlags::TimeScaleMask);
			Sprite.AnimationFlags = Actor->AnimationFlags & 0xC0000000;
			Sprite.ExtraOf = Actor->ExtraOf != nullptr ? Actor->ExtraOf : Actor;
			Sprite.SoundSet = Actor->SoundSet;
			Sprite.ParentAnimation = Actor->Animation();
		}
		Sprite.Weight = AnimationScale;
		Sprite.Position = {int32{Point[0]} * 0x10000, int32{Point[1]} * 0x10000, int32{Point[2]} * 0x10000};
		Start(Sprite, File, Header);
		SetUpKind(Task);
		return &Task;
	}

	void FSpriteSystem::SetUpKind(FSpriteTask& Task)
	{
		FSprite& Sprite = Task.Sprite;
		const int32 Kind = Sprite.Kind();
		if (Kind == CountingKind)
		{
			Task.bCountsUpdates = true;
		}
		else if (Kind == LineKind || Kind == TileKind)
		{
			// Drawn as a line or a tile by their kind: the code of the primitive, a frame of 1.
			Sprite.Code = Kind == LineKind ? 0x68 : 0x60;
			Sprite.Frame = 1;
			if (Kind == TileKind)
			{
				Sprite.FrameTop = 3;
			}
		}
		else if (Kind >= EyeControllerKind && Kind <= LastControllerKind)
		{
			BattleMode->SetUpController(Task);
		}
	}

	void FSpriteSystem::Insert(FKernelTaskEntry Entry)
	{
		Entry.bCounted = bCountingTasks;
		if (bCountingTasks)
		{
			++CountedTasks;
		}
		TaskList.insert(TaskList.begin(), std::move(Entry));
		if (bRunning)
		{
			++RunCursor;
		}
	}

	void FSpriteSystem::Erase(std::size_t Index)
	{
		if (TaskList[Index].bCounted)
		{
			--CountedTasks;
		}
		if (bRunning && Index < RunCursor)
		{
			--RunCursor;
		}
		if (TaskList[Index].Sprite)
		{
			FreeTasks.push_back(std::move(TaskList[Index].Sprite));
		}
		TaskList.erase(TaskList.begin() + static_cast<std::ptrdiff_t>(Index));
	}

	void FSpriteSystem::LinkTask(IKernelTask& Task, const FSprite* Owner)
	{
		FKernelTaskEntry Entry{nullptr, &Task};
		Entry.Owner = Owner;
		Entry.OwnerNumber = Owner != nullptr ? OwnerNumber(*Owner) : 0;
		Insert(std::move(Entry));
	}

	void FSpriteSystem::UnlinkTask(IKernelTask& Task)
	{
		const auto Found = std::find_if(TaskList.begin(), TaskList.end(), [&Task](const FKernelTaskEntry& Entry) { return Entry.Task == &Task; });
		if (Found != TaskList.end())
		{
			Erase(static_cast<std::size_t>(Found - TaskList.begin()));
		}
	}

	bool FSpriteSystem::IsLinked(const IKernelTask& Task) const
	{
		return std::any_of(TaskList.begin(), TaskList.end(), [&Task](const FKernelTaskEntry& Entry) { return Entry.Task == &Task; });
	}

	void FSpriteSystem::Uncount(const IKernelTask& Task)
	{
		for (FKernelTaskEntry& Entry : TaskList)
		{
			if (Entry.Task == &Task && Entry.bCounted)
			{
				Entry.bCounted = false;
				--CountedTasks;
			}
		}
	}

	void FSpriteSystem::RunTasks()
	{
		if (HitStop != 0)
		{
			--HitStop;
			return;
		}
		bRunning = true;
		RunCursor = 0;
		while (RunCursor < TaskList.size())
		{
			FKernelTaskEntry& Entry = TaskList[RunCursor++];
			if (Entry.Sprite)
			{
				UpdateTask(*Entry.Sprite);
			}
			else
			{
				Entry.Task->Update();
			}
		}
		bRunning = false;
	}

	void FSpriteSystem::UpdateTask(FSpriteTask& Task)
	{
		if (Task.bCountsUpdates)
		{
			++CountedUpdates;
		}
		if (Task.UpdateRole != nullptr)
		{
			Task.UpdateRole->Update(Task);
			return;
		}
		UpdateSpriteTask(Task);
	}

	void FSpriteSystem::UpdateSpriteTask(FSpriteTask& Task)
	{
		FSprite& Sprite = Task.Sprite;
		TickAnimation(Sprite);
		Move(Sprite);
		if (Sprite.Pc != FSprite::Stopped)
		{
			if ((Sprite.TimeFlags & SpriteTimeFlags::UpdateTwice) == 0)
			{
				return;
			}
			TickAnimation(Sprite);
			Move(Sprite);
			if (Sprite.Pc != FSprite::Stopped)
			{
				return;
			}
		}
		EndTask(Task);
	}

	void FSpriteSystem::EndTask(FSpriteTask& Task)
	{
		if (Task.EndRole != nullptr)
		{
			Task.EndRole->End(Task);
			return;
		}
		DestroyTask(Task);
	}

	void FSpriteSystem::KillChildren(const FSprite& Owner)
	{
		if (Owner.Task != nullptr && Owner.Task->bDrawingIsOwnChild)
		{
			// The task's own drawing is filed under it as a child of its own, and ends with the others.
			Owner.Task->bDrawn = false;
		}
		const uint32 Number = OwnerNumber(Owner);
		const auto KeyOf = [](const FKernelTaskEntry& Entry) -> const void* {
			return Entry.Sprite ? static_cast<const void*>(Entry.Sprite.get()) : static_cast<const void*>(Entry.Task);
		};
		// The kernel walks the list once, ending each child as it comes to it and going on from the
		// task that followed it; a child's end may end children of its own, which are newer and so
		// already passed.
		std::size_t Index = 0;
		while (Index < TaskList.size())
		{
			FKernelTaskEntry& Entry = TaskList[Index];
			FSpriteTask* Task = Entry.Sprite.get();
			const bool bChild = Task != nullptr
				? Task->Owner == &Owner && (Task->Marks & OutlivesOwner) == 0 && (Task->Marks & NumberMask) == Number
				: Entry.Owner == &Owner && Entry.OwnerNumber == Number;
			if (!bChild)
			{
				++Index;
				continue;
			}
			const void* Next = Index + 1 < TaskList.size() ? KeyOf(TaskList[Index + 1]) : nullptr;
			if (Task != nullptr)
			{
				EndTask(*Task);
			}
			else
			{
				IKernelTask& Killed = *Entry.Task;
				Erase(Index);
				Killed.OnKilled();
			}
			if (Next == nullptr)
			{
				return;
			}
			const auto Found = std::find_if(TaskList.begin(), TaskList.end(), [&KeyOf, Next](const FKernelTaskEntry& Other) { return KeyOf(Other) == Next; });
			if (Found != TaskList.end())
			{
				Index = static_cast<std::size_t>(Found - TaskList.begin());
			}
		}
	}

	FSprite* FSpriteSystem::FindChild(const FSprite& Owner, const FSpriteResource& Resource, int32 Animation)
	{
		for (const FKernelTaskEntry& Entry : TaskList)
		{
			if (IsDrawingChild(Entry, Owner, Resource, Animation))
			{
				return &Entry.Sprite->Sprite;
			}
		}
		return nullptr;
	}

	void FSpriteSystem::EndChildren(const FSprite& Owner, const FSpriteResource& Resource, std::optional<int32> Animation)
	{
		// As the kernel's walk, each child ended goes on from the task that followed it.
		std::size_t Index = 0;
		while (Index < TaskList.size())
		{
			if (!IsDrawingChild(TaskList[Index], Owner, Resource, Animation))
			{
				++Index;
				continue;
			}
			const FSpriteTask* Next = Index + 1 < TaskList.size() ? TaskList[Index + 1].Sprite.get() : nullptr;
			const IKernelTask* NextTask = Index + 1 < TaskList.size() ? TaskList[Index + 1].Task : nullptr;
			EndTask(*TaskList[Index].Sprite);
			if (Next == nullptr && NextTask == nullptr)
			{
				return;
			}
			const auto Found = std::find_if(TaskList.begin(), TaskList.end(), [Next, NextTask](const FKernelTaskEntry& Other) {
				return Next != nullptr ? Other.Sprite.get() == Next : Other.Task == NextTask;
			});
			if (Found == TaskList.end())
			{
				return;
			}
			Index = static_cast<std::size_t>(Found - TaskList.begin());
		}
	}

	bool FSpriteSystem::IsDrawingChild(const FKernelTaskEntry& Entry, const FSprite& Owner, const FSpriteResource& Resource, std::optional<int32> Animation) const
	{
		const FSpriteTask* Task = Entry.Sprite.get();
		return Task != nullptr && Task->Owner == &Owner && (Task->Marks & NumberMask) == OwnerNumber(Owner) && (Task->Marks & DetachedWithOwner) != 0
			&& Task->Sprite.Resource == &Resource && (!Animation || Task->Sprite.Animation() == *Animation);
	}

	void FSpriteSystem::DetachChildren(const FSprite& Owner)
	{
		const uint32 Number = OwnerNumber(Owner);
		for (const FKernelTaskEntry& Entry : TaskList)
		{
			FSpriteTask* Child = Entry.Sprite.get();
			if (Child != nullptr && Child->Owner == &Owner && (Child->Marks & NumberMask) == Number && (Child->Marks & DetachedWithOwner) != 0)
			{
				Child->Sprite.Parent = nullptr;
			}
		}
	}

	void FSpriteSystem::DestroyTask(FSpriteTask& Task)
	{
		FSprite& Sprite = Task.Sprite;
		if ((Sprite.TimeFlags & SpriteTimeFlags::KillChildrenOnDestroy) != 0)
		{
			KillChildren(Sprite);
		}
		if ((Sprite.NextWord & SpriteNextFlags::HasChildren) != 0)
		{
			DetachChildren(Sprite);
		}
		if (Sprite.Layout() == SpriteDrawFlags::LayoutParts)
		{
			Unqueue(Sprite);
		}
		RemoveTask(Task);
	}

	void FSpriteSystem::RemoveTask(FSpriteTask& Task)
	{
		FSprite& Sprite = Task.Sprite;
		Unqueue(Sprite);
		// Whatever else still points at the sprite would read freed memory in the kernel.
		for (FKernelTaskEntry& Entry : TaskList)
		{
			if (Entry.Owner == &Sprite)
			{
				Entry.Owner = nullptr;
			}
			FSpriteTask* Other = Entry.Sprite.get();
			if (Other == nullptr)
			{
				continue;
			}
			if (Other->Sprite.Parent == &Sprite)
			{
				Other->Sprite.Parent = nullptr;
			}
			if (Other->Sprite.Target == &Sprite)
			{
				Other->Sprite.Target = nullptr;
			}
			if (Other->Sprite.ExtraOf == &Sprite)
			{
				Other->Sprite.ExtraOf = nullptr;
			}
			if (Other->Owner == &Sprite)
			{
				Other->Owner = nullptr;
			}
		}
		const auto Found = std::find_if(TaskList.begin(), TaskList.end(), [&Task](const FKernelTaskEntry& Entry) { return Entry.Sprite.get() == &Task; });
		if (Found == TaskList.end())
		{
			return;
		}
		Erase(static_cast<std::size_t>(Found - TaskList.begin()));
	}

	void FSpriteSystem::EndTasks()
	{
		while (!TaskList.empty())
		{
			if (TaskList.front().Sprite)
			{
				DestroyTask(*TaskList.front().Sprite);
			}
			else
			{
				Erase(0);
			}
		}
	}
}
