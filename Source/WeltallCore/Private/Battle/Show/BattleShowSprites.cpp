#include "Battle/Show/BattleShowSprites.h"

#include "Battle/BattleContext.h"
#include "Battle/Show/BattleShowCamera.h"
#include "Battle/Show/BattleShowFloor.h"
#include "Battle/Show/BattleSpriteFiles.h"
#include "Battle/Show/BattleShowTransition.h"
#include "Game/PsxRandom.h"
#include "Foundation/ProcessorDivide.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"
#include "Graphics/SpriteFile.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int16 CombatantScale = 0x2000;
		constexpr int32 EyeControllerKind = 10;
		constexpr uint32 ShadowFlag = 0x4;
		constexpr uint32 NumberMask = 0x1FFFFFFF;

		/** The high halves of a sprite's 16.16 position. */
		FGteVector Place(const FSprite& Sprite)
		{
			return {static_cast<int16>(Sprite.Position[0] >> 16), static_cast<int16>(Sprite.Position[1] >> 16), static_cast<int16>(Sprite.Position[2] >> 16)};
		}

		int32 Wrap(int64 Value)
		{
			return static_cast<int32>(static_cast<uint32>(static_cast<uint64>(Value)));
		}

		/** Division by 2^Bits rounded toward zero. */
		int32 DivideByPower(int64 Value, int32 Bits)
		{
			const int32 Truncated = Wrap(Value);
			return (Truncated < 0 ? static_cast<int32>(static_cast<int64>(Truncated) + (int64{1} << Bits) - 1) : Truncated) >> Bits;
		}

		void SetSlot(FSprite& Sprite, int32 Slot)
		{
			Sprite.AnimationFlags = (Sprite.AnimationFlags & 0x3FFFFFFF) | static_cast<uint32>(Slot) << 30;
			Sprite.TimeFlags = (Sprite.TimeFlags & ~3u) | (static_cast<uint32>(Slot) >> 2 & 3);
		}
	}

	/** A combatant's sprite: still while the combatants are hidden; it ends with its children. */
	class FBattleSprites::FCombatantRole final : public ISpriteTaskRole
	{
	public:
		explicit FCombatantRole(FBattleSprites& InOwner) : Owner(InOwner) {}

		void Update(FSpriteTask& Task) override
		{
			if (Owner.bCombatantsHidden)
			{
				return;
			}
			FSprite& Sprite = Task.Sprite;
			Owner.Sprites->TickAnimation(Sprite);
			Owner.Sprites->Move(Sprite);
			if ((Sprite.TimeFlags & SpriteTimeFlags::UpdateTwice) != 0)
			{
				Owner.Sprites->TickAnimation(Sprite);
				Owner.Sprites->Move(Sprite);
			}
		}

		void End(FSpriteTask& Task) override
		{
			Owner.Forget(Task);
			Owner.Sprites->KillChildren(Task.Sprite);
			Owner.Sprites->RemoveTask(Task);
		}

	private:
		FBattleSprites& Owner;
	};

	/** A 3D combatant's proxy: at its object's root, running a script only when one was started. */
	class FBattleSprites::FProxyRole final : public ISpriteTaskRole
	{
	public:
		explicit FProxyRole(FBattleSprites& InOwner) : Owner(InOwner) {}

		void Update(FSpriteTask& Task) override
		{
			FSprite& Sprite = Task.Sprite;
			std::array<int32, 3> Root{};
			if (!Owner.Services->ObjectRoot(SlotOf(Sprite), Root))
			{
				return;
			}
			if (Sprite.Wait == 0)
			{
				Sprite.Pc = FSprite::Stopped;
			}
			if (Sprite.Pc != FSprite::Stopped)
			{
				Owner.Sprites->TickAnimation(Sprite);
				Owner.Sprites->Move(Sprite);
				if ((Sprite.TimeFlags & SpriteTimeFlags::UpdateTwice) != 0)
				{
					Owner.Sprites->TickAnimation(Sprite);
					Owner.Sprites->Move(Sprite);
				}
			}
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Sprite.Position[Axis] = Wrap(int64{Root[Axis]} * 0x10000);
			}
		}

		void End(FSpriteTask& Task) override
		{
			Owner.Forget(Task);
			Owner.Sprites->RemoveTask(Task);
		}

	private:
		FBattleSprites& Owner;
	};

	/**
	 * A camera controller: its place is
	 * where the eye (kind 10) or the look-at (11) heads; the last to end gives the camera back its mode.
	 */
	class FBattleSprites::FControllerRole final : public ISpriteTaskRole
	{
	public:
		explicit FControllerRole(FBattleSprites& InOwner) : Owner(InOwner) {}

		void Update(FSpriteTask& Task) override
		{
			FSprite& Sprite = Task.Sprite;
			Owner.Sprites->TickAnimation(Sprite);
			Owner.Sprites->Move(Sprite);
			Publish(Sprite);
			if (Sprite.Pc != FSprite::Stopped)
			{
				if ((Sprite.TimeFlags & SpriteTimeFlags::UpdateTwice) == 0)
				{
					return;
				}
				Owner.Sprites->TickAnimation(Sprite);
				Owner.Sprites->Move(Sprite);
				Publish(Sprite);
				if (Sprite.Pc != FSprite::Stopped)
				{
					return;
				}
			}
			Owner.Sprites->EndTask(Task);
		}

		void End(FSpriteTask& Task) override
		{
			FSprite& Sprite = Task.Sprite;
			FBattleCamera& Shot = *Owner.Camera;
			if (Sprite.Kind() == EyeControllerKind)
			{
				if (Owner.EyeController == &Task)
				{
					Owner.EyeController = nullptr;
					if (!Shot.bLocked)
					{
						Shot.DesiredEye = Owner.SavedDesiredEye;
					}
				}
			}
			else if (Owner.TargetController == &Task)
			{
				Owner.TargetController = nullptr;
				if (!Shot.bLocked)
				{
					Shot.DesiredTarget = Owner.SavedDesiredTarget;
				}
			}
			if ((Sprite.TimeFlags & SpriteTimeFlags::KillChildrenOnDestroy) != 0)
			{
				Owner.Sprites->KillChildren(Sprite);
			}
			Owner.Sprites->RemoveTask(Task);
			Owner.ControllerCount = static_cast<uint8>(Owner.ControllerCount - 1);
			if (Owner.ControllerCount == 0)
			{
				Shot.SetMode(Shot.ModeAfter);
			}
		}

	private:
		void Publish(const FSprite& Sprite)
		{
			std::array<int32, 3>& Point = Sprite.Kind() == EyeControllerKind ? Owner.Camera->EyePoint : Owner.Camera->TargetPoint;
			Point = Sprite.Position;
		}

		FBattleSprites& Owner;
	};

	/** A sprite orbiting its goal (system command 20): its velocity read as a radius and two angles. */
	class FBattleSprites::FOrbitRole final : public ISpriteTaskRole
	{
	public:
		explicit FOrbitRole(FBattleSprites& InOwner) : Owner(InOwner) {}

		void Update(FSpriteTask& Task) override
		{
			FSprite& Sprite = Task.Sprite;
			Owner.Sprites->TickAnimation(Sprite);
			const FGteVector Angles{0, static_cast<int16>(Sprite.Velocity[1] >> 13), static_cast<int16>(Sprite.Velocity[2] >> 13)};
			const int32 Radius = Sprite.SizeFactor != 0 ? DivideByPower(int64{Sprite.Velocity[0] >> 13} * Sprite.SizeFactor, 10) : Sprite.Velocity[0] >> 13;
			const FGteLongVector Around = ApplyMatrix(RotationXyz(Angles), {static_cast<int16>(Radius), 0, 0});
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Sprite.Position[Axis] = Wrap(int64{Wrap(int64{Around[Axis]} + Sprite.SavedPoint[Axis])} * 0x10000);
			}
			if (Sprite.Pc == FSprite::Stopped)
			{
				Owner.Sprites->EndTask(Task);
			}
		}

		void End(FSpriteTask& Task) override
		{
			Owner.Sprites->DestroyTask(Task);
		}

	private:
		FBattleSprites& Owner;
	};

	/** An effect's sprite that follows a bone: only its update is replaced; its end stays the one it had. */
	class FBattleSprites::FBoneFollowRole final : public ISpriteTaskRole
	{
	public:
		explicit FBoneFollowRole(FBattleSprites& InOwner) : Owner(InOwner) {}

		void Update(FSpriteTask& Task) override { Owner.FollowBone(Task); }
		void End(FSpriteTask& Task) override { Owner.Sprites->DestroyTask(Task); }

	private:
		FBattleSprites& Owner;
	};

	namespace
	{
		constexpr uint8 WatchTaskKind = 1;
		constexpr uint8 GlueTaskKind = 2;
		constexpr uint8 TrailTaskKind = 3;
		constexpr uint8 ShatterTaskKind = 4;
		constexpr uint8 WalkWatchTaskKind = 5;

		/** The distance from the sprite's place to its goal. */
		int32 DistanceToGoal(const FSprite& Sprite)
		{
			const int32 X = Sprite.SavedPoint[0] - static_cast<int16>(Sprite.Position[0] >> 16);
			const int32 Y = Sprite.SavedPoint[1] - static_cast<int16>(Sprite.Position[1] >> 16);
			const int32 Z = Sprite.SavedPoint[2] - static_cast<int16>(Sprite.Position[2] >> 16);
			return SquareRoot0(Wrap(int64{GteSquare(X)} + GteSquare(Z) + GteSquare(Y)));
		}

		bool Running(const FSprite& Sprite)
		{
			const uint32 State = Sprite.State();
			return State != 0 && State != 1;
		}

		/** Whether the anchor routines read a sprite's anchor slots: a sprite of parts with anchors. */
		bool ReadsAnchors(const FSprite& Sprite)
		{
			return Sprite.bHasTransform && Sprite.Layout() == SpriteDrawFlags::LayoutParts && Sprite.bHasAnchors;
		}
	}

	/**
	 * A watch on a sprite's way to its goal (instruction FB): once the distance
	 * grows or falls under the least, the sprite's script jumps; it ends then, when the script waits no
	 * more, or when the animation changed.
	 */
	struct FBattleSprites::FWatchTask final : public IKernelTask
	{
		FBattleSprites* Owner = nullptr;
		FSprite* Sprite = nullptr;
		int32 Distance = 0;
		int32 Minimum = 0;
		uint32 Jump = 0;
		int8 Animation = 0;
		bool bLinked = false;

		void Update() override
		{
			const int32 Previous = Distance;
			Distance = DistanceToGoal(*Sprite);
			bool bEnd = false;
			if (Previous < Distance || Distance < Minimum)
			{
				bEnd = true;
				Sprite->Wait = 1;
				Sprite->Pc = Jump;
			}
			if (Sprite->Wait == 0 || Sprite->Animation() != Animation)
			{
				bEnd = true;
			}
			if (bEnd)
			{
				Owner->Sprites->UnlinkTask(*this);
				bLinked = false;
			}
		}

		void OnKilled() override { bLinked = false; }
	};

	/**
	 * A battle event's watch on a walking sprite: once the distance to the goal grows or
	 * falls under the least, the sprite takes the animation after this one (its stance when it names none) and the
	 * watch ends.
	 */
	struct FBattleSprites::FWalkWatchTask final : public IKernelTask
	{
		FBattleSprites* Owner = nullptr;
		FSprite* Sprite = nullptr;
		int32 Distance = 0;
		int32 Minimum = 0;
		int8 Animation = 0;
		bool bLinked = false;

		void Update() override
		{
			const int32 Previous = Distance;
			Distance = DistanceToGoal(*Sprite);
			if (Previous < Distance || Distance < Minimum)
			{
				const int32 Next = Sprite->NextAnimation();
				Owner->Sprites->SetAnimation(*Sprite, Next < 0 ? 1 : Next);
				Owner->Sprites->UnlinkTask(*this);
				bLinked = false;
			}
		}

		void OnKilled() override { bLinked = false; }
	};

	/**
	 * A glue (system command 03): the sprite's target moved each step so that its
	 * anchor slot meets the sprite's, until the sprite's animation changes or stops running.
	 */
	struct FBattleSprites::FGlueTask final : public IKernelTask
	{
		FBattleSprites* Owner = nullptr;
		FSprite* Sprite = nullptr;
		int32 OwnSlot = 0;
		int32 TargetSlot = 0;
		int8 Animation = 0;
		bool bLinked = false;

		void Update() override
		{
			FSprite* Target = Sprite->Target;
			// Both points are built in one word of the stack, so a target without anchors takes the sprite's own
			// point as its offset; the word before a sprite without anchors is whatever the stack held.
			std::array<int16, 2> Point{};
			if (Target == nullptr || !ReadsAnchors(*Sprite) || !Owner->AnchorPoint(*Sprite, OwnSlot, Point))
			{
				Owner->ReportFault(0xC303);
				return;
			}
			const std::array<int16, 2> Own = Point;
			if (!Owner->AnchorPoint(*Target, TargetSlot, Point))
			{
				Owner->ReportFault(0xC303);
				return;
			}
			const std::array<int16, 2> Other = Point;
			Target->Position[0] = Wrap(int64{Target->Position[0]} + int64{static_cast<int16>(Own[0] - Other[0])} * 0x10000);
			Target->Position[1] = Wrap(int64{Target->Position[1]} + int64{static_cast<int16>(Own[1] - Other[1])} * 0x10000);
			if (Sprite->Animation() != Animation || !Running(*Sprite))
			{
				Owner->Sprites->UnlinkTask(*this);
				bLinked = false;
			}
		}

		void OnKilled() override { bLinked = false; }
	};

	/**
	 * A weapon trail (system command 01): five of the sprite's anchor slots read again
	 * whenever its frame changes, the drawn ribbon's points eased halfway toward them each step (the
	 * first at once), until the animation changes or stops running.
	 */
	struct FBattleSprites::FTrailTask final : public IKernelTask
	{
		FBattleSprites* Owner = nullptr;
		FSprite* Sprite = nullptr;
		uint16 Frame = 0;
		int8 Animation = 0;
		/** The sprite's blend, the trail's width, and the script's variable: its width, and whether each half of its spans lies behind the sprite. */
		uint8 Blend = 0;
		int32 Width = 0;
		uint8 Variable = 0;
		std::array<FGteVector, 5> Read{};
		std::array<FGteVector, 5> Drawn{};
		bool bLinked = false;

		void Update() override
		{
			// The words every trail's drawing reads until the next trail steps.
			Owner->TrailWords.Entry = Sprite->OtIndex;
			Owner->TrailWords.Width = static_cast<int16>(Width);
			Owner->TrailWords.Blend = Blend;
			Owner->TrailWords.Stepped = this;
			if (Sprite->Frame != Frame)
			{
				Frame = Sprite->Frame;
				ReadAnchors(*Sprite, Read);
			}
			for (std::size_t Point = 1; Point < Drawn.size(); ++Point)
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Drawn[Point][Axis] = static_cast<int16>(Drawn[Point][Axis] + static_cast<int16>((Read[Point][Axis] - Drawn[Point][Axis]) / 2));
				}
			}
			Drawn[0] = Read[0];
			if (Sprite->Animation() != Animation || !Running(*Sprite))
			{
				Owner->Sprites->UnlinkTask(*this);
				bLinked = false;
			}
		}

		void OnKilled() override { bLinked = false; }

		/** The anchors: the slots' offsets at the sprite's scale (8192 as 1), nothing without anchors. */
		static void ReadAnchors(const FSprite& Sprite, std::array<FGteVector, 5>& Points)
		{
			if (!ReadsAnchors(Sprite))
			{
				return;
			}
			for (std::size_t Point = 0; Point < Points.size(); ++Point)
			{
				const FSpriteAnchor& Anchor = Sprite.Anchors[TrailSlots[Point]];
				int32 X = static_cast<int8>(Anchor.OffsetX);
				if (Sprite.IsMirroredFacing())
				{
					X = -X;
				}
				Points[Point] = {static_cast<int16>(DivideByPower(int64{X} * Sprite.Scale, 13)), static_cast<int16>(DivideByPower(int64{static_cast<int8>(Anchor.OffsetY)} * Sprite.Scale, 13)), 0};
			}
		}
	};

	/** A screen shatter a script started (system command 18): it flies apart for as long as the battle runs. */
	struct FBattleSprites::FShatterTask final : public IKernelTask
	{
		FBattleSprites* Owner = nullptr;
		FBattleShatter Shatter;
		bool bLinked = false;

		void Update() override { Shatter.Step(Owner->DrawBuffer); }
		void OnKilled() override { bLinked = false; }
	};

	/** A task of Pool that runs nothing, or one more: the game makes a task for each a script starts, however many run. */
	template <typename TTask>
	TTask& FBattleSprites::FreeTask(std::vector<std::unique_ptr<TTask>>& Pool)
	{
		const auto Free = std::find_if(Pool.begin(), Pool.end(), [](const std::unique_ptr<TTask>& Task) { return !Task->bLinked; });
		if (Free != Pool.end())
		{
			return **Free;
		}
		Pool.push_back(std::make_unique<TTask>());
		Pool.back()->Owner = this;
		return *Pool.back();
	}

	void FBattleSprites::StartWatch(FSprite& Sprite, int32 Minimum, uint32 Jump)
	{
		FWatchTask& Watch = FreeTask(Watches);
		Sprites->LinkTask(Watch, &Sprite);
		Watch.bLinked = true;
		Watch.Sprite = &Sprite;
		Watch.Distance = DistanceToGoal(Sprite);
		Watch.Minimum = Minimum;
		Watch.Jump = Jump;
		Watch.Animation = Sprite.Animation();
		Sprite.TimeFlags |= SpriteTimeFlags::KillChildrenOnDestroy;
	}

	void FBattleSprites::StartWalkWatch(FSprite& Sprite, int32 Minimum)
	{
		FWalkWatchTask& Watch = FreeTask(WalkWatches);
		Sprites->LinkTask(Watch, &Sprite);
		Watch.bLinked = true;
		Watch.Sprite = &Sprite;
		Watch.Animation = Sprite.Animation();
		Watch.Distance = DistanceToGoal(Sprite);
		Watch.Minimum = Minimum;
		Sprite.TimeFlags |= SpriteTimeFlags::KillChildrenOnDestroy;
	}

	void FBattleSprites::StartGlue(FSprite& Sprite, uint8 Slots)
	{
		FGlueTask& Glue = FreeTask(Glues);
		Sprites->LinkTask(Glue, &Sprite);
		Glue.bLinked = true;
		Glue.Sprite = &Sprite;
		Glue.Animation = Sprite.Animation();
		Glue.OwnSlot = Slots & 0xF;
		Glue.TargetSlot = Slots >> 4;
		Glue.Update();
	}

	void FBattleSprites::StartTrail(FSprite& Sprite, const FSpriteOperands& Count)
	{
		const std::optional<FSpriteOperands> Variable = Sprites->VariableOperands(Sprite, Count.U8(0));
		if (!Variable)
		{
			return;
		}
		FTrailTask& Trail = FreeTask(Trails);
		Sprites->LinkTask(Trail, &Sprite);
		Trail.bLinked = true;
		Trail.Sprite = &Sprite;
		Trail.Frame = Sprite.Frame;
		Trail.Variable = Count.U8(0);
		Trail.Animation = Sprite.Animation();
		Trail.Blend = static_cast<uint8>(Sprite.DrawFlags >> 5 & 7);
		Trail.Width = Variable->Bytes[0] != 0 ? Variable->Bytes[0] : 4;
		FTrailTask::ReadAnchors(Sprite, Trail.Drawn);
		FTrailTask::ReadAnchors(Sprite, Trail.Read);
	}

	void FBattleSprites::StartShatter()
	{
		auto Made = std::make_unique<FShatterTask>();
		FShatterTask& Task = *Made;
		Shatters.push_back(std::move(Made));
		Task.Owner = this;
		Sprites->LinkTask(Task);
		Task.bLinked = true;
		Task.Shatter.Create(*Random, *Arctangents);
	}

	void FBattleSprites::EndGlues()
	{
		// Each glue in the list, newest first, ended in turn.
		for (;;)
		{
			const auto& Entries = Sprites->Tasks();
			const auto Found = std::find_if(Entries.begin(), Entries.end(), [this](const FKernelTaskEntry& Entry) { return ScriptTask(Entry.Task).Kind == GlueTaskKind; });
			if (Found == Entries.end())
			{
				return;
			}
			IKernelTask& Glue = *Found->Task;
			Sprites->UnlinkTask(Glue);
			Glue.OnKilled();
		}
	}

	void FBattleSprites::EndTrail(FSprite& Sprite)
	{
		const uint32 Number = FSpriteSystem::OwnerNumber(Sprite);
		for (const FKernelTaskEntry& Entry : Sprites->Tasks())
		{
			const FScriptTaskView View = ScriptTask(Entry.Task);
			if (View.Kind == TrailTaskKind && Entry.Owner == &Sprite && (Entry.OwnerNumber & NumberMask) == Number)
			{
				IKernelTask& Trail = *Entry.Task;
				Sprites->UnlinkTask(Trail);
				Trail.OnKilled();
				return;
			}
		}
	}

	FBattleSprites::FScriptTaskView FBattleSprites::ScriptTask(const IKernelTask* Task) const
	{
		if (Task == nullptr)
		{
			return {};
		}
		for (const std::unique_ptr<FWatchTask>& Watch : Watches)
		{
			if (Watch.get() == Task)
			{
				return {WatchTaskKind, Watch->Sprite};
			}
		}
		for (const std::unique_ptr<FGlueTask>& Glue : Glues)
		{
			if (Glue.get() == Task)
			{
				return {GlueTaskKind, Glue->Sprite};
			}
		}
		for (const std::unique_ptr<FTrailTask>& Trail : Trails)
		{
			if (Trail.get() == Task)
			{
				return {TrailTaskKind, Trail->Sprite};
			}
		}
		for (const std::unique_ptr<FShatterTask>& Shatter : Shatters)
		{
			if (Shatter.get() == Task)
			{
				return {ShatterTaskKind, nullptr};
			}
		}
		for (const std::unique_ptr<FWalkWatchTask>& Watch : WalkWatches)
		{
			if (Watch.get() == Task)
			{
				return {WalkWatchTaskKind, Watch->Sprite};
			}
		}
		return {};
	}

	FBattleSprites::FScriptTaskState FBattleSprites::ScriptTaskState(const IKernelTask* Task) const
	{
		FScriptTaskState State;
		for (const std::unique_ptr<FWatchTask>& Watch : Watches)
		{
			if (Watch.get() == Task)
			{
				State.Animation = Watch->Animation;
				State.Distance = Watch->Distance;
				State.Minimum = Watch->Minimum;
				State.Jump = Watch->Jump;
			}
		}
		for (const std::unique_ptr<FWalkWatchTask>& Watch : WalkWatches)
		{
			if (Watch.get() == Task)
			{
				State.Animation = Watch->Animation;
				State.Distance = Watch->Distance;
				State.Minimum = Watch->Minimum;
			}
		}
		for (const std::unique_ptr<FGlueTask>& Glue : Glues)
		{
			if (Glue.get() == Task)
			{
				State.Animation = Glue->Animation;
				State.OwnSlot = Glue->OwnSlot;
				State.TargetSlot = Glue->TargetSlot;
			}
		}
		for (const std::unique_ptr<FTrailTask>& Trail : Trails)
		{
			if (Trail.get() == Task)
			{
				State.Animation = Trail->Animation;
				State.Frame = Trail->Frame;
				State.Blend = Trail->Blend;
				State.Width = Trail->Width;
				State.Read = Trail->Read;
				State.Drawn = Trail->Drawn;
			}
		}
		return State;
	}

	std::optional<FBattleSprites::FTrailDrawing> FBattleSprites::TrailDrawing(const IKernelTask* Task)
	{
		for (const std::unique_ptr<FTrailTask>& Trail : Trails)
		{
			if (Trail.get() != Task)
			{
				continue;
			}
			// The stepping trail's variable as it reads now; once that trail has ended, as it last read.
			const FTrailTask* Stepped = TrailWords.Stepped;
			if (Stepped != nullptr && Stepped->bLinked)
			{
				if (const std::optional<FSpriteOperands> Variable = Sprites->VariableOperands(*Stepped->Sprite, Stepped->Variable))
				{
					TrailWords.Behind = {Variable->Bytes[0], Variable->Bytes[1]};
				}
			}
			return FTrailDrawing{Trail->Sprite, Trail->Drawn, TrailWords.Entry, TrailWords.Width, TrailWords.Blend, TrailWords.Behind};
		}
		return std::nullopt;
	}

	bool FBattleSprites::AnchorPoint(const FSprite& Sprite, int32 Slot, std::array<int16, 2>& Point)
	{
		if (ReadsAnchors(Sprite))
		{
			if (Slot >= FSprite::AnchorCount)
			{
				return false;
			}
			const FSpriteAnchor& Anchor = Sprite.Anchors[static_cast<std::size_t>(Slot)];
			int32 X = static_cast<int8>(Anchor.OffsetX);
			if (Sprite.IsMirroredFacing())
			{
				X = -X;
			}
			Point = {static_cast<int16>(DivideByPower(int64{X} * Sprite.Scale, 12)), static_cast<int16>(DivideByPower(int64{static_cast<int8>(Anchor.OffsetY)} * Sprite.Scale, 12))};
		}
		Point = {static_cast<int16>(Point[0] + static_cast<int16>(Sprite.Position[0] >> 16)), static_cast<int16>(Point[1] + static_cast<int16>(Sprite.Position[1] >> 16))};
		return true;
	}

	void FBattleSprites::HomeOnGoal(FSprite& Sprite, uint8 Turn)
	{
		// Homing: the velocity turned toward the goal by at most Turn x 4 in each of its two angles.
		const int32 X = Sprite.SavedPoint[0] - static_cast<int16>(Sprite.Position[0] >> 16);
		const int32 Y = Sprite.SavedPoint[1] - static_cast<int16>(Sprite.Position[1] >> 16);
		const int32 Z = Sprite.SavedPoint[2] - static_cast<int16>(Sprite.Position[2] >> 16);
		const int32 FlatToGoal = SquareRoot0(Wrap(int64{GteSquare(X)} + GteSquare(Z)));
		const int32 YawToGoal = GteRatan2(Z, X, *Arctangents);
		const int32 PitchToGoal = GteRatan2(Y, static_cast<int16>(FlatToGoal), *Arctangents);
		const std::array<int32, 3> Moving{Sprite.Velocity[0] >> 7, Sprite.Velocity[1] >> 7, Sprite.Velocity[2] >> 7};
		const int32 Speed = SquareRoot0(Wrap(int64{GteSquare(Moving[1])} + GteSquare(Moving[0]) + GteSquare(Moving[2])));
		const int32 Flat = SquareRoot0(Wrap(int64{GteSquare(Moving[0])} + GteSquare(Moving[2])));
		const int32 Yaw = GteRatan2(Moving[2], Moving[0], *Arctangents);
		const int32 Pitch = GteRatan2(Moving[1], static_cast<int16>(Flat), *Arctangents);
		const int32 Limit = Turn * 4;
		const auto Clamped = [Limit](int32 Difference) {
			// The difference as a signed 12-bit angle, held to the limit.
			const int32 Angle = static_cast<int32>(static_cast<uint32>(Difference) << 20) >> 20;
			const int32 Size = Angle < 0 ? -Angle : Angle;
			return Limit < Size ? (Angle < 0 ? -Limit : Limit) : Angle;
		};
		FGteVector Angles{0, static_cast<int16>(-Yaw), 0};
		Angles[1] = static_cast<int16>(Angles[1] + Clamped(static_cast<int32>(static_cast<uint16>(-YawToGoal)) - static_cast<int32>(static_cast<uint16>(Angles[1]))));
		Angles[2] = static_cast<int16>(Pitch + Clamped(static_cast<int32>(static_cast<uint32>(PitchToGoal) & 0xFFFF) - static_cast<int32>(static_cast<uint32>(Pitch) & 0xFFFF)));
		const FGteLongVector Turned = ApplyMatrix(RotationXyz(Angles), {static_cast<int16>(Speed), 0, 0});
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Sprite.Velocity[Axis] = Wrap(int64{Turned[Axis]} << 7);
		}
	}

	FBattleSprites::FBattleSprites() = default;
	FBattleSprites::~FBattleSprites() = default;

	void FBattleSprites::Begin()
	{
		CombatantRole = std::make_unique<FCombatantRole>(*this);
		ProxyRole = std::make_unique<FProxyRole>(*this);
		ControllerRole = std::make_unique<FControllerRole>(*this);
		OrbitRole = std::make_unique<FOrbitRole>(*this);
		BoneFollowRole = std::make_unique<FBoneFollowRole>(*this);
		BoneFollows.clear();
		Sprites->EndTasks();
		Sprites->BattleMode = this;
		Camera->Controllers = this;
		Sprites->TickExtraFrames = 0;
		Sprites->AnimationScale = CombatantScale;
		Sprites->bCountingTasks = false;
		Sprites->CountedTasks = 0;
		Sprites->HitStop = 0;
		SlotSprites = {};
		SlotTasks = {};
		EyeController = nullptr;
		TargetController = nullptr;
		ControllerCount = 0;
		Watches.clear();
		Glues.clear();
		Trails.clear();
		TrailWords = FTrailWords{};
		Shatters.clear();
		for (int32 Index = 0; Index < WatchCapacity; ++Index)
		{
			Watches.push_back(std::make_unique<FWatchTask>());
			Watches.back()->Owner = this;
		}
		for (int32 Index = 0; Index < GlueCapacity; ++Index)
		{
			Glues.push_back(std::make_unique<FGlueTask>());
			Glues.back()->Owner = this;
		}
		for (int32 Index = 0; Index < TrailCapacity; ++Index)
		{
			Trails.push_back(std::make_unique<FTrailTask>());
			Trails.back()->Owner = this;
		}
	}

	void FBattleSprites::End()
	{
		Sprites->EndTasks();
		Sprites->BattleMode = nullptr;
		Camera->Controllers = nullptr;
		SlotSprites = {};
		SlotTasks = {};
	}

	int32 FBattleSprites::SlotOf(const FSprite& Sprite)
	{
		return static_cast<int32>((Sprite.TimeFlags & 3) << 2 | Sprite.AnimationFlags >> 30);
	}

	FSpriteTask* FBattleSprites::SlotTask(int32 Slot) const
	{
		return Slot >= 0 && Slot < SlotCount ? SlotTasks[static_cast<std::size_t>(Slot)] : nullptr;
	}

	void FBattleSprites::RemoveCombatant(int32 Slot)
	{
		FSpriteTask* Task = SlotTask(Slot);
		if (Task == nullptr)
		{
			return;
		}
		// The original ends the children after the task, finding them by the task it has just freed; the port
		// forgets an ended task's children, so it ends them first.
		Sprites->KillChildren(Task->Sprite);
		Sprites->EndTask(*Task);
	}

	void FBattleSprites::FreeProxy(int32 Slot)
	{
		FSpriteTask* Task = SlotTask(Slot);
		if (Task == nullptr)
		{
			return;
		}
		SlotTasks[static_cast<std::size_t>(Slot)] = nullptr;
		SlotSprites[static_cast<std::size_t>(Slot)] = nullptr;
		Sprites->RetireTask(*Task);
	}

	void FBattleSprites::Forget(const FSpriteTask& Task)
	{
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			if (SlotTasks[static_cast<std::size_t>(Slot)] == &Task)
			{
				SlotTasks[static_cast<std::size_t>(Slot)] = nullptr;
				SlotSprites[static_cast<std::size_t>(Slot)] = nullptr;
			}
		}
		if (Play.Actor == &Task.Sprite)
		{
			Play.Actor = nullptr;
		}
		if (Play.Performer == &Task.Sprite)
		{
			Play.Performer = nullptr;
		}
		if (Play.Caster == &Task.Sprite)
		{
			Play.Caster = nullptr;
		}
		for (FSprite*& Target : Play.Targets)
		{
			if (Target == &Task.Sprite)
			{
				Target = nullptr;
			}
		}
	}

	FSprite& FBattleSprites::CreateCombatant(int32 Slot, const FSpriteFile& File, const FBattleCombatantLook& Look)
	{
		// A combatant's sprite task, which files the task's drawing under no one.
		FSpriteTask& Task = Sprites->CreateTask();
		Task.bDrawingIsOwnChild = false;
		FSprite& Sprite = Task.Sprite;
		Sprites->Create(Sprite, File, Look.ClutX, Look.ClutY, Look.TextureX, Look.TextureY, Look.PaletteSet);
		Sprite.Task = &Task;
		Sprite.Position = {};
		Sprite.NextWord = (Sprite.NextWord & ~0xFFu) | static_cast<uint8>(Look.Animation);
		Sprite.DrawFlags |= ShadowFlag;
		Sprite.Heading = 0;
		Sprites->SetScale(Sprite, CombatantScale);
		Sprite.Weight = CombatantScale;
		Sprite.FloorTriangle = 0;
		Sprites->SetAnimation(Sprite, Look.Animation);
		Task.UpdateRole = CombatantRole.get();
		Task.EndRole = CombatantRole.get();

		// The setup's placing.
		Sprite.Resource->TextureX = static_cast<int16>(Look.TextureX);
		Sprite.Resource->TextureY = static_cast<int16>(Look.TextureY);
		Sprite.Extra.ModeTexture = {Sprite.Resource->TextureX, Sprite.Resource->TextureY};
		SlotSprites[static_cast<std::size_t>(Slot)] = &Sprite;
		SlotTasks[static_cast<std::size_t>(Slot)] = &Task;
		SetSlot(Sprite, Slot);
		const FBattleSlot& Record = Context->Slots[static_cast<std::size_t>(Slot)];
		Sprite.Position[0] = Wrap(int64{static_cast<int16>(Record.Position.X)} * 0x10000);
		Sprite.Position[2] = Wrap(int64{static_cast<int16>(Record.Position.Z)} * 0x10000);
		const int32 Facing = Look.bFacesLeft ? 0x800 : 0;
		Sprites->SetFacing(Sprite, Facing);
		Sprites->SetDirection(Sprite, Facing);
		if (Record.Presence != 0)
		{
			Sprite.Wait = 0;
		}
		return Sprite;
	}

	FSpriteTask& FBattleSprites::CreateEventObject(const FSpriteFile& File, const std::array<int16, 3>& Place)
	{
		const bool bWasCounting = Sprites->bCountingTasks;
		Sprites->bCountingTasks = false;
		FSpriteTask& Task = Sprites->CreateTask();
		Sprites->bCountingTasks = bWasCounting;
		FSprite& Sprite = Task.Sprite;
		// The sprite reset and laid out whole, then its own data as its resource. The reset comes after the sprite
		// was given its task, and clears the word that names it.
		Sprite = FSprite{};
		Sprite.Task = &Task;
		Sprite.bTaskUnlinked = true;
		Sprite.Code = 0x2D;
		Sprite.TimeFlags = 0x100u << SpriteTimeFlags::TimeScaleShift;
		Sprite.bHasTransform = true;
		Sprite.bHasAnchors = true;
		Sprite.Resource = &Sprite.OwnResource;
		Sprite.PrimaryFile = &File;
		Sprite.BuildFlags = 4u << SpriteBuildFlags::KindShift;
		Sprites->SetResource(Sprite, &File);
		Sprite.Position = {Wrap(int64{Place[0]} * 0x10000), Wrap(int64{Place[1]} * 0x10000), Wrap(int64{Place[2]} * 0x10000)};
		Sprites->SetScale(Sprite, CombatantScale);
		Sprite.Weight = CombatantScale;
		Task.UpdateRole = CombatantRole.get();
		Task.EndRole = CombatantRole.get();
		return Task;
	}

	void FBattleSprites::DestroyEventObject(FSpriteTask& Task)
	{
		Sprites->KillChildren(Task.Sprite);
		Sprites->RemoveTask(Task);
	}

	FSprite& FBattleSprites::CreateProxy(int32 Slot, int32 Height)
	{
		if (FSprite* Kept = SlotSprites[static_cast<std::size_t>(Slot)])
		{
			return *Kept;
		}
		const bool bWasCounting = Sprites->bCountingTasks;
		Sprites->bCountingTasks = false;
		FSpriteTask& Task = Sprites->CreateTask();
		FSprite& Sprite = Task.Sprite;
		// The sprite reset and laid out whole, drawing from the common effects' resource.
		Sprite = FSprite{};
		Sprite.Task = &Task;
		Sprite.Code = 0x2D;
		Sprite.TimeFlags = 0x100u << SpriteTimeFlags::TimeScaleShift;
		Sprite.bHasTransform = true;
		Sprite.bHasAnchors = true;
		Sprite.Resource = &CommonEffects;
		Sprite.BuildFlags = 4u << SpriteBuildFlags::KindShift;
		Sprite.Weight = Sprites->AnimationScale;
		const FBattleSlot& Record = Context->Slots[static_cast<std::size_t>(Slot)];
		Sprite.Position = {Wrap(int64{Record.Position.X} * 0x10000), 0, Wrap(int64{Record.Position.Z} * 0x10000)};
		Sprite.FrameTop = static_cast<int16>(Height);
		Sprite.Weight = CombatantScale;
		Sprite.FrameHalfHeight = static_cast<int16>(static_cast<uint16>(Sprite.FrameTop) >> 1);
		Sprites->SetScale(Sprite, CombatantScale);
		SlotSprites[static_cast<std::size_t>(Slot)] = &Sprite;
		SlotTasks[static_cast<std::size_t>(Slot)] = &Task;
		Sprites->bCountingTasks = bWasCounting;
		SetSlot(Sprite, Slot);
		Task.UpdateRole = ProxyRole.get();
		Task.EndRole = ProxyRole.get();
		return Sprite;
	}

	std::array<FBattleCameraSubject, FBattleSprites::SlotCount> FBattleSprites::CameraSubjects() const
	{
		std::array<FBattleCameraSubject, SlotCount> Subjects{};
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			FBattleCameraSubject& Subject = Subjects[static_cast<std::size_t>(Slot)];
			const FSprite* Sprite = SlotSprites[static_cast<std::size_t>(Slot)];
			const FBattleSlot& Record = Context->Slots[static_cast<std::size_t>(Slot)];
			Subject.bPresent = Sprite != nullptr;
			Subject.bHidden = Record.Presence != 0;
			Subject.bInGear = Record.InGear != 0;
			if (Sprite != nullptr)
			{
				Subject.Position = Sprite->Position;
				Subject.Height = Sprite->FrameTop;
			}
		}
		return Subjects;
	}

	EBattleSpriteRoutine FBattleSprites::UpdateRoutine(const FSpriteTask& Task) const
	{
		const ISpriteTaskRole* Role = Task.UpdateRole;
		if (Role == nullptr)
		{
			return Task.bCountsUpdates ? EBattleSpriteRoutine::Counted : EBattleSpriteRoutine::Kernel;
		}
		return Role == CombatantRole.get() ? EBattleSpriteRoutine::Combatant
			: Role == ProxyRole.get() ? EBattleSpriteRoutine::Proxy
			: Role == ControllerRole.get() ? EBattleSpriteRoutine::Controller
			: Role == BoneFollowRole.get() ? EBattleSpriteRoutine::FollowBone
			: EBattleSpriteRoutine::Orbit;
	}

	EBattleSpriteRoutine FBattleSprites::EndRoutine(const FSpriteTask& Task) const
	{
		const ISpriteTaskRole* Role = Task.EndRole;
		return Role == nullptr ? EBattleSpriteRoutine::Kernel
			: Role == CombatantRole.get() ? EBattleSpriteRoutine::Combatant
			: Role == ProxyRole.get() ? EBattleSpriteRoutine::Proxy
			: EBattleSpriteRoutine::Controller;
	}

	void FBattleSprites::FrameSlots(uint32 Slots)
	{
		Camera->Frame(Slots, CameraSubjects(), *Gte);
	}

	void FBattleSprites::Step(FSprite& Sprite)
	{
		// The kernel's ticking enters the interpreter with its loop counter, 0, in the register a BC mode divides by.
		RunStep(Sprite, 0);
	}

	void FBattleSprites::SnapToFloor(FSprite& Sprite)
	{
		FGteVector Point = Place(Sprite);
		const int32 Triangle = Floor->TriangleUnder(Point, Sprite.FloorTriangle);
		Floor->Height(Point, Triangle);
		Sprite.FloorTriangle = Triangle;
		Sprite.FloorY = Point[1];
	}

	FSpriteResource& FBattleSprites::KindResource(int32 Kind)
	{
		return Kind == 5 ? CommonEffects : EffectResource;
	}

	void FBattleSprites::SetUpController(FSpriteTask& Task)
	{
		FSprite& Sprite = Task.Sprite;
		const int32 Kind = Sprite.Kind();
		// Kinds 12 and 13 are 10 and 11 that take over from a controller already running.
		Sprite.Frame = static_cast<uint16>(Kind >= 12 ? 1 : 0);
		if (Kind >= 12)
		{
			Sprite.BuildFlags = (Sprite.BuildFlags & ~SpriteBuildFlags::KindMask) | static_cast<uint32>(Kind - 2) << SpriteBuildFlags::KindShift;
		}
		const bool bEye = Sprite.Kind() == EyeControllerKind;
		FSpriteTask*& Running = bEye ? EyeController : TargetController;
		if (Running == nullptr)
		{
			if (bEye)
			{
				SavedDesiredEye = Camera->DesiredEye;
			}
			else
			{
				SavedDesiredTarget = Camera->DesiredTarget;
			}
			Running = &Task;
		}
		else if (Running->Sprite.Frame != 1 && Sprite.Frame == 1)
		{
			Sprites->EndTask(*Running);
			Running = &Task;
		}
		else
		{
			// Not the camera's: it does nothing and ends at its first update.
			Sprite.Wait = 0;
			Sprite.Pc = FSprite::Stopped;
		}
		ControllerCount = static_cast<uint8>(ControllerCount + 1);
		Sprite.Heading = static_cast<int16>((Sprite.TimeFlags & SpriteTimeFlags::FacingMirrored) != 0 ? 0x800 : 0);
		Task.UpdateRole = ControllerRole.get();
		Task.EndRole = ControllerRole.get();
		Camera->SetMode(BattleCameraModes::Controllers);
		// It starts at the eye's controller place (a look-at controller of kind 11 at the look-at's).
		Sprite.Position = Kind == 11 ? Camera->TargetPoint : Camera->EyePoint;
	}

	void FBattleSprites::EndControllers()
	{
		// Each ended and then forgotten: an end may come back here when it was the last.
		if (EyeController != nullptr)
		{
			Sprites->EndTask(*EyeController);
			EyeController = nullptr;
		}
		if (TargetController != nullptr)
		{
			Sprites->EndTask(*TargetController);
			TargetController = nullptr;
		}
	}

	FSpriteBattlePoint FBattleSprites::Point(uint32 Mode, const FSprite& Sprite)
	{
		FSpriteBattlePoint Result;
		Result.bValid = true;
		const auto HighHalves = [](const std::array<int32, 3>& Point) {
			return FGteVector{static_cast<int16>(Point[0] >> 16), static_cast<int16>(Point[1] >> 16), static_cast<int16>(Point[2] >> 16)};
		};
		switch (Mode)
		{
		case 2:
		case 4:
		{
			// The targets' centre, the sprite counted in for mode 4; the sums wrap, the division truncates.
			std::array<int32, 3> Sum{};
			int32 Count = 0;
			for (const FSprite* Target : Play.Targets)
			{
				if (Target == nullptr)
				{
					break;
				}
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Sum[Axis] = Wrap(int64{Sum[Axis]} + Target->Position[Axis]);
				}
				++Count;
			}
			if (Mode == 4)
			{
				++Count;
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Sum[Axis] = Wrap(int64{Sum[Axis]} + Sprite.Position[Axis]);
				}
			}
			for (int32& Axis : Sum)
			{
				Axis = ProcessorDivide(Axis, Count);
			}
			Result.Point = HighHalves(Sum);
			return Result;
		}
		case 5:
		{
			// Nothing summed, divided by whatever the interpreter's register held: 0 gives the processor's -1.
			const int32 Axis = ProcessorDivide(0, static_cast<int32>(StepRegister)) >> 16;
			Result.Point = {static_cast<int16>(Axis), static_cast<int16>(Axis), static_cast<int16>(Axis)};
			return Result;
		}
		case 6:
			Result.Point = HighHalves(Camera->EyePoint);
			return Result;
		case 7:
			Result.Point = HighHalves(Camera->TargetPoint);
			return Result;
		case 0x17:
			Result.Point = {static_cast<int16>((0xA0 - (Gte->OffsetX >> 16)) * 2), static_cast<int16>((0x70 - (Gte->OffsetY >> 16)) * 2), static_cast<int16>(Sprite.Position[2] >> 16)};
			return Result;
		case 0x26:
		{
			const FBattleSlot& Record = Context->Slots[static_cast<std::size_t>(SlotOf(Sprite))];
			Result.Point = {static_cast<int16>(Record.Position.X), 0, static_cast<int16>(Record.Position.Z)};
			return Result;
		}
		default:
			Result.bValid = false;
			return Result;
		}
	}

	const FGteMatrix& FBattleSprites::View()
	{
		return Camera->View;
	}

	void FBattleSprites::PlaySound(const FSprite& Sprite, uint8 Effect, bool bSystem)
	{
		const int32 Set = bSystem ? SystemSoundSet : Sprite.SoundSet;
		if (Set >= 0)
		{
			Services->PlaySound(static_cast<uint32>(Effect) | static_cast<uint32>(Set) << 16);
		}
	}

	FSpriteFile* FBattleSprites::WritableFile(const FSpriteFile& File)
	{
		for (FSpriteFile* Kept : WritableFiles)
		{
			if (Kept == &File)
			{
				return Kept;
			}
		}
		return nullptr;
	}

	void FBattleSprites::ModelTinted(const FSprite& Sprite)
	{
		Services->RetintModel(Sprite);
	}

	int32 FBattleSprites::Install(const FBattleSpriteFile& File)
	{
		// An effect's sprites draw from the effect's resource at the effect's place in VRAM.
		constexpr int16 EffectTextureX = 0x380;
		constexpr int16 EffectTextureY = 0x100;
		constexpr int16 EffectClutY = 0x1F4;
		int32 SoundSet = -1;
		const std::vector<FBattleSpriteSection>& Sections = File.Sections();
		for (std::size_t Index = 0; Index < Sections.size(); ++Index)
		{
			const FBattleSpriteSection& Section = Sections[Index];
			switch (Section.Kind)
			{
			case EBattleSpriteSectionKind::SoundBank:
				Play.bSystemBankLoaded = false;
				Play.bHitVoices = false;
				Services->LoadSoundBank(File, Section);
				break;
			case EBattleSpriteSectionKind::SoundEffects:
				Services->RegisterSoundEffects(File, Section);
				SoundSet = File.Sprite().Word(Section.Offset + 0x14);
				break;
			case EBattleSpriteSectionKind::SpriteResource:
			{
				const FSpriteFile* Resource = File.SectionSprite(Index);
				if (Resource == nullptr)
				{
					ReportFault(0xC0FA);
					return SoundSet;
				}
				Sprites->ParseResource(EffectResource, *Resource, EffectTextureX, EffectTextureY, 0, EffectClutY);
				break;
			}
			}
		}
		return SoundSet;
	}

	int32 FBattleSprites::HeadingBetween(const FSprite& From, const FSprite& To) const
	{
		const int32 Across = static_cast<int16>(From.Position[2] >> 16) - static_cast<int16>(To.Position[2] >> 16);
		const int32 Along = static_cast<int16>(From.Position[0] >> 16) - static_cast<int16>(To.Position[0] >> 16);
		return -GteRatan2(Across, Along, *Arctangents) & 0xFFF;
	}

	int16 FBattleSprites::GoalFloor(FSprite& Sprite)
	{
		FGteVector Point{Sprite.SavedPoint[0], Sprite.SavedPoint[1], Sprite.SavedPoint[2]};
		const std::optional<int32> Found = Floor->FindTriangle(Point, Sprite.FloorTriangle, 4);
		const int32 Triangle = Found.value_or(-1) >= 0 ? *Found : Floor->ScanTriangles(Point);
		Floor->Height(Point, Triangle);
		return Sprite.SavedPoint[1] < Point[1] ? Sprite.SavedPoint[1] : Point[1];
	}

	namespace
	{
		/** The heading, and the velocity along it at the speed. */
		void SetHeadingSpeed(FSprite& Sprite, int16 Heading)
		{
			Sprite.Heading = Heading;
			const int32 Speed = Sprite.Speed >> 3;
			Sprite.Velocity[0] = Wrap(int64{GteCos(Heading) >> 1} * Speed) >> 8;
			Sprite.Velocity[2] = Wrap(-int64{Wrap(int64{GteSin(Heading) >> 1} * Speed)}) >> 8;
		}
	}

	void FBattleSprites::ThrowArc(FSprite& Sprite)
	{
		const int32 FloorY = GoalFloor(Sprite);
		const int32 Start = Sprite.Position[1];
		const int32 AlongX = Sprite.SavedPoint[0] - static_cast<int16>(Sprite.Position[0] >> 16);
		const int32 AlongZ = Sprite.SavedPoint[2] - static_cast<int16>(Sprite.Position[2] >> 16);
		const int32 Heading = GteRatan2(AlongZ, AlongX, *Arctangents);
		const int32 Distance = SquareRoot0(GteSquare(AlongX) + GteSquare(AlongZ));
		const int32 Rise = ProcessorDivide(Wrap(int64{-Sprite.Gravity} * Distance * 0x10), Sprite.Speed >> 11);
		const int32 Drop = ProcessorDivide(Wrap(int64{Sprite.Speed} * (Wrap(int64{FloorY} * 0x10000 - Start) >> 16)), Distance);
		Sprite.Velocity[1] = Wrap(int64{Rise} + Drop);
		SetHeadingSpeed(Sprite, static_cast<int16>(-Heading));
	}

	void FBattleSprites::JumpArc(FSprite& Sprite)
	{
		for (int32& Axis : Sprite.Position)
		{
			Axis = static_cast<int32>(static_cast<uint32>(Axis) & 0xFFFF0000u);
		}
		const int32 AlongX = Sprite.SavedPoint[0] - static_cast<int16>(Sprite.Position[0] >> 16);
		const int32 AlongZ = Sprite.SavedPoint[2] - static_cast<int16>(Sprite.Position[2] >> 16);
		// The steps to come back down to the height it leaves from.
		const int32 Steps = Wrap(-int64{ProcessorDivide(Wrap(int64{Sprite.Velocity[1]} * 2), Sprite.Gravity)});
		const int32 Heading = GteRatan2(AlongZ, AlongX, *Arctangents);
		const int32 Distance = SquareRoot0(GteSquare(AlongX) + GteSquare(AlongZ));
		Sprite.Speed = Steps == 0 ? 0 : ProcessorDivide(Wrap(int64{Distance} * 0x10000), Steps);
		const int32 FloorY = GoalFloor(Sprite);
		if (Steps != 0)
		{
			Sprite.Velocity[1] = Wrap(int64{Sprite.Velocity[1]} + ProcessorDivide(Wrap(int64{FloorY} * 0x10000 - Sprite.Position[1]), Steps));
		}
		SetHeadingSpeed(Sprite, static_cast<int16>(-Heading));
		Sprites->ApplyGravity(Sprite);
	}

	int32 FBattleSprites::TargetIndex(const FSprite* Target) const
	{
		for (int32 Index = 0; Index < Play.TargetCount; ++Index)
		{
			if (Play.Targets[static_cast<std::size_t>(Index)] == Target)
			{
				return Index;
			}
		}
		return Play.TargetCount;
	}

	void FBattleSprites::NextTarget(FSprite& Sprite)
	{
		const int32 Index = TargetIndex(Sprite.Target);
		Sprite.Target = Index < Play.TargetCount ? Play.Targets[static_cast<std::size_t>(Index + 1)] : Play.Targets[0];
	}

	void FBattleSprites::StartOrbit(FSprite& Sprite)
	{
		// The task's update becomes the orbit; its end stays what it was.
		Sprite.Frame = 1;
		if (Sprite.Task != nullptr && Sprites->ReachesTask(Sprite, 0))
		{
			Sprite.Task->UpdateRole = OrbitRole.get();
		}
	}

	void FBattleSprites::ReportFault(uint32 Value)
	{
		Sprites->Report(ESpriteFault::UnsupportedInstruction, Value);
	}
}
