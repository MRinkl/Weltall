#pragma once

#include "Battle/Show/BattleTmdModel.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMatrix.h"
#include "Graphics/SpriteSystem.h"

#include <array>
#include <memory>
#include <vector>

namespace Weltall
{
	class FBattleExtensions;
	struct FBattleModelDrawing;
	class FPsxRandom;
	class FSpriteFile;
	struct FGteArctangents;

	/**
	 * The sprite an extension's task works on, by its task and the task's number: the original keeps a pointer, which a
	 * sprite that has ended leaves pointing at freed memory (the port stops there).
	 */
	struct FBattleExtensionSprite
	{
		FSprite* Sprite = nullptr;
		const FSpriteTask* Task = nullptr;
		uint32 TaskId = 0;
	};

	/**
	 * EXT2's wave over a sprite (system command 19h): a task under the sprite's, its phase moved by its speed each step;
	 * the amplitude and frequency, with their steps from strip to strip, shape its drawing.
	 */
	struct FBattleWave final : public IKernelTask
	{
		FBattleExtensionSprite Sprite;
		int32 Amplitude = 0;
		int32 AmplitudeStep = 0;
		int32 Frequency = 0;
		int32 FrequencyStep = 0;
		int32 Phase = 0;
		int32 Speed = 0;
		bool bLinked = false;

		void Update() override { Phase += Speed; }
		void OnKilled() override { bLinked = false; }
	};

	/**
	 * EXT4's and EXT5's scroll of a sprite's picture (system commands 24h, 25h): a task under the sprite's. EXT4 keeps the
	 * sprite where it stands and adds its velocity to the scroll; EXT5 takes its speed off the scroll and puts it back at the
	 * place it had as it began whenever it is drawn.
	 */
	struct FBattleScroll final : public IKernelTask
	{
		FBattleExtensions* Owner = nullptr;
		FBattleExtensionSprite Sprite;
		/** 4 or 5. */
		int32 Module = 0;
		std::array<int32, 3> Scroll{};
		/** EXT5's: the sprite's place as it began. */
		std::array<int32, 3> Held{};
		bool bLinked = false;

		void Update() override;
		void OnKilled() override { bLinked = false; }
	};

	/** A shard: one primitive of the model, turning and flying (EXT3's 54h-byte record). */
	struct FBattleShard
	{
		std::array<int16, 3> Rotation{};
		std::array<int16, 3> Spin{};
		/** 16.16. */
		std::array<int32, 3> Place{};
		std::array<int32, 3> Velocity{};
		int32 Fall = 0;
		/** Its corners about its centre (a triangle's fourth is its third). */
		std::array<FGteVector, 4> Corners{};
		/** Its polygon as the model's packets hold it: its shape, its texels, and each corner's colour and code in either draw buffer. */
		uint8 CornerCount = 3;
		bool bTextured = false;
		bool bGouraud = false;
		std::array<FMdlUv, 4> Uvs{};
		uint16 Clut = 0;
		uint16 TexturePage = 0;
		std::array<std::array<uint32, 4>, 2> ColorCodes{};
	};

	/** EXT3's shards (system command 1Dh): a task of no owner, the model's primitives flying apart until their steps run out. */
	struct FBattleShards final : public IKernelTask
	{
		FBattleExtensions* Owner = nullptr;
		/** The model the shards are cut from (its file and the object's place in it) and the matrix it was drawn with. */
		const FSpriteFile* ModelFile = nullptr;
		uint32 Model = 0;
		FGteMatrix Matrix;
		std::vector<FBattleShard> Shards;
		int32 StepsLeft = 0;
		/** Counts the sets started: the same for one set while it flies. */
		uint32 Serial = 0;
		bool bLinked = false;

		void Update() override;
	};

	/**
	 * The animation extension modules' effects the sprites' system commands start:
	 * EXT2's waves, EXT3's shards and EXT4's and EXT5's scrolls, each a task of the kernel's update list. Their drawing's logic
	 * (the sprite's entry and matrix, EXT5's hold of its place) is the show's, which draws them.
	 */
	class FBattleExtensions
	{
	public:
		/** The inline data's measures system command 1Dh works out, in the order it hands them over. */
		struct FShardMeasures
		{
			int32 Fall = 0;
			int32 Speed = 0;
			int32 SpeedRange = 0;
			int32 SpinRange = 0;
			int32 Steps = 0;
		};

		FSpriteSystem* Sprites = nullptr;
		FPsxRandom* Random = nullptr;
		const FGteArctangents* Arctangents = nullptr;

		/** A new battle: none running. */
		WELTALLCORE_API void Begin();

		/** The wave's six values as system command 19h works them out (phase, amplitude and its step, frequency and its step, speed). */
		WELTALLCORE_API void StartWave(FSprite& Sprite, const std::array<int32, 6>& Values);
		/**
		 * Shards: the sprite's attached model cut into its primitives, each drawn a speed and a spin (four
		 * rand() each), and its packets taken as Packets says they stand. False when the model reads past its file or holds
		 * a kind of primitive no model of the game has (a fault).
		 */
		WELTALLCORE_API bool StartShards(const FSprite& Sprite, const FGteMatrix& Matrix, const FShardMeasures& Measures, const FBattleModelDrawing& Packets);
		/** The scroll modules' start: the sprite's +ACh flag 20h set (its end ends its children). */
		WELTALLCORE_API void StartScroll(FSprite& Sprite, int32 Module);

		/** The running effect a task of the list is, none for another task. */
		WELTALLCORE_API FBattleWave* Wave(const IKernelTask* Task) const;
		WELTALLCORE_API FBattleScroll* Scroll(const IKernelTask* Task) const;
		WELTALLCORE_API FBattleShards* Shards(const IKernelTask* Task) const;
		/** Whether any of them runs (the module in memory must not change under them). */
		WELTALLCORE_API bool AnyRunning() const;
		/** The task's sprite while it lives; none once it has ended (a sprite fault is reported). */
		WELTALLCORE_API FSprite* Live(const FBattleExtensionSprite& Sprite) const;

	private:
		friend struct FBattleShards;

		std::vector<std::unique_ptr<FBattleWave>> Waves;
		std::vector<std::unique_ptr<FBattleScroll>> Scrolls;
		std::vector<std::unique_ptr<FBattleShards>> ShardSets;
		uint32 ShardSerial = 0;
		/** Scratch: the model a set is cut from. */
		FBattleTmdModel ShardModel;
	};
}
