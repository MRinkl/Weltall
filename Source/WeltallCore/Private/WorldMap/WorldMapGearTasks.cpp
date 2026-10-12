#include "WorldMap/WorldMapOps.h"

#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "WorldMap/WorldMapInput.h"
#include "WorldMap/WorldMapStateLayout.h"

namespace Weltall::WorldMapOps
{
	namespace
	{
		constexpr int32 GearSpeed = 12;
		constexpr int32 GearScale = 0x2000;
		constexpr int32 KneelingAnimation = 3;
		constexpr int32 FirstGearTask = FWorldMapContext::GearSlot;
		/** Gear 1's dust, and the other gears' from their slot; the smoke of each gear's dismount. */
		constexpr int32 LeadGearDust = 0x2C;
		constexpr int32 GearDustBase = 0x28;
		constexpr int32 LeadGearSmoke = 5;
		/** The gears' radius and height for the colliders. */
		constexpr int32 GearRadius = 0x18;
		constexpr int32 GearHeight = 0x30;
		/** How far behind on the trail gears 2 and 3 walk. */
		constexpr std::array<int32, 3> TrailOffsets{0, 0xF, 0x1F};
		/** A gear walks out 96 units from the Yggdrasil when the party leaves it. */
		constexpr int32 DisembarkReach = 0x60;
		/** Chu-Chu's gear leaves no smoke and throws no shadow on foot. */
		constexpr uint8 ChuChu = 7;

		constexpr int16 WalkToSlotMessage = 1;
		constexpr int16 LeaveGearMessage = 2;
		constexpr int16 DisembarkMessage = 3;
		constexpr int16 AboardMessage = 4;
		constexpr int16 GatherMessage = 5;
		constexpr int16 GearBackMessage = 7;
		constexpr int16 DismountMessage = 8;

		namespace GearStep
		{
			constexpr int16 Parked = 0;
			constexpr int16 Ridden = 1;
			constexpr int16 Aboard = 2;
			constexpr int16 BoardSecond = 8;
			constexpr int16 BoardThird = 9;
			constexpr int16 WalkToShip = 10;
			constexpr int16 WalkingToShip = 11;
			constexpr int16 Embark = 12;
			constexpr int16 WaitForGears = 0x10;
			constexpr int16 CallSecond = 0x11;
			constexpr int16 CallThird = 0x12;
			constexpr int16 Dismount = 0x18;
			constexpr int16 Smoke = 0x19;
			constexpr int16 Kneel = 0x1A;
			constexpr int16 StepDown = 0x20;
			constexpr int16 CallLeader = 0x21;
			constexpr int16 Reset = 0x22;
			constexpr int16 Disembark = 0x30;
			constexpr int16 Disembarking = 0x31;
			constexpr int16 Landed = 0x32;
			constexpr int16 Done = 0x40;
		}

		/** The other gears' steps where they differ. */
		namespace FollowerStep
		{
			constexpr int16 WalkToSlot = 8;
			constexpr int16 WalkingToSlot = 9;
			constexpr int16 Embark = 10;
			constexpr int16 GoToLead = 0x10;
			constexpr int16 GoingToLead = 0x11;
			constexpr int16 ReportToLead = 0x12;
		}

		FWorldMapTask& TaskAt(FWorldMapContext& Context, int32 Slot)
		{
			return Context.Tasks[static_cast<std::size_t>(Slot)];
		}

		void CopySpot(FWorldMapVector& To, const FWorldMapVector& From)
		{
			To.X = From.X;
			To.Y = From.Y;
			To.Z = From.Z;
		}

		bool SameSpot(const FWorldMapVector& A, const FWorldMapVector& B)
		{
			return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
		}

		void HeadFor(FWorldMapContext& Context, FWorldMapTask& Task, const FWorldMapVector& Target)
		{
			const FWorldMapHeading Heading = WorldMapDirectionTo(Task.Position, Target, *Context.Arctangents);
			Task.Heading = Heading.Heading;
			Task.Direction.X = Heading.DirectionX;
			Task.Direction.Z = Heading.DirectionZ;
			Task.Word(WorldMapTaskWord::WalkX) = Target.X >> 12;
			Task.Word(WorldMapTaskWord::WalkZ) = Target.Z >> 12;
		}

		int32 WalkOn(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			return WalkTowardWorldMapPoint(Context.Ground, Task.Position, Task.Direction, Task.Speed, Task.Word(WorldMapTaskWord::WalkX),
				Task.Word(WorldMapTaskWord::WalkZ));
		}

		void ClearDirection(FWorldMapTask& Task)
		{
			Task.Direction.Z = 0;
			Task.Direction.Y = 0;
			Task.Direction.X = 0;
		}

		/** The slot's gear's sprite at the program's place for it, kneeling unless ridden. */
		void CreateGearSprite(FWorldMapContext& Context, int32 Slot, int32 PartySlot)
		{
			const FWorldMapSpritePlace& Place = Context.Tables->GearSprites[static_cast<std::size_t>(PartySlot)];
			CreateSprite(Context, Slot, *Context.GearFiles[static_cast<std::size_t>(PartySlot)], Place.ClutX, Place.ClutY, Place.TextureX,
				Place.TextureY, Riding(Context, PartySlot) == 1 ? 0 : KneelingAnimation, GearScale);
		}

		/** A slot's gear: a gear on the map (parked where the game state keeps it, or brought under its rider) or none. */
		FResult InitGearSprite(FWorldMapContext& Context, int32 Slot, int32 PartySlot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			const std::size_t Gear = static_cast<std::size_t>(PartySlot);
			const bool bMember = PartyCharacter(Context, PartySlot) != 0xFF;
			const bool bOnMap = (static_cast<uint16>(StateWord(Context, WorldMapStateLayout::GearFlags(Gear))) & 0x3FFF) >= 0x400;
			const bool bRidden = Riding(Context, PartySlot) == 1;
			const auto Absent = [&](FResult Result)
			{
				Task.Hidden = 1;
				Task.Position.Y = 0;
				Task.Position.Z = 0;
				Task.Position.X = 0;
				return Result;
			};
			if (!bMember)
			{
				return Absent(WorldMapTaskState::Idle);
			}
			if (bRidden)
			{
				CreateGearSprite(Context, Slot, PartySlot);
				Task.Hidden = 0;
				Task.Position.X = Context.Start.X;
				Task.Position.Z = Context.Start.Z;
				Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
				SetStateWord(Context, WorldMapStateLayout::GearFlags(Gear), WorldMapStateLayout::GearOnMap);
				return WorldMapTaskState::Update;
			}
			if (bOnMap)
			{
				CreateGearSprite(Context, Slot, PartySlot);
				Task.Hidden = 0;
				Task.Position.X = static_cast<int32>(static_cast<uint32>(static_cast<uint16>(StateWord(Context, WorldMapStateLayout::GearX(Gear)))) << 12);
				Task.Position.Z = static_cast<int32>(static_cast<uint32>(static_cast<uint16>(StateWord(Context, WorldMapStateLayout::GearZ(Gear)))) << 12);
				Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
				return WorldMapTaskState::Update;
			}
			if (CharacterGear(Context, PartyCharacter(Context, PartySlot)) == 0xFF)
			{
				return Absent(WorldMapTaskState::Idle);
			}
			CreateGearSprite(Context, Slot, PartySlot);
			return Absent(WorldMapTaskState::Update);
		}

		void WriteGearPlace(FWorldMapContext& Context, const FWorldMapTask& Task, int32 PartySlot)
		{
			const std::size_t Gear = static_cast<std::size_t>(PartySlot);
			SetStateWord(Context, WorldMapStateLayout::GearX(Gear), static_cast<int16>(Task.Position.X >> 12));
			SetStateWord(Context, WorldMapStateLayout::GearZ(Gear), static_cast<int16>(Task.Position.Z >> 12));
			SetStateWord(Context, WorldMapStateLayout::GearHeading(Gear), Task.Heading);
		}

		/** Leaving the Yggdrasil: the gear out of the hull, heading the landing direction, walking 96 units out. */
		void LeaveVehicle(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			Task.Position.X = static_cast<int32>(static_cast<uint32>(static_cast<int32>(StateWord(Context, WorldMapStateLayout::VehicleX))) << 12);
			Task.Position.Y = static_cast<int32>(static_cast<uint32>(static_cast<int32>(StateWord(Context, WorldMapStateLayout::VehicleY))) << 12);
			Task.Position.Z = static_cast<int32>(static_cast<uint32>(static_cast<int32>(StateWord(Context, WorldMapStateLayout::VehicleZ))) << 12);
			Task.Hidden = 0;
			const int16 Heading = static_cast<int16>(TaskAt(Context, FWorldMapContext::VehicleSlot).Word(WorldMapTaskWord::LandingDirection));
			Task.Heading = Heading;
			FWorldMapVector Out = Task.Position;
			Out.X = Task.Position.X + GteSin(Heading) * DisembarkReach;
			Out.Z = Task.Position.Z + GteCos(Task.Heading) * -DisembarkReach;
			HeadFor(Context, Task, Out);
		}

		void Smoke(FWorldMapContext& Context, const FWorldMapTask& Task, int32 Effect)
		{
			const std::array<int16, 4> Place{static_cast<int16>(Task.Position.X >> 12), static_cast<int16>(Task.Position.Y >> 12),
				static_cast<int16>(Task.Position.Z >> 12), 0};
			Context.Particles.Start(Effect, Place, std::nullopt);
		}

		bool CastsShadow(const FWorldMapContext& Context, const FWorldMapTask& Task, int32 PartySlot)
		{
			return Task.Step != GearStep::Aboard && (Context.TravelMode == WorldMapTravel::Gears || PartyCharacter(Context, PartySlot) != ChuChu);
		}
	}

	FResult InitGear(FWorldMapContext& Context, int32 Slot, int32 PartySlot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		const FResult Result = InitGearSprite(Context, Slot, PartySlot);
		ClearDirection(Task);
		Task.Heading = StateWord(Context, WorldMapStateLayout::GearHeading(static_cast<std::size_t>(PartySlot)));
		Task.Speed = GearSpeed;
		if (PartySlot != 0)
		{
			Task.Word(WorldMapTaskWord::Follow) = TrailOffsets[static_cast<std::size_t>(PartySlot)];
		}
		Task.Word(WorldMapTaskWord::DrawnFacing) = Task.Heading;
		const int32 Mode = Context.TravelMode;
		if (Mode > 0 && Mode < 4)
		{
			if (Riding(Context, PartySlot) == 1)
			{
				Task.Step = GearStep::Ridden;
				Task.Position.X = Context.Start.X;
				Task.Position.Z = Context.Start.Z;
				Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
				Task.Heading = static_cast<int16>(Context.StartYaw);
				if (PartySlot == 0)
				{
					SetControlled(Context, Task);
					FillTrail(Context, Task.Position, Task.Heading);
				}
			}
		}
		else if (Mode >= 4 && Mode < 8)
		{
			Task.Step = GearStep::Aboard;
			Task.Hidden = 1;
		}
		WriteGearPlace(Context, Task, PartySlot);
		return Result;
	}

	FResult RestoreGear(FWorldMapContext& Context, int32 Slot, int32 PartySlot)
	{
		const FResult Result = InitGearSprite(Context, Slot, PartySlot);
		if (Context.TravelMode > 3 && Context.TravelMode < 8)
		{
			TaskAt(Context, Slot).Hidden = 1;
		}
		return Result;
	}

	FResult UpdateGear1(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		const int32 Members = Context.MemberCount;
		switch (Task.Message)
		{
		case AboardMessage:
			Task.Step = GearStep::WaitForGears;
			Task.Message = 0;
			Task.Word(WorldMapTaskWord::Follow) = 1;
			SetRiding(Context, 0, 1);
			break;
		case DisembarkMessage:
			Task.Message = 0;
			Task.Step = GearStep::Disembark;
			break;
		case GearBackMessage:
			Task.Message = 0;
			++Task.Word(WorldMapTaskWord::Follow);
			if (Members == Task.Word(WorldMapTaskWord::Follow))
			{
				Task.Step = GearStep::Ridden;
				Context.TravelMode = WorldMapTravel::Gears;
				Context.Colliders.Count = 0;
			}
			break;
		case DismountMessage:
			Task.Message = 0;
			Task.Step = GearStep::Dismount;
			break;
		default:
			break;
		}

		std::optional<int16> NextStep;
		switch (Task.Step)
		{
		case GearStep::Parked:
		case GearStep::Ridden:
		{
			if (Riding(Context, 0) != 1)
			{
				if (SpriteAnimation(Context, Task) != KneelingAnimation)
				{
					SetSpriteAnimation(Context, Task, KneelingAnimation);
					Context.Particles.Stop(LeadGearDust);
				}
				break;
			}
			const EWorldMapInput Input = ReadGearInput(Context, Task);
			if (Input == EWorldMapInput::Board)
			{
				Task.Step = GearStep::BoardSecond;
			}
			else if (Input == EWorldMapInput::Enter)
			{
				Context.Running = 0;
				Context.ExitCode = 0;
			}
			else if (Input == EWorldMapInput::Disembark)
			{
				if (Context.Ground.CanTraverse(WorldMapTravel::OnFoot, Context.Ground.TerrainType(Task.Position.X, Task.Position.Z)))
				{
					Task.Step = GearStep::StepDown;
				}
			}
			else
			{
				if (Task.Direction.X == 0 && Task.Direction.Y == 0 && Task.Direction.Z == 0)
				{
					if (SpriteAnimation(Context, Task) != 0)
					{
						SetSpriteAnimation(Context, Task, 0);
						Context.Particles.Stop(LeadGearDust);
					}
				}
				else
				{
					if (SpriteAnimation(Context, Task) != 1)
					{
						SetSpriteAnimation(Context, Task, 1);
					}
					UpdateDust(Context, LeadGearDust, Task);
				}
				const int32 Speed = Task.Speed << 12;
				FWorldMapVector& Out = Context.MoveResult;
				bool bFree = MoveOnWorldMapGround(Context.Ground, Task.Position, Task.Direction, Out, Speed, Context.TravelMode);
				if (!bFree)
				{
					Task.Direction = Out;
					bFree = MoveOnWorldMapGround(Context.Ground, Task.Position, Task.Direction, Out, Speed, Context.TravelMode);
					if (!bFree)
					{
						Task.Direction.Z = 0;
						Task.Direction.X = 0;
					}
				}
				if (bFree)
				{
					Context.Contact = Context.Colliders.Find(Context.Ground, Out, GearRadius, GearHeight);
					const std::size_t Pass = Context.Contact.Slot == FWorldMapContext::VehicleSlot ? Context.Contact.Contact + 3u : Context.Contact.Contact;
					if (Context.Tables->ColliderPasses[Pass] != 0)
					{
						Task.Position = Out;
						if (Task.Direction.X != 0 || Task.Direction.Z != 0)
						{
							PushTrail(Context, Task);
							Context.Encounters.Tick(*Context.Random);
						}
					}
				}
				else
				{
					Context.Contact = Context.Colliders.Find(Context.Ground, Task.Position, GearRadius, GearHeight);
				}
				FindTrigger(Context, Task.Position, 1);
				ClearDirection(Task);
				SetControlled(Context, Task);
			}
			Context.Colliders.Count = 0;
			break;
		}
		case GearStep::Aboard:
		{
			const FWorldMapTask& Vehicle = TaskAt(Context, FWorldMapContext::VehicleSlot);
			Task.Position.X = Vehicle.Position.X;
			Task.Position.Z = Vehicle.Position.Z;
			Task.Heading = Vehicle.Heading;
			break;
		}
		case GearStep::BoardSecond:
		case GearStep::BoardThird:
		{
			const int32 PartySlot = Task.Step == GearStep::BoardSecond ? 1 : 2;
			if (PartyCharacter(Context, PartySlot) != 0xFF)
			{
				if (Riding(Context, PartySlot) == 1)
				{
					if (SendMessage(Context, FirstGearTask + PartySlot, WalkToSlotMessage))
					{
						TaskAt(Context, FirstGearTask + PartySlot).MessageArgument = Context.Contact.Slot;
						++Task.Step;
					}
					break;
				}
				SendMessage(Context, FWorldMapContext::LeaderSlot + PartySlot, WalkToSlotMessage);
				TaskAt(Context, FWorldMapContext::LeaderSlot + PartySlot).MessageArgument = Context.Contact.Slot;
				SendMessage(Context, FirstGearTask + PartySlot, DismountMessage);
			}
			NextStep = static_cast<int16>(Task.Step + 1);
			break;
		}
		case GearStep::WalkToShip:
		case GearStep::WalkingToShip:
			if (Task.Step == GearStep::WalkToShip)
			{
				HeadFor(Context, Task, TaskAt(Context, Context.Contact.Slot).Position);
				SetSpriteAnimation(Context, Task, 1);
				++Task.Step;
			}
			if (WalkOn(Context, Task) == 3)
			{
				++Task.Step;
			}
			SetControlled(Context, Task);
			UpdateDust(Context, LeadGearDust, Task);
			break;
		case GearStep::Embark:
			if (SendMessage(Context, Context.Contact.Slot, AboardMessage))
			{
				Task.Hidden = 1;
				Task.Step = GearStep::Aboard;
				Context.Particles.Stop(LeadGearDust);
			}
			break;
		case GearStep::WaitForGears:
		{
			uint8 Ready = Riding(Context, 1);
			if (Members != 2)
			{
				if (Members < 3)
				{
					if (Members == 1)
					{
						Task.Step = GearStep::Ridden;
					}
					break;
				}
				if (Members != 3)
				{
					break;
				}
				Ready = static_cast<uint8>(Riding(Context, 2) & Riding(Context, 1));
			}
			if (Ready != 0)
			{
				NextStep = static_cast<int16>(Task.Step + 1);
			}
			break;
		}
		case GearStep::CallSecond:
			if (PartyCharacter(Context, 1) == 0xFF || SendMessage(Context, FirstGearTask + 1, GatherMessage))
			{
				NextStep = static_cast<int16>(Task.Step + 1);
			}
			break;
		case GearStep::CallThird:
			if (PartyCharacter(Context, 2) == 0xFF || SendMessage(Context, FirstGearTask + 2, GatherMessage))
			{
				Task.Step = GearStep::Done;
			}
			break;
		case GearStep::Dismount:
			if (PartyCharacter(Context, 0) == ChuChu)
			{
				Task.Timer = 1;
				Task.Step = GearStep::Kneel;
				break;
			}
			Smoke(Context, Task, LeadGearSmoke);
			Task.Timer = 8;
			NextStep = static_cast<int16>(Task.Step + 1);
			break;
		case GearStep::Smoke:
			Task.Timer = static_cast<int16>(Task.Timer - 1);
			if (Task.Timer > 0)
			{
				break;
			}
			Task.Hidden = 1;
			Task.Timer = 0x10;
			NextStep = static_cast<int16>(Task.Step + 1);
			break;
		case GearStep::Kneel:
			Task.Timer = static_cast<int16>(Task.Timer - 1);
			if (Task.Timer < 1)
			{
				SetSpriteAnimation(Context, Task, KneelingAnimation);
				Task.Hidden = 1;
				Task.Step = GearStep::Aboard;
			}
			break;
		case GearStep::StepDown:
		case GearStep::CallLeader:
			if (Task.Step == GearStep::StepDown)
			{
				SetSpriteAnimation(Context, Task, 0);
				if (PartyCharacter(Context, 1) != 0xFF)
				{
					SendMessage(Context, FirstGearTask + 1, LeaveGearMessage);
				}
				if (PartyCharacter(Context, 2) != 0xFF)
				{
					SendMessage(Context, FirstGearTask + 2, LeaveGearMessage);
				}
				ClearDirection(Task);
				++Task.Step;
			}
			if (!SendMessage(Context, FWorldMapContext::LeaderSlot, LeaveGearMessage))
			{
				break;
			}
			TaskAt(Context, FWorldMapContext::LeaderSlot).MessageArgument = static_cast<int16>(Slot);
			NextStep = static_cast<int16>(Task.Step + 1);
			break;
		case GearStep::Reset:
			Task.Step = GearStep::Parked;
			break;
		case GearStep::Disembark:
		case GearStep::Disembarking:
			if (Task.Step == GearStep::Disembark)
			{
				LeaveVehicle(Context, Task);
				SetSpriteAnimation(Context, Task, 1);
				++Task.Step;
			}
			if (WalkOn(Context, Task) == 3)
			{
				SetSpriteAnimation(Context, Task, 0);
				ClearDirection(Task);
				++Task.Step;
			}
			SetControlled(Context, Task);
			break;
		case GearStep::Landed:
			FillTrail(Context, Task.Position, Task.Heading);
			Task.Step = GearStep::Ridden;
			Context.TravelMode = WorldMapTravel::Gears;
			break;
		default:
			break;
		}
		if (NextStep)
		{
			Task.Step = *NextStep;
		}
		if (CastsShadow(Context, Task, 0))
		{
			AddShadow(Context, 1, Task.Position);
		}
		WriteGearPlace(Context, Task, 0);
		return WorldMapTaskState::Update;
	}

	FResult UpdateGear(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		const int32 PartySlot = Slot - FirstGearTask;
		const int32 Dust = Slot + GearDustBase;
		int32 Target = -1;
		switch (Task.Message)
		{
		case WalkToSlotMessage:
			Task.Step = FollowerStep::WalkToSlot;
			Task.Message = 0;
			Target = Task.MessageArgument;
			break;
		case LeaveGearMessage:
			Task.Message = 0;
			Task.Step = GearStep::StepDown;
			break;
		case DisembarkMessage:
			Task.Message = 0;
			Task.Step = GearStep::Disembark;
			break;
		case AboardMessage:
			Task.Step = GearStep::Done;
			Task.Message = 0;
			SetRiding(Context, PartySlot, 1);
			break;
		case GatherMessage:
			Task.Message = 0;
			Task.Step = FollowerStep::GoToLead;
			break;
		case DismountMessage:
			Task.Message = 0;
			Task.Step = GearStep::Dismount;
			break;
		default:
			break;
		}

		std::optional<int16> NextStep;
		switch (Task.Step)
		{
		case GearStep::Parked:
		case GearStep::Ridden:
			if (Riding(Context, PartySlot) == 1)
			{
				const FWorldMapTrailEntry& Entry = Context.Trail[static_cast<std::size_t>((Context.TrailIndex - Task.Word(WorldMapTaskWord::Follow)) & 0x1F)];
				if (SameSpot(Task.Position, Entry.Position))
				{
					if (SpriteAnimation(Context, Task) != 0)
					{
						SetSpriteAnimation(Context, Task, 0);
						Context.Particles.Stop(Dust);
					}
				}
				else
				{
					if (SpriteAnimation(Context, Task) != 1)
					{
						SetSpriteAnimation(Context, Task, 1);
					}
					UpdateDust(Context, Dust, Task);
				}
				CopySpot(Task.Position, Entry.Position);
				Task.Heading = Entry.Heading;
			}
			else if (SpriteAnimation(Context, Task) != KneelingAnimation)
			{
				SetSpriteAnimation(Context, Task, KneelingAnimation);
				Context.Particles.Stop(Dust);
			}
			break;
		case GearStep::Aboard:
		{
			const FWorldMapTask& Vehicle = TaskAt(Context, FWorldMapContext::VehicleSlot);
			Task.Position.X = Vehicle.Position.X;
			Task.Position.Z = Vehicle.Position.Z;
			Task.Heading = Vehicle.Heading;
			break;
		}
		case FollowerStep::WalkToSlot:
		case FollowerStep::WalkingToSlot:
			if (Task.Step == FollowerStep::WalkToSlot)
			{
				if (Target < 0)
				{
					Context.Fault = MakeError("a world map gear walks to a slot no message named");
					break;
				}
				HeadFor(Context, Task, TaskAt(Context, Target).Position);
				SetSpriteAnimation(Context, Task, 1);
				++Task.Step;
			}
			if (WalkOn(Context, Task) == 3)
			{
				++Task.Step;
			}
			UpdateDust(Context, Dust, Task);
			break;
		case FollowerStep::Embark:
			if (SendMessage(Context, Task.MessageArgument, AboardMessage))
			{
				Task.Hidden = 1;
				Task.Step = GearStep::Aboard;
				Context.Particles.Stop(Dust);
			}
			break;
		case FollowerStep::GoToLead:
		case FollowerStep::GoingToLead:
			if (Task.Step == FollowerStep::GoToLead)
			{
				const FWorldMapTask& Lead = TaskAt(Context, FirstGearTask);
				const int32 DeltaX = Lead.Position.X - Task.Position.X;
				const int32 DeltaZ = Lead.Position.Z - Task.Position.Z;
				const int32 Heading = (GteRatan2(DeltaZ, DeltaX, *Context.Arctangents) + 0x400) & 0xFFF;
				Task.Heading = static_cast<int16>(Heading);
				Task.Direction.X = GteSin(Heading);
				Task.Direction.Z = -GteCos(Task.Heading);
				Task.Word(WorldMapTaskWord::WalkX) = Lead.Position.X >> 12;
				Task.Word(WorldMapTaskWord::WalkZ) = Lead.Position.Z >> 12;
				SetSpriteAnimation(Context, Task, 1);
				++Task.Step;
			}
			if (WalkOn(Context, Task) == 3)
			{
				NextStep = static_cast<int16>(Task.Step + 1);
			}
			break;
		case FollowerStep::ReportToLead:
			if (SendMessage(Context, FirstGearTask, GearBackMessage))
			{
				NextStep = GearStep::Ridden;
			}
			break;
		case GearStep::Dismount:
			if (PartyCharacter(Context, PartySlot) == ChuChu)
			{
				Task.Timer = 1;
				NextStep = GearStep::Kneel;
			}
			else
			{
				Smoke(Context, Task, Slot - FirstGearTask + LeadGearSmoke);
				Task.Timer = 8;
				NextStep = static_cast<int16>(Task.Step + 1);
			}
			break;
		case GearStep::Smoke:
			Task.Timer = static_cast<int16>(Task.Timer - 1);
			if (Task.Timer > 0)
			{
				break;
			}
			Task.Hidden = 1;
			Task.Timer = 0x10;
			NextStep = static_cast<int16>(Task.Step + 1);
			break;
		case GearStep::Kneel:
			Task.Timer = static_cast<int16>(Task.Timer - 1);
			if (Task.Timer > 0)
			{
				break;
			}
			SetSpriteAnimation(Context, Task, KneelingAnimation);
			Task.Hidden = 1;
			NextStep = GearStep::Aboard;
			break;
		case GearStep::StepDown:
			SetSpriteAnimation(Context, Task, 0);
			ClearDirection(Task);
			NextStep = static_cast<int16>(Task.Step + 1);
			break;
		case GearStep::CallLeader:
			if (SendMessage(Context, Slot - 3, LeaveGearMessage))
			{
				TaskAt(Context, Slot - 3).MessageArgument = static_cast<int16>(Slot);
				NextStep = static_cast<int16>(Task.Step + 1);
			}
			break;
		case GearStep::Reset:
			Task.Step = GearStep::Parked;
			break;
		case GearStep::Disembark:
			LeaveVehicle(Context, Task);
			SetSpriteAnimation(Context, Task, 1);
			NextStep = static_cast<int16>(Task.Step + 1);
			break;
		case GearStep::Disembarking:
			if (WalkOn(Context, Task) == 3)
			{
				SetSpriteAnimation(Context, Task, 0);
				ClearDirection(Task);
				NextStep = static_cast<int16>(Task.Step + 1);
			}
			break;
		case GearStep::Landed:
			NextStep = GearStep::Ridden;
			break;
		default:
			break;
		}
		if (NextStep)
		{
			Task.Step = *NextStep;
		}
		if (CastsShadow(Context, Task, PartySlot))
		{
			AddShadow(Context, 1, Task.Position);
		}
		WriteGearPlace(Context, Task, PartySlot);
		return WorldMapTaskState::Update;
	}
}
