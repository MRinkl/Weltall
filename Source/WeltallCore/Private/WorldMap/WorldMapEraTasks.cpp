#include "WorldMap/WorldMapOps.h"

#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"
#include "WorldMap/WorldMapStateLayout.h"

#include <cstdlib>

namespace Weltall::WorldMapOps
{
	namespace
	{
		/** Thames stands still until the story reaches this value, then cruises its waypoints. */
		constexpr uint16 ThamesCruiseStory = 0xCD;
		constexpr int32 ThamesMooring[2] = {0xD80000, 0x7280000};
		constexpr int16 ThamesFullSpeed = 0x4000;
		constexpr int32 ThamesSlowSpeed = 0x1000;
		constexpr int32 ThamesEffect = 0x13;
		constexpr int32 ThamesRadius = 0x80;
		constexpr int32 ThamesHeight = 0xB0;
		/** How near a waypoint Thames turns for the next, and how near the ship it slows and stops. */
		constexpr int32 WaypointReach = 8;
		constexpr int32 ThamesStopDistance = 0x100;
		constexpr int32 ThamesSlowDistance = 0x300;
		constexpr int32 ThamesShipLowest = -0xC0;
		constexpr int32 ShevatStart[3] = {0, static_cast<int32>(0xFFD80000u), 0x4800000};
		constexpr int32 ShevatDrift = 0xB50;
		constexpr int32 ShevatRadius = 0x180;
		constexpr int32 ShevatHeight = 0xC0;
		constexpr int32 ShevatStopDistance = 0x300;
		constexpr int32 ShevatShipHighest = -0x240;
		/** The model the story object's parts hang on, the story's value it shows at, and where. */
		constexpr int32 StoryHolder = 69;
		constexpr uint16 StoryShown = 0x99;
		constexpr int32 StoryPlace[2] = {0x2000000, 0x4120000};
		constexpr int32 LandmarkBObject = 75;

		FWorldMapTask& TaskAt(FWorldMapContext& Context, int32 Slot)
		{
			return Context.Tasks[static_cast<std::size_t>(Slot)];
		}

		FWorldMapObject& ObjectAt(FWorldMapContext& Context, int32 Number)
		{
			return Context.Objects[static_cast<std::size_t>(Number)];
		}

		uint16 Story(const FWorldMapContext& Context)
		{
			return static_cast<uint16>(StateWord(Context, WorldMapStateLayout::StoryVariable));
		}

		void PlaceObject(FWorldMapObject& Object, const FWorldMapVector& Position)
		{
			Object.X = Position.X >> 12;
			Object.Y = Position.Y >> 12;
			Object.Z = Position.Z >> 12;
		}

		/** The ship's place against another's (whole units): the distance across, and the ship's height. */
		int32 DistanceToShip(const FWorldMapContext& Context, const FWorldMapTask& Task, int32& ShipY)
		{
			int32 DeltaX = static_cast<uint16>(StateWord(Context, WorldMapStateLayout::VehicleX)) - (Task.Position.X >> 12);
			int32 DeltaZ = static_cast<uint16>(StateWord(Context, WorldMapStateLayout::VehicleZ)) - (Task.Position.Z >> 12);
			ShipY = StateWord(Context, WorldMapStateLayout::VehicleY);
			Context.Ground.WrapDeltaUnits(DeltaX, DeltaZ);
			return SquareRoot0(DeltaX * DeltaX + DeltaZ * DeltaZ);
		}

		int32 ThamesObject(const FWorldMapContext& Context)
		{
			return Context.Tables->ThamesObjects[static_cast<std::size_t>(Context.Era)];
		}

		int32 ShevatObject(const FWorldMapContext& Context)
		{
			return Context.Tables->ShevatObjects[static_cast<std::size_t>(Context.Era)];
		}

		/** The way (4.12) from Thames to its waypoint, the waypoint first passed when Thames is on it. */
		FGteLongVector ThamesWay(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			int32& Waypoint = Task.Word(WorldMapTaskWord::ThamesWaypoint);
			const std::array<uint16, 2>& Next = Context.Tables->ThamesWaypoints[static_cast<std::size_t>(Waypoint)];
			const int32 ToX = std::abs(Task.Position.X - (Next[0] << 12)) >> 12;
			const int32 ToZ = std::abs(Task.Position.Z - (Next[1] << 12)) >> 12;
			if (SquareRoot0(ToX * ToX + ToZ * ToZ) < WaypointReach)
			{
				Waypoint = (Waypoint + 1) & 7;
			}
			const std::array<uint16, 2>& Goal = Context.Tables->ThamesWaypoints[static_cast<std::size_t>(Waypoint)];
			int32 WayX = Goal[0] - (Task.Position.X >> 12);
			int32 WayZ = Goal[1] - (Task.Position.Z >> 12);
			Context.Ground.WrapDeltaUnits(WayX, WayZ);
			return VectorNormal({WayX, 0, WayZ});
		}

		/** A model's matrix turned to look along a way (4.12), up being up. */
		FGteMatrix LookAlong(const FGteLongVector& Way)
		{
			const FGteLongVector Side = VectorNormal(OuterProduct12({0, FGteMatrix::One, 0}, Way));
			const FGteLongVector Up = VectorNormal(OuterProduct12(Way, Side));
			FGteMatrix Rows;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Rows.M[0][Axis] = static_cast<int16>(Side[Axis]);
				Rows.M[1][Axis] = static_cast<int16>(Up[Axis]);
				Rows.M[2][Axis] = static_cast<int16>(Way[Axis]);
			}
			return Transpose(Rows);
		}

		/** The angles (x, y, z) a rotation matrix turns by, for an effect's turn; none for a matrix whose last row has neither x nor z. */
		FGteVector AnglesOf(const FGteMatrix& Matrix, const FGteArctangents& Arctangents)
		{
			FGteVector Angles{};
			AnglesFromMatrix(Matrix, Arctangents, Angles);
			return Angles;
		}

		void SaveThames(FWorldMapContext& Context, const FWorldMapTask& Task)
		{
			SetStateWord(Context, WorldMapStateLayout::ThamesX, static_cast<int16>(Task.Position.X >> 12));
			SetStateWord(Context, WorldMapStateLayout::ThamesZ, static_cast<int16>(Task.Position.Z >> 12));
			SetStateWord(Context, WorldMapStateLayout::ThamesWaypoint, static_cast<int16>(Task.Word(WorldMapTaskWord::ThamesWaypoint)));
		}

		void SaveShevat(FWorldMapContext& Context, const FWorldMapTask& Task)
		{
			SetStateWord(Context, WorldMapStateLayout::ShevatXLow, static_cast<int16>(Task.Position.X));
			SetStateWord(Context, WorldMapStateLayout::ShevatX, static_cast<int16>(Task.Position.X >> 12));
			SetStateWord(Context, WorldMapStateLayout::ShevatZLow, static_cast<int16>(Task.Position.Z));
			SetStateWord(Context, WorldMapStateLayout::ShevatZ, static_cast<int16>(Task.Position.Z >> 12));
		}

		/** The game's rotation matrix (x, y, z) of a spinning model: its own y angle kept, the spin its z. */
		void Spin(FWorldMapObject& Object, int16 Y, int16 Z)
		{
			Object.Matrix.M = RotationXyz({0, Y, Z}).M;
		}
	}

	FResult RestoreThames(FWorldMapContext& Context, int32)
	{
		const int32 Hull = ThamesObject(Context);
		for (int32 Part = 4; Part > 0; --Part)
		{
			AttachObject(Context, Hull, Hull - Part);
		}
		return WorldMapTaskState::Update;
	}

	FResult InitThames(FWorldMapContext& Context, int32 Slot)
	{
		RestoreThames(Context, Slot);
		FWorldMapTask& Task = TaskAt(Context, Slot);
		Task.Speed = ThamesFullSpeed;
		Task.Step = 0;
		if (Story(Context) < ThamesCruiseStory)
		{
			Task.Position.X = ThamesMooring[0];
			Task.Position.Z = ThamesMooring[1];
			ObjectAt(Context, ThamesObject(Context)).Matrix = FGteMatrix::Identity();
			Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
			return WorldMapTaskState::Update;
		}
		int32& Waypoint = Task.Word(WorldMapTaskWord::ThamesWaypoint);
		uint32 Z = 0;
		if (StateWord(Context, WorldMapStateLayout::ThamesSaved) == 0)
		{
			SetStateWord(Context, WorldMapStateLayout::ThamesSaved, 1);
			Waypoint = 0;
			Task.Position.X = Context.Tables->ThamesWaypoints[0][0] << 12;
			Z = Context.Tables->ThamesWaypoints[0][1];
		}
		else
		{
			Waypoint = static_cast<uint16>(StateWord(Context, WorldMapStateLayout::ThamesWaypoint));
			Task.Position.X = static_cast<int32>(static_cast<uint32>(static_cast<uint16>(StateWord(Context, WorldMapStateLayout::ThamesX))) << 12);
			Z = static_cast<uint16>(StateWord(Context, WorldMapStateLayout::ThamesZ));
		}
		Task.Position.Z = static_cast<int32>(Z << 12);
		Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
		const FGteLongVector Way = ThamesWay(Context, Task);
		for (std::array<int16, 2>& Entry : Context.ThamesWake)
		{
			Entry = {static_cast<int16>(Way[0]), static_cast<int16>(Way[2])};
		}
		Task.Word(WorldMapTaskWord::ThamesWakeRead) = 1;
		Task.Word(WorldMapTaskWord::ThamesWakeWrite) = 0;
		Task.Direction.X = Context.ThamesWake[1][0];
		Task.Direction.Y = 0;
		Task.Direction.Z = Context.ThamesWake[1][1];
		return WorldMapTaskState::Update;
	}

	FResult UpdateThames(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		FWorldMapObject& Hull = ObjectAt(Context, ThamesObject(Context));
		if (Story(Context) >= ThamesCruiseStory)
		{
			const FGteLongVector Way = ThamesWay(Context, Task);
			// The heading eases toward the way (63/64 old), then waits 32 ticks in the wake before Thames follows it.
			Task.Direction.X = ((Way[0] + Task.Direction.X * 63) * 64) >> 12;
			Task.Direction.Z = ((Way[2] + Task.Direction.Z * 63) * 64) >> 12;
			const FGteLongVector Eased = VectorNormal({Task.Direction.X, Task.Direction.Y, Task.Direction.Z});
			Task.Direction.X = Eased[0];
			Task.Direction.Y = Eased[1];
			Task.Direction.Z = Eased[2];
			int32& Write = Task.Word(WorldMapTaskWord::ThamesWakeWrite);
			int32& Read = Task.Word(WorldMapTaskWord::ThamesWakeRead);
			Context.ThamesWake[static_cast<std::size_t>(Write)] = {static_cast<int16>(Task.Direction.X), static_cast<int16>(Task.Direction.Z)};
			Write = (Write + 1) & 0x1F;
			Hull.Matrix.M = LookAlong({Task.Direction.X, Task.Direction.Y, -Task.Direction.Z}).M;
			const std::array<int16, 2>& Followed = Context.ThamesWake[static_cast<std::size_t>(Read)];
			Read = (Read + 1) & 0x1F;
			const int32 Pace = Task.Speed >> 12;
			Task.Position.X += Followed[0] * Pace;
			Task.Position.Z += Followed[1] * Pace;
			Context.Ground.Wrap(Task.Position);
			Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
			PlaceObject(Hull, Task.Position);
			int32 ShipY = 0;
			const int32 Distance = DistanceToShip(Context, Task, ShipY);
			if (Distance < ThamesStopDistance && ShipY > ThamesShipLowest)
			{
				Task.Speed = 0;
			}
			else if (Distance < ThamesSlowDistance && ShipY > ThamesShipLowest)
			{
				Task.Speed = static_cast<int16>(Task.Speed - 0x100);
				if (Task.Speed < 0)
				{
					Task.Speed = 0;
				}
			}
			else
			{
				Task.Speed = static_cast<int16>(Task.Speed + 0x200);
				if (Task.Speed > ThamesFullSpeed)
				{
					Task.Speed = ThamesFullSpeed;
				}
			}
			if (Task.Speed <= ThamesSlowSpeed)
			{
				Context.Particles.Stop(ThamesEffect);
			}
			else
			{
				const std::array<int16, 4> Place{static_cast<int16>(Hull.X), static_cast<int16>(Hull.Y), static_cast<int16>(Hull.Z), 0};
				Context.Particles.Start(ThamesEffect, Place, AnglesOf(Transpose(Hull.Matrix), *Context.Arctangents));
			}
		}
		else
		{
			Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
			PlaceObject(Hull, Task.Position);
		}
		SaveThames(Context, Task);
		Context.Colliders.Add(Slot, {Task.Position.X, Task.Position.Y + 0x30000, Task.Position.Z, 0}, ThamesRadius, ThamesHeight);
		return WorldMapTaskState::Update;
	}

	FResult RestoreShevat(FWorldMapContext& Context, int32)
	{
		const int32 First = ShevatObject(Context);
		for (const std::array<int16, 2>& Link : Context.Tables->ShevatAttachments)
		{
			AttachObject(Context, First + Link[0], First + Link[1]);
		}
		return WorldMapTaskState::Update;
	}

	FResult InitShevat(FWorldMapContext& Context, int32 Slot)
	{
		RestoreShevat(Context, Slot);
		FWorldMapTask& Task = TaskAt(Context, Slot);
		Task.Word(WorldMapTaskWord::ShevatSpinStep) = 4;
		Task.Step = 0;
		Task.Word(WorldMapTaskWord::ShevatSpin) = 0;
		Task.Word(WorldMapTaskWord::ShevatTurn) = 0;
		Task.Word(WorldMapTaskWord::ShevatTurnStep) = 0xC;
		Task.Position.Y = ShevatStart[1];
		if (StateWord(Context, WorldMapStateLayout::ShevatSaved) == 0)
		{
			SetStateWord(Context, WorldMapStateLayout::ShevatSaved, 1);
			Task.Position.X = ShevatStart[0];
			Task.Position.Z = ShevatStart[2];
		}
		else
		{
			// The saved low half is the whole place's, so its bits 12-15 count twice.
			const auto Restored = [&](std::size_t Whole, std::size_t Low) {
				return static_cast<int32>(static_cast<uint32>(StateWord(Context, Whole)) * 0x1000u + static_cast<uint16>(StateWord(Context, Low)));
			};
			Task.Position.X = Restored(WorldMapStateLayout::ShevatX, WorldMapStateLayout::ShevatXLow);
			Task.Position.Z = Restored(WorldMapStateLayout::ShevatZ, WorldMapStateLayout::ShevatZLow);
		}
		Task.Direction.Z = ShevatDrift;
		Task.Direction.X = ShevatDrift;
		Task.Direction.Y = 0;
		Task.Speed = 1;
		SaveShevat(Context, Task);
		return WorldMapTaskState::Update;
	}

	FResult UpdateShevat(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		const int32 First = ShevatObject(Context);
		int32& Spin = Task.Word(WorldMapTaskWord::ShevatSpin);
		int32& Turn = Task.Word(WorldMapTaskWord::ShevatTurn);
		Spin = (Spin + Task.Word(WorldMapTaskWord::ShevatSpinStep)) & 0xFFF;
		Turn = (Turn + Task.Word(WorldMapTaskWord::ShevatTurnStep)) & 0xFFF;
		ObjectAt(Context, First + 5).Matrix.M = RotationYxz({0, static_cast<int16>(Spin), 0}).M;
		// The turning ring's matrix goes to the three models under it as well.
		const FGteMatrix Ring = RotationYxz({0, static_cast<int16>(Turn), 0});
		for (int32 Part = 3; Part >= 0; --Part)
		{
			ObjectAt(Context, First + Part).Matrix.M = Ring.M;
		}
		int32 ShipY = 0;
		const int32 Distance = DistanceToShip(Context, Task, ShipY);
		Task.Speed = Distance < ShevatStopDistance && ShipY < ShevatShipHighest ? 0 : 1;
		Task.Position.X += Task.Direction.X * Task.Speed;
		Task.Position.Z += Task.Direction.Z * Task.Speed;
		Context.Ground.Wrap(Task.Position);
		PlaceObject(ObjectAt(Context, First + 12), Task.Position);
		Context.Colliders.Add(Slot, Task.Position, ShevatRadius, ShevatHeight);
		SaveShevat(Context, Task);
		return WorldMapTaskState::Update;
	}

	FResult InitSpinnerPair(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		Task.Word(WorldMapTaskWord::SpinAngle) = 0;
		Task.Word(WorldMapTaskWord::SpinStep) = 8;
		return WorldMapTaskState::Update;
	}

	FResult UpdateSpinnerPair(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		int32& Angle = Task.Word(WorldMapTaskWord::SpinAngle);
		Angle = (Angle + Task.Word(WorldMapTaskWord::SpinStep)) & 0xFFF;
		for (const uint16 Number : Context.Tables->SpinnerPairObjects[static_cast<std::size_t>(Context.Era)])
		{
			FWorldMapObject& Object = ObjectAt(Context, Number);
			Spin(Object, Object.Angles[1], static_cast<int16>(Angle));
		}
		return WorldMapTaskState::Update;
	}

	FResult RestoreFastSpinnerPair(FWorldMapContext& Context, int32)
	{
		// The pair's models draw semi-transparent, the background with a quarter of themselves.
		for (const uint16 Number : Context.Tables->FastSpinnerPairObjects[static_cast<std::size_t>(Context.Era)])
		{
			ObjectAt(Context, Number).Blend = 3;
		}
		return WorldMapTaskState::Update;
	}

	FResult InitFastSpinnerPair(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		Task.Word(WorldMapTaskWord::SpinAngle) = 0;
		Task.Word(WorldMapTaskWord::SpinStep) = 0x10;
		return RestoreFastSpinnerPair(Context, Slot);
	}

	FResult UpdateFastSpinnerPair(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		int32& Angle = Task.Word(WorldMapTaskWord::SpinAngle);
		Angle = (Angle + Task.Word(WorldMapTaskWord::SpinStep)) & 0xFFF;
		for (const uint16 Number : Context.Tables->FastSpinnerPairObjects[static_cast<std::size_t>(Context.Era)])
		{
			ObjectAt(Context, Number).Matrix.M = RotationXyz({0, static_cast<int16>(Angle), 0}).M;
		}
		return WorldMapTaskState::Update;
	}

	namespace
	{
		FResult HideStoryObject(FWorldMapContext& Context)
		{
			ObjectAt(Context, StoryHolder).Hidden = 1;
			for (const int16 Number : Context.Tables->StoryObjects)
			{
				ObjectAt(Context, Number).Hidden = 1;
			}
			return WorldMapTaskState::Idle;
		}
	}

	FResult RestoreStoryObject(FWorldMapContext& Context, int32 Slot)
	{
		// The original hides the landmark whatever the story; the fix brings it back as the entry does.
		if (Context.Options.bStoryObjectBackAfterBattle && Story(Context) == StoryShown)
		{
			return InitStoryObject(Context, Slot);
		}
		return HideStoryObject(Context);
	}

	FResult InitStoryObject(FWorldMapContext& Context, int32 Slot)
	{
		for (const int16 Number : Context.Tables->StoryObjects)
		{
			AttachObject(Context, StoryHolder, Number);
		}
		FWorldMapTask& Task = TaskAt(Context, Slot);
		Task.Step = 0;
		FResult Result = WorldMapTaskState::Update;
		if (Story(Context) == StoryShown)
		{
			Task.Position.X = StoryPlace[0];
			Task.Position.Z = StoryPlace[1];
			Task.Position.Y = 0;
		}
		else
		{
			Result = HideStoryObject(Context);
		}
		PlaceObject(ObjectAt(Context, StoryHolder), Task.Position);
		return Result;
	}

	FResult UpdateStoryObject(FWorldMapContext& Context, int32 Slot)
	{
		if (Story(Context) == StoryShown)
		{
			PlaceObject(ObjectAt(Context, StoryHolder), TaskAt(Context, Slot).Position);
		}
		return WorldMapTaskState::Update;
	}

	FResult InitLandmarkCollider(FWorldMapContext& Context, int32 Slot, ELandmark Landmark)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		const std::size_t Era = static_cast<std::size_t>(Context.Era);
		const int32 Number = Landmark == ELandmark::A ? Context.Tables->LandmarkAObjects[Era]
			: Landmark == ELandmark::B ? LandmarkBObject : Context.Tables->LandmarkCObjects[Era];
		const FWorldMapObject& Object = ObjectAt(Context, Number);
		Task.Position.X = Object.X << 12;
		Task.Position.Y = Object.Y * 0x1000 + (Landmark == ELandmark::A ? 0x40000 : 0);
		Task.Position.Z = Object.Z << 12;
		return WorldMapTaskState::Update;
	}

	FResult UpdateLandmarkCollider(FWorldMapContext& Context, int32 Slot, ELandmark Landmark)
	{
		static constexpr int32 Radii[3] = {0x68, 0x10C, 0xC9};
		static constexpr int32 Heights[3] = {0x60, 0x1A6, 0x392};
		const std::size_t Index = static_cast<std::size_t>(Landmark);
		Context.Colliders.Add(Slot, TaskAt(Context, Slot).Position, Radii[Index], Heights[Index]);
		return WorldMapTaskState::Update;
	}
}
