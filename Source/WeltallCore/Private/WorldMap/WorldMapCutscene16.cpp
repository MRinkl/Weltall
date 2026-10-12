#include "WorldMap/WorldMapCutsceneDirector.h"

#include "Game/PsxRandom.h"
#include "Graphics/GteMatrix.h"

#include <algorithm>

namespace Weltall::WorldMapOps
{
	namespace Scene16
	{
		using R = EWorldMapRoutine;

		constexpr FWorldMapVector SceneStart{0x4000000, static_cast<int32>(0xFFF40000), 0x4000000, 0};
		constexpr uint16 FieldMap = 0x1FA;
		constexpr uint16 FieldEntrance = 0;
		/** The tasks' slots, as the start adds them. */
		constexpr int32 CoreSlot = 3;
		constexpr int32 PillarSlot = 4;
		constexpr int32 WaverControlSlot = 6;
		/** The models: the pillar's two shells and the core inside them. */
		constexpr int32 PillarShells[] = {0, 1};
		constexpr int32 Core = 2;
		constexpr int32 ModelCount = 3;
		constexpr int32 Sounds[] = {0x2E, 0x2F, 0x30};

		FWorldMapTask& TaskAt(FWorldMapContext& Context, int32 Slot)
		{
			return Context.Tasks[static_cast<std::size_t>(Slot)];
		}

		FWorldMapObject& ObjectAt(FWorldMapContext& Context, int32 Number)
		{
			return Context.Objects[static_cast<std::size_t>(Number)];
		}

		/** The director's steps (+20h) besides the wait and the end. */
		namespace Step
		{
			constexpr int16 WaverBursts = 2;
			constexpr int16 WaverWake = 3;
			constexpr int16 WaverSwell = 4;
			constexpr int16 WaverEbb = 5;
			constexpr int16 WaverScatter = 6;
			constexpr int16 WaverCalm = 7;
			constexpr int16 PlaySounds = 8;
			constexpr int16 Glow = 0x10;
			constexpr int16 Harden = 0x11;
			constexpr int16 FadeToBlack = 0x3F;
		}

		/** The waver control's steps (+20h), which are its messages too. */
		namespace WaverStep
		{
			constexpr int16 Bursts = 1;
			constexpr int16 Swell = 2;
			constexpr int16 Ebb = 3;
			constexpr int16 Scatter = 4;
			constexpr int16 Calm = 5;
		}

		constexpr int16 GlowMessage = 1;
		constexpr int16 HardenMessage = 2;

		const FWorldMapDirectorScript& Script(const FWorldMapContext& Context)
		{
			return Context.Tables->Cutscenes.Directors.Scene16;
		}

		FResult InitDirector(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Word(DirectorScriptIndex) = 0;
			TakeDirectorStep(Context, Task, Script(Context));
			Task.Word(DirectorScriptIndex) = 1;
			return WorldMapTaskState::Update;
		}

		FResult UpdateDirector(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			switch (Task.Step)
			{
			case DirectorStep::Wait:
				WaitDirector(Context, Task, Script(Context));
				return WorldMapTaskState::Update;
			case Step::WaverBursts:
				SendMessage(Context, WaverControlSlot, WaverStep::Bursts);
				break;
			case Step::WaverWake:
				// No message: the control only runs on as it was.
				SendMessage(Context, WaverControlSlot, 0);
				break;
			case Step::WaverSwell:
				SendMessage(Context, WaverControlSlot, WaverStep::Swell);
				break;
			case Step::WaverEbb:
				SendMessage(Context, WaverControlSlot, WaverStep::Ebb);
				break;
			case Step::WaverScatter:
				SendMessage(Context, WaverControlSlot, WaverStep::Scatter);
				break;
			case Step::WaverCalm:
				SendMessage(Context, WaverControlSlot, WaverStep::Calm);
				break;
			case Step::PlaySounds:
				for (const int32 Sound : Sounds)
				{
					PlayCutsceneEffect(Context, Sound);
				}
				break;
			case Step::Glow:
				SendMessage(Context, CoreSlot, GlowMessage);
				SendMessage(Context, PillarSlot, GlowMessage);
				break;
			case Step::Harden:
				SendMessage(Context, CoreSlot, HardenMessage);
				break;
			case Step::FadeToBlack:
				SetSongVolume(Context, 0, WorldMapSongFadeTicks);
				SendFade(Context, DirectorFade::Out, DirectorFade::Black, 4);
				break;
			case DirectorStep::Leave:
				LeaveCutscene(Context, Task);
				return WorldMapTaskState::Update;
			default:
				return WorldMapTaskState::Update;
			}
			Task.Step = DirectorStep::Wait;
			return WorldMapTaskState::Update;
		}

		/** The camera eases in from 2432 units to 1584, levelling out and swinging round. */
		constexpr int16 EasingIn = 1;
		constexpr int32 FirstDistance = 0x980000;
		constexpr int16 FirstPitch = -0x80;
		constexpr int16 FirstYaw = -0x200;
		constexpr FCameraEase EaseIn{-0x10000, 0x630000, 0x1000, 0x10000, 0x10000, 0x4B0000};

		FResult InitCamera(FWorldMapContext& Context, int32 Slot)
		{
			InitDirectedCamera(Context, TaskAt(Context, Slot));
			return WorldMapTaskState::Update;
		}

		FResult UpdateCamera(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			if (Task.Message == EasingIn)
			{
				Task.Step = EasingIn;
				Task.Message = 0;
				StartCameraEase(Context, Task, FirstDistance, FirstPitch, FirstYaw);
			}
			if (Context.LookAt == 0)
			{
				SetCameraOrbit(Context, Context.CameraDistance, {Context.CameraPitch, Context.CameraYaw, Context.CameraRoll});
			}
			if (Task.Step == EasingIn)
			{
				EaseCamera(Context, Task, EaseIn);
			}
			ShakeCamera(Context, Task);
			return WorldMapTaskState::Update;
		}

		/** The glowing models' packets: blended from the 4-bit page at (180h, 0), their own CLUT kept, their texture tinted by the task's colour. */
		constexpr uint16 GlowPage = 6;
		constexpr int32 AddingQuarter = 3;
		constexpr int32 Adding = 1;
		/** The tasks' words: the colour's red, green and blue (+50h, +54h, +58h). */
		constexpr int32 Red = 0;
		constexpr int32 Green = 1;
		constexpr int32 Blue = 2;
		constexpr int16 Glowing = 1;

		void SetAddLook(FWorldMapObject& Object, int32 Blend)
		{
			// The packets keep their built colours until the task's first update colours the buffer then drawn, before anything draws it.
			FWorldMapPacketLook Look;
			Look.Page = static_cast<uint16>(GlowPage | Blend << 5);
			Look.bSemiTransparent = true;
			Object.Look = Look;
		}

		/** The task's colour into the packets of the buffer being drawn. */
		void ColourPackets(FWorldMapContext& Context, const FWorldMapTask& Task, int32 Number)
		{
			FWorldMapObject& Object = ObjectAt(Context, Number);
			if (Object.Look)
			{
				Object.Look->Colours[static_cast<std::size_t>(Context.DrawBuffer & 1)] = {static_cast<uint8>(Task.Word(Red)), static_cast<uint8>(Task.Word(Green)),
					static_cast<uint8>(Task.Word(Blue))};
			}
		}

		bool HasModels(FWorldMapContext& Context)
		{
			if (Context.Objects.size() < static_cast<std::size_t>(ModelCount))
			{
				Context.Fault = MakeError("scene 16 has " + std::to_string(Context.Objects.size()) + " models where it lights three");
				return false;
			}
			return true;
		}

		FResult InitCore(FWorldMapContext& Context, int32 Slot)
		{
			if (!HasModels(Context))
			{
				return WorldMapTaskState::Init;
			}
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Step = 0;
			Task.Position.X = Context.Start.X;
			Task.Position.Y = Context.Start.Y - 0x100000;
			Task.Word(Red) = 0;
			Task.Word(Green) = 0;
			Task.Word(Blue) = 0;
			Task.Position.Z = Context.Start.Z;
			FWorldMapObject& Object = ObjectAt(Context, Core);
			SetAddLook(Object, Adding);
			Object.X = Task.Position.X >> 12;
			Object.Y = Task.Position.Y >> 12;
			Object.Z = Task.Position.Z >> 12;
			return WorldMapTaskState::Update;
		}

		FResult UpdateCore(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			if (Task.Message == GlowMessage)
			{
				Task.Message = 0;
				Task.Step = Glowing;
			}
			else if (Task.Message == HardenMessage)
			{
				Task.Message = 0;
				Task.Step = 0;
				// Only the buffer being drawn turns opaque: the other blends on until a second message comes on a tick of its own.
				if (FWorldMapObject& Object = ObjectAt(Context, Core); Object.Look)
				{
					Object.Look->bOpaque[static_cast<std::size_t>(Context.DrawBuffer & 1)] = true;
				}
			}
			if (Task.Step == Glowing)
			{
				++Task.Word(Red);
				++Task.Word(Green);
				++Task.Word(Blue);
				if (Task.Word(Red) > 0xFE)
				{
					Task.Word(Blue) = 0xFF;
					Task.Word(Green) = 0xFF;
					Task.Word(Red) = 0xFF;
					Task.Step = 0;
				}
			}
			ColourPackets(Context, Task, Core);
			return WorldMapTaskState::Update;
		}

		/** The pillar stands seven times its model's height. */
		constexpr std::array<int32, 3> PillarScale{0x1000, 0x7000, 0x1000};
		constexpr int32 PillarRed = 0xFC;

		FResult InitPillar(FWorldMapContext& Context, int32 Slot)
		{
			if (!HasModels(Context))
			{
				return WorldMapTaskState::Init;
			}
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Step = 0;
			Task.Position.X = Context.Start.X;
			Task.Position.Y = Context.Start.Y;
			Task.Word(Red) = 0;
			Task.Word(Green) = 0;
			Task.Word(Blue) = 0;
			Task.Position.Z = Context.Start.Z;
			for (const int32 Shell : PillarShells)
			{
				FWorldMapObject& Object = ObjectAt(Context, Shell);
				Object.Matrix.M = ScaleColumns(FGteMatrix::Identity(), PillarScale).M;
				SetAddLook(Object, AddingQuarter);
			}
			return WorldMapTaskState::Update;
		}

		FResult UpdatePillar(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			if (Task.Message == GlowMessage)
			{
				Task.Message = 0;
				Task.Step = Glowing;
			}
			if (Task.Step == Glowing)
			{
				Task.Word(Red) += 4;
				Task.Word(Green) += 2;
				Task.Word(Blue) += 1;
				if (Task.Word(Red) > PillarRed - 1)
				{
					Task.Word(Red) = PillarRed;
					Task.Step = 0;
				}
			}
			for (const int32 Shell : PillarShells)
			{
				ColourPackets(Context, Task, Shell);
			}
			return WorldMapTaskState::Update;
		}

		FResult InitWaver(FWorldMapContext& Context, int32)
		{
			Context.Waver.emplace();
			Context.Waver->Reach.fill(1);
			return WorldMapTaskState::Update;
		}

		FResult UpdateWaver(FWorldMapContext& Context, int32 Slot)
		{
			if (!Context.Waver)
			{
				Context.Fault = MakeError("the screen's waver runs before it was set up");
				return TaskAt(Context, Slot).State;
			}
			FWorldMapScreenWaver& Waver = *Context.Waver;
			for (std::size_t Row = 0; Row < Waver.Shifts.size(); ++Row)
			{
				const int32 Drawn = Context.Random->Next();
				const uint16 Reach = Waver.Reach[Row];
				if (Reach == 0)
				{
					Context.Fault = MakeError("the screen's waver shifts a row within nothing: the original stops on a division by zero");
					return TaskAt(Context, Slot).State;
				}
				Waver.Shifts[Row] = static_cast<int16>(static_cast<int16>(Drawn % Reach) - static_cast<int16>(Reach >> 1));
			}
			return WorldMapTaskState::Update;
		}

		/** The waver control's word: the reach every row starts from (+50h). */
		constexpr int32 BaseReach = 0;
		constexpr int32 FullReach = 0x40;
		constexpr int32 CalmReach = 2;

		FResult InitWaverControl(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			Task.Step = 0;
			Task.Word(BaseReach) = 1;
			return WorldMapTaskState::Update;
		}

		/** A row's reach drawn anew, one time in four: 1 to 64. */
		void Scatter(FPsxRandom& Random, uint16& Reach)
		{
			if ((Random.Next() & 3) == 0)
			{
				Reach = static_cast<uint16>((Random.Next() & 0x3F) + 1);
			}
		}

		/** A band of rows from First, a row longer while each draw of its length says so, scattered. */
		void ScatterBand(FPsxRandom& Random, FWorldMapScreenWaver& Waver, int32 First)
		{
			for (int32 Row = First; Row < First + (Random.Next() & 0x1F) + 1; ++Row)
			{
				Scatter(Random, Waver.Reach[static_cast<std::size_t>(Row)]);
			}
		}

		FResult UpdateWaverControl(FWorldMapContext& Context, int32 Slot)
		{
			FWorldMapTask& Task = TaskAt(Context, Slot);
			if (!Context.Waver)
			{
				Context.Fault = MakeError("the screen's waver is steered before it was set up");
				return Task.State;
			}
			if (Task.Message >= WaverStep::Bursts && Task.Message <= WaverStep::Calm)
			{
				Task.Step = Task.Message;
				Task.Message = 0;
			}
			FWorldMapScreenWaver& Waver = *Context.Waver;
			FPsxRandom& Random = *Context.Random;
			int32& Base = Task.Word(BaseReach);
			switch (Task.Step)
			{
			case WaverStep::Bursts:
			{
				Waver.Reach.fill(static_cast<uint16>(Base));
				const int32 Top = Random.Next() & 0x3F;
				ScatterBand(Random, Waver, Top);
				const int32 Middle = (Random.Next() & 0x3F) + 0x40;
				ScatterBand(Random, Waver, Middle);
				const int32 Bottom = (Random.Next() & 0x1F) + 0x80;
				ScatterBand(Random, Waver, Bottom);
				break;
			}
			case WaverStep::Swell:
				Base = std::min(Base + 1, FullReach);
				Waver.Reach.fill(static_cast<uint16>(Base));
				break;
			case WaverStep::Ebb:
				Base = std::max(Base - 1, CalmReach);
				Waver.Reach.fill(static_cast<uint16>(Base));
				break;
			case WaverStep::Scatter:
				for (uint16& Reach : Waver.Reach)
				{
					Scatter(Random, Reach);
				}
				break;
			case WaverStep::Calm:
				Waver.Reach.fill(CalmReach);
				break;
			default:
				break;
			}
			return WorldMapTaskState::Update;
		}

		constexpr FCutsceneRoutine Routines[] = {
			{R::InitScene16Director, InitDirector},
			{R::UpdateScene16Director, UpdateDirector},
			{R::InitScene16Camera, InitCamera},
			{R::UpdateScene16Camera, UpdateCamera},
			{R::InitScene16Core, InitCore},
			{R::UpdateScene16Core, UpdateCore},
			{R::InitScene16Pillar, InitPillar},
			{R::UpdateScene16Pillar, UpdatePillar},
			{R::InitScreenWaver, InitWaver},
			{R::UpdateScreenWaver, UpdateWaver},
			{R::InitWaverControl, InitWaverControl},
			{R::UpdateWaverControl, UpdateWaverControl},
		};

		void Start(FWorldMapContext& Context)
		{
			StopFieldSong(Context);
			StartCutsceneWorld(Context, SceneStart);
			StartCutsceneSong(Context);
			AddTask(Context, R::InitFade, R::UpdateFade);
			AddTask(Context, R::InitScene16Director, R::UpdateScene16Director);
			AddTask(Context, R::InitScene16Camera, R::UpdateScene16Camera);
			AddTask(Context, R::InitScene16Core, R::UpdateScene16Core);
			AddTask(Context, R::InitScene16Pillar, R::UpdateScene16Pillar);
			AddTask(Context, R::InitScreenWaver, R::UpdateScreenWaver);
			AddTask(Context, R::InitWaverControl, R::UpdateWaverControl);
			AddTask(Context, R::InitCutsceneFrame, R::UpdateCutsceneFrame);
			ResetEncounters(Context);
		}

		void End(FWorldMapContext& Context)
		{
			StopCutsceneEffects(Context);
			EndCutscene(Context, FieldMap, FieldEntrance);
		}
	}

	/** The world's centre: the screen wavers in bursts and swells, a pillar of light and its core glow into view. */
	const FCutscene Scene16Cutscene{{16, 16}, 2, 0x10, Scene16::Start, Scene16::End, Scene16::Routines};
}
