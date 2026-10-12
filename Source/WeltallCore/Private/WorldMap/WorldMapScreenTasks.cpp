#include "WorldMap/WorldMapOps.h"

namespace Weltall::WorldMapOps
{
	namespace
	{
		constexpr int16 FadeInMessage = 0xC;
		constexpr int16 FadeOutMessage = 0xD;
		constexpr int32 FadeStartLevel = 0xF8;
		constexpr int32 FadeFullLevel = 0xFF;
		constexpr int32 WindowWidthUnits = 0x20;
		constexpr int32 PlaceNameLines = 1;
		constexpr int32 NoticeLines = 4;
		constexpr uint8 WindowGlyphsPerTick = 8;

		namespace FadeStep
		{
			constexpr int16 In = 0;
			constexpr int16 Out = 1;
		}

		FWorldMapTask& TaskAt(FWorldMapContext& Context, int32 Slot)
		{
			return Context.Tasks[static_cast<std::size_t>(Slot)];
		}

		void ShowMessage(FWorldMapContext& Context, FMesWindow& Window, int16 Message)
		{
			Window.Reset();
			Window.Queue(Context.Messages->String(Message));
		}

		/** The place name's and the notice's update: the window follows the message the tasks found under the party. */
		void FollowMessage(FWorldMapContext& Context, FWorldMapTask& Task, FMesWindow& Window, int16 Message)
		{
			if (Task.Step == 0)
			{
				if (Message != -1)
				{
					ShowMessage(Context, Window, Message);
					Task.Step = 1;
					Task.Word(WorldMapTaskWord::Shown) = Message;
				}
			}
			else if (Task.Step == 1)
			{
				if (Message == -1)
				{
					Window.Reset();
					Task.Step = 0;
				}
				else if (Message != Task.Word(WorldMapTaskWord::Shown))
				{
					ShowMessage(Context, Window, Message);
					Task.Word(WorldMapTaskWord::Shown) = Message;
				}
			}
			Window.Update(*Context.Text);
		}

		FMesWindow MakeWindow(int32 Lines)
		{
			FMesWindow Window(WindowWidthUnits, Lines);
			Window.SetSpeed(WindowGlyphsPerTick);
			return Window;
		}
	}

	FResult InitFade(FWorldMapContext& Context, int32 Slot)
	{
		TaskAt(Context, Slot).Word(WorldMapTaskWord::FadeLevel) = FadeStartLevel;
		Context.FadePageBlend = Context.FadeBlend;
		return WorldMapTaskState::Update;
	}

	FResult UpdateFade(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		if (Task.Message == FadeInMessage || Task.Message == FadeOutMessage)
		{
			Task.Step = Task.Message == FadeInMessage ? FadeStep::In : FadeStep::Out;
			Task.Message = 0;
			Context.Fading = 1;
			Context.FadePageBlend = Context.FadeBlend;
		}
		int32& Level = Task.Word(WorldMapTaskWord::FadeLevel);
		FResult Result = WorldMapTaskState::Update;
		if (Task.Step == FadeStep::In)
		{
			Context.FadeShown = Level;
			Level -= Context.FadeStep;
			if (Level >= 0)
			{
				Task.Timer = static_cast<int16>(Task.Timer + 1);
				return Result;
			}
			Result = WorldMapTaskState::Idle;
			Level = 0;
		}
		else if (Task.Step == FadeStep::Out)
		{
			Context.FadeShown = Level;
			Level += Context.FadeStep;
			if (Level < FadeFullLevel)
			{
				Task.Timer = static_cast<int16>(Task.Timer + 1);
				return Result;
			}
			Level = FadeFullLevel;
		}
		else
		{
			return Result;
		}
		Context.Fading = 0;
		Task.Timer = static_cast<int16>(Task.Timer + 1);
		return Result;
	}

	FResult InitPlaceName(FWorldMapContext& Context, int32)
	{
		Context.Place = -1;
		Context.PlaceNameWindow = MakeWindow(PlaceNameLines);
		Context.PlaceNameWindow.HideBox();
		Context.PlaceNameWindow.Reset();
		return WorldMapTaskState::Update;
	}

	FResult UpdatePlaceName(FWorldMapContext& Context, int32 Slot)
	{
		FollowMessage(Context, TaskAt(Context, Slot), Context.PlaceNameWindow, Context.Place);
		return WorldMapTaskState::Update;
	}

	FResult InitNotice(FWorldMapContext& Context, int32)
	{
		Context.Notice = -1;
		Context.NoticeWindow = MakeWindow(NoticeLines);
		// The program sets the place name's no-box flag again here, not the notice's: the notice keeps its box over its backdrop.
		Context.PlaceNameWindow.HideBox();
		Context.NoticeWindow.Reset();
		return WorldMapTaskState::Update;
	}

	FResult UpdateNotice(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		FollowMessage(Context, Task, Context.NoticeWindow, Context.Notice);
		Context.bNoticeBackdrop = Task.Step == 1;
		return WorldMapTaskState::Update;
	}
}
