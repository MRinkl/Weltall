#include "WorldMap/WorldMapOps.h"

#include "Game/PsxRandom.h"
#include "Graphics/GteVectors.h"

namespace Weltall::WorldMapOps
{
	namespace
	{
		/** The wave phase's step a tick, which the sea's height reads. */
		constexpr int32 WaveStep = 0x40;
		/** The sprites turn toward their heading by at most this a tick. */
		constexpr int32 FacingTurn = 0x100;
		/** The model InitObjects hides, whatever the scene. */
		constexpr int32 HiddenObject = 4;
		/** The era 0 spinner's two models and its turn a tick. */
		constexpr int32 SpinnerObjects[2] = {12, 14};
		constexpr int32 SpinnerStep = 8;
		constexpr int32 CloudTiles = 4;
		constexpr int32 CloudTileUnits = 0x800;
		/** A cloud drifts east and north by a multiple (1-4) of these a tick, and wraps within the four tiles. */
		constexpr int32 CloudDriftXStep = 0xDDB;
		constexpr int32 CloudDriftZStep = -0x800;
		constexpr int32 CloudSpan = CloudTiles * CloudTileUnits * 0x1000;

		int32 Negate(int32 Value)
		{
			return static_cast<int32>(0u - static_cast<uint32>(Value));
		}

		int32 ShiftLeft(int32 Value, int32 Shift)
		{
			return static_cast<int32>(static_cast<uint32>(Value) << Shift);
		}

		void PlaceCamera(FWorldMapContext& Context)
		{
			if (Context.LookAt == 0)
			{
				SetCameraMatrix(Context);
			}
			else
			{
				LookAtCamera(Context);
			}
		}

		void TickParticles(FWorldMapContext& Context)
		{
			Context.Particles.Tick(*Context.Random, *Context.Arctangents);
			if (Context.Drawing != nullptr)
			{
				Context.Drawing->DrawParticles(Context);
			}
		}

		/** The frame after the sprites: the models placed, the clouds drifted, the world drawn, the waves moved on. */
		void FinishFrame(FWorldMapContext& Context)
		{
			PlaceAttachedObjects(Context);
			// The clouds drift as their drawing starts, after the world's; nothing drawn before them reads them.
			DriftClouds(Context);
			if (Context.Drawing != nullptr)
			{
				Context.Drawing->DrawWorld(Context);
			}
			Context.bTerrainRestarted = false;
			Context.WavePhase += WaveStep;
		}

		/** The sprites' logic: each sprite placed about the camera's target, its depth measured, and a near one turned and animated. */
		void PlaceSprites(FWorldMapContext& Context)
		{
			std::array<int32, FWorldMapContext::TaskCount> Depths{};
			for (int32 Slot = 0; Slot < FWorldMapContext::TaskCount; ++Slot)
			{
				const FWorldMapTask& Task = Context.Tasks[static_cast<std::size_t>(Slot)];
				FSprite* Sprite = SlotSprite(Context, Task);
				if (Task.Hidden != 0 || Sprite == nullptr)
				{
					continue;
				}
				FWorldMapVector Delta{Task.Position.X - Context.CameraTarget.X, 0, Task.Position.Z - Context.CameraTarget.Z, 0};
				Context.Ground.WrapDelta(Delta);
				Sprite->Position[0] = ShiftLeft(Delta.X, 4);
				Sprite->Position[2] = ShiftLeft(Negate(Delta.Z), 4);
				Sprite->Position[1] = ShiftLeft(Task.Position.Y, 4);
			}
			FGte Gte;
			Gte.Transform = Context.CameraMatrix;
			for (int32 Slot = 0; Slot < FWorldMapContext::TaskCount; ++Slot)
			{
				const FWorldMapTask& Task = Context.Tasks[static_cast<std::size_t>(Slot)];
				const FSprite* Sprite = SlotSprite(Context, Task);
				if (Task.Hidden != 0 || Sprite == nullptr)
				{
					continue;
				}
				// The sprite's place in whole units: the high halves of its 16.16 position.
				const FGteVector Vertex{static_cast<int16>(Sprite->Position[0] >> 16), static_cast<int16>(Sprite->Position[1] >> 16),
					static_cast<int16>(Sprite->Position[2] >> 16)};
				Depths[static_cast<std::size_t>(Slot)] = Gte.Rtps(Vertex).ScreenZ;
			}
			for (int32 Slot = 0; Slot < FWorldMapContext::TaskCount; ++Slot)
			{
				FWorldMapTask& Task = Context.Tasks[static_cast<std::size_t>(Slot)];
				FSprite* Sprite = SlotSprite(Context, Task);
				const int32 Depth = Depths[static_cast<std::size_t>(Slot)];
				if (Task.Hidden != 0 || Sprite == nullptr || Depth >= FWorldMapContext::SpriteDepthLimit)
				{
					Context.SpriteDepths[static_cast<std::size_t>(Slot)] = -1;
					continue;
				}
				Context.SpriteDepths[static_cast<std::size_t>(Slot)] = Depth;
				if (Context.Drawing != nullptr)
				{
					Context.Drawing->DrawSprite(Context, Slot, *Sprite, Depth >> 4);
				}
				const int32 Heading = Task.Heading;
				const int32 Drawn = Task.Word(WorldMapTaskWord::DrawnFacing);
				int32 Turn = Heading - Drawn;
				if (Turn < 0)
				{
					Turn += 0x1000;
				}
				int32 Facing = Heading;
				if (Turn <= 0x800)
				{
					if (Turn >= FacingTurn)
					{
						Facing = Drawn + FacingTurn;
					}
				}
				else if (Turn - 0x1000 <= -FacingTurn)
				{
					Facing = Drawn - FacingTurn;
				}
				Task.Word(WorldMapTaskWord::DrawnFacing) = Facing;
				Context.Sprites.SetFacing(*Sprite, (Facing - Context.CameraYaw - Context.ViewTurn - 0x400) & 0xFFF);
				Context.Sprites.TickAnimation(*Sprite);
			}
		}
	}

	FResult UpdateFrame(FWorldMapContext& Context, int32)
	{
		Context.FrameTask = EWorldMapFrameTask::Map;
		PlaceCamera(Context);
		TickParticles(Context);
		PlaceSprites(Context);
		Context.ShadowsShown = Context.ShadowCount;
		Context.ShadowCount = 0;
		FinishFrame(Context);
		return WorldMapTaskState::Update;
	}

	FResult UpdateCutsceneFrame(FWorldMapContext& Context, int32, EWorldMapFrameTask Kind)
	{
		Context.FrameTask = Kind;
		PlaceCamera(Context);
		TickParticles(Context);
		FinishFrame(Context);
		return WorldMapTaskState::Update;
	}

	FResult InitIdleTask(FWorldMapContext&, int32)
	{
		return WorldMapTaskState::Idle;
	}

	void InitObjects(FWorldMapContext& Context)
	{
		Context.Objects.clear();
		for (const FWorldMapPlacement& Placement : Context.Scene->Placements)
		{
			FWorldMapObject Object;
			Object.Part = Placement.Part;
			Object.Flags = Placement.Flags;
			Object.X = Placement.X;
			Object.Y = Placement.Y;
			Object.Z = -Placement.Z;
			Object.Angles = Placement.Angles;
			Object.Matrix.M = RotationYxz(Placement.Angles).M;
			Context.Objects.push_back(Object);
		}
		if (Context.Objects.size() <= static_cast<std::size_t>(HiddenObject))
		{
			// The program hides the fifth model whatever the scene: with fewer, its halfword lands past the models' block.
			Context.StrayHiddenWrite = static_cast<int32>((HiddenObject - static_cast<int32>(Context.Objects.size())) * WorldMapObjectSize);
			return;
		}
		Context.StrayHiddenWrite = -1;
		Context.Objects[HiddenObject].Hidden = 1;
	}

	void AttachObject(FWorldMapContext& Context, int32 Parent, int32 Child)
	{
		Context.Objects[static_cast<std::size_t>(Child)].Parent = Parent;
	}

	void PlaceAttachedObjects(FWorldMapContext& Context)
	{
		for (const FWorldMapObject& Object : Context.Objects)
		{
			if (Object.Hidden != 0)
			{
				continue;
			}
			for (int32 Parent = Object.Parent; Parent != -1;)
			{
				FWorldMapObject& Holder = Context.Objects[static_cast<std::size_t>(Parent)];
				Holder.Matrix.T = {Holder.X, Holder.Y, Negate(Holder.Z)};
				Parent = Holder.Parent;
			}
		}
	}

	FResult InitSpinner(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = Context.Tasks[static_cast<std::size_t>(Slot)];
		Task.Word(WorldMapTaskWord::SpinAngle) = 0;
		Task.Word(WorldMapTaskWord::SpinStep) = SpinnerStep;
		return WorldMapTaskState::Update;
	}

	FResult UpdateSpinner(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = Context.Tasks[static_cast<std::size_t>(Slot)];
		const int32 Angle = (Task.Word(WorldMapTaskWord::SpinAngle) + Task.Word(WorldMapTaskWord::SpinStep)) & 0xFFF;
		Task.Word(WorldMapTaskWord::SpinAngle) = Angle;
		for (const int32 Number : SpinnerObjects)
		{
			if (Context.Objects.size() <= static_cast<std::size_t>(Number))
			{
				Context.Fault = MakeError("the world map's spinner turns model " + std::to_string(Number) + ", which the scene lacks");
				return Task.State;
			}
			FWorldMapObject& Object = Context.Objects[static_cast<std::size_t>(Number)];
			Object.Matrix.M = RotationXyz({0, Object.Angles[1], static_cast<int16>(Angle)}).M;
		}
		return WorldMapTaskState::Update;
	}

	void InitClouds(FWorldMapContext& Context)
	{
		std::size_t Index = 0;
		for (int32 TileZ = 0; TileZ < CloudTiles; ++TileZ)
		{
			for (int32 TileX = 0; TileX < CloudTiles; ++TileX)
			{
				for (const std::array<int32, 2>& Place : Context.Tables->CloudPlaces)
				{
					FWorldMapCloud& Cloud = Context.Clouds[Index++];
					Cloud.Position.X = (Place[0] + TileX * CloudTileUnits) * 0x1000;
					Cloud.Position.Z = (Place[1] + TileZ * CloudTileUnits) * 0x1000;
					Cloud.Position.Y = ((Context.Random->Next() >> 10) * -8 - 0x200) * 0x1000;
				}
			}
		}
		for (FWorldMapCloud& Cloud : Context.Clouds)
		{
			const int32 Speed = (Context.Random->Next() & 3) + 1;
			Cloud.DriftX = static_cast<int16>(Speed * CloudDriftXStep);
			Cloud.DriftZ = static_cast<int16>(Speed * CloudDriftZStep);
			Cloud.Look = static_cast<int16>(Context.Random->Next() & 1);
		}
	}

	void DriftClouds(FWorldMapContext& Context)
	{
		const auto Drift = [](int32& Coordinate, int32 Step)
		{
			Coordinate += Step;
			if (Coordinate >= CloudSpan)
			{
				Coordinate -= CloudSpan;
			}
			if (Coordinate < 0)
			{
				Coordinate += CloudSpan;
			}
		};
		for (FWorldMapCloud& Cloud : Context.Clouds)
		{
			Drift(Cloud.Position.X, Cloud.DriftX);
			Drift(Cloud.Position.Z, Cloud.DriftZ);
		}
	}
}
