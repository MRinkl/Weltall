#include "WorldMap/WorldMapCutsceneScript.h"

#include "Graphics/GteMatrix.h"
#include "Graphics/GteVectors.h"

#include <string>

namespace Weltall::WorldMapOps
{
	namespace Scene17
	{
		using R = EWorldMapRoutine;

		constexpr FWorldMapVector FirstPlace{0x70A9000, static_cast<int32>(0xFFEB8000), 0x42AA000, 0};
		constexpr uint16 FieldMap = 0x269;
		constexpr uint16 FieldEntrance = 2;
		/** The ribbons' tasks, as the start adds them, each with its own model. */
		constexpr int32 FirstRibbonSlot = 3;
		constexpr int32 RibbonCount = 5;
		constexpr int32 FirstRibbonModel = 81;

		FWorldMapTask& TaskAt(FWorldMapContext& Context, int32 Slot)
		{
			return Context.Tasks[static_cast<std::size_t>(Slot)];
		}

		FResult InitScript(FWorldMapContext& Context, int32 Slot)
		{
			TaskAt(Context, Slot).Word(ScriptAt) = 0;
			return WorldMapTaskState::Update;
		}

		/** The camera's steps (+20h): what it eases toward between the script's messages. */
		namespace CameraStep
		{
			constexpr int16 Still = 0;
			constexpr int16 Rise = 1;
			constexpr int16 PullBack = 2;
			constexpr int16 Close = 3;
			constexpr int16 ShakeGrows = 4;
			constexpr int16 Turn = 6;
			constexpr int16 Sweep = 8;
			constexpr int16 Drift = 11;
		}

		/**
		 * A message that cuts: the step it leaves the camera in, its distance and angles, the place it shows, if a new
		 * one, and whether the shake ends.
		 */
		struct FCut
		{
			int16 Message;
			int16 Step;
			int32 Distance;
			int16 Pitch;
			int16 Yaw;
			bool bPlace;
			FWorldMapVector Place;
			bool bCalm;
		};

		constexpr FCut Cuts[] = {
			{2, CameraStep::Still, 0x3E0000, -0x20, 0xD70, false, {}, false},
			{6, 5, 0x320000, 0x40, 0xD40, true, {0x1379000, static_cast<int32>(0xFFEE0000), 0x4B2A000, 0}, true},
			{8, 7, 0x100000, -0x90, -0x510, true, {0x7529000, static_cast<int32>(0xFFE74000), 0x2A3A000, 0}, false},
			{10, 9, 0x320000, -0x160, 0xDE0, true, {0x2659000, static_cast<int32>(0xFFEF0000), 0x6D8A000, 0}, false},
			{11, 10, 0x290000, -0x1A0, 0x160, true, {0x4BA9000, static_cast<int32>(0xFFD78000), 0x1DDA000, 0}, false},
		};

		/** A message that only sets the camera's step. */
		struct FMove
		{
			int16 Message;
			int16 Step;
		};

		constexpr FMove Moves[] = {
			{1, CameraStep::Rise}, {3, CameraStep::PullBack}, {4, CameraStep::Close}, {5, CameraStep::ShakeGrows},
			{7, CameraStep::Turn}, {9, CameraStep::Sweep}, {12, CameraStep::Drift},
		};

		/** Where the camera closes in on, and where it drifts to at the end. */
		constexpr FWorldMapVector ClosePlace{0x7499000, static_cast<int32>(0xFFE74000), 0x408A000, 0};
		constexpr FWorldMapVector DriftPlace{0x4CD9000, static_cast<int32>(0xFFEA4000), 0x1C8A000, 0};
		constexpr FWorldMapVector DriftStep{0x2600, 0x2580, -0x2A00, 0};
		constexpr int32 ShakeGrowth = 0x200;
		constexpr int32 FullShake = 0x8000;
		constexpr int32 NoShake = 0x1000;

		FResult InitCamera(FWorldMapContext& Context, int32 Slot)
		{
			InitScriptCamera(Context, TaskAt(Context, Slot), 0x500000, -0x220, 0);
			return WorldMapTaskState::Update;
		}

		void ReadCameraMessage(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			const int16 Message = Task.Message;
			for (const FMove& Move : Moves)
			{
				if (Move.Message != Message)
				{
					continue;
				}
				Task.Message = 0;
				Task.Step = Move.Step;
				if (Move.Step == CameraStep::Rise)
				{
					Task.Word(ScriptCameraWord::GoalPitch) = Task.Direction.X;
					Task.Word(ScriptCameraWord::GoalYaw) = Task.Direction.Y;
					Task.Word(ScriptCameraWord::GoalRoll) = Task.Direction.Z;
				}
				else if (Move.Step == CameraStep::Close)
				{
					Context.Controlled.X = ClosePlace.X;
					Context.Controlled.Y = ClosePlace.Y;
					Context.Controlled.Z = ClosePlace.Z;
				}
				return;
			}
			for (const FCut& Cut : Cuts)
			{
				if (Cut.Message != Message)
				{
					continue;
				}
				Task.Message = 0;
				Task.Step = Cut.Step;
				CutScriptCamera(Context, Task, Cut.Distance, Cut.Pitch, Cut.Yaw);
				if (Cut.bCalm)
				{
					Task.Word(ScriptCameraWord::Shake) = NoShake;
				}
				if (Cut.bPlace)
				{
					// The camera's target takes the place only as the eases end this tick: the orbit is set about the place before once more.
					Task.Position.X = Context.Controlled.X = Cut.Place.X;
					Task.Position.Y = Context.Controlled.Y = Cut.Place.Y;
					Task.Position.Z = Context.Controlled.Z = Cut.Place.Z;
				}
				return;
			}
		}

		FResult UpdateCamera(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			ReadCameraMessage(Context, Task);
			if (Context.LookAt == 0)
			{
				SetCameraOrbit(Context, Context.CameraDistance, {Context.CameraPitch, Context.CameraYaw, Context.CameraRoll});
			}
			int32& Pitch = Task.Word(ScriptCameraWord::GoalPitch);
			int32& Yaw = Task.Word(ScriptCameraWord::GoalYaw);
			int32& Distance = Task.Word(ScriptCameraWord::GoalDistance);
			switch (Task.Step)
			{
			case CameraStep::Rise:
				Pitch = StepTowards(Pitch, -0x20000, 0x5D17);
				Yaw = StepTowards(Yaw, 0x580000, 0x10000);
				break;
			case CameraStep::PullBack:
				Pitch = StepTowards(Pitch, -0x1F0000, -0x10000);
				Yaw = StepTowards(Yaw, 0x1290000, 0x10000);
				Distance = StepTowards(Distance, 0x490000, 0x10000);
				break;
			case CameraStep::Close:
				Pitch = StepTowards(Pitch, -0x10000, 0x10000);
				Yaw = StepTowards(Yaw, 0x1290000, 0x10000);
				Distance = StepTowards(Distance, 0x2B0000, -0x10000);
				break;
			case CameraStep::ShakeGrows:
			{
				int32& Shake = Task.Word(ScriptCameraWord::Shake);
				Shake += ShakeGrowth;
				if (Shake > FullShake)
				{
					Shake = FullShake;
					Task.Step = CameraStep::Still;
				}
				break;
			}
			case CameraStep::Turn:
				Yaw = StepTowards(Yaw, 0xF40000, 0x4000);
				break;
			case CameraStep::Sweep:
				Pitch = StepTowards(Pitch, 0x50000, 0x2000);
				Yaw = StepTowards(Yaw, 0x10000, 0x8000);
				break;
			case CameraStep::Drift:
				Pitch = StepTowards(Pitch, -0x80000, 0x2400);
				Yaw = StepTowards(Yaw, 0x2C0000, 0x2C00);
				Distance = StepTowards(Distance, 0x100000, -0x3200);
				Context.Controlled.X = StepTowards(Context.Controlled.X, DriftPlace.X, DriftStep.X);
				Context.Controlled.Y = StepTowards(Context.Controlled.Y, DriftPlace.Y, DriftStep.Y);
				Context.Controlled.Z = StepTowards(Context.Controlled.Z, DriftPlace.Z, DriftStep.Z);
				Context.Ground.Wrap(Context.Controlled);
				break;
			default:
				break;
			}
			FollowScriptCamera(Context, Task);
			return WorldMapTaskState::Update;
		}

		/**
		 * A ribbon's words (+50h to +78h), which a stop's record fills from its fourth word on: its colour, the steps of
		 * its red and green, its turn about y, its wave's phase, its length (4.12 of the model's) and the length's growth.
		 */
		namespace RibbonWord
		{
			constexpr int32 Red = 0;
			constexpr int32 Green = 1;
			constexpr int32 Blue = 2;
			constexpr int32 RedStep = 3;
			constexpr int32 GreenStep = 4;
			constexpr int32 Yaw = 6;
			constexpr int32 Phase = 7;
			constexpr int32 Length = 8;
			constexpr int32 Growth = 9;
			constexpr int32 Count = 11;
		}
		constexpr std::size_t RecordPlace = 0;
		constexpr std::size_t RecordWords = 3;
		/** The messages: the ribbons go to a stop and grow out again, or freeze where they are. */
		constexpr int16 FreezeMessage = 3;
		constexpr int16 StopMessages[] = {1, 2, 4};
		constexpr int16 Frozen = 0;
		constexpr int16 Flowing = 1;
		/** The ribbon's own page (4-bit at (2C0h, 100h)) adding a quarter of it to the picture. */
		constexpr uint16 RibbonPage = 0x7B;
		constexpr int32 LowRed = 0x40;
		constexpr int32 LowGreen = 0x80;
		constexpr int32 Brightest = 0xFF;
		constexpr int32 PhaseStep = 8;
		constexpr int32 WaveDepth = 6;

		FWorldMapObject* RibbonModel(FWorldMapContext& Context, int32 Slot)
		{
			const std::size_t Model = static_cast<std::size_t>(FirstRibbonModel + Slot - FirstRibbonSlot);
			if (Model >= Context.Objects.size())
			{
				Context.Fault = MakeError("scene 17 has " + std::to_string(Context.Objects.size()) + " models where its ribbons are models 81 to 85");
				return nullptr;
			}
			return &Context.Objects[Model];
		}

		FResult InitRibbon(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapObject* Model = RibbonModel(Context, Slot);
			if (Model == nullptr)
			{
				return WorldMapTaskState::Init;
			}
			FWorldMapPacketLook Look;
			Look.Page = RibbonPage;
			Look.bSemiTransparent = true;
			Model->Look = Look;
			return WorldMapTaskState::Update;
		}

		/** A colour's step: it turns back at the brightest and at its lowest. */
		void StepColour(int32& Colour, int32& Step, int32 Lowest)
		{
			Colour += Step;
			if (Colour > Brightest)
			{
				Colour = Brightest;
				Step = -Step;
			}
			else if (Colour < Lowest)
			{
				Colour = Lowest;
				Step = -Step;
			}
		}

		FResult UpdateRibbon(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapObject* Model = RibbonModel(Context, Slot);
			if (Model == nullptr)
			{
				return WorldMapTaskState::Init;
			}
			FWorldMapTask& Task = TaskAt(Context, Slot);
			if (Task.Message == FreezeMessage)
			{
				Task.Message = 0;
				Task.Step = Frozen;
			}
			for (std::size_t Stop = 0; Stop < std::size(StopMessages); ++Stop)
			{
				if (Task.Message != StopMessages[Stop])
				{
					continue;
				}
				const std::array<int16, 14>& Record = Context.Tables->Cutscenes.Scripts.RibbonStops[Stop][static_cast<std::size_t>(Slot - FirstRibbonSlot)];
				Task.Message = 0;
				Task.Step = Flowing;
				Model->X = Record[RecordPlace];
				Model->Y = Record[RecordPlace + 1];
				Model->Z = Record[RecordPlace + 2];
				for (int32 Word = 0; Word < RibbonWord::Count; ++Word)
				{
					Task.Word(Word) = Record[RecordWords + static_cast<std::size_t>(Word)];
				}
				break;
			}
			if (Task.Step == Flowing)
			{
				// Turned about y, as long as it has grown, and its depth waving through nothing to six times itself, either way round.
				const FGteMatrix Turned = RotationYxz({0, static_cast<int16>(Task.Word(RibbonWord::Yaw)), 0});
				Model->Matrix.M =
					ScaleColumns(Turned, {Task.Word(RibbonWord::Length) & 0x7FFF, FGteMatrix::One, GteSin(Task.Word(RibbonWord::Phase)) * WaveDepth}).M;
				StepColour(Task.Word(RibbonWord::Red), Task.Word(RibbonWord::RedStep), LowRed);
				StepColour(Task.Word(RibbonWord::Green), Task.Word(RibbonWord::GreenStep), LowGreen);
				Task.Word(RibbonWord::Phase) = (Task.Word(RibbonWord::Phase) + PhaseStep) & 0xFFF;
				Task.Word(RibbonWord::Length) += Task.Word(RibbonWord::Growth);
			}
			// Only the packets this tick draws take the colour: the other buffer's keep the tick before's.
			Model->Look->Colours[static_cast<std::size_t>(Context.DrawBuffer & 1)] = {static_cast<uint8>(Task.Word(RibbonWord::Red)),
				static_cast<uint8>(Task.Word(RibbonWord::Green)), static_cast<uint8>(Task.Word(RibbonWord::Blue))};
			return WorldMapTaskState::Update;
		}

		/** The terrain about a new start point: with nothing to read, only the sea's waves start again. */
		FResult UpdateRestream(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			if (Task.Message == 1)
			{
				Task.Message = 0;
				Context.WavePhase = 0;
				Context.bTerrainRestarted = true;
			}
			return WorldMapTaskState::Update;
		}

		constexpr FCutsceneRoutine Routines[] = {
			{R::InitScene17Script, InitScript},
			{R::InitScene17Camera, InitCamera},
			{R::UpdateScene17Camera, UpdateCamera},
			{R::InitRibbon, InitRibbon},
			{R::UpdateRibbon, UpdateRibbon},
			{R::InitRestream, [](FWorldMapContext&, int32) -> FResult { return WorldMapTaskState::Update; }},
			{R::UpdateRestream, UpdateRestream},
		};

		void Start(FWorldMapContext& Context)
		{
			if (Context.Sound != nullptr)
			{
				Context.Sound->StopFieldSong();
			}
			StartCutsceneWorld(Context, FirstPlace);
			if (Context.Sound != nullptr)
			{
				Context.Sound->LoadBank();
				Context.Sound->RegisterEffects();
				Context.Sound->LoadSong(EWorldMapSong::Map);
				Context.Sound->PlaySong(WorldMapFullVolume, 0);
			}
			AddTask(Context, R::InitFade, R::UpdateFade);
			AddTask(Context, R::InitScene17Script, R::RunScript);
			AddTask(Context, R::InitScene17Camera, R::UpdateScene17Camera);
			for (int32 Ribbon = 0; Ribbon < RibbonCount; ++Ribbon)
			{
				AddTask(Context, R::InitRibbon, R::UpdateRibbon);
			}
			AddTask(Context, R::InitRestream, R::UpdateRestream);
			AddTask(Context, R::InitCutsceneFrameTrees, R::UpdateCutsceneFrameTrees);
			ResetEncounters(Context);
		}

		void End(FWorldMapContext& Context)
		{
			StopCutsceneEffects(Context);
			EndCutscene(Context, FieldMap, FieldEntrance);
		}
	}

	/** Ribbons of light spread from the Mass Driver over the Ethos' headquarters, Lahan, the Orphanage and Nisan, on a script. */
	const FCutscene Scene17Cutscene{{17, 17}, 2, 0x10, Scene17::Start, Scene17::End, Scene17::Routines};
}
