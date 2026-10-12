#pragma once

#include "Field/EventContext.h"
#include "Field/FieldCamera.h"
#include "Foundation/Result.h"
#include "Graphics/GteMatrix.h"
#include "Graphics/SpriteDrawing.h"

#include <optional>

namespace Weltall
{
	class FSprite;
	class FSpriteFile;

	/** One of the passes the field draws a character's sprite in: its matrix, its sorting and which pieces it keeps. */
	struct FFieldSpritePass
	{
		/** The camera's screen rotation at the actor's scale, and the sprite's position through the view: what the kernel draws its parts with. */
		FGteMatrix Matrix;
		/** The ordering-table entry the parts are linked into (the scenery's, 16 depth units each); one outside the table is never drawn. */
		int32 OtIndex = 0;
		/** Every piece of the parts, or those above or below the wading line ClipLine (texels below the origin). */
		ESpriteClip Clip = ESpriteClip::None;
		int32 ClipLine = 0;
	};

	/**
	 * A spawned child's model as the kernel's drawing of its children draws it, each tick before they
	 * run: the part a script attached to the child's sprite (its offset in the script's file) with the
	 * texture page its packets were built on, the sprite's matrix at its place, and whether the fog's
	 * draw mode draws it.
	 */
	struct FFieldSpriteModelDraw
	{
		const FSpriteFile* File = nullptr;
		uint32 Part = 0;
		bool bTextureOverride = false;
		int16 TextureX = 0;
		int16 TextureY = 0;
		/** Model to world: SpriteModelMatrix's. */
		FGteMatrix Matrix;
		bool bFogged = false;
		/** The child's number: the same child from tick to tick. */
		uint32 Key = 0;
	};

	/** What the field's drawing of its characters draws, pass by pass, as it draws it. */
	class IFieldCharacterDrawing
	{
	public:
		virtual ~IFieldCharacterDrawing() = default;

		/** The actor's sprite drawn in a pass: its parts carry the pass's tint, and its hidden anchor groups the pass's. */
		virtual void DrawSprite(int32 Actor, const FSprite& Sprite, const FFieldSpritePass& Pass) = 0;

		/** A spawned child's model drawn, in the order the kernel draws its children. */
		virtual void DrawSpriteModel(const FFieldSpriteModelDraw& Draw) = 0;
	};

	/**
	 * What the field's drawing of its characters leaves for the next tick, as it runs once a tick after
	 * the actors and the camera have moved: the sprites' pending frames built, the spawned children's
	 * models drawn, and the children run; each sprite actor's record taking its position, its off-screen flag from where its sprite
	 * falls on the original's 320 x 224 screen (an actor off it neither moves nor animates; with the fix
	 * FEventOptions::bActorsRunInWideView, in the widest view the port shows through the player's view), and the
	 * tint and anchor groups of the pass that draws it; then the animations of the actors that animate.
	 * View is the view the camera built this tick. Drawing, when given, is told of every pass drawn.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> UpdateFieldCharacters(FEventContext& Context, const FFieldView& View,
		IFieldCharacterDrawing* Drawing = nullptr);
}
