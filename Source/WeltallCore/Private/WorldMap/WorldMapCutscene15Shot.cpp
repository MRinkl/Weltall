#include "WorldMap/WorldMapCutscene15.h"

#include "Graphics/GteMath.h"
#include "Graphics/GteMatrix.h"
#include "Graphics/GteVectors.h"

namespace Weltall::WorldMapOps::Scene15
{
	/** The shot's and its rings' packets: blended from the page at (300h, 100h) with the CLUT at (0, 1FFh), their texture tinted blue. */
	constexpr uint16 ShotPage = 0x1C;
	constexpr uint16 ShotClut = 0x7FC0;
	constexpr std::array<uint8, 3> ShotColour{0x3C, 0x3C, 0xC0};
	constexpr int32 AddingQuarter = 3;
	constexpr int32 Adding = 1;

	void SetShotLook(FWorldMapObject& Object, int32 Blend)
	{
		FWorldMapPacketLook Look;
		Look.Page = static_cast<uint16>(ShotPage | Blend << 5);
		Look.Clut = ShotClut;
		Look.bSemiTransparent = true;
		Look.Colours = {ShotColour, ShotColour};
		Object.Look = Look;
	}

	/** The shot's steps (+20h). */
	namespace ShotStep
	{
		constexpr int16 Flying = 0;
		constexpr int16 LastFlight = 1;
		constexpr int16 Diving = 2;
		constexpr int16 Sinking = 3;
		constexpr int16 Grazing = 0x10;
		constexpr int16 FlyingOn = 0x11;
		constexpr int16 Passing = 0x18;
		constexpr int16 Struck = 0x40;
		constexpr int16 Gone = 0x41;
	}
	/** The shot's words: where it holds (+50h, +54h: x and z, 20.12). */
	constexpr int32 HoldX = 0;
	constexpr int32 HoldZ = 1;
	/** How far behind the start it begins each flight, in ticks of flight (the multipliers of its direction). */
	constexpr int32 FirstLead = -0x3680;
	/** The basis it and its rings lie along: the course's side, up (y points down), and the course. */
	constexpr FGteMatrix CourseBasis{{{{0xDA6, 0, 0x85A}, {0, -0x1000, 0}, {-0x85A, 0, 0xDA6}}}, {}};
	constexpr std::array<int32, 3> ShotScale{0x800, 0x800, 0x1800};
	constexpr int16 Spin = 0x100;

	void SetCourse(FWorldMapTask& Task, const FWorldMapShotEntrance& Shot)
	{
		Task.Direction.X = Course.X;
		Task.Direction.Y = Course.Y;
		Task.Direction.Z = Course.Z;
		Task.Word(HoldX) = static_cast<int32>(Shot.Hold[0]) << 12;
		Task.Word(HoldZ) = static_cast<int32>(Shot.Hold[1]) << 12;
	}

	/** The angle the trails are turned by: the course's, about y. */
	FGteVector CourseAngles(const FWorldMapContext& Context)
	{
		return {0, static_cast<int16>(GteRatan2(-Course.X, Course.Z, *Context.Arctangents) & 0xFFF), 0};
	}

	void HideShot(FWorldMapContext& Context)
	{
		for (const int32 Part : ShotParts)
		{
			ObjectAt(Context, Part).Hidden = 1;
		}
	}

	/** A model spun a step about its length and laid along a basis. */
	void SpinAlong(FWorldMapObject& Object, const FGteMatrix& Basis)
	{
		Object.Angles[2] = static_cast<int16>((Object.Angles[2] + Spin) & 0xFFF);
		Object.Matrix.M = MulMatrix(Basis, RotationYxz(Object.Angles)).M;
	}

	FResult InitShot(FWorldMapContext& Context, int32 Slot)
	{
		const FWorldMapShotEntrance* Shot = Entrance(Context);
		if (Shot == nullptr)
		{
			return WorldMapTaskState::Init;
		}
		if (Context.Objects.size() < static_cast<std::size_t>(ModelCount))
		{
			Context.Fault = MakeError("scene 15 has " + std::to_string(Context.Objects.size()) + " models where it moves 11");
			return WorldMapTaskState::Init;
		}
		AttachObject(Context, ShotBody, 2);
		AttachObject(Context, ShotBody, 3);
		SetShotLook(ObjectAt(Context, 1), AddingQuarter);
		SetShotLook(ObjectAt(Context, 2), AddingQuarter);
		SetShotLook(ObjectAt(Context, 3), Adding);
		FWorldMapObject& Body = ObjectAt(Context, ShotBody);
		Body.Hidden = 0;
		Body.Angles = {0, 0, 0};
		Body.Matrix.M = RotationYxz(Body.Angles).M;
		FWorldMapTask& Task = TaskAt(Context, Slot);
		Task.Step = ShotStep::Flying;
		SetCourse(Task, *Shot);
		Task.Position.X = Context.Start.X + Task.Direction.X * FirstLead;
		Task.Position.Y = Context.Start.Y;
		Task.Position.Z = Context.Start.Z + Task.Direction.Z * FirstLead;
		return WorldMapTaskState::Update;
	}

	FResult UpdateShot(FWorldMapContext& Context, int32 Slot)
	{
		const FWorldMapShotEntrance* Shot = Entrance(Context);
		if (Shot == nullptr)
		{
			return WorldMapTaskState::Update;
		}
		FWorldMapTask& Task = TaskAt(Context, Slot);
		FWorldMapVector& Place = Task.Position;
		FWorldMapVector& Way = Task.Direction;
		FResult Result = WorldMapTaskState::Update;
		if (Task.Message != 0)
		{
			SetCourse(Task, *Shot);
		}
		switch (Task.Message)
		{
		case ShotMessage::FirstFlight:
			Task.Message = 0;
			Task.Step = ShotStep::Flying;
			Place.X = Context.Start.X + Way.X * FirstLead;
			Place.Y = Context.Start.Y;
			Place.Z = Context.Start.Z + Way.Z * FirstLead;
			break;
		case ShotMessage::SecondFlight:
			Task.Message = 0;
			Task.Step = ShotStep::Flying;
			Place.X = Context.Start.X + Way.X * -0x980;
			Place.Z = Context.Start.Z + Way.Z * FirstLead + Way.Z * 0x2D * 0x100;
			break;
		case ShotMessage::ThirdFlight:
			Task.Message = 0;
			Task.Step = ShotStep::Flying;
			Place.X = Context.Start.X + Way.X * -0x1880;
			Place.Z = Context.Start.Z + Way.Z * -0x1880;
			break;
		case ShotMessage::LastFlight:
		case ShotMessage::PassFlight:
			Task.Step = Task.Message == ShotMessage::LastFlight ? ShotStep::LastFlight : ShotStep::Passing;
			Task.Message = 0;
			Place.X = Context.Start.X + Way.X * -0x380;
			Place.Z = Context.Start.Z + Way.Z * FirstLead + Way.Z * 0x33 * 0x100;
			break;
		case ShotMessage::Dive:
			Task.Step = ShotStep::Diving;
			Way.X = Dive.X;
			Way.Y = Dive.Y;
			Task.Message = 0;
			Way.Z = Dive.Z;
			Place.X = HitX;
			Place.Z = HitZ;
			break;
		case ShotMessage::GrazeFlight:
			Task.Message = 0;
			Task.Step = ShotStep::Grazing;
			// The start of entrances 0 and 2 with the last flight's lead, its z not yet wrapped into the world.
			Place.X = Way.X * 0x3300 + 0x56F2000;
			Place.Z = Way.Z * 0x3300 + 0x7F2C000;
			break;
		default:
			break;
		}
		Place.X += Way.X * FlightStep;
		Place.Y += Way.Y * FlightStep;
		Place.Z += Way.Z * FlightStep;
		switch (Task.Step)
		{
		case ShotStep::Flying:
			if (Place.X < Task.Word(HoldX) && Task.Word(HoldZ) < Place.Z)
			{
				Place.X = Task.Word(HoldX);
				Place.Z = Task.Word(HoldZ);
				SendRings(Context, RingMessage::Hold);
			}
			break;
		case ShotStep::LastFlight:
			if (Place.X < HitX && HitZ < Place.Z)
			{
				Place.X = HitX;
				Place.Z = HitZ;
				Task.Step = ShotStep::Struck;
				Context.Particles.Start(HitEffect, EffectPlace(Place), std::nullopt);
			}
			break;
		case ShotStep::Diving:
			if (Place.Y > -0x80000)
			{
				const std::array<int16, 4> Ground{static_cast<int16>(GroundX >> 12), static_cast<int16>(Context.Ground.Height(GroundX, GroundZ) >> 12),
					static_cast<int16>(GroundZ >> 12), 0};
				for (const int32 Effect : GroundEffects)
				{
					Context.Particles.Start(Effect, Ground, std::nullopt);
				}
				Task.Step = ShotStep::Sinking;
			}
			break;
		case ShotStep::Sinking:
			if (Place.Y > 0x100000)
			{
				Place.X += Way.X * -FlightStep;
				Place.Y += Way.Y * -FlightStep;
				Place.Z += Way.Z * -FlightStep;
				HideShot(Context);
				Task.Step = ShotStep::Gone;
			}
			break;
		case ShotStep::Grazing:
			if (Place.X < HitX && HitZ < Place.Z)
			{
				Task.Step = ShotStep::FlyingOn;
				const std::array<int16, 4> Hit{static_cast<int16>(HitX >> 12), static_cast<int16>(Place.Y >> 12), static_cast<int16>(HitZ >> 12), 0};
				Context.Particles.Start(HitEffect, Hit, std::nullopt);
				Context.Particles.Start(GrazeEffect, Hit, std::nullopt);
			}
			break;
		case ShotStep::FlyingOn:
			if (Place.X < GrazeEndX && GrazeEndZ < Place.Z)
			{
				Place.X = GrazeEndX;
				Place.Z = GrazeEndZ;
				HideShot(Context);
				SendRings(Context, RingMessage::Linger);
				Result = WorldMapTaskState::Idle;
			}
			break;
		case ShotStep::Passing:
			if (Place.X < PassEndX && PassEndZ < Place.Z)
			{
				HideShot(Context);
				SendRings(Context, RingMessage::Hold);
				Task.Step = ShotStep::Gone;
			}
			break;
		case ShotStep::Struck:
			Place.X = HitX;
			Place.Z = HitZ;
			break;
		case ShotStep::Gone:
			Result = WorldMapTaskState::Idle;
			break;
		default:
			break;
		}
		Context.Ground.Wrap(Place);
		FWorldMapObject& Body = ObjectAt(Context, ShotBody);
		Body.X = Place.X >> 12;
		Body.Y = Place.Y >> 12;
		Body.Z = Place.Z >> 12;
		FGteVector Angles{};
		switch (Task.Step)
		{
		case ShotStep::Flying:
		case ShotStep::LastFlight:
		case ShotStep::Grazing:
		case ShotStep::Passing:
			SpinAlong(Body, CourseBasis);
			Angles = CourseAngles(Context);
			break;
		case ShotStep::Diving:
		case ShotStep::Sinking:
		{
			// The dive's basis: its side and its lift from the way back up it (the drawing's z runs against the world's), under an up that points down the screen.
			const FGteLongVector Back{-Dive.X, -Dive.Y, Dive.Z};
			const FGteLongVector Side = VectorNormal(OuterProduct12({0, 0x1000, 0}, Back));
			const FGteLongVector Lift = VectorNormal(OuterProduct12(Back, Side));
			FGteMatrix Basis;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Basis.M[0][Axis] = static_cast<int16>(Side[Axis]);
				Basis.M[1][Axis] = static_cast<int16>(Lift[Axis]);
				Basis.M[2][Axis] = static_cast<int16>(Back[Axis]);
			}
			AnglesFromMatrix(Basis, *Context.Arctangents, Angles);
			SpinAlong(Body, Transpose(Basis));
			break;
		}
		case ShotStep::FlyingOn:
		case ShotStep::Gone:
			Context.Particles.Stop(TrailEffect);
			return Result;
		default:
			return Result;
		}
		Body.Matrix.M = ScaleColumns(Body.Matrix, ShotScale).M;
		Context.Particles.Start(TrailEffect, EffectPlace(Place), Angles);
		return Result;
	}

	/** A ring's steps (+20h): riding with the shot, and left behind (shrinking once its timer has run out). */
	namespace RingStep
	{
		constexpr int16 Riding = 0;
		constexpr int16 Left = 1;
	}
	/** A ring's word: its width (+5Ch, 4.12). */
	constexpr int32 RingWidth = 3;
	constexpr int32 RingLength = 0x2000;
	constexpr int32 RingShrink = 0x80;
	/** The rings' trails start 2048 units behind the shot. */
	constexpr int32 RingTrailX = 0x42D000;
	constexpr int32 RingTrailZ = -0x6D3000;

	uint16 RingScale(const FWorldMapContext& Context, int32 Slot)
	{
		return Context.Tables->Cutscenes.Directors.ShotRingScales[static_cast<std::size_t>(Slot - FirstRingSlot)];
	}

	void SetRingCourse(FWorldMapTask& Task)
	{
		Task.Direction.X = Course.X;
		Task.Direction.Y = Course.Y;
		Task.Direction.Z = Course.Z;
	}

	FResult InitRing(FWorldMapContext& Context, int32 Slot)
	{
		if (Slot < FirstRingSlot || Slot > LastRingSlot || Context.Objects.size() < static_cast<std::size_t>(ModelCount))
		{
			Context.Fault = MakeError("scene 15's ring in slot " + std::to_string(Slot) + " has no model");
			return WorldMapTaskState::Init;
		}
		FWorldMapTask& Task = TaskAt(Context, Slot);
		SetShotLook(ObjectAt(Context, Slot), Slot == LastRingSlot ? Adding : AddingQuarter);
		SetRingCourse(Task);
		Task.Step = RingStep::Riding;
		Task.Timer = 0x3C;
		Task.Word(RingWidth) = RingScale(Context, Slot);
		return WorldMapTaskState::Update;
	}

	/** A ring shown again at its first width, unturned, its trails stopped. */
	void ResetRing(FWorldMapContext& Context, FWorldMapTask& Task, int32 Slot)
	{
		Task.Word(RingWidth) = RingScale(Context, Slot);
		FWorldMapObject& Object = ObjectAt(Context, Slot);
		Object.Hidden = 0;
		Object.Angles = {0, 0, 0};
		Object.Matrix.M = RotationYxz(Object.Angles).M;
		for (const int32 Effect : RingEffects)
		{
			Context.Particles.Stop(Effect);
		}
	}

	FResult UpdateRing(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		FWorldMapObject& Object = ObjectAt(Context, Slot);
		switch (Task.Message)
		{
		case RingMessage::Wake:
			Task.Message = 0;
			break;
		case RingMessage::Hold:
			Task.Message = 0;
			Task.Step = RingStep::Left;
			break;
		case RingMessage::Ride:
			Task.Timer = 0x3C;
			SetRingCourse(Task);
			Task.Message = 0;
			Task.Step = RingStep::Riding;
			ResetRing(Context, Task, Slot);
			break;
		case RingMessage::Shrink:
			Task.Timer = 4;
			Task.Step = RingStep::Left;
			Task.Message = 0;
			ResetRing(Context, Task, Slot);
			break;
		case RingMessage::Linger:
			Task.Step = RingStep::Left;
			Task.Message = 0;
			Task.Timer = 0x1E;
			break;
		default:
			break;
		}
		if (Task.Step == RingStep::Riding)
		{
			const FWorldMapVector& Shot = TaskAt(Context, ShotSlot).Position;
			Task.Position.X = Shot.X;
			Task.Position.Y = Shot.Y;
			Task.Position.Z = Shot.Z;
			const std::array<int16, 4> Behind{static_cast<int16>((Task.Position.X + RingTrailX) >> 12), static_cast<int16>(Task.Position.Y >> 12),
				static_cast<int16>((Task.Position.Z + RingTrailZ) >> 12), 0};
			const FGteVector Angles = CourseAngles(Context);
			for (const int32 Effect : RingEffects)
			{
				Context.Particles.Start(Effect, Behind, Angles);
			}
		}
		else if (Task.Step == RingStep::Left)
		{
			Task.Timer = static_cast<int16>(Task.Timer - 1);
			if (Task.Timer < 0)
			{
				Task.Timer = 0;
				Task.Word(RingWidth) -= RingShrink;
			}
		}
		SpinAlong(Object, CourseBasis);
		Object.X = Task.Position.X >> 12;
		Object.Y = Task.Position.Y >> 12;
		Object.Z = Task.Position.Z >> 12;
		Object.Matrix.M = ScaleColumns(Object.Matrix, {Task.Word(RingWidth), Task.Word(RingWidth), RingLength}).M;
		if (Task.Word(RingWidth) < 0)
		{
			Task.Word(RingWidth) = 0;
			Object.Hidden = 1;
			return WorldMapTaskState::Idle;
		}
		return WorldMapTaskState::Update;
	}

	FResult InitLightRing(FWorldMapContext& Context, int32 Slot)
	{
		if (Context.Objects.size() < static_cast<std::size_t>(ModelCount))
		{
			Context.Fault = MakeError("scene 15 has " + std::to_string(Context.Objects.size()) + " models where it moves 11");
			return WorldMapTaskState::Init;
		}
		FWorldMapTask& Task = TaskAt(Context, Slot);
		Task.Position.X = GroundX;
		Task.Position.Y = -0x80000;
		Task.Step = 0;
		Task.Position.Z = GroundZ;
		return InitGlowRing(Context, Task, LightRing);
	}

	FResult UpdateLightRing(FWorldMapContext& Context, int32 Slot)
	{
		return UpdateGlowRing(Context, TaskAt(Context, Slot), LightRing, 3);
	}
}
