#include "BattleObjectMachine.h"

#include "Battle/Show/BattleShowCamera.h"
#include "Battle/Show/BattleShowFloor.h"
#include "Battle/Show/BattleStageFiles.h"
#include "Graphics/GteVectors.h"

#include <string>

namespace Weltall::BattleObjectMachine
{
	namespace
	{
		constexpr uint8 Running = 1;
		constexpr uint8 Ended = 2;
		/** Kinds below 2 follow a slot (and the middle of it and a second); the others head for a point. */
		constexpr uint8 FirstPointKind = 2;
		constexpr int32 AvoidMargin = 0x80;
		constexpr int32 AvoidReach = 6;
		constexpr int32 LookAhead = 9;
		constexpr int32 CombatantSlots = 11;

		int32 Over4096(int32 Product)
		{
			return (Product < 0 ? Product + 0xFFF : Product) >> 12;
		}

		/** A word's place, and the root's when the slot has an object (the height word of a record is never written). */
		struct FFollowed
		{
			uint16 X = 0;
			uint16 Y = 0;
			uint16 Z = 0;
		};

		std::optional<FFollowed> Followed(FBattleObjects& Objects, int32 Slot)
		{
			if (Slot < 0 || Slot >= FBattleObjects::SlotCount)
			{
				Objects.Fail("the model camera follows slot " + std::to_string(Slot) + ", past the objects' slots");
				return std::nullopt;
			}
			if (const FBattleObject* Object = Objects.Object(Slot))
			{
				const FGearBone& Root = Object->Bones[0];
				return FFollowed{static_cast<uint16>(Root.Position[0]), static_cast<uint16>(Root.Position[1]), static_cast<uint16>(Root.Position[2])};
			}
			const std::optional<std::array<int16, 3>> Place = SlotPlace(Objects, Slot);
			if (!Place)
			{
				return std::nullopt;
			}
			return FFollowed{static_cast<uint16>((*Place)[0]), static_cast<uint16>((*Place)[1]), static_cast<uint16>((*Place)[2])};
		}

		int32 HalfTowardZero(int32 Value)
		{
			return (Value - (Value >> 31)) >> 1;
		}

		/** The eye moved out of the nearest combatant it stands in, looked for a sixth of the way ahead. */
		void AvoidObjects(FBattleObjects& Objects, const std::array<int16, 2>& Ahead, std::array<int16, 3>& Eye)
		{
			int32 Best = -1;
			int16 BestReach = 0;
			int16 NewX = 0;
			int16 NewZ = 0;
			for (int32 Slot = 0; Slot < CombatantSlots && !Objects.Faulted(); ++Slot)
			{
				const int32 Reach = Objects.Radius(Slot) + AvoidMargin;
				const FBattleObject* Object = Objects.Object(Slot);
				if (Object == nullptr || Object->bVisible == 0)
				{
					continue;
				}
				const FGearBone& Root = Object->Bones[0];
				if (!(Root.Position[1] - Objects.Height(Slot) < Eye[1] && (Best < 0 || BestReach < static_cast<int16>(Reach))))
				{
					continue;
				}
				const int32 AtX = static_cast<uint16>(Root.Position[0]) - Ahead[0] / AvoidReach;
				const int32 Dx = static_cast<int16>(static_cast<uint16>(Eye[0]) - AtX);
				const int32 AtZ = static_cast<uint16>(Root.Position[2]) - Ahead[1] / AvoidReach;
				const int32 Dz = static_cast<int16>(static_cast<uint16>(Eye[2]) - AtZ);
				const int32 Far = GearMachine::Distance(Objects, Dx, 0, Dz) + 1;
				const int32 Radius = static_cast<int16>(Reach);
				if (Far < Radius)
				{
					NewX = static_cast<int16>(AtX + GearMachine::Divide(Objects, Dx * Radius, Far));
					NewZ = static_cast<int16>(AtZ + GearMachine::Divide(Objects, Dz * Radius, Far));
					Best = Slot;
					BestReach = static_cast<int16>(Reach);
				}
			}
			if (Best >= 0)
			{
				Eye[0] = NewX;
				Eye[2] = NewZ;
			}
		}

		/** AvoidObjects, looking from the eye toward the look-at. */
		void AvoidObjectsToward(FBattleObjects& Objects, const FGteVector& Target, std::array<int16, 3>& Eye)
		{
			const int16 Dx = static_cast<int16>(Target[0] - Eye[0]);
			const int16 Dz = static_cast<int16>(Target[2] - Eye[2]);
			const int32 Far = GearMachine::Distance(Objects, Dx, 0, Dz) + 1;
			const std::array<int16, 2> Ahead = {static_cast<int16>(GearMachine::Divide(Objects, Dx << LookAhead, Far)),
				static_cast<int16>(GearMachine::Divide(Objects, Dz << LookAhead, Far))};
			AvoidObjects(Objects, Ahead, Eye);
		}

		/**
		 * The floor's height a little ahead of the eye toward the look-at, looked for
		 * once a frame while that point is inside the stage's bounds; the last found otherwise.
		 */
		int16 FloorClamp(FBattleObjects& Objects, int32 Frame)
		{
			FBattleModelCamera& Camera = Objects.ModelCamera;
			const FGteVector& Eye = Objects.Camera->Eye;
			const FGteVector& Target = Objects.Camera->Target;
			int32 Dy = Target[1] - Eye[1];
			Dy = (Dy < 0 ? Dy + 3 : Dy) >> 2;
			const int32 Dx = static_cast<int16>(Target[0] - Eye[0]);
			const int32 Dz = static_cast<int16>(Target[2] - Eye[2]);
			const int32 Far = GearMachine::Distance(Objects, Dx, Dy, Dz) + 1;
			FGteVector Ahead = {static_cast<int16>(Eye[0] + static_cast<int16>(GearMachine::Divide(Objects, Dx << LookAhead, Far))),
				static_cast<int16>(Eye[1] + static_cast<int16>(GearMachine::Divide(Objects, Dy << LookAhead, Far))),
				static_cast<int16>(Eye[2] + static_cast<int16>(GearMachine::Divide(Objects, Dz << LookAhead, Far)))};
			const std::array<int16, 4> Bounds = Objects.Stage->CameraBounds();
			const bool bInside = Bounds[1] < Ahead[0] && Ahead[0] < Bounds[0] && Bounds[2] < Ahead[2] && Ahead[2] < Bounds[3];
			if (bInside && Frame != Camera.FloorFrame)
			{
				Camera.FloorFrame = static_cast<int16>(Frame);
				int32 Triangle = Objects.Floor->FindTriangle(Ahead, Camera.FloorTriangle, 5).value_or(-1);
				if (static_cast<int16>(Triangle) < 0)
				{
					Triangle = Objects.Floor->ScanTriangles(Ahead);
				}
				if (static_cast<int16>(Triangle) >= 0)
				{
					Camera.FloorTriangle = static_cast<int16>(Triangle);
					Objects.Floor->Height(Ahead, Camera.FloorTriangle);
					Camera.FloorHeight = Ahead[1];
				}
			}
			return Camera.FloorHeight;
		}
	}

	void StartCameraTrack(FBattleObjects& Objects, int32 Track, uint8 Kind, uint8 Tag, const std::array<int16, 6>& Words, int16 Ticks)
	{
		if (Objects.Camera->bLocked)
		{
			return;
		}
		if (Track < 0 || Track >= FBattleModelCamera::TrackCount)
		{
			Objects.Fail("the model camera starts track " + std::to_string(Track) + ", past its nine");
			return;
		}
		int16& Channel = Objects.ModelCamera.Tracks[static_cast<std::size_t>(Track)];
		if (Channel == FGearBone::NoChannel)
		{
			Channel = GearMachine::AllocateChannel(Objects);
		}
		if (Channel == FGearBone::NoChannel)
		{
			return;
		}
		FGearChannel& Moving = Objects.Channels[Channel];
		Moving.bUsed = true;
		Moving.Loop = 0;
		Moving.Kind = Kind;
		Moving.Tag = Tag;
		Moving.Words = Words;
		Moving.StreamStart = {};
		Moving.StreamRead = {};
		Moving.Tick = 0;
		Moving.Length = Ticks;
	}

	void FreeCameraTracks(FBattleObjects& Objects)
	{
		for (int16& Channel : Objects.ModelCamera.Tracks)
		{
			GearMachine::FreeChannel(Objects, Channel);
		}
	}

	void StepCameraTracks(FBattleObjects& Objects, int32 Ticks, int32 Frame)
	{
		FBattleModelCamera& Camera = Objects.ModelCamera;
		FBattleCamera& View = *Objects.Camera;
		Camera.Events = 0;
		for (int32 Tick = 0; Tick < Ticks && !Objects.Faulted(); ++Tick)
		{
			for (int32 Track = 0; Track < FBattleModelCamera::TrackCount && !Objects.Faulted(); ++Track)
			{
				int16& Channel = Camera.Tracks[static_cast<std::size_t>(Track)];
				if (Channel == FGearBone::NoChannel)
				{
					continue;
				}
				FGearChannel& Moving = Objects.Channels[Channel];
				const uint8 Kind = Moving.Kind;
				uint16 X = 0;
				uint32 Y = 0;
				uint16 Z = 0;
				if ((Kind & 0xF) < FirstPointKind)
				{
					const std::optional<FFollowed> First = Followed(Objects, Moving.Words[3]);
					if (!First)
					{
						return;
					}
					X = First->X;
					Z = First->Z;
					Y = static_cast<uint32>(First->Y) - static_cast<uint16>(Moving.Words[4]);
					if (Moving.Words[5] >= 0)
					{
						// The middle of the two: an object's root as it is, a record's words unsigned.
						std::array<int32, 3> Other{};
						if (const FBattleObject* Object = NamedObject(Objects, static_cast<uint32>(Moving.Words[5])))
						{
							Other = Object->Bones[0].Position;
						}
						else if (const std::optional<FFollowed> Place = Followed(Objects, Moving.Words[5]))
						{
							Other = {Place->X, Place->Y, Place->Z};
						}
						if (Objects.Faulted())
						{
							return;
						}
						X = static_cast<uint16>(HalfTowardZero(static_cast<int16>(X) + Other[0]));
						Y = static_cast<uint32>(HalfTowardZero(static_cast<int16>(Y) + Other[1] - Moving.Words[4]));
						Z = static_cast<uint16>(HalfTowardZero(static_cast<int16>(Z) + Other[2]));
					}
				}
				else
				{
					X = static_cast<uint16>(Moving.Words[3]);
					Y = static_cast<uint16>(Moving.Words[4]);
					Z = static_cast<uint16>(Moving.Words[5]);
				}
				if (Track == FBattleModelCamera::TargetTrack)
				{
					Y -= static_cast<uint16>(Camera.TargetHeight);
					X = static_cast<uint16>(X + static_cast<int16>(Over4096(-Camera.TargetDistance * GteSin(Camera.TargetYaw))));
					Z = static_cast<uint16>(Z + static_cast<int16>(Over4096(-Camera.TargetDistance * GteCos(Camera.TargetYaw))));
				}
				int16 Height = static_cast<int16>(Y);
				if (Track == FBattleModelCamera::EyeTrack)
				{
					const int32 Rise = Over4096(Camera.EyeDistance * GteSin(Camera.EyePitch));
					const int32 Across = Over4096(Camera.EyeDistance * GteCos(Camera.EyePitch));
					X = static_cast<uint16>(X + static_cast<int16>(Over4096(-Across * GteSin(Camera.EyeYaw))));
					const int32 Raised = static_cast<int32>(Y + (static_cast<uint32>(-static_cast<int32>(static_cast<uint16>(Camera.EyeHeight))) - static_cast<uint32>(Rise)));
					Height = static_cast<int16>(Raised);
					Z = static_cast<uint16>(Z + static_cast<int16>(Over4096(-Across * GteCos(Camera.EyeYaw))));
					std::array<int16, 3> Eye = {static_cast<int16>(X), Height, static_cast<int16>(Z)};
					AvoidObjectsToward(Objects, View.Target, Eye);
					X = static_cast<uint16>(Eye[0]);
					Z = static_cast<uint16>(Eye[2]);
					const int16 Floor = FloorClamp(Objects, Frame);
					if (Floor < static_cast<int16>(Raised))
					{
						Height = Floor;
					}
				}
				if (Camera.bFollow && Track >= FBattleModelCamera::TargetTrack)
				{
					Moving.Words[0] = static_cast<int16>(X);
					Moving.Words[1] = Height;
					Moving.Words[2] = static_cast<int16>(Z);
				}
				int32 NowX = 0;
				int16 NowY = 0;
				int16 NowZ = 0;
				if ((Kind & 1) == 0)
				{
					const int32 Step = static_cast<int16>(Moving.Tick + 1);
					NowX = Moving.Words[0] + GearMachine::Divide(Objects, (static_cast<int16>(X) - Moving.Words[0]) * Step, Moving.Length);
					NowY = static_cast<int16>(Moving.Words[1] + GearMachine::Divide(Objects, (Height - Moving.Words[1]) * Step, Moving.Length));
					NowZ = static_cast<int16>(Moving.Words[2] + GearMachine::Divide(Objects, (static_cast<int16>(Z) - Moving.Words[2]) * Step, Moving.Length));
				}
				else
				{
					NowX = Moving.Words[0];
					NowY = Moving.Words[1];
					NowZ = Moving.Words[2];
					const int32 StepX = GearMachine::Divide(Objects, static_cast<int16>(X) - NowX, Moving.Length);
					const int32 StepY = GearMachine::Divide(Objects, Height - NowY, Moving.Length);
					const int32 StepZ = GearMachine::Divide(Objects, static_cast<int16>(Z) - NowZ, Moving.Length);
					if ((StepX & 0xFFFF) == 0 && (StepY & 0xFFFF) == 0 && (StepZ & 0xFFFF) == 0)
					{
						Moving.Tick = Moving.Length;
					}
					else
					{
						NowX += StepX;
						NowY = static_cast<int16>(NowY + StepY);
						NowZ = static_cast<int16>(NowZ + StepZ);
						Moving.Tick = 0;
					}
					Moving.Words[0] = static_cast<int16>(NowX);
					Moving.Words[1] = NowY;
					Moving.Words[2] = NowZ;
				}
				if (Objects.Faulted())
				{
					return;
				}
				Moving.Tick = static_cast<int16>(Moving.Tick + 1);
				if (Moving.Tick < Moving.Length)
				{
					if (Moving.Tag == Camera.Watched)
					{
						Camera.Events |= Running;
					}
				}
				else if (Track < FBattleModelCamera::TargetTrack)
				{
					if (Moving.Tag == Camera.Watched)
					{
						Camera.Events |= Ended;
					}
					GearMachine::FreeChannel(Objects, Channel);
				}
				else
				{
					Moving.Tick = static_cast<int16>(Moving.Length - 1);
				}
				if (Track < FBattleModelCamera::TargetTrack)
				{
					Camera.Channel(Track) = static_cast<int16>(NowX);
				}
				else
				{
					FGteVector& Point = Track == FBattleModelCamera::TargetTrack ? View.Target : View.Eye;
					Point = {static_cast<int16>(NowX), NowY, NowZ};
				}
			}
		}
		Camera.bFollow = false;
	}
}
