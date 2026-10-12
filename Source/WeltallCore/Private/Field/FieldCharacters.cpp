#include "Field/FieldCharacters.h"
#include "FieldDirections.h"

#include "Field/FieldWalkmesh.h"
#include "Graphics/Gte.h"
#include "Graphics/GteVectors.h"
#include "Graphics/SpriteSystem.h"

#include <string>

namespace Weltall
{
	namespace
	{
		/** The wading passes: the pieces above the state's line with the first tint, those below it with the second. */
		constexpr uint32 WadingAbove = 0x20;
		constexpr uint32 WadingBelow = 0x40;
		/** Animations -34 and -33 draw the sprite twice: anchor group 4 alone, then group 3 alone. */
		constexpr int32 FirstTwoPassAnimation = -34;
		constexpr uint32 OnlyGroup4 = 0xEF;
		constexpr uint32 OnlyGroup3 = 0xF7;
		/** The first of the two passes sorts this many entries nearer; the second by a point this far below the sprite's origin. */
		constexpr int32 FrontPassEntries = 16;
		constexpr int16 BackPassPoint = 300;
		constexpr int16 LadderSortHeight = 0x80;
		/** The character drawn larger while the party is in gears. */
		constexpr uint16 ChuChu = 7;

		FGte CharacterGte(const FEventContext& Context)
		{
			FGte Gte;
			Gte.OffsetX = FieldScreenCenterX << 16;
			Gte.OffsetY = FieldScreenCenterY << 16;
			Gte.ProjectionDistance = static_cast<uint16>(Context.Camera.ProjectionDistance);
			return Gte;
		}

		/**
		 * The direction of the view shown in eighths of a turn, as the field numbers them for the ladders: the camera's,
		 * turned as the player turned the view. The original's view only ever stood at the camera's eighths; the ladders'
		 * rule judges the view as seen, so a climber seen head-on is not sorted as one seen from the side.
		 */
		uint32 ShownDirection(const FEventContext& Context)
		{
			return static_cast<uint32>(FieldDirections::ShownCameraEighth(Context.Camera, Context.ViewTurn));
		}

		bool IsOnScreen(const FGteProjection& Point)
		{
			return static_cast<uint32>(Point.ScreenY + 9) < 0x143 && static_cast<uint32>(Point.ScreenX + 39) < 399;
		}

		/** How much wider than the 4:3 screen's each way the widest view the port shows (32:9) is, in screen units. */
		constexpr int32 WideViewExtra = 160 * 32 * 3 / (9 * 4) - 160;

		/** The same test across the widest view, with the point projected through View. */
		bool IsInWideView(FGte Gte, const FGteMatrix& View, const FGteMatrix& Placement, int16 SortHeight)
		{
			Gte.Transform = CompMatrix(View, Placement);
			const FGteProjection Point = Gte.Rtps({0, SortHeight, 0});
			return static_cast<uint32>(Point.ScreenY + 9) < 0x143 && static_cast<uint32>(Point.ScreenX + 39 + WideViewExtra) < 399 + 2 * WideViewExtra;
		}

		/** The ordering-table entry of a projected point: its depth / 4, as the GTE gives it, then / 4 again. */
		int32 OtIndexOf(const FGteProjection& Point)
		{
			return Point.ScreenZ >> 2 >> 2;
		}

		/** Sprites sort two entries nearer than their point. */
		int32 NearerOtIndex(const FGteProjection& Point)
		{
			const int32 Index = OtIndexOf(Point);
			return Index > 1 ? Index - 2 : Index;
		}

		/** The matrix the kernel draws an actor's sprite with: the camera's screen rotation at three quarters of the actor's scale. */
		FGteMatrix ActorSpriteMatrix(const FEventContext& Context, const FSprite& Sprite, const FEventActorState& State, const FFieldView& View)
		{
			std::array<int32, 3> Scale{};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Scale[Axis] = State.Scale[Axis] * 3 >> 2;
				if (State.Character == ChuChu && Context.PilotSpriteBase != 0)
				{
					Scale[Axis] = Scale[Axis] * 5 >> 2;
				}
			}
			FGteMatrix Rotation = ScaleColumns(View.ScreenRotation, Scale);
			Rotation.T = {};
			return SpriteDrawMatrix(Sprite, Rotation, View.View);
		}

		/** An actor's sprite as the field draws it: the tints and anchor groups each pass leaves on it, and the passes. */
		class FActorDraw
		{
		public:
			FActorDraw(FEventContext& InContext, FSpriteSystem& InKernel, FSprite& InSprite, const FEventActorState& InState, int32 InActor,
				IFieldCharacterDrawing* InDrawing)
				: Context(InContext), Kernel(InKernel), Sprite(InSprite), State(InState), Actor(InActor), Drawing(InDrawing)
			{
			}

			/** Gte holds the actor's matrix under the view, Point its sorting point's projection. */
			void Draw(const FGte& Gte, const FGteProjection& Point, const FFieldView& View)
			{
				const FFieldFog& Fog = Context.Render.Fog;
				if (Fog.bOn)
				{
					const FGteColor Faded = Gte.DepthCueColor({Fog.Color[0], Fog.Color[1], Fog.Color[2], 0}, Point.DepthCue);
					Kernel.SetTint(Sprite, Faded.R, Faded.G, Faded.B);
				}
				FFieldSpritePass Pass;
				if (Drawing != nullptr)
				{
					Pass.Matrix = ActorSpriteMatrix(Context, Sprite, State, View);
				}
				Pass.OtIndex = NearerOtIndex(Point);
				const bool bSpriteShown = (State.Flags1 & EventActorFlags1::SpriteHidden) == 0;
				if (static_cast<uint16>(static_cast<int16>(State.Animation) - FirstTwoPassAnimation) < 2)
				{
					if (bSpriteShown)
					{
						DrawTwoPasses(Gte, Pass);
					}
					return;
				}
				SetShownGroups(0);
				if (!bSpriteShown)
				{
					return;
				}
				const uint32 Wading = State.SpriteDraw & (WadingAbove | WadingBelow);
				if (Wading == 0)
				{
					ApplyTint(0);
					Notify(Pass);
					return;
				}
				if ((Wading & WadingAbove) != 0)
				{
					ApplyTint(0);
					FFieldSpritePass Above = Pass;
					// The pieces above the line sort by a point on it, as the whole sprite sorts by a point above its feet.
					const int16 LinePoint = static_cast<int16>((State.DrawSize - Context.Camera.Pitch / 3) * 2);
					Above.OtIndex = NearerOtIndex(Gte.Rtps({0, LinePoint, 0}));
					Above.Clip = ESpriteClip::KeepAbove;
					Above.ClipLine = State.DrawSize;
					Notify(Above);
				}
				if ((Wading & WadingBelow) != 0)
				{
					ApplyTint(1);
					FFieldSpritePass Below = Pass;
					Below.Clip = ESpriteClip::KeepBelow;
					Below.ClipLine = State.DrawSize;
					Notify(Below);
				}
			}

		private:
			void DrawTwoPasses(const FGte& Gte, FFieldSpritePass Pass)
			{
				SetTint(0);
				SetShownGroups(OnlyGroup4);
				Pass.OtIndex -= FrontPassEntries;
				Notify(Pass);
				if (Drawing != nullptr)
				{
					// The second pass sorts by a point in the space the first pass's drawing left the GTE in.
					FGte Left = Gte;
					Left.Transform = DrawSpriteParts(Sprite, Pass.Matrix, Pass.OtIndex, nullptr);
					Pass.OtIndex = OtIndexOf(Left.Rtps({0, BackPassPoint, 0}));
				}
				SetTint(1);
				SetShownGroups(OnlyGroup3);
				Notify(Pass);
			}

			void Notify(const FFieldSpritePass& Pass)
			{
				if (Drawing != nullptr)
				{
					Drawing->DrawSprite(Actor, Sprite, Pass);
				}
			}

			void SetTint(std::size_t Tint)
			{
				Kernel.SetTint(Sprite, State.Tints[3 * Tint], State.Tints[3 * Tint + 1], State.Tints[3 * Tint + 2]);
			}

			/** An actor's own tint, which fog's replaces. */
			void ApplyTint(std::size_t Tint)
			{
				if (!Context.Render.Fog.bOn)
				{
					SetTint(Tint);
				}
			}

			void SetShownGroups(uint32 HiddenGroups)
			{
				Sprite.DrawFlags = (Sprite.DrawFlags & ~(0xFFu << SpriteDrawFlags::HiddenGroupsShift)) | HiddenGroups << SpriteDrawFlags::HiddenGroupsShift;
			}

			FEventContext& Context;
			FSpriteSystem& Kernel;
			FSprite& Sprite;
			const FEventActorState& State;
			int32 Actor;
			IFieldCharacterDrawing* Drawing;
		};

		FError Unsupported(int32 Actor, const char* What)
		{
			return MakeError("field actor " + std::to_string(Actor) + ": " + What);
		}

		/**
		 * The kernel's drawing of the spawned children, newest first: a child of the kinds with a
		 * transform draws the model its script attached, if any; a child of parts would draw them, which
		 * no field sprite spawns; the other kinds draw nothing.
		 */
		std::optional<FError> DrawSpriteModels(const FSpriteSystem& Kernel, IFieldCharacterDrawing* Drawing)
		{
			for (const FKernelTaskEntry& Entry : Kernel.Tasks())
			{
				// Only the sprites' own children are drawn here; a mode's tasks draw themselves.
				const FSpriteTask* Task = Entry.Sprite.get();
				if (Task == nullptr || !Task->bDrawn)
				{
					continue;
				}
				const FSprite& Sprite = Task->Sprite;
				if (Sprite.Layout() == SpriteDrawFlags::LayoutParts)
				{
					return MakeError("a sprite's spawned child of kind " + std::to_string(Sprite.Kind()) + " is drawn, which the field's sprites never ask for");
				}
				if (Sprite.Layout() != SpriteDrawFlags::LayoutTransform || Sprite.ModelFile == nullptr || Drawing == nullptr)
				{
					continue;
				}
				FFieldSpriteModelDraw Draw;
				Draw.File = Sprite.ModelFile;
				Draw.Part = Sprite.Model;
				Draw.bTextureOverride = Sprite.bModelTextureOverride;
				Draw.TextureX = Sprite.ModelTextureX;
				Draw.TextureY = Sprite.ModelTextureY;
				Draw.Matrix = SpriteModelMatrix(Sprite);
				Draw.bFogged = (Sprite.BuildFlags & SpriteBuildFlags::Fog) != 0;
				Draw.Key = Task->Id;
				Drawing->DrawSpriteModel(Draw);
			}
			return std::nullopt;
		}
	}

	std::optional<FError> UpdateFieldCharacters(FEventContext& Context, const FFieldView& View, IFieldCharacterDrawing* Drawing)
	{
		FSpriteSystem* Kernel = Context.Models != nullptr ? Context.Models->SpriteSystem() : nullptr;
		if (Kernel == nullptr)
		{
			return MakeError("the field's characters have no sprite system");
		}
		Kernel->BuildPendingFrames();
		if (auto Error = DrawSpriteModels(*Kernel, Drawing))
		{
			return Error;
		}
		Kernel->RunTasks();

		FGte Gte = CharacterGte(Context);
		if (Context.Render.Fog.bOn)
		{
			const FFieldFog& Fog = Context.Render.Fog;
			Gte.FarColor = {Fog.FarColor[0] << 4, Fog.FarColor[1] << 4, Fog.FarColor[2] << 4};
			if (!Gte.SetFogNearFar(Fog.Near, Fog.Far, Context.Camera.ProjectionDistance))
			{
				return MakeError("the field's fog divides by a projection distance of 0");
			}
		}
		// A point two thirds of the camera's pitch above the feet, as the original divides.
		const int16 SortHeight = static_cast<int16>(Context.Camera.Pitch / 3 * -2);
		const int32 ActorCount = static_cast<int32>(Context.States.size());
		int32 GearsMet = 0;
		for (int32 Actor = 0; Actor < ActorCount; ++Actor)
		{
			FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Actor)];
			if ((Record.Flags & FieldActorFlag::NoModel) == 0)
			{
				continue;
			}
			FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
			Record.Position = Record.Translation;
			if ((State.Flags1 & EventActorFlags1::Gear) != 0)
			{
				if (auto Error = PlaceFieldGear(Context, Actor, GearsMet++))
				{
					return Error;
				}
				continue;
			}
			FSprite* Sprite = Context.Models->ActorSprite(Actor);
			if (Sprite == nullptr)
			{
				return Unsupported(Actor, "a sprite actor without a sprite is drawn");
			}
			FGteMatrix Placement;
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				Placement.M[Row] = Record.Matrix[Row];
			}
			Placement.T = Record.Translation;
			Gte.Transform = CompMatrix(View.View, Placement);
			FGteProjection Point = Gte.Rtps({0, SortHeight, 0});
			const bool bOnScreen = IsOnScreen(Point)
				|| (Context.Options.bActorsRunInWideView && IsInWideView(Gte, Context.ShownView ? *Context.ShownView : View.View, Placement, SortHeight));
			State.Flags1 = bOnScreen ? State.Flags1 & ~EventActorFlags1::OffScreen : State.Flags1 | EventActorFlags1::OffScreen;
			if ((Record.Flags & FieldActorFlag::Hidden) != 0 || (Point.Flag & GteFlag::Error) != 0)
			{
				continue;
			}
			if ((State.FloorAttributes & FieldFloorAttribute::Ladder) != 0)
			{
				const uint32 Across = (ShownDirection(Context) - (((State.FloorAttributes >> 11) - 2) & 7)) & 7;
				if (Across != 0 && Across != 4)
				{
					Point = Gte.Rtps({0, static_cast<int16>(Across < 4 ? -LadderSortHeight : LadderSortHeight), 0});
				}
			}
			FActorDraw(Context, *Kernel, *Sprite, State, Actor, Drawing).Draw(Gte, Point, View);
		}

		for (int32 Actor = 0; Actor < ActorCount; ++Actor)
		{
			const FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Actor)];
			const FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
			bool bTicks = false;
			if ((Record.Flags & (FieldActorFlag::NoModel | FieldActorFlag::Hidden)) == FieldActorFlag::NoModel)
			{
				bTicks = (State.Flags1 & (EventActorFlags1::KeepsRunningOffScreen | EventActorFlags1::OffScreen)) != EventActorFlags1::OffScreen && (State.Flags1 & EventActorFlags1::AnimationPaused) == 0
					&& (State.Flags0 & EventActorFlags0::Frozen) == 0;
			}
			else
			{
				bTicks = (State.Flags1 & EventActorFlags1::NoAnimations) != 0;
			}
			if (!bTicks)
			{
				continue;
			}
			FSprite* Sprite = Context.Models->ActorSprite(Actor);
			if (Sprite == nullptr)
			{
				return Unsupported(Actor, "the animation of an actor without a sprite is ticked");
			}
			Kernel->TickAnimation(*Sprite);
		}
		return std::nullopt;
	}
}
