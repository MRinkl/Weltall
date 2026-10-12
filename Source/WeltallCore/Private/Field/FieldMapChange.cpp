#include "Field/FieldMapChange.h"

#include "Field/EventMachine.h"
#include "Field/FieldActorSounds.h"
#include "Field/FieldFrameVariables.h"
#include "Field/FieldRuntime.h"
#include "Field/FieldSprites.h"
#include "Field/FieldTick.h"
#include "Foundation/ProcessorDivide.h"
#include "Input/KernelPad.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		constexpr int32 MainLoopStrayWait = 0xFF;
		/** The transitions a map change runs (12's operand): past the last, the original loads no map. */
		namespace Transition
		{
			constexpr int32 FadeToBlack = 0;
			constexpr int32 Flash = 1;
			constexpr int32 Zoom = 2;
			constexpr int32 Cut = 3;
			constexpr int32 ZoomWithoutRedraw = 4;
			constexpr int32 CutMovingPicture = 5;
			constexpr int32 FadeWithoutPicture = 6;
		}
		constexpr int32 DefaultTransition = Transition::Zoom;
		constexpr int32 DefaultTransitionTicks = 0x20;
		/** The ticks a cut's next map runs under the picture of the last. */
		constexpr int32 CutTicks = 4;
		/** Transition 5 moves the picture's area and the rows below it to here. */
		constexpr FVramRect MovedPicture{0x2C0, 0x100, 0x140, 0xFF};
		constexpr int16 MovedPictureX = 0x140;
		constexpr int16 MovedPictureY = 0xFF;
		/** The picture fades from full brightness (16.16) over the ticks, and zooms by 0x20 a tick up to 0x22C0. */
		constexpr int32 FullCoverBrightness = 0x800000;
		constexpr int32 CoverZoomStep = 0x20;
		constexpr int32 CoverZoomLimit = 0x22C0;
	}

	void KeepAcrossMaps(const FEventContext& From, FEventContext& To)
	{
		To.PartyCharacters = From.PartyCharacters;

		const FFieldCamera& Camera = From.Camera;
		To.Camera.TargetFrom = Camera.TargetFrom;
		To.Camera.TargetTo = Camera.TargetTo;
		To.Camera.EyeFrom = Camera.EyeFrom;
		To.Camera.EyeTo = Camera.EyeTo;
		To.Camera.TargetMove.At = Camera.TargetMove.At;
		To.Camera.TargetMove.Step = Camera.TargetMove.Step;
		To.Camera.EyeMove.At = Camera.EyeMove.At;
		To.Camera.EyeMove.Step = Camera.EyeMove.Step;
		for (const auto Ramp : {&FFieldCamera::ZoomRamp, &FFieldCamera::PitchRamp, &FFieldCamera::ProjectionRamp})
		{
			(To.Camera.*Ramp).Value = (Camera.*Ramp).Value;
			(To.Camera.*Ramp).Step = (Camera.*Ramp).Step;
		}
		To.Camera.ScriptYaw = Camera.ScriptYaw;
		To.Camera.ScriptPitch = Camera.ScriptPitch;
		To.Camera.ScriptDistance = Camera.ScriptDistance;
		To.Camera.FollowTicks = Camera.FollowTicks;
		To.Camera.ReturnTicks = Camera.ReturnTicks;
		To.Camera.ShakeTicks = Camera.ShakeTicks;
		To.Camera.ViewPitch = Camera.ViewPitch;

		To.Encounters.Countdown = From.Encounters.Countdown;
		To.Encounters.Weights = From.Encounters.Weights;
		To.Encounters.Formation = From.Encounters.Formation;
		To.BattlingResult = From.BattlingResult;
		To.bBattleRequested = From.bBattleRequested;
		To.Requests.bScriptedBattle = From.Requests.bScriptedBattle;
		To.Requests.BattleKind = From.Requests.BattleKind;
		To.Requests.bWorldMap = From.Requests.bWorldMap;
		To.Requests.bBattling = From.Requests.bBattling;
		To.Requests.BattlingParameters = From.Requests.BattlingParameters;
		To.Requests.bProgram = From.Requests.bProgram;
		To.Requests.Program = From.Requests.Program;
		To.Requests.Movie = From.Requests.Movie;
		To.Requests.Movie.Cues = FFieldMovieRequest::NoCues;

		To.PadLatched = From.PadLatched;
		To.bGearsLocked = From.bGearsLocked;
		To.Messages = From.Messages;
		To.MessagesOpened = From.MessagesOpened;
		To.Particles.Setup = From.Particles.Setup;
		To.Particles.Owner = From.Particles.Owner;
		To.Particles.SpaceBits = From.Particles.SpaceBits;
		To.Particles.GearSlot = From.Particles.GearSlot;
		To.Particles.GearJoint = From.Particles.GearJoint;
		To.Particles.Current = From.Particles.Current;
		To.Clock = From.Clock;
		for (std::size_t Layer = 0; Layer < To.Render.Fades.size(); ++Layer)
		{
			To.Render.Fades[Layer].Step = From.Render.Fades[Layer].Step;
		}
		To.Render.Cover = From.Render.Cover;
		To.Render.Compass.Needle = From.Render.Compass.Needle;
		To.Render.bHiResDisplay = From.Render.bHiResDisplay;
		// The load sets only the gears' light and colour matrices' nine entries each.
		constexpr int32 SetEntries = 9;
		for (int32 Matrix = 0; Matrix < 2; ++Matrix)
		{
			for (int32 Word = SetEntries; Word < FFieldGearLights::MatrixWords; ++Word)
			{
				const std::size_t At = static_cast<std::size_t>(Matrix * FFieldGearLights::MatrixWords + Word);
				To.Render.GearLights.Words[At] = From.Render.GearLights.Words[At];
			}
		}
		// The load resets the gears' count and pool alone: the animations they last played and the texture
		// shifts stay for the next map (a return from a battle plays the kept animations again).
		To.Gears.Animations = From.Gears.Animations;
		To.Gears.TextureShifts = From.Gears.TextureShifts;
		// The point lights keep their colours, radii and places; the load leaves them following nobody.
		To.Render.GearLights.Points = From.Render.GearLights.Points;
		for (FFieldGearLights::FPointLight& Point : To.Render.GearLights.Points)
		{
			Point.Actor = FFieldGearLights::FPointLight::Off;
		}
		To.Movement.JumpInterval = From.Movement.JumpInterval;
		To.Controls.bTouchLatched = From.Controls.bTouchLatched;
		To.MenuProgramParam = From.MenuProgramParam;
		To.MenusPending = From.MenusPending;
		To.Movie.Frame = From.Movie.Frame;
		To.Render.bEffectTexturesSaved = From.Render.bEffectTexturesSaved;
	}

	std::optional<FError> FFieldRuntime::BeginMapChange(bool& bFramesFollow)
	{
		bFramesFollow = false;
		FEventContext& Context = FieldContext;
		Context.bFieldRunning = false;
		const FEventActorState* Leader = Context.StateOf(Context.LeaderActor);
		if (Leader == nullptr)
		{
			return MakeError("field map " + std::to_string(Context.RunningMap) + " changes maps, and its leader, actor " + std::to_string(Context.LeaderActor)
				+ ", has no script state");
		}
		SaveFieldVariables(Context, *Leader);
		for (int32 Effect = 0; Effect < FFieldParticles::EffectCount; ++Effect)
		{
			Context.Particles.End(Effect);
		}
		StopFieldActorSoundsForExit(Context);
		Context.Messages.CloseAll();
		Transition = Context.Requests.MapTransition;
		const int32 Ticks = Context.Requests.MapTransitionTicks;
		if (Transition < Transition::FadeToBlack || Transition > Transition::FadeWithoutPicture)
		{
			// Carried past, the map changes with a cut.
			if (!RecoverUnreached(Context.Unreached, "a map change with a transition past the seven"))
			{
				return MakeError("field map " + std::to_string(Context.RunningMap) + " changes maps with transition " + std::to_string(Transition)
					+ ", with which the original loads no map");
			}
			Transition = Transition::Cut;
		}
		FFieldRenderState& Render = Context.Render;
		if (Transition != Transition::FadeWithoutPicture)
		{
			Render.SaveEffectTextures();
		}
		switch (Transition)
		{
		case Transition::FadeToBlack:
		case Transition::Flash:
		case Transition::FadeWithoutPicture:
			if (Transition == Transition::Flash)
			{
				Render.StartWhiteOut(Ticks);
			}
			else
			{
				Render.StartFadeOut(Ticks);
			}
			if (Transition != Transition::FadeWithoutPicture)
			{
				Render.Cover.Take(false, true);
			}
			if (Ticks > 0)
			{
				FramesLeft = Ticks;
				Phase = EPhase::MapFadeOut;
				bFramesFollow = true;
				return std::nullopt;
			}
			return LoadNextMap();
		case Transition::Zoom:
		case Transition::ZoomWithoutRedraw:
		case Transition::Cut:
		case Transition::CutMovingPicture:
		{
			const bool bZoom = Transition == Transition::Zoom || Transition == Transition::ZoomWithoutRedraw;
			Render.Cover.Take(bZoom, Transition != Transition::ZoomWithoutRedraw);
			if (auto Error = LoadNextMap())
			{
				return Error;
			}
			if (Transition == Transition::CutMovingPicture)
			{
				// Once the next map's textures are in, the picture's area goes to where its models show it.
				FFieldVramOperation Move;
				Move.Kind = EFieldVramChange::Copy;
				Move.Rect = MovedPicture;
				Move.ToX = MovedPictureX;
				Move.ToY = MovedPictureY;
				FieldContext.Render.PictureMoves.push_back(Move);
				FieldContext.Render.WroteVram({MovedPictureX, MovedPictureY, MovedPicture.Width, MovedPicture.Height});
			}
			if (bZoom)
			{
				FieldContext.Messages.bMapEntering = true;
				FieldContext.Render.StartFadeIn(Ticks);
				CoverBrightness = FullCoverBrightness;
				if (Ticks <= 0)
				{
					return std::nullopt;
				}
			}
			FramesLeft = 0;
			Phase = EPhase::MapEntry;
			bFramesFollow = true;
			return std::nullopt;
		}
		default:
			return std::nullopt;
		}
	}

	std::optional<FError> FFieldRuntime::LoadNextMap()
	{
		const FEventContext Before = std::move(FieldContext);
		const int32 Next = Before.MapNumber;
		if (auto Error = LoadMap(Next, &Before))
		{
			return Error;
		}
		if (auto Error = FEventMachine::InitMap(FieldContext))
		{
			return Error;
		}
		if (auto Error = FinishLoad(Next))
		{
			return Error;
		}
		if (auto Error = ActorSprites->System().FaultError())
		{
			return Error;
		}
		// The change's transition outlives the load's reset of it until the change is over.
		FieldContext.Requests.MapTransition = Before.Requests.MapTransition;
		FieldContext.Requests.MapTransitionTicks = Before.Requests.MapTransitionTicks;
		if (Transition == Transition::Flash)
		{
			FieldContext.Render.StartWhiteIn(FieldContext.Requests.MapTransitionTicks);
		}
		else if (Transition == Transition::FadeToBlack || Transition == Transition::FadeWithoutPicture)
		{
			FieldContext.Render.StartFadeIn(FieldContext.Requests.MapTransitionTicks);
		}
		return std::nullopt;
	}

	std::optional<FError> FFieldRuntime::EndMapChange()
	{
		FEventContext& Context = FieldContext;
		if (Transition != Transition::FadeWithoutPicture)
		{
			Context.Render.RestoreEffectTextures();
		}
		Context.Requests.MapTransition = DefaultTransition;
		Context.Requests.MapTransitionTicks = DefaultTransitionTicks;
		Context.Messages.bMapEntering = false;
		Services.Pad->Reset();
		Context.bFieldRunning = true;
		Phase = EPhase::MainLoop;
		return std::nullopt;
	}

	std::optional<FError> FFieldRuntime::TickMapFade()
	{
		FieldContext.DrawBuffer ^= 1;
		FieldContext.Render.Cover.bShown = Transition != Transition::FadeWithoutPicture;
		FieldContext.Render.TickFades(FieldContext.Options.bFadesRiseFromBlack);
		if (--FramesLeft > 0)
		{
			return std::nullopt;
		}
		if (auto Error = LoadNextMap())
		{
			return Error;
		}
		if (auto Error = EndMapChange())
		{
			return Error;
		}
		return RunIteration(EStep::AfterMapChange);
	}

	std::optional<FError> FFieldRuntime::TickMapEntry()
	{
		FieldContext.Render.Cover.bShown = true;
		if (auto Error = RunFrame(MainLoopStrayWait, true))
		{
			return Error;
		}
		FEventContext& Context = FieldContext;
		FFieldCover& Cover = Context.Render.Cover;
		const bool bZoom = Transition == Transition::Zoom || Transition == Transition::ZoomWithoutRedraw;
		const int32 Ticks = bZoom ? Context.Requests.MapTransitionTicks : CutTicks;
		if (bZoom)
		{
			Cover.Brightness[(Context.DrawBuffer + 1) & 1] = static_cast<uint8>(CoverBrightness >> 16);
			CoverBrightness = std::max(CoverBrightness - ProcessorDivide(FullCoverBrightness, Ticks), 0);
			if (Cover.Scale < CoverZoomLimit)
			{
				Cover.Scale += CoverZoomStep;
			}
		}
		if (++FramesLeft < Ticks)
		{
			return std::nullopt;
		}
		if (auto Error = EndMapChange())
		{
			return Error;
		}
		return RunIteration(EStep::AfterMapChange);
	}
}
