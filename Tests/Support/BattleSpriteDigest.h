#pragma once

#include "Foundation/Types.h"
#include "Support/OracleDigest.h"

#include <functional>
#include <utility>

namespace Weltall
{
	class FBattleExtensions;
	class IKernelTask;
	class FBattleFadeTask;
	class FBattleMarkers;
	class FBattleNumbers;
	class FBattleShakeTask;
	class FBattleSprites;
	class FBattleTintFadeTask;
	class FSprite;
	class FSpriteFile;
	class FSpriteSystem;
	struct FSpriteResource;
	struct FSpriteTask;
}

namespace Weltall::Testing
{
	/** What the battle sprite oracles digest by (file, offset): a file of the case by its number, FEh none, FFh another. */
	inline constexpr uint32 NoFile = 0xFE;
	inline constexpr uint32 Elsewhere = 0xFF;
	/** A file the original has freed while a sprite still points into it. */
	inline constexpr uint32 FreedFile = 0xFD;
	/** A sprite's place in the update list when it has none: no sprite, or one that ended; a task out of the list. */
	inline constexpr int32 NoSprite = 0xFFFE;
	inline constexpr int32 Unlisted = 0xFFFF;

	/**
	 * A battle sprite as the sprites' oracle digests it: its motion,
	 * look, files and script places as (file, offset), its resource, its parent and target by their
	 * places in the update list, its sound set, floor, stack, words, transform, anchors or model and parts.
	 */
	class FBattleSpriteDigester
	{
	public:
		const FSpriteSystem* Sprites = nullptr;
		const FBattleSprites* Battle = nullptr;
		/** The target markers and the actor's glow, when the case's show has them; the animation extensions' tasks likewise. */
		const FBattleMarkers* Markers = nullptr;
		const FBattleExtensions* Extensions = nullptr;
		/** A task the case has besides the show's own (digest_other_task), digested: whether it was one. */
		std::function<bool(FOracleDigest&, const IKernelTask*)> OtherTask;
		/** The (file, offset) of an offset in a file the case knows; NoFile for none. */
		std::function<std::pair<uint32, uint32>(const FSpriteFile*, uint32)> Place;

		int32 IndexOf(const FSprite* Sprite) const;
		int32 IndexOfTask(const FSpriteTask* Task) const;
		void DigestPlace(FOracleDigest& Out, const FSpriteFile* File, uint32 Offset) const;
		void DigestResource(FOracleDigest& Out, const FSpriteResource& Resource) const;
		void DigestSprite(FOracleDigest& Out, const FSprite& Sprite) const;
		/**
		 * The update list whole (the oracle's digest_tasks): each sprite with its task's routines, drawing, owner and
		 * marks, the scripts' watches, glues and trails, the screen's fade, shake and tint fade, the numbers, their
		 * bursts and the total's panel, the target markers and the actor's glow; EFh for a task none of these is
		 * (the oracle fails there).
		 */
		void DigestTasks(FOracleDigest& Out, const FBattleFadeTask& Fade, const FBattleShakeTask& Shake, const FBattleTintFadeTask& TintFade,
			const FBattleNumbers& Numbers) const;

	private:
		/** An animation extension's task (digest_extension_task): false for another task. */
		bool DigestExtensionTask(FOracleDigest& Out, const IKernelTask* Task) const;
	};
}
