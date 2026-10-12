#include "GearMachine.h"

#include <string>

namespace Weltall::GearMachine
{
	namespace
	{
		constexpr int32 PointLights = 2;
		constexpr uint8 RelativeToModel = 0x80;

		/** How far an event of each kind reaches, as the step moves past it; 0 for a kind it does not move past. */
		uint32 EventBytes(uint8 Kind, bool bOn)
		{
			switch (Kind)
			{
			case 1:
				return 0x14;
			case 2:
				return bOn ? 0x12 : 6;
			case 3:
			case 4:
			case 9:
				return bOn ? 0x1C : 6;
			case 5:
				return 8;
			case 6:
				return 4;
			case 7:
				return 6;
			case 8:
				return 0xA;
			default:
				return 0;
			}
		}

		/** One event; false where the step does not move past it (it still counts as done). */
		bool RunEvent(FGearModels& Models, FGearModel& Model, const FGearCodeRef& Event)
		{
			const uint8 Kind = ReadU8(Models, Event + 2);
			const uint8 Which = ReadU8(Models, Event + 3);
			const bool bOn = ReadU8(Models, Event + 4) != 0;
			switch (Kind)
			{
			case 2:
				// A light switched off stays off; the port has no point lights to switch on.
				if (bOn || Which >= PointLights)
				{
					Models.Fail("a gear's timeline sets a point light, which the port does not light");
				}
				break;
			case 3:
			case 4:
				// A trail is only ever stopped (the one start has no caller), so stopping one changes nothing.
				if (Which >= Model.Trails)
				{
					Models.Fail("a gear's timeline stops trail " + std::to_string(Which) + ", which it does not have");
				}
				break;
			case 7:
				if (FGearBone* Bone = BoneAt(Models, Model, ReadU8(Models, Event + 4)))
				{
					Bone->bShown = ReadU8(Models, Event + 5) & 1;
				}
				break;
			case 8:
				for (int32 Slot = 0; Slot < 8; ++Slot)
				{
					if (((Model.PlayMask >> Slot) & 1) != 0 && Models.Model(Slot) != nullptr && ReadU8(Models, Event + 5) != 0)
					{
						Models.Fail("a gear's timeline starts animations on the play mask's models, choosing how by a word the original never writes");
						break;
					}
				}
				break;
			case 9:
				if (bOn && Which < Model.TextureAnimations && (ReadU8(Models, Event + 6) & RelativeToModel) != 0 && Model.TextureX < 0)
				{
					// A texture animation placed relative to a model without a texture place is not started nor passed.
					return false;
				}
				if ((!bOn && Which >= Model.TextureAnimations) || (bOn && Which < Model.TextureAnimations))
				{
					Models.Fail("a gear's timeline runs a texture animation, which the port does not have");
				}
				break;
			default:
				break;
			}
			return EventBytes(Kind, bOn) != 0;
		}
	}

	void StartTimeline(FGearModels& Models, FGearModel& Model, const FGearRecord& Record, int32 Loop)
	{
		(void)Models;
		if (Record.Events == 0)
		{
			Model.TimelineTick = -1;
			return;
		}
		Model.TimelineTick = 0;
		Model.TimelineLoop = Loop != 0 ? Record.Length : int16{-1};
		Model.TimelineDone = 0;
		Model.TimelineEvents = Record.Events;
		Model.NextEvent = Record.At + static_cast<int32>(Record.EventOffset);
		Model.FirstEvent = Model.NextEvent;
	}

	void StepTimeline(FGearModels& Models, FGearModel& Model)
	{
		if (Model.TimelineTick < 0)
		{
			return;
		}
		while (Model.TimelineDone < Model.TimelineEvents && !Models.Faulted())
		{
			const FGearCodeRef Event = Model.NextEvent;
			if (ReadS16(Models, Event) != Model.TimelineTick)
			{
				break;
			}
			if (RunEvent(Models, Model, Event))
			{
				Model.NextEvent = Event + static_cast<int32>(EventBytes(ReadU8(Models, Event + 2), ReadU8(Models, Event + 4) != 0));
			}
			Model.TimelineDone = static_cast<int16>(Model.TimelineDone + 1);
		}
		Model.TimelineTick = static_cast<int16>(Model.TimelineTick + 1);
		if (Model.TimelineLoop >= 0 && Model.TimelineLoop <= Model.TimelineTick)
		{
			Model.TimelineTick = 0;
			Model.TimelineDone = 0;
			Model.NextEvent = Model.FirstEvent;
		}
	}
}
