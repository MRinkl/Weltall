#pragma once

#include "GearEngineSteps.h"
#include "Gears/GearModels.h"
#include "Graphics/GteVectors.h"

#include <array>
#include <optional>

/** The field's and the gear shop's module's own steps, shared by its files, beside the engine's (GearEngineSteps.h). */
namespace Weltall::GearMachine
{
	/** Record Number (FEh and above: the header's; bit 7 of its byte into bFlag). */
	std::optional<FGearRecord> Record(FGearModels& Models, FGearModel& Model, uint32 Number, bool& bFlag);
	/** The same for a record that is played: none faults, where the original would read past its bank. */
	std::optional<FGearRecord> UsedRecord(FGearModels& Models, FGearModel& Model, uint32 Number, bool& bFlag);
	int32 Radius(const FGearModels& Models, int32 Slot);

	// The timeline of a tick (GearTimeline.cpp).
	void StartTimeline(FGearModels& Models, FGearModel& Model, const FGearRecord& Record, int32 Loop);
	void StepTimeline(FGearModels& Models, FGearModel& Model);

	// The scripts (GearScript.cpp, GearScriptOps*.cpp).
	/** The slot selectors FDh, FEh, FFh... as the scripts name models; Mask the slot's bit. */
	uint32 SelectModel(FGearModels& Models, const FGearModel& Model, uint32 Selector, uint16& Mask);
	uint32 FirstMaskSlot(const FGearModels& Models);
	/** Starts script Animation of Owner's scripts on the model in Slot (queued while its queue holds). */
	void StartScript(FGearModels& Models, int32 Slot, int32 OwnerSlot, int32 Animation);
	void RunScript(FGearModels& Models, int32 Slot, int32 Events, int32 Ticks);

	// Attachments (GearAttachments.cpp).
	void FollowAttachment(FGearModels& Models, FGearModel& Model);

	// A model's tick and what its drawing keeps (GearTick.cpp).
	/** The root pinned to the floor, the bones' matrices, Ticks steps of the channels and timeline, then the script. */
	void Update(FGearModels& Models, int32 Slot, int32 Ticks);
	/** The cloth's step and the frame the renderer draws. */
	void Draw(FGearModels& Models, int32 Slot);
	void ClearFrame(FGearModels& Models, int32 Slot);
}
