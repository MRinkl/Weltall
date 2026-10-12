#include "BattlingFightOps.h"
#include "BattlingMath.h"

namespace Weltall::BattlingFightOps
{
	namespace
	{
		using namespace BattlingMath;

		/** The five sprite sets the program loads: each one's frame count and rise (the second and fourth rise). */
		struct FSpriteSet
		{
			uint8 Count;
			int16 Rise;
		};

		constexpr std::array<FSpriteSet, 5> SpriteSets{{{12, 0}, {16, -2}, {12, 0}, {16, -2}, {16, 0}}};
		constexpr uint8 TintedSet = 2;
		constexpr uint8 TrailFrames = 7;
		constexpr int32 SparkGravity = 4;
		constexpr int32 SparkSpread = 0x300;
		constexpr int32 SparkSpeed = 8;
		constexpr int32 SparkSpeedRange = 0x20;
		constexpr int16 SparkLife = 0x20;

		FBattlingEffect* FreeSlot(FBattlingEffects& Effects)
		{
			for (FBattlingEffect& Slot : Effects.Slots)
			{
				if (!Slot.bActive)
				{
					return &Slot;
				}
			}
			return nullptr;
		}

		std::array<int16, 3> Short(const FBattlingPoint& Point)
		{
			return {static_cast<int16>(Point[0]), static_cast<int16>(Point[1]), static_cast<int16>(Point[2])};
		}

		void AddSprite(FBattlingFight& Fight, const std::array<int16, 3>& Point, uint8 Set)
		{
			FBattlingEffect* Slot = FreeSlot(Fight.Effects);
			if (Slot == nullptr)
			{
				return;
			}
			Slot->bActive = true;
			Slot->Kind = EBattlingEffectKind::Sprite;
			Slot->Frame = 0;
			Slot->Set = Set;
			Slot->Count = SpriteSets[Set].Count;
			Slot->RiseSpeed = 0;
			Slot->RiseAcceleration = SpriteSets[Set].Rise;
			Slot->Position = Point;
			Slot->bTinted = Set == TintedSet;
			if (Slot->bTinted)
			{
				Slot->Colour = Fight.Effects.Colour;
			}
		}

		/** A point jittered by up to Range / 2 on each axis, three draws. */
		template <typename TValue, std::size_t Count>
		void Jitter(FPsxRandom& Random, std::array<TValue, Count>& Point, int32 Range)
		{
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Point[Axis] = static_cast<TValue>(Point[Axis] + Random.Next() % Range - Range / 2);
			}
		}

		/** The effect's tick that links a source's segments: one tick before this one. */
		uint16 PreviousTick(const FBattlingEffects& Effects)
		{
			return static_cast<uint16>(Effects.Tick - 1);
		}

		/** Three strands of seven jittered segments from From to To, coloured by style: 0 blue sparks, 1 yellow, 2 the rubber band's chain. */
		void LineChain(FBattlingFight& Fight, const FBattlingPoint& From, const FBattlingPoint& To, int32 Style)
		{
			const FBattlingPoint Step{(To[0] - From[0]) / 7, (To[1] - From[1]) / 7, (To[2] - From[2]) / 7};
			for (int32 Strand = 0; Strand < 3; ++Strand)
			{
				FBattlingPoint Along = From;
				FBattlingPoint Previous = From;
				for (int32 Segment = 0; Segment < 7; ++Segment)
				{
					for (std::size_t Axis = 0; Axis < 3; ++Axis)
					{
						Along[Axis] += Step[Axis];
					}
					FBattlingPoint Jittered = Along;
					Jitter(Fight.Random, Jittered, 0x40);
					std::array<uint8, 3> Colour{};
					if (Style == 1)
					{
						const int32 Value = Fight.Random.Next() % 0x100 + 0x40;
						const uint8 Bright = Value < 0x100 ? static_cast<uint8>(Value) : static_cast<uint8>(0xFF);
						Colour = {Bright, Bright, static_cast<uint8>(Value / 3)};
					}
					else if (Style == 0)
					{
						const uint8 Red = static_cast<uint8>(Fight.Random.Next() & 0x3F);
						const uint8 Green = static_cast<uint8>(Fight.Random.Next() % 0xBF + 0x40);
						Colour = {Red, Green, 0xFF};
					}
					else
					{
						Colour = (Segment & 1) == 0 ? std::array<uint8, 3>{0xFF, 0, 0} : std::array<uint8, 3>{0xFF, 0xFF, 0xFF};
					}
					AddLine(Fight, Previous, Segment == 6 ? To : Jittered, Colour);
					Previous = Jittered;
				}
			}
		}
	}

	void SpawnEffect(FBattlingFight& Fight, const FBattlingPoint& Point, int32 Effect)
	{
		std::array<int16, 3> Place = Short(Point);
		if (Effect >= 8 && Effect <= 12)
		{
			Jitter(Fight.Random, Place, 0x30);
			Effect -= 8;
		}
		if (Effect >= 0 && Effect <= 4)
		{
			AddSprite(Fight, Place, static_cast<uint8>(Effect));
		}
	}

	void AddTrail(FBattlingFight& Fight, int32 Side, const FBattlingPoint& Point, uint32 Id, int32 Kind)
	{
		FBattlingEffects& Effects = Fight.Effects;
		const uint16 Pass = static_cast<uint16>(Fight.Pass);
		for (const FBattlingEffect& Slot : Effects.Slots)
		{
			if (Slot.bActive && Slot.Kind == EBattlingEffectKind::Trail && Slot.Id == Id && Slot.Pass == Pass)
			{
				return;
			}
		}
		FBattlingEffect* Slot = FreeSlot(Effects);
		if (Slot == nullptr)
		{
			return;
		}
		Slot->Side = static_cast<uint8>(Side & 1);
		Slot->Count = TrailFrames;
		Slot->Kind = EBattlingEffectKind::Trail;
		Slot->bActive = true;
		Slot->Frame = 0;
		Slot->Position = Short(Point);
		Slot->Pass = Pass;
		Slot->Id = Id;
		Slot->Link = -1;
		Slot->Tick = Effects.Tick;
		Slot->Length = Fight.Tables.TrailLengths[static_cast<std::size_t>(Kind)];
		for (std::size_t Index = 0; Index < Effects.Slots.size(); ++Index)
		{
			const FBattlingEffect& Other = Effects.Slots[Index];
			if (Other.bActive && Other.Id == Id && &Other != Slot && Other.Kind == EBattlingEffectKind::Trail && Other.Tick == PreviousTick(Effects))
			{
				Slot->Link = static_cast<int8>(Index);
			}
		}
	}

	void AddSwept(FBattlingFight& Fight, int32 Side, const FBattlingPoint& A, const FBattlingPoint& B, uint32 Id)
	{
		FBattlingEffects& Effects = Fight.Effects;
		const uint16 Pass = static_cast<uint16>(Fight.Pass);
		for (const FBattlingEffect& Slot : Effects.Slots)
		{
			if (Slot.bActive && Slot.Kind == EBattlingEffectKind::Swept && Slot.Id == Id && Slot.Pass == Pass)
			{
				return;
			}
		}
		FBattlingEffect* Slot = FreeSlot(Effects);
		if (Slot == nullptr)
		{
			return;
		}
		Slot->Side = static_cast<uint8>(Side & 1);
		Slot->Count = TrailFrames;
		Slot->Kind = EBattlingEffectKind::Swept;
		Slot->bActive = true;
		Slot->Frame = 0;
		Slot->Position = Short(A);
		Slot->Pass = Pass;
		Slot->End = Short(B);
		Slot->Id = Id;
		Slot->Link = -1;
		Slot->Tick = Effects.Tick;
		for (std::size_t Index = 0; Index < Effects.Slots.size(); ++Index)
		{
			const FBattlingEffect& Other = Effects.Slots[Index];
			if (Other.bActive && Other.Id == Id && &Other != Slot && Other.Kind == EBattlingEffectKind::Swept && Other.Tick == PreviousTick(Effects))
			{
				Slot->Link = static_cast<int8>(Index);
			}
		}
	}

	void SpawnLineEffect(FBattlingFight& Fight, const FBattlingPoint& From, const FBattlingPoint& To, int32 Effect)
	{
		switch (Effect)
		{
		case 0x11:
			LineChain(Fight, From, To, 0);
			break;
		case 0x12:
			LineChain(Fight, From, To, 1);
			break;
		case 0x13:
			LineChain(Fight, From, To, 2);
			break;
		default:
			break;
		}
	}

	void AddLine(FBattlingFight& Fight, const FBattlingPoint& From, const FBattlingPoint& To, const std::array<uint8, 3>& Colour)
	{
		FBattlingEffects& Effects = Fight.Effects;
		if (Effects.LinesAdded >= FBattlingEffects::LineCount)
		{
			return;
		}
		FBattlingLine& Line = Effects.Lines[static_cast<std::size_t>(Effects.LinesAdded)];
		Line.From = Short(From);
		Line.To = Short(To);
		Line.Colour = Colour;
		++Effects.LinesAdded;
	}

	void SpawnDust(FBattlingFight& Fight, const FBattlingPoint& Point, int32 Count)
	{
		for (int32 Index = 0; Index < FBattlingEffects::DustSpawnCount; ++Index)
		{
			if (Count == 0)
			{
				return;
			}
			FBattlingDust& Dust = Fight.Effects.Dust[static_cast<std::size_t>(Index)];
			if (Dust.bLive)
			{
				continue;
			}
			const int16 X = static_cast<int16>(Point[0] - 0x20 + Fight.Random.Next() % 0x40);
			const int16 Z = static_cast<int16>(Point[2] - 0x20 + Fight.Random.Next() % 0x40);
			Dust.Position[0] = X;
			Dust.Position[2] = Z;
			if (!FBattlingTerrain::Covers(X, Z))
			{
				Fight.Fault = EBattlingFault::OutsideHeightfield;
				return;
			}
			const int16 Ground = static_cast<int16>(Fight.Terrain.CellAt((Z >> 8) * FBattlingTerrain::Size + (X >> 8)));
			Dust.Position[1] = Ground;
			Dust.Ground = Ground;
			const int32 Speed = Fight.Random.Next();
			--Count;
			Dust.bLive = true;
			Dust.Speed = static_cast<int8>(-(Speed % 10 + 10));
		}
	}

	void SpawnSplashes(FBattlingFight& Fight, const FBattlingPoint& Base, const FBattlingPoint& PreviousBase)
	{
		if (Base[1] <= 0x80 && PreviousBase[1] <= 0x80)
		{
			return;
		}
		FGteLongVector Motion{Base[2] - PreviousBase[2], Base[1] - PreviousBase[1], PreviousBase[0] - Base[0]};
		int32 Count = Length3(Motion[0], Motion[1], Motion[2]) / 6;
		if (Count < 1)
		{
			Count = 1;
		}
		Motion[1] = 0x1000;
		int32 Across = Length2(Motion[0], Motion[2]);
		if (Across < 0x10)
		{
			Across = 0;
		}
		int32 NormalX = 0;
		int32 NormalZ = 0;
		if (Motion[2] != 0 || Motion[0] != 0)
		{
			const FGteLongVector Normal = VectorNormal(Motion);
			NormalX = Normal[0];
			NormalZ = Normal[2];
		}
		int32 Rise = Base[1] - PreviousBase[1];
		if (Rise < 0)
		{
			Rise = -Rise;
		}
		uint32 Life = static_cast<uint32>(Rise / 3 + 1);
		if (0x78 < Life)
		{
			Life = 0x78;
		}
		const int8 Spread = static_cast<int8>(Across / 7);
		for (int32 Index = 0; Index < FBattlingEffects::SplashUsedCount; ++Index)
		{
			if (Count == 0)
			{
				return;
			}
			FBattlingSplash& Splash = Fight.Effects.Splashes[static_cast<std::size_t>(Index)];
			if (Splash.Life != 0)
			{
				continue;
			}
			if (Across == 0)
			{
				Splash.SpeedX = static_cast<int8>((Fight.Random.Next() & 0xF) - 8);
				Splash.SpeedZ = static_cast<int8>((Fight.Random.Next() & 0xF) - 8);
			}
			else
			{
				const int32 Amount = Fight.Random.Next() % 0x1000;
				if ((Fight.Random.Next() & 1) == 0)
				{
					Splash.SpeedX = static_cast<int8>(-Spread - static_cast<int8>(NormalX * Amount >> 14));
					Splash.SpeedZ = static_cast<int8>(-Spread - static_cast<int8>(NormalZ * Amount >> 14));
				}
				else
				{
					Splash.SpeedX = static_cast<int8>(static_cast<int8>(NormalX * Amount >> 14) + Spread);
					Splash.SpeedZ = static_cast<int8>(static_cast<int8>(NormalZ * Amount >> 14) + Spread);
				}
			}
			if (Rise < 9)
			{
				Splash.SpeedY = -4;
				Splash.Life = 0x14;
			}
			else
			{
				const int32 Value = Fight.Random.Next();
				Splash.Life = static_cast<int16>(Life);
				Splash.SpeedY = static_cast<int16>(1 - static_cast<int16>(Value % (Rise / 2)));
			}
			Splash.Position[0] = static_cast<int16>(static_cast<int16>(Base[0]) - 0x10 + Fight.Random.Next() % 0x20 + Splash.SpeedX * 2);
			Splash.Position[1] = -0x10;
			Splash.Position[2] = static_cast<int16>(static_cast<int16>(Base[2]) - 0x10 + Fight.Random.Next() % 0x20 + Splash.SpeedZ * 2);
			--Count;
		}
	}

	void TickEffects(FBattlingFight& Fight)
	{
		FBattlingEffects& Effects = Fight.Effects;
		for (FBattlingEffect& Slot : Effects.Slots)
		{
			if (!Slot.bActive)
			{
				continue;
			}
			if (Slot.Frame == Slot.Count)
			{
				Slot.bActive = false;
				continue;
			}
			++Slot.Frame;
			if (Slot.Kind == EBattlingEffectKind::Sprite)
			{
				const int16 Speed = Slot.RiseSpeed;
				Slot.RiseSpeed = static_cast<int16>(Speed + Slot.RiseAcceleration);
				Slot.Position[1] = static_cast<int16>(Slot.Position[1] + Speed + Slot.RiseAcceleration);
			}
		}
		Effects.LinesShown = Effects.LinesAdded;
		Effects.LinesAdded = 0;
		++Effects.Tick;
	}

	void ClearEffects(FBattlingFight& Fight)
	{
		for (FBattlingEffect& Slot : Fight.Effects.Slots)
		{
			Slot.bActive = false;
		}
	}

	void ClearSplashesAndDust(FBattlingFight& Fight)
	{
		for (FBattlingSplash& Splash : Fight.Effects.Splashes)
		{
			Splash.Position = {};
			Splash.Life = 0;
		}
		Fight.Fighters[0].SandDust = 0;
		Fight.Fighters[1].SandDust = 0;
		for (FBattlingDust& Dust : Fight.Effects.Dust)
		{
			Dust.bLive = false;
			Dust.Position = {};
		}
	}

	void ClearSparks(FBattlingFight& Fight)
	{
		for (FBattlingSpark& Spark : Fight.Effects.Sparks)
		{
			Spark.Life = 0;
		}
	}

	void StepDrawnEffects(FBattlingFight& Fight, bool bDustAndSplashes)
	{
		FBattlingEffects& Effects = Fight.Effects;
		Effects.DrawnDustCount = 0;
		Effects.DrawnSplashCount = 0;
		// The dust steps half its grains a frame, those of the draw buffer's parity, each drawn before its step.
		for (int32 Index = static_cast<int32>(Fight.Pass & 1); bDustAndSplashes && Index < FBattlingEffects::DustStepCount; Index += 2)
		{
			FBattlingDust& Dust = Effects.Dust[static_cast<std::size_t>(Index)];
			if (!Dust.bLive)
			{
				continue;
			}
			Effects.DrawnDust[static_cast<std::size_t>(Effects.DrawnDustCount++)] = Dust.Position;
			const int16 Height = static_cast<int16>(Dust.Position[1] + Dust.Speed);
			Dust.Position[1] = Height;
			if (Dust.Ground <= Height)
			{
				Dust.bLive = false;
			}
			Dust.Speed = static_cast<int8>(Dust.Speed + 2);
		}
		for (int32 Index = 0; bDustAndSplashes && Index < FBattlingEffects::SplashUsedCount; ++Index)
		{
			FBattlingSplash& Splash = Effects.Splashes[static_cast<std::size_t>(Index)];
			if (Splash.Life == 0)
			{
				continue;
			}
			Splash.Position[0] = static_cast<int16>(Splash.Position[0] + Splash.SpeedX);
			Splash.Position[2] = static_cast<int16>(Splash.Position[2] + Splash.SpeedZ);
			// Drawn between its moves across and its move up or down.
			Effects.DrawnSplashes[static_cast<std::size_t>(Effects.DrawnSplashCount++)] = Splash.Position;
			Splash.Position[1] = static_cast<int16>(Splash.Position[1] + Splash.SpeedY);
			Splash.SpeedY = static_cast<int16>(Splash.SpeedY + 2);
			if (0x10 < Splash.Position[1])
			{
				Splash.Position[1] = 0x10;
			}
			Splash.Life = static_cast<int16>(Splash.Life - 1);
		}

		// The hit sparks: the tick's request starts sparks in free slots (directions from three draws each), then every live
		// spark falls and bounces on the plane y = 0.
		int32 Requested = Effects.SparkRequest;
		for (FBattlingSpark& Spark : Effects.Sparks)
		{
			if (Spark.Life != 0)
			{
				continue;
			}
			if (--Requested == -1)
			{
				break;
			}
			const int32 Lean = Fight.Random.Next() % SparkSpread;
			const int32 Turn = Fight.Random.Next();
			const std::array<int16, 3> Direction{static_cast<int16>(GteSin(Turn) * GteSin(Lean) >> 12), static_cast<int16>(-GteCos(Lean)),
				static_cast<int16>(GteCos(Turn) * GteSin(Lean) >> 12)};
			const int32 Speed = SparkSpeed + Fight.Random.Next() % SparkSpeedRange;
			Spark.Velocity = ScaleVector(Direction, Speed);
		}
		Requested = Effects.SparkRequest;
		for (FBattlingSpark& Spark : Effects.Sparks)
		{
			if (Spark.Life != 0)
			{
				continue;
			}
			if (--Requested == -1)
			{
				break;
			}
			Spark.Position = {static_cast<int16>(Effects.SparkSource[0]), static_cast<int16>(Effects.SparkSource[1]),
				static_cast<int16>(Effects.SparkSource[2])};
			Spark.Life = SparkLife;
		}
		for (std::size_t Index = 0; Index < Effects.Sparks.size(); ++Index)
		{
			FBattlingSpark& Spark = Effects.Sparks[Index];
			Effects.SparksDrawn[Index] = Spark.Life != 0;
			if (Spark.Life == 0)
			{
				continue;
			}
			--Spark.Life;
			Spark.Velocity[1] = static_cast<int16>(Spark.Velocity[1] + SparkGravity);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Spark.Position[Axis] = static_cast<int16>(Spark.Position[Axis] + Spark.Velocity[Axis]);
			}
			if (0 < Spark.Position[1])
			{
				const int16 Bounce = static_cast<int16>((Spark.Velocity[1] * -2) / 3);
				Spark.Velocity[1] = Bounce;
				if ((Bounce < 0 ? -Bounce : Bounce) < 4)
				{
					Spark.Life = 0;
				}
			}
		}
	}

	void InitSparkTiles(FBattlingFight& Fight)
	{
		for (FBattlingSpark& Spark : Fight.Effects.Sparks)
		{
			const int16 Size = static_cast<int16>(Fight.Random.Next() % 2 + 2);
			Spark.Height = Size;
			Spark.Width = static_cast<int16>(Size << 1);
		}
	}

	void InitSplashTiles(FBattlingFight& Fight)
	{
		for (FBattlingSplash& Splash : Fight.Effects.Splashes)
		{
			Splash.Width = static_cast<int16>(Fight.Random.Next() % 3 + 2);
			Splash.Height = static_cast<int16>(Fight.Random.Next() % 3 + 2);
		}
	}
}
