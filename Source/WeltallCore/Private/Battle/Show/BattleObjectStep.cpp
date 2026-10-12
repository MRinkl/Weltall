#include "BattleObjectMachine.h"

#include "Battle/BattleContext.h"
#include "Battle/Show/BattleShowFloor.h"
#include "Battle/Show/BattleStageFiles.h"
#include "Graphics/GteVectors.h"

#include <string>

namespace Weltall
{
	namespace
	{
		using namespace BattleObjectMachine;

		constexpr int32 MaxAccumulated = 6;
		constexpr uint16 WindPhaseStep = 56;
		constexpr uint16 PulsePhaseStep = 0x80;
		constexpr int32 WhoFirstTarget = 0xFF;
		constexpr int32 WhoPerformer = 0xFE;
		constexpr int32 WhoSelf = 0xFD;
		constexpr int32 WhoOwner = 0xFC;
		constexpr int32 WhoStage = 0xFA;
		constexpr int32 NoTargetSlot = 13;

		/** The kernel's shadow scaling of a byte: Value + Level * Scale / 256 (the division C's), clamped to 255 above. */
		uint8 AddScaled(int16 Level, int16 Scale, uint8 Value)
		{
			int32 Product = Scale * Level;
			if (Product < 0)
			{
				Product += 0xFF;
			}
			const int32 Sum = Value + (Product >> 8);
			return static_cast<int16>(Sum) > 0xFF ? uint8{0xFF} : static_cast<uint8>(Sum);
		}

		void StagePushOut(FBattleObjects& Objects, int32 Slot)
		{
			for (const FBattleStageObstacle& Obstacle : Objects.Stage->Obstacles())
			{
				PushOut(Objects, Slot, Obstacle.X, Obstacle.Z, Objects.Radius(Slot) + Obstacle.Radius);
			}
		}

		/** The floor, the matrices, Ticks steps of the tracks and key events, then the sequence. */
		void Step(FBattleObjects& Objects, int32 Slot, int32 Ticks, int32 CatchUp)
		{
			FBattleObject& Object = *Objects.Object(Slot);
			uint32 Events = 0;
			if (Object.bVisible != 0)
			{
				SnapToFloor(Objects, Object);
				if (Object.ScaledBones == 0)
				{
					ComputeGearBones(Object.Bones, Object.Scale);
				}
				else if (!ComputeGearBonesScaled(Object.Bones, Object.Scale))
				{
					Objects.Fail("object " + std::to_string(Slot) + "'s scaled bones divide by a parent's scale of 0");
					return;
				}
				for (int32 Tick = 0; Tick < Ticks && !Objects.Faulted(); ++Tick)
				{
					Events |= GearMachine::StepChannels(Objects, Object);
					RunKeyEvents(Objects, Slot);
				}
			}
			if (!Objects.Faulted() && Objects.Object(Slot) != nullptr)
			{
				RunSequence(Objects, Slot, static_cast<int32>(Events), Ticks, CatchUp);
			}
		}
	}

	namespace BattleObjectMachine
	{
		int32 TargetDistance(FBattleObjects& Objects, const FBattleObject& Object)
		{
			return GearMachine::TargetDistance(Objects, Object);
		}

		void SnapToFloor(FBattleObjects& Objects, FBattleObject& Object)
		{
			FGearBone& Root = Object.Bones[0];
			FGteVector Point = {static_cast<int16>(Root.Position[0]), static_cast<int16>(Root.Position[1]), static_cast<int16>(Root.Position[2])};
			const std::optional<int32> Found = Objects.Floor->FindTriangle(Point, Object.FloorTriangle, 4);
			Object.FloorTriangle = static_cast<int16>(*Found);
			if (Object.FloorTriangle < 0)
			{
				Object.FloorTriangle = static_cast<int16>(Objects.Floor->ScanTriangles(Point));
			}
			Objects.Floor->Height(Point, Object.FloorTriangle);
			Object.FloorY = Point[1];
			if (Object.NoFloorSnap == 0)
			{
				Root.Position[1] = Point[1];
			}
		}

		void PushOut(FBattleObjects& Objects, int32 Slot, int32 X, int32 Z, int32 Radius)
		{
			FBattleObject* Object = Objects.Object(Slot);
			if (Object == nullptr)
			{
				Objects.Fail("the empty slot " + std::to_string(Slot) + " is pushed out");
				return;
			}
			FGearBone& Root = Object->Bones[0];
			const int32 Dx = X - Root.Position[0];
			const int32 Dz = Z - Root.Position[2];
			const int32 Distance = GearMachine::Distance(Objects, Dx, 0, Dz) + 1;
			if (Distance >= Radius || Objects.Faulted())
			{
				return;
			}
			int32 Away = GearMachine::Ratan2(Objects, Dz, -Dx);
			const uint32 Facing = static_cast<uint32>(Root.Angles[1] - Away) & 0xFFF;
			if (Facing - 0x401 < 0x7FF)
			{
				Away += 0x800;
			}
			const auto Push = [&](int32 Wave) {
				int32 Product = Wave * (Radius - Distance);
				return (Product < 0 ? Product + 0xFFF : Product) >> 12;
			};
			Root.Position[0] = X - GearMachine::Divide(Objects, Dx * Radius, Distance) - Push(GteSin(Away));
			Root.Position[2] = Z - GearMachine::Divide(Objects, Dz * Radius, Distance) - Push(GteCos(Away));
		}

		void UpdateTargetPoint(FBattleObjects& Objects, FBattleObject& Object)
		{
			const int32 Who = Object.Who;
			uint32 Slot = 0;
			if (Who == WhoFirstTarget)
			{
				while (Slot < NoTargetSlot && ((Object.Targets >> Slot) & 1) == 0)
				{
					++Slot;
				}
			}
			if (Who == WhoPerformer)
			{
				Slot = Objects.Performer;
			}
			if (Who == WhoSelf)
			{
				Slot = Object.Slot;
			}
			if (Who == WhoOwner)
			{
				Slot = Object.Owner;
			}
			if (Who == WhoStage)
			{
				Slot = FBattleObjects::StageSlot;
			}
			if (static_cast<uint32>(Who - 1) < 0x7F)
			{
				Slot = static_cast<uint32>(Who - 1);
			}
			if (Slot >= static_cast<uint32>(FBattleObjects::SlotCount))
			{
				// The original takes the word past its 32 objects' pointers for an object.
				Objects.Fail("an object's target follows slot " + std::to_string(Slot) + ", past the objects' slots");
				return;
			}
			const FBattleObject* Followed = Objects.Object(static_cast<int32>(Slot));
			if (Followed == nullptr || Slot == Object.Slot)
			{
				if (const std::optional<std::array<int16, 3>> Place = SlotPlace(Objects, static_cast<int32>(Slot)))
				{
					Object.Target = *Place;
				}
				return;
			}
			const FGearBone& Root = Followed->Bones[0];
			FGteMatrix Place = Root.Local;
			if (Object.WhoBone != 0)
			{
				if (static_cast<std::size_t>(Object.WhoBone) >= Followed->Bones.size())
				{
					Objects.Fail("an object's target follows bone " + std::to_string(Object.WhoBone) + ", which its object does not have");
					return;
				}
				Place = CompMatrix(Root.Local, Followed->Bones[static_cast<std::size_t>(Object.WhoBone)].World);
			}
			const FGteLongVector Point = RotTrans(Place, {Object.WhoOffset[0], Object.WhoOffset[1], Object.WhoOffset[2]});
			Object.Target = {static_cast<int16>(Point[0]), static_cast<int16>(Point[1]), static_cast<int16>(Point[2])};
			const int16 Turn = Object.Bones[0].Rotation;
			if (Turn != FGearBone::NoChannel)
			{
				FGearChannel& Channel = Objects.Channels[Turn];
				if (static_cast<uint8>(Channel.Kind - GearChannelKind::TurnToward) < 2)
				{
					Channel.Words[3] = Object.Target[0];
					Channel.Words[4] = Object.Target[1];
					Channel.Words[5] = Object.Target[2];
				}
			}
		}

		void FollowOwner(FBattleObjects& Objects, FBattleObject& Object)
		{
			const FBattleObject* Host = Objects.Object(Object.Host);
			if (Host == nullptr)
			{
				Object.Host = FBattleObject::NoHost;
				return;
			}
			if ((Object.Flags & BattleObjectFlags::OwnVisibility) == 0)
			{
				Object.bVisible = Host->bVisible;
			}
			if (Object.bVisible == 0)
			{
				return;
			}
			const int32 Bone = static_cast<int16>(Object.HostBone);
			if (Bone < 0 || static_cast<std::size_t>(Bone) >= Host->Bones.size())
			{
				Objects.Fail("an object rides bone " + std::to_string(Bone) + ", which its host does not have");
				return;
			}
			const FGearBone& HostRoot = Host->Bones[0];
			FGearBone& Root = Object.Bones[0];
			if (Object.HostTurns != 0)
			{
				const FGteMatrix Turn = Bone == 0 ? HostRoot.World : MulMatrix(HostRoot.World, Host->Bones[static_cast<std::size_t>(Bone)].World);
				Root.Local.M = MulMatrix(Turn, Root.Local).M;
				Root.World.M = MulMatrix(Turn, Root.World).M;
			}
			const FGteMatrix Place = Bone == 0 ? HostRoot.Local : CompMatrix(HostRoot.Local, Host->Bones[static_cast<std::size_t>(Bone)].World);
			const FGteLongVector Where = RotTrans(Place, {Object.AttachOffset[0], Object.AttachOffset[1], Object.AttachOffset[2]});
			Root.Local.T = Where;
			Root.Position = Where;
		}
	}

	void FBattleObjects::UpdateDraw(const FGteMatrix& View, int32 CatchUp, int32 Buffer)
	{
		if (Faulted())
		{
			return;
		}
		Accumulator += 1 + CatchUp;
		if (Accumulator > MaxAccumulated)
		{
			Accumulator = MaxAccumulated;
		}
		int32 Ticks = 0;
		while (Accumulator > 1)
		{
			Accumulator -= 2;
			++Ticks;
		}
		WindPhase = static_cast<uint16>(WindPhase + WindPhaseStep * Ticks);
		ClothWind = static_cast<int16>((GteCos(static_cast<int16>(WindPhase)) + 0x1000) / 800 + 4);
		PulsePhase = static_cast<uint16>(PulsePhase + PulsePhaseStep);
		const int32 Wave = GteCos(static_cast<int16>(PulsePhase)) + 0x1000;
		PulseLevel = static_cast<int16>((Wave < 0 ? Wave + 0x1F : Wave) >> 5);
		const std::array<uint8, 3> Ambient = Stage->AmbientColour();
		for (std::size_t Channel = 0; Channel < 3; ++Channel)
		{
			PulsedAmbient[Channel] = AddScaled(PulseLevel, 0x20, Ambient[Channel]);
		}
		const FBattleObject* Acting = Object(Performer);
		if (Acting != nullptr && (Acting->Flags & BattleObjectFlags::Immovable) == 0)
		{
			for (int32 Slot = 0; Slot < CombatantSlots && !Faulted(); ++Slot)
			{
				const FBattleObject* Other = Object(Slot);
				if (Other == nullptr || Performer == Slot || Other->Host != FBattleObject::NoHost || ((PlayTargets >> Slot) & 1) != 0 || Other->bVisible == 0)
				{
					continue;
				}
				const int32 OtherY = Other->Bones[0].Position[1];
				const int32 MoverY = Object(Performer)->Bones[0].Position[1];
				if (OtherY - Height(Slot) < MoverY && MoverY - Height(Performer) < OtherY)
				{
					BattleObjectMachine::PushOut(*this, Performer, Other->Bones[0].Position[0], Other->Bones[0].Position[2], Radius(Slot) + Radius(Performer));
				}
			}
			StagePushOut(*this, Performer);
		}
		for (int32 Slot = 0; Slot < SlotCount && !Faulted(); ++Slot)
		{
			if (Object(Slot) != nullptr)
			{
				Step(*this, Slot, Ticks, CatchUp);
			}
		}
		if (ModelCamera.bOn && !Faulted())
		{
			BattleObjectMachine::StepCameraTracks(*this, Ticks, Buffer);
		}
		for (int32 Slot = 0; Slot < SlotCount && !Faulted(); ++Slot)
		{
			FBattleObject* Riding = Object(Slot);
			if (Riding != nullptr && Riding->Host != FBattleObject::NoHost)
			{
				BattleObjectMachine::FollowOwner(*this, *Riding);
			}
		}
		if (Faulted())
		{
			return;
		}
		UpdatePointLights();
		Gte->ColorMatrix = ColourMatrix;
		for (int32 Slot = 0; Slot < StageSlot && bObjectsVisible && !Faulted(); ++Slot)
		{
			if (Object(Slot) != nullptr)
			{
				DrawObject(Slot, View, CatchUp);
			}
		}
		if (!Faulted())
		{
			DrawTrails(View, Ticks);
		}
	}

	void FBattleObjects::StepStageClutAnimations(int32 CatchUp)
	{
		if (FBattleObject* StageObject = Object(StageSlot))
		{
			for (FBattleClutAnimation& Animation : StageObject->ClutAnimations)
			{
				BattleObjectMachine::StepClutAnimation(*this, *StageObject, Animation, CatchUp);
			}
		}
	}

	void FBattleObjects::BeginStageTint(const FVramRect& Palettes)
	{
		if (FBattleObject* StageObject = Object(StageSlot))
		{
			for (FBattleClutAnimation& Animation : StageObject->ClutAnimations)
			{
				if (Animation.bActive)
				{
					Animation.Feeds = FBattleClutAnimation::FeedsStageTint;
				}
			}
		}
		// A blending animation that never steps: room for a first source, read from VRAM, and for a second.
		BattleObjectMachine::FBattleClutStart Start;
		Start.Type = 1;
		Start.Flags = 0x400 | 0x200 | 1;
		Start.Values = {static_cast<uint16>(Palettes.X), static_cast<uint16>(Palettes.Y), 0, 0, 0, 0};
		Start.X = Palettes.X;
		Start.Y = Palettes.Y;
		Start.Width = Palettes.Width;
		Start.Height = Palettes.Height;
		BattleObjectMachine::StartClutAnimation(*this, StageTintPalette, Start);
	}

	void FBattleObjects::EndStageTint()
	{
		if (!StageTintPalette.bActive)
		{
			return;
		}
		if (FBattleObject* StageObject = Object(StageSlot))
		{
			for (FBattleClutAnimation& Animation : StageObject->ClutAnimations)
			{
				if (Animation.bActive)
				{
					Animation.Feeds = FBattleClutAnimation::FeedsNothing;
				}
			}
		}
		Services->LoadImage(StageTintPalette.Rect, StageTintPalette.Source);
		BattleObjectMachine::StopClutAnimation(*this, StageTintPalette);
	}
}
