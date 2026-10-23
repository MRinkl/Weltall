#pragma once

#include "Battle/Show/BattleShowCamera.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMatrix.h"
#include "Graphics/SpriteSystem.h"
#include "Graphics/Vram.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace Weltall
{
	class FBattleFadeTask;
	class FBattleSpriteFile;
	struct FBattleBoneEffect;
	struct FBattleSpriteSection;
	class FBattleFloor;
	class FBattleNumbers;
	class FBattleShakeTask;
	class FBattleTintFadeTask;
	class FPsxRandom;
	class FSpriteFile;
	struct FBattleContext;
	struct FGte;
	struct FGteArctangents;

	/**
	 * What Battle's sprite code reaches outside the sprites and the show's engines they drive: the 3D
	 * objects, the effects' extension modules, the sound driver, VRAM and the disc. The show answers
	 * them; each call is where the original makes it.
	 */
	class IBattleSpriteServices
	{
	public:
		virtual ~IBattleSpriteServices() = default;

		/** A read from the disc is still under way. */
		virtual bool FilesPending() = 0;
		/** Where slot Slot's 3D object stands (its skeleton's translation); false when the slot has none. */
		virtual bool ObjectRoot(int32 Slot, std::array<int32, 3>& Translation) = 0;
		/** Offset from bone Bone of slot Slot's object, its height the object's floor when bOnFloor; false when it has no such bone. */
		virtual bool BonePlace(int32 Slot, int32 Bone, const FGteVector& Offset, bool bOnFloor, std::array<int32, 3>& Place) = 0;
		/** A 3D target reacts: slot Slot's object runs sequence Sequence, hit by the slots of FromSlots. */
		virtual void ReactModel(int32 Slot, uint16 FromSlots, int32 Sequence) = 0;
		/** The 3D objects shown or hidden. */
		virtual void SetObjectsVisible(bool bVisible) = 0;
		/** Special model Model of directory 40 read (46), made in slot 11 and run as sequence Sequence on Slots (47), freed (51). */
		virtual void LoadSpecialModel(int32 Model) = 0;
		virtual void RunSpecialModel(int32 Sequence, uint16 Slots) = 0;
		virtual void FreeSpecialModel() = 0;
		/** A model sprite's packets were tinted again: the polygons the frame begun drew from them take the colour. */
		virtual void RetintModel(const FSprite& Sprite) = 0;
		/**
		 * An effect of the extension module in memory, started by system command Command for the sprite:
		 * 19's wave with the six values it works out from its inline data, 1D's shards with the five
		 * measures (size, lift, spread, spin and fall), 24 and 25 with their operand, 54 alone.
		 */
		virtual void StartExtension(int32 Command, FSprite& Sprite, const FSpriteOperands& Operand, const std::array<int32, 6>& Arguments) = 0;
		/** VRAM: a copy (MoveImage), an inline image list uploaded, the VRAM effects overwrote put back. */
		virtual void MoveImage(const FVramRect& Rect, int32 X, int32 Y) = 0;
		virtual void UploadImages(const FSpriteFile& File, uint32 At) = 0;
		virtual void RestoreVram() = 0;
		/**
		 * Sounds, by the driver's number: a set's id in the high half, the effect in the low. Played
		 * plain, played as the system's, stopped, played on a voice.
		 */
		virtual void PlaySound(uint32 Sound) = 0;
		virtual void PlaySystemSound(uint32 Sound) = 0;
		virtual void StopSound(uint32 Sound) = 0;
		virtual void PlaySoundOn(uint32 Sound, int32 Voice) = 0;
		/** The music's volume moves to Volume over Steps. */
		virtual void SetMusicVolume(int32 Volume, int32 Steps) = 0;
		/** A sprite file's sound effects registered with the driver. */
		virtual void RegisterSoundEffects(const FBattleSpriteFile& File, const FBattleSpriteSection& Section) = 0;
		/** A sprite file's sound bank loaded as the battle's, the bank it loaded last unloaded first. */
		virtual void LoadSoundBank(const FBattleSpriteFile& File, const FBattleSpriteSection& Section) = 0;
	};

	/**
	 * What the action being shown shares with the sprites: its actor and targets, the record played,
	 * and the flags the scripts and the playback hand each other. The playback fills it; the scripts
	 * read and set it.
	 */
	struct FBattlePlayState
	{
		/** The sprite performing the action (in the event's variables, +3B4h), none between actions. */
		FSprite* Actor = nullptr;
		/** The playback controller's performer (its +4h): the performer slot's sprite as it was set, none as it is made. */
		FSprite* Performer = nullptr;
		/** The record played, counted from 1; branch F8 reads the one before it. */
		int32 Record = 0;
		/** The targets' sprites in turn, none after the last, and how many. */
		std::array<FSprite*, 12> Targets{};
		int32 TargetCount = 0;
		/** The targets' slots. */
		uint16 TargetMask = 0;
		/** The effect's cast ended (8F). */
		bool bCastEnded = false;
		/** The animation within its file of the effect a cast record names (bits 9-14 of its parameter). */
		int32 EffectAnimation = 0;
		/** The sprite that asked for the effect that follows the cast (system command 2F). */
		FSprite* Caster = nullptr;
		/** The effect images are to be read again after the effect (37). */
		bool bReloadEffectImages = false;
		/** The actor's hit voice may play, and the slots whose hit voice played this action. */
		bool bHitVoices = false;
		uint16 HitVoiced = 0;
		/** The music lowered for the effect; system command 6A takes it back. */
		bool bMusicLowered = false;
		/** The HUD hidden. */
		bool bHudHidden = false;
		/** Set by system commands 5F (1) and 60 (the sprite's slot + 2); no code of the game reads it. */
		uint8 ScriptMark = 0;
		/** The battle's own sound bank is in sound memory; an effect's bank takes its place. */
		bool bSystemBankLoaded = false;
	};

	/** How a combatant's sprite is made: the arguments it is made with, and the setup's facing. */
	struct FBattleCombatantLook
	{
		int32 ClutX = 0;
		int32 ClutY = 0;
		int32 TextureX = 0;
		int32 TextureY = 0;
		int32 PaletteSet = 0;
		int32 Animation = 0;
		/** Its default target stands to its left: it faces left. */
		bool bFacesLeft = false;
	};

	/** The routine a sprite task runs as its update or its end. */
	enum class EBattleSpriteRoutine : uint8
	{
		/** The kernel's. */
		Kernel,
		/** The kernel's, counting its updates (kind 7). */
		Counted,
		Combatant,
		Proxy,
		Controller,
		Orbit,
		/** An effect's sprite that follows a 3D object's bone, then runs the update it had. */
		FollowBone,
	};

	/** The drawings Battle's scripts give a sprite task in place of its kind's (FSpriteTask::DrawRole). */
	namespace BattleSpriteDraws
	{
		/** A line from where it was to where it is (system command 05). */
		inline constexpr uint8 Streak = 1;
		/** A line to its parent (1E). */
		inline constexpr uint8 LineToParent = 2;
		/** The kernel's own drawing routine (2C). */
		inline constexpr uint8 Kernel = 3;
	}

	/**
	 * Battle's sprites: the kernel's sprite system in battle
	 * mode, Battle's interpreter taking every script step (its own instructions and its system
	 * commands, the kernel's for the rest), the combatants' sprites and the 3D combatants' proxies, the
	 * camera controllers, and the tasks the scripts start (watches, glues, trails, orbits, shatters).
	 */
	class FBattleSprites final : public ISpriteBattleMode, public IBattleCameraControllers
	{
	public:
		static constexpr int32 SlotCount = 11;
		/** The pools of the scripts' own tasks as a battle starts, so that few steps allocate; a pool grows when its tasks all run. */
		static constexpr int32 WatchCapacity = 32;
		static constexpr int32 GlueCapacity = 16;
		static constexpr int32 TrailCapacity = 8;
		/** The anchor slots a trail runs through. */
		static constexpr std::array<uint8, 5> TrailSlots{1, 2, 3, 5, 6};

		WELTALLCORE_API FBattleSprites();
		WELTALLCORE_API ~FBattleSprites() override;
		FBattleSprites(const FBattleSprites&) = delete;
		FBattleSprites& operator=(const FBattleSprites&) = delete;

		/** What it works with, which the show sets before Begin. */
		FSpriteSystem* Sprites = nullptr;
		FPsxRandom* Random = nullptr;
		const FGteArctangents* Arctangents = nullptr;
		/** The GTE's state the show keeps: its offset is the screen's centre, which effects move. */
		FGte* Gte = nullptr;
		FBattleContext* Context = nullptr;
		FBattleCamera* Camera = nullptr;
		FBattleFloor* Floor = nullptr;
		FBattleFadeTask* Fade = nullptr;
		FBattleShakeTask* Shake = nullptr;
		FBattleTintFadeTask* TintFade = nullptr;
		FBattleNumbers* Numbers = nullptr;
		IBattleSpriteServices* Services = nullptr;

		FBattlePlayState Play;
		/** The resource blocks kind 5 (the common effects, 44/2) and kind 6 (the effect's own) draw from. */
		FSpriteResource CommonEffects;
		FSpriteResource EffectResource;
		/** The battle's own sound set's id, which B0 and the hit voices play from; -1 none. */
		int32 SystemSoundSet = -1;
		/** The files the scripts may write their variables into: those the show keeps in memory. */
		std::vector<FSpriteFile*> WritableFiles;
		/** Each slot's sprite: a combatant's own or a 3D combatant's proxy. */
		std::array<FSprite*, SlotCount> SlotSprites{};
		/** The combatants' sprites stand still and are not drawn (system commands 11 and 13). */
		bool bCombatantsHidden = false;
		/** The stage hidden and the screen cleared to black (12, 14): the clear (on, red, green, blue) and the one kept aside. */
		bool bStageHidden = false;
		std::array<uint8, 4> Clear{};
		std::array<uint8, 4> SavedClear{};
		/** The draw buffer this frame fills, whose colours a shatter darkens. */
		int32 DrawBuffer = 0;
		/** The camera controllers: the task of each and how many run. */
		FSpriteTask* EyeController = nullptr;
		FSpriteTask* TargetController = nullptr;
		uint8 ControllerCount = 0;
		/** The desired eye and look-at the controllers took over, given back when they end. */
		FGteVector SavedDesiredEye{};
		FGteVector SavedDesiredTarget{};
		/** The counted wait's counter and the special model's steps, which the objects count up. */
		int32 CountedWaits = 0;
		int32 SpecialModelSteps = 0;
		/** Counts the models' packets built, which numbers each build. */
		uint32 ModelBuilds = 0;

		/** Makes the sprite system run in battle mode with this interpreter, every task ended. */
		WELTALLCORE_API void Begin();
		/** Out of battle mode: every task ended. */
		WELTALLCORE_API void End();

		/**
		 * A combatant's sprite, placed as the setup places it: slot Slot's combatant from File, at
		 * its slot's place facing its side, still when its slot is hidden.
		 */
		WELTALLCORE_API FSprite& CreateCombatant(int32 Slot, const FSpriteFile& File, const FBattleCombatantLook& Look);
		/** The sprite a 3D combatant's slot keeps for the scripts, Height tall, at its slot's place (a slot with a sprite keeps it). */
		WELTALLCORE_API FSprite& CreateProxy(int32 Slot, int32 Height);
		/** Every slot's number over the record played: a number for each combatant it has an amount for (op 8B). */
		WELTALLCORE_API void ShowNumbers();
		/** A slot's sprite as its task, none for an empty slot. */
		WELTALLCORE_API FSpriteTask* SlotTask(int32 Slot) const;
		/** Slot Slot's sprite ended with every child under it (none: nothing). */
		WELTALLCORE_API void RemoveCombatant(int32 Slot);
		/**
		 * Slot Slot's proxy freed as a gear drop frees it: the slot forgets it, while the play's pointers to it (the actor,
		 * the targets) and its children's still read it as it was (none: nothing).
		 */
		WELTALLCORE_API void FreeProxy(int32 Slot);
		/**
		 * A battle event's object: a sprite of its own data run as the combatants' sprites
		 * are and left out of the tasks the playback counts, at Place.
		 */
		WELTALLCORE_API FSpriteTask& CreateEventObject(const FSpriteFile& File, const std::array<int16, 3>& Place);
		/** A battle event's object ended: its task's children, then its task, whatever frame it had waiting to be built. */
		WELTALLCORE_API void DestroyEventObject(FSpriteTask& Task);
		/**
		 * The watch on a battle event's walk: a task under the sprite that, once its distance to its goal
		 * grows or falls under Minimum, gives it the animation after this one (its stance when it names none) and ends.
		 */
		WELTALLCORE_API void StartWalkWatch(FSprite& Sprite, int32 Minimum);
		/** The slot a sprite belongs to (its flags' slot bits). */
		WELTALLCORE_API static int32 SlotOf(const FSprite& Sprite);
		/** What the camera's framing sees of each slot. */
		WELTALLCORE_API std::array<FBattleCameraSubject, SlotCount> CameraSubjects() const;
		/** The routines a sprite task runs as its update and as its end. */
		WELTALLCORE_API EBattleSpriteRoutine UpdateRoutine(const FSpriteTask& Task) const;
		WELTALLCORE_API EBattleSpriteRoutine EndRoutine(const FSpriteTask& Task) const;

		// ISpriteBattleMode
		void Step(FSprite& Sprite) override;
		void SnapToFloor(FSprite& Sprite) override;
		FSpriteResource& KindResource(int32 Kind) override;
		void SetUpController(FSpriteTask& Task) override;
		FSprite* Actor() override { return Play.Actor; }
		FSpriteBattlePoint Point(uint32 Mode, const FSprite& Sprite) override;
		const FGteMatrix& View() override;
		void PlaySound(const FSprite& Sprite, uint8 Effect, bool bSystem) override;
		FSpriteFile* WritableFile(const FSpriteFile& File) override;
		void ModelTinted(const FSprite& Sprite) override;

		// IBattleCameraControllers
		void EndControllers() override;

		/**
		 * The file's further sections put to use (a sound bank loaded, sound effects
		 * registered, a sprite resource taken as the effect's own resource); the id of the last sound
		 * effect set, -1 for none.
		 */
		WELTALLCORE_API int32 Install(const FBattleSpriteFile& File);

		/**
		 * An effect at a bone (a 3D object's key event 1): a sprite of the common effects or of the effect's resource at the
		 * event's point, turned and scaled, following its object's bone when the event asks. False after a sprite fault.
		 */
		WELTALLCORE_API bool SpawnBoneEffect(const FBattleBoneEffect& Effect);
		/** Two objects swapped slots: the sprites that follow either one's bones follow it to its new slot, as the original keeps the object itself. */
		void FollowSwappedObjects(int32 First, int32 Second);

		/** A jump's vertical speed and heading to land on the goal (+A0h). */
		WELTALLCORE_API void JumpArc(FSprite& Sprite);
		/** A throw's vertical speed and heading toward the goal's floor. */
		WELTALLCORE_API void ThrowArc(FSprite& Sprite);
		/** The heading from To toward From (x, z), 4096 a turn. */
		WELTALLCORE_API int32 HeadingBetween(const FSprite& From, const FSprite& To) const;

		/** A script's task, for the tests' view of the list: a watch, a glue, a trail, a shatter or an event's walk watch, and its sprite. */
		struct FScriptTaskView
		{
			uint8 Kind = 0;
			const FSprite* Sprite = nullptr;
		};
		/** What an update list entry that is not a sprite is, when it is one of the scripts' tasks. */
		WELTALLCORE_API FScriptTaskView ScriptTask(const IKernelTask* Task) const;
		/** A script's task's state: a watch's, a glue's or a trail's fields. */
		struct FScriptTaskState
		{
			int8 Animation = 0;
			/** A watch: the distance last measured, the least, and where the script jumps. */
			int32 Distance = 0;
			int32 Minimum = 0;
			uint32 Jump = 0;
			/** A glue: the anchor slots that meet. */
			int32 OwnSlot = 0;
			int32 TargetSlot = 0;
			/** A trail: the frame last read, the blend, the width, the points read and drawn. */
			uint16 Frame = 0;
			uint8 Blend = 0;
			int32 Width = 0;
			std::array<FGteVector, 5> Read{};
			std::array<FGteVector, 5> Drawn{};
		};
		WELTALLCORE_API FScriptTaskState ScriptTaskState(const IKernelTask* Task) const;

		/**
		 * A script's trail as its drawing takes it: its own sprite and points, and what the last
		 * trail to step left for every trail drawn until the next one steps (the game keeps them in words all trails
		 * share): that trail's sprite's ordering-table entry, its width and blend, and the two bytes of its script's
		 * variable that put each half of the spans behind the sprite (not 0) or before it.
		 */
		struct FTrailDrawing
		{
			FSprite* Sprite = nullptr;
			std::array<FGteVector, 5> Points{};
			int16 Entry = 0;
			int16 Width = 0;
			uint8 Blend = 0;
			std::array<uint8, 2> Behind{};
		};
		/** The drawing of a task that is a script's trail, none for another task. */
		WELTALLCORE_API std::optional<FTrailDrawing> TrailDrawing(const IKernelTask* Task);

	private:
		class FCombatantRole;
		class FProxyRole;
		class FControllerRole;
		class FOrbitRole;
		class FBoneFollowRole;
		struct FWatchTask;
		struct FGlueTask;
		struct FTrailTask;
		struct FShatterTask;
		struct FWalkWatchTask;
		template <typename TTask>
		TTask& FreeTask(std::vector<std::unique_ptr<TTask>>& Pool);

		/** The interpreter's loop, with the stale register a BC mode divides by (0 as the kernel's ticking enters). */
		void RunStep(FSprite& Sprite, uint32 StaleRegister);
		void SystemCommand(FSprite& Sprite, uint8 Command, const FSpriteOperands& Operand);
		void AttachModel(FSprite& Sprite, uint32 At, int32 Offset);
		bool ScaleModelVertices(const FSpriteFile& File, uint32 Model);
		void HomeOnGoal(FSprite& Sprite, uint8 Turn);
		void StartWatch(FSprite& Sprite, int32 Minimum, uint32 Jump);
		void StartGlue(FSprite& Sprite, uint8 Slots);
		void StartTrail(FSprite& Sprite, const FSpriteOperands& Count);
		void StartShatter();
		void StartOrbit(FSprite& Sprite);
		void EndGlues();
		void EndTrail(FSprite& Sprite);
		/**
		 * An anchor slot's place in the view's plane, built in Point: the slot's offset, or
		 * Point as it was for a sprite without anchors, plus the sprite's place. False for a slot past the anchors.
		 */
		bool AnchorPoint(const FSprite& Sprite, int32 Slot, std::array<int16, 2>& Point);
		/** The floor's height at the goal (for the arcs), the goal's own when it lies higher. */
		int16 GoalFloor(FSprite& Sprite);
		/** The target's index in the action's targets, the count when it is none of them. */
		int32 TargetIndex(const FSprite* Target) const;
		void NextTarget(FSprite& Sprite);
		void FrameSlots(uint32 Slots);
		void ReportFault(uint32 Value);
		/** A slot's sprite ended: the slot and the play forget it. */
		void Forget(const FSpriteTask& Task);
		/** The sprite put at its object's bone, then the update it had. */
		void FollowBone(FSpriteTask& Task);

		/** A sprite that follows a bone: its task (and the task's number), the object's slot, the bone, the offset, the update it had. */
		struct FBoneFollow
		{
			const FSpriteTask* Task = nullptr;
			uint32 TaskId = 0;
			int32 Slot = 0;
			int32 Bone = 0;
			FGteVector Offset{};
			bool bOnFloor = false;
			ISpriteTaskRole* Update = nullptr;
			/** Where the bone last put it: once its object is freed, the original goes on reading the bone where the heap left it. */
			std::optional<std::array<int32, 3>> LastPlace;
		};
		std::vector<FBoneFollow> BoneFollows;

		/** The interpreter's s0 as a BC mode reads it: 0 as the ticking enters, the last value it held after. */
		uint32 StepRegister = 0;
		std::unique_ptr<FCombatantRole> CombatantRole;
		std::unique_ptr<FProxyRole> ProxyRole;
		std::unique_ptr<FControllerRole> ControllerRole;
		std::unique_ptr<FOrbitRole> OrbitRole;
		std::unique_ptr<ISpriteTaskRole> BoneFollowRole;
		std::vector<std::unique_ptr<FWatchTask>> Watches;
		std::vector<std::unique_ptr<FGlueTask>> Glues;
		std::vector<std::unique_ptr<FTrailTask>> Trails;
		/** What the last trail's step left for the trails' drawing; the stepping trail, whose variable is read as a trail is drawn. */
		struct FTrailWords
		{
			int16 Entry = 0;
			int16 Width = 0;
			uint8 Blend = 0;
			const FTrailTask* Stepped = nullptr;
			std::array<uint8, 2> Behind{};
		};
		FTrailWords TrailWords;
		std::vector<std::unique_ptr<FShatterTask>> Shatters;
		std::vector<std::unique_ptr<FWalkWatchTask>> WalkWatches;
		std::array<FSpriteTask*, SlotCount> SlotTasks{};
	};
}
