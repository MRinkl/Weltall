#include "Battle/Show/BattleShowExtensions.h"

#include "Battle/Show/BattleModelDrawing.h"

#include "Game/PsxRandom.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"
#include "Graphics/SpriteFile.h"

#include <algorithm>
#include <optional>

namespace Weltall
{
	namespace
	{
		/** The sprite's flag (+ACh) whose end ends its children with it. */
		constexpr uint32 KillsChildren = 0x20;
		/** A TMD object: its vertices (8 bytes each), its primitives and their count, at these words. */
		constexpr uint32 VertexTop = 0;
		constexpr uint32 PrimitiveTop = 0x10;
		constexpr uint32 PrimitiveCount = 0x14;
		/** A primitive's mode bit for a quad, and its type as the shards read it: mode bits 2-4 with 100h when it is lit. */
		constexpr uint8 QuadMode = 8;

		std::optional<uint32> ReadU32(const FSpriteFile& File, uint32 Offset)
		{
			if (Offset + 4 > File.Size())
			{
				return std::nullopt;
			}
			return static_cast<uint32>(File.Word(Offset)) | static_cast<uint32>(File.Word(Offset + 2)) << 16;
		}

		/** Where a primitive of the type keeps its three or four vertex indices (bytes from its start), as EXT3 reads them. */
		std::array<uint8, 4> IndexPlaces(uint32 Type)
		{
			switch (Type)
			{
			case 0x00: return {0x08, 0x0A, 0x0C, 0x0C};
			case 0x04: return {0x14, 0x16, 0x18, 0x18};
			case 0x08: return {0x08, 0x0A, 0x0C, 0x0E};
			case 0x0C: return {0x18, 0x1A, 0x1C, 0x1E};
			case 0x10: return {0x10, 0x12, 0x14, 0x14};
			case 0x14: return {0x1C, 0x1E, 0x20, 0x20};
			case 0x18: return {0x14, 0x16, 0x18, 0x1A};
			case 0x1C: return {0x24, 0x26, 0x28, 0x2A};
			case 0x100: return {0x0A, 0x0C, 0x0E, 0x0E};
			case 0x104: return {0x12, 0x14, 0x16, 0x16};
			case 0x108: return {0x0A, 0x0C, 0x0E, 0x10};
			case 0x10C: return {0x16, 0x18, 0x1A, 0x1C};
			// The lit gouraud triangle's first index is read from +10h (not +12h like the textured one's).
			case 0x110: return {0x10, 0x16, 0x1A, 0x1A};
			case 0x114: return {0x12, 0x16, 0x1A, 0x1A};
			default: return {0x16, 0x1A, 0x1E, 0x22};
			}
		}

		/** (rand() & FFh) × Range >> 8, the negative rounded toward zero. */
		int32 Drawn(FPsxRandom& Random, int32 Range)
		{
			int32 Value = (Random.Next() & 0xFF) * Range;
			if (Value < 0)
			{
				Value += 0xFF;
			}
			return Value >> 8;
		}

		/** A spin: the draw less half of itself, rounded as the original rounds it (never below 0 for a range that is not). */
		int16 SpinDrawn(FPsxRandom& Random, int32 Range)
		{
			int32 Product = (Random.Next() & 0xFF) * Range;
			if (Product < 0)
			{
				Product += 0xFF;
			}
			const int32 Value = Product >> 8;
			const int32 Sign = static_cast<int32>(static_cast<uint32>(Product) >> 31);
			return static_cast<int16>(Value - ((Value + Sign) >> 1));
		}

		template <typename TTask>
		TTask* Find(const std::vector<std::unique_ptr<TTask>>& Tasks, const IKernelTask* Task)
		{
			for (const std::unique_ptr<TTask>& Running : Tasks)
			{
				if (Running.get() == Task && Running->bLinked)
				{
					return Running.get();
				}
			}
			return nullptr;
		}

		template <typename TTask>
		TTask& Take(std::vector<std::unique_ptr<TTask>>& Tasks)
		{
			for (std::unique_ptr<TTask>& Kept : Tasks)
			{
				if (!Kept->bLinked)
				{
					*Kept = TTask{};
					return *Kept;
				}
			}
			Tasks.push_back(std::make_unique<TTask>());
			return *Tasks.back();
		}
	}

	void FBattleScroll::Update()
	{
		FSprite* Scrolled = Owner->Live(Sprite);
		if (Scrolled == nullptr)
		{
			return;
		}
		if (Module == 4)
		{
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Scroll[Axis] = static_cast<int32>(static_cast<uint32>(Scroll[Axis]) + static_cast<uint32>(Scrolled->Velocity[Axis]));
				Scrolled->Position[Axis] = static_cast<int32>(static_cast<uint32>(Scrolled->Position[Axis]) - static_cast<uint32>(Scrolled->Velocity[Axis]));
			}
			return;
		}
		Scroll[0] = static_cast<int32>(static_cast<uint32>(Scroll[0]) - static_cast<uint32>(Scrolled->Speed));
	}

	void FBattleShards::Update()
	{
		for (FBattleShard& Shard : Shards)
		{
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Shard.Place[Axis] = static_cast<int32>(static_cast<uint32>(Shard.Place[Axis]) + static_cast<uint32>(Shard.Velocity[Axis]));
			}
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Shard.Rotation[Axis] = static_cast<int16>(Shard.Rotation[Axis] + Shard.Spin[Axis]);
			}
			Shard.Velocity[1] = static_cast<int32>(static_cast<uint32>(Shard.Velocity[1]) + static_cast<uint32>(Shard.Fall));
		}
		if (--StepsLeft < 0)
		{
			bLinked = false;
			Owner->Sprites->UnlinkTask(*this);
		}
	}

	void FBattleExtensions::Begin()
	{
		for (std::unique_ptr<FBattleWave>& Wave : Waves)
		{
			Wave->bLinked = false;
		}
		for (std::unique_ptr<FBattleScroll>& Scroll : Scrolls)
		{
			Scroll->bLinked = false;
		}
		for (std::unique_ptr<FBattleShards>& Set : ShardSets)
		{
			Set->bLinked = false;
		}
	}

	void FBattleExtensions::StartWave(FSprite& Sprite, const std::array<int32, 6>& Values)
	{
		if (!Sprites->ReachesTask(Sprite, 0))
		{
			return;
		}
		FBattleWave& Wave = Take(Waves);
		Wave.Sprite = {&Sprite, Sprite.Task, Sprite.Task != nullptr ? Sprite.Task->Id : 0};
		Wave.Phase = Values[0];
		Wave.Amplitude = Values[1];
		Wave.AmplitudeStep = Values[2];
		Wave.Frequency = Values[3];
		Wave.FrequencyStep = Values[4];
		Wave.Speed = Values[5];
		Wave.bLinked = true;
		Sprites->LinkTask(Wave, &Sprite);
	}

	void FBattleExtensions::StartScroll(FSprite& Sprite, int32 Module)
	{
		if (!Sprites->ReachesTask(Sprite, 0))
		{
			return;
		}
		FBattleScroll& Scroll = Take(Scrolls);
		Scroll.Owner = this;
		Scroll.Sprite = {&Sprite, Sprite.Task, Sprite.Task != nullptr ? Sprite.Task->Id : 0};
		Scroll.Module = Module;
		if (Module == 5)
		{
			Scroll.Held = Sprite.Position;
		}
		Scroll.bLinked = true;
		Sprites->LinkTask(Scroll, &Sprite);
		Sprite.TimeFlags |= KillsChildren;
	}

	bool FBattleExtensions::StartShards(const FSprite& Sprite, const FGteMatrix& Matrix, const FShardMeasures& Measures, const FBattleModelDrawing& Packets)
	{
		if (Sprite.ModelFile == nullptr || !ShardModel.Read(*Sprite.ModelFile, Sprite.Model))
		{
			return false;
		}
		const FSpriteFile& File = *Sprite.ModelFile;
		const uint32 Model = Sprite.Model;
		const std::optional<uint32> Vertices = ReadU32(File, Model + VertexTop);
		const std::optional<uint32> Primitives = ReadU32(File, Model + PrimitiveTop);
		const std::optional<uint32> Count = ReadU32(File, Model + PrimitiveCount);
		if (!Vertices || !Primitives || !Count || *Count != ShardModel.Primitives.size())
		{
			return false;
		}
		FBattleShards& Set = Take(ShardSets);
		Set.Owner = this;
		Set.ModelFile = &File;
		Set.Model = Model;
		Set.Matrix = Matrix;
		Set.StepsLeft = Measures.Steps;
		Set.Serial = ++ShardSerial;
		Set.Shards.assign(*Count, FBattleShard{});
		uint32 Primitive = Model + *Primitives;
		for (std::size_t Number = 0; Number < Set.Shards.size(); ++Number)
		{
			FBattleShard& Shard = Set.Shards[Number];
			// The sprite's packets become the set's, with the colours they hold in each buffer.
			Packets.TakePackets(Sprite, ShardModel, Number, Shard);
			if (Primitive + 4 > File.Size())
			{
				return false;
			}
			const uint8 Mode = File.Byte(Primitive + 3);
			const uint32 Type = (Mode & 0x1Cu) | ((File.Byte(Primitive + 2) ^ 1u) & 1u) << 8;
			const std::array<uint8, 4> Places = IndexPlaces(Type);
			std::array<FGteVector, 4> Corner{};
			for (std::size_t Index = 0; Index < Corner.size(); ++Index)
			{
				const uint32 At = Model + *Vertices + 8 * static_cast<uint32>(File.Word(Primitive + Places[Index]));
				if (At + 6 > File.Size())
				{
					return false;
				}
				Corner[Index] = {static_cast<int16>(File.Word(At)), static_cast<int16>(File.Word(At + 2)), static_cast<int16>(File.Word(At + 4))};
			}
			// Its centre: the average of its corners, rounded toward zero, from sums that wrap in 16 bits.
			const bool bQuad = (Mode & QuadMode) != 0;
			FGteVector Centre{};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				int16 Sum = static_cast<int16>(Corner[0][Axis] + Corner[1][Axis] + Corner[2][Axis]);
				if (bQuad)
				{
					Sum = static_cast<int16>(Sum + Corner[3][Axis]);
				}
				Centre[Axis] = static_cast<int16>(bQuad ? Sum / 4 : Sum / 3);
			}
			if (!bQuad)
			{
				Corner[3] = Corner[2];
			}
			for (std::size_t Index = 0; Index < Corner.size(); ++Index)
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Shard.Corners[Index][Axis] = static_cast<int16>(Corner[Index][Axis] - Centre[Axis]);
				}
			}
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Shard.Place[Axis] = int32{Centre[Axis]} * 0x10000;
			}
			// Its speed away from the model's origin (the heading toward its centre), then its spin.
			const int32 Speed = Measures.Speed + Drawn(*Random, Measures.SpeedRange);
			const int32 Flat = SquareRoot0(GteSquare(Centre[0]) + GteSquare(Centre[2]));
			const FGteVector Angles{0, static_cast<int16>(GteRatan2(Centre[2], Centre[0], *Arctangents)), static_cast<int16>(GteRatan2(Centre[1], Flat, *Arctangents))};
			Shard.Velocity = ApplyMatrixLV(RotationXyz(Angles), {Speed, 0, 0});
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Shard.Spin[Axis] = SpinDrawn(*Random, Measures.SpinRange);
			}
			Shard.Fall = Measures.Fall;
			Primitive += (static_cast<uint32>(File.Byte(Primitive + 1)) + 1) * 4;
		}
		Set.bLinked = true;
		Sprites->LinkTask(Set);
		return true;
	}

	FBattleWave* FBattleExtensions::Wave(const IKernelTask* Task) const
	{
		return Find(Waves, Task);
	}

	FBattleScroll* FBattleExtensions::Scroll(const IKernelTask* Task) const
	{
		return Find(Scrolls, Task);
	}

	FBattleShards* FBattleExtensions::Shards(const IKernelTask* Task) const
	{
		return Find(ShardSets, Task);
	}

	FSprite* FBattleExtensions::Live(const FBattleExtensionSprite& Sprite) const
	{
		for (const FKernelTaskEntry& Entry : Sprites->Tasks())
		{
			if (Entry.Sprite.get() == Sprite.Task && Entry.Sprite->Id == Sprite.TaskId)
			{
				return Sprite.Sprite;
			}
		}
		// The sprite ended under the task: the original goes on through its pointer into freed memory.
		Sprites->Report(ESpriteFault::UnsupportedInstruction, 0xE0E);
		return nullptr;
	}

	bool FBattleExtensions::AnyRunning() const
	{
		const auto Running = [](const auto& Tasks) { return std::any_of(Tasks.begin(), Tasks.end(), [](const auto& Task) { return Task->bLinked; }); };
		return Running(Waves) || Running(Scrolls) || Running(ShardSets);
	}
}
