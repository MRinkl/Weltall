#include "Battle/Show/BattleShowObjects.h"

#include "Battle/BattleContext.h"
#include "Battle/Show/BattleStageFiles.h"
#include "Graphics/SpriteSystem.h"
#include "BattleObjectMachine.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		/** A sequence queued while one runs: the queue's room. */
		constexpr uint8 QueueFull = 5;
		constexpr int32 FirstExtensionSequence = 0x50;
		/** The record's results start at 18h; a slot past the eleven writes the bytes after them. */
		constexpr std::size_t ResultsOffset = 0x18;
	}

	FBattleObjects::FBattleObjects() : FGearEngine("the battle's objects")
	{
	}

	FBattleObjects::~FBattleObjects() = default;

	FGearMotionPack* FBattleObjects::HoldPack(FGearMotionPack Pack)
	{
		HeldPacks.push_back(std::make_unique<FGearMotionPack>(std::move(Pack)));
		HeldPacks.back()->Slot = static_cast<int32>(HeldPacks.size() + HeldExtensions.size() - 1);
		return HeldPacks.back().get();
	}

	FBattleSequenceExtension* FBattleObjects::HoldExtension(FBattleSequenceExtension Extension)
	{
		HeldExtensions.push_back(std::make_unique<FBattleSequenceExtension>(std::move(Extension)));
		HeldExtensions.back()->Pack.Slot = static_cast<int32>(HeldPacks.size() + HeldExtensions.size() - 1);
		return HeldExtensions.back().get();
	}

	void FBattleObjects::ReleasePack(const FGearMotionPack* Pack)
	{
		if (Pack != nullptr)
		{
			ReleasedPacks.push_back(Pack);
		}
	}

	bool FBattleObjects::IsReleased(const FGearMotionPack* Pack) const
	{
		return std::find(ReleasedPacks.begin(), ReleasedPacks.end(), Pack) != ReleasedPacks.end();
	}

	bool FBattleObjects::HoldsPack(const FGearMotionPack* Pack) const
	{
		// An extension reads on once let go: its block keeps its bytes for the bone still streaming from it.
		for (const std::unique_ptr<FBattleSequenceExtension>& Held : HeldExtensions)
		{
			if (&Held->Pack == Pack)
			{
				return true;
			}
		}
		if (IsReleased(Pack))
		{
			return false;
		}
		for (const std::unique_ptr<FGearMotionPack>& Held : HeldPacks)
		{
			if (Held.get() == Pack)
			{
				return true;
			}
		}
		return false;
	}

	void FBattleObjects::Init(const FBattleStageRecord& Record)
	{
		Stage = &Record;
		Accumulator = 0;
		WindPhase = 0;
		bSignalled = false;
		bShadows = false;
		PulsePhase = 0;
		bObjectsVisible = true;
		const int16 Tracks = Record.TrackPoolSize();
		if (Tracks > 0)
		{
			Channels.Init(Tracks, false);
		}
		else
		{
			Fail("the stage asks for a track pool of " + std::to_string(Tracks));
		}
		// The pool keeps a quad more than it hands out: where a full pool's requests draw.
		TrailSegments.assign(static_cast<std::size_t>(std::max<int32>(Record.TrailPoolSize() + 1, 0)), FBattleTrailSegment{});
		TrailLowest = 0;
		ModelCamera.Tracks.fill(FGearBone::NoChannel);
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			Slots[static_cast<std::size_t>(Slot)].bPresent = false;
		}
		for (FBattlePointLight& Light : PointLights)
		{
			Light.Strength = 0;
		}
		// The stage's light: its direction in the light matrix's first row, its colour in the colour matrix's first column.
		const FGteVector Direction = Record.LightDirection();
		const FGteVector Colour = Record.LightColour();
		LightMatrix = FGteMatrix{};
		LightMatrix.M[0] = {Direction[0], Direction[1], Direction[2]};
		ColourMatrix = FGteMatrix{};
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			ColourMatrix.M[Row][0] = Colour[Row];
		}
		if (Gte != nullptr)
		{
			// The stage's setup leaves the GTE its colour matrix and its ambient light.
			const std::array<uint8, 3> Ambient = Record.AmbientColour();
			Gte->ColorMatrix = ColourMatrix;
			Gte->BackColor = {Ambient[0] * 16, Ambient[1] * 16, Ambient[2] * 16};
		}
	}

	int32 FBattleObjects::Height(int32 Slot) const
	{
		const FBattleObject* Found = Object(Slot);
		if (Found == nullptr)
		{
			return 0;
		}
		return Found->Height * ((Found->Scale * Found->Bones[0].Scale[1]) >> 12) >> 12;
	}

	int32 FBattleObjects::Radius(int32 Slot) const
	{
		const FBattleObject* Found = Object(Slot);
		if (Found == nullptr)
		{
			return 0;
		}
		const bool bFromX = (Found->Flags & BattleObjectFlags::RadiusFromX) != 0;
		const int32 Size = bFromX ? Found->RadiusX : Found->RadiusZ;
		const int32 BoneScale = bFromX ? Found->Bones[0].Scale[0] : Found->Bones[0].Scale[2];
		return Size * ((Found->Scale * BoneScale) >> 12) >> 12;
	}

	bool FBattleObjects::Root(int32 Slot, std::array<int32, 3>& Translation)
	{
		const FBattleObject* Found = Object(Slot);
		if (Found == nullptr)
		{
			return false;
		}
		Translation = Found->Bones[0].Position;
		return true;
	}

	void FBattleObjects::SetStance(int32 Slot, uint8 NewStance)
	{
		if (FBattleObject* Found = Object(Slot))
		{
			Found->Stance = NewStance;
		}
	}

	void FBattleObjects::Swap(int32 Hidden, int32 Shown)
	{
		FBattleObject* ToHide = Object(Hidden);
		FBattleObject* ToShow = Object(Shown);
		if (ToHide == nullptr || ToShow == nullptr)
		{
			Fail("objects " + std::to_string(Hidden) + " and " + std::to_string(Shown) + " swap places, one of them missing");
			return;
		}
		ToHide->bVisible = 0;
		ToShow->bVisible = 1;
		std::swap(Slots[static_cast<std::size_t>(Hidden)], Slots[static_cast<std::size_t>(Shown)]);
		Services->ObjectsSwapped(Hidden, Shown);
	}

	int32 FBattleObjects::PlaceOf(const FBattleObject& Object) const
	{
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			if (&Slots[static_cast<std::size_t>(Slot)].Object == &Object)
			{
				return Slot;
			}
		}
		return Object.Slot;
	}

	bool FBattleObjects::IsPlaying()
	{
		for (int32 Slot = 0; Slot < CombatantSlots; ++Slot)
		{
			const FBattleObject* Found = Object(Slot);
			if (Found != nullptr && Found->Playing != 0)
			{
				return true;
			}
		}
		return false;
	}

	bool FBattleObjects::HoldsExtension()
	{
		for (int32 Slot = 0; Slot < CombatantSlots; ++Slot)
		{
			const FBattleObject* Found = Object(Slot);
			if (Found != nullptr && Found->Extension != nullptr)
			{
				return true;
			}
		}
		return false;
	}

	void FBattleObjects::FreeExtensions()
	{
		for (int32 Slot = 0; Slot < CombatantSlots; ++Slot)
		{
			if (FBattleObject* Found = Object(Slot); Found != nullptr && Found->Extension != nullptr)
			{
				BattleObjectMachine::FreeExtension(*this, *Found);
			}
		}
	}

	bool FBattleObjects::TakeIdleWait()
	{
		const bool bAsked = bIdleWaitAsked;
		bIdleWaitAsked = false;
		return bAsked;
	}

	void FBattleObjects::StartSequence(int32 Slot, int32 OwnerSlot, int32 Sequence)
	{
		FBattleObject* Target = Object(Slot);
		const FBattleObject* Owner = Object(OwnerSlot);
		if (Target == nullptr || Owner == nullptr || Faulted())
		{
			return;
		}
		if (Target->Queue != 0)
		{
			if (Target->Queue < QueueFull)
			{
				++Target->Queue;
			}
			Target->QueuedOwners[static_cast<std::size_t>(Target->Queue - 2)] = Owner->Slot;
			Target->QueuedSequences[static_cast<std::size_t>(Target->Queue - 2)] = static_cast<uint8>(Sequence);
			return;
		}
		std::optional<uint32> Script;
		FGearMotionPack* Pack = nullptr;
		if (Sequence < FirstExtensionSequence)
		{
			Pack = Owner->Motion;
			Script = Pack != nullptr ? Pack->Script(Sequence) : std::nullopt;
		}
		else if (Owner->Extension != nullptr && Owner->bExtensionInstalled)
		{
			// The extension's tables are the original's to read only once op 05's install has relocated them.
			Pack = &Owner->Extension->Pack;
			Script = Pack->Script(Sequence - FirstExtensionSequence);
		}
		if (!Script || !HoldsPack(Pack))
		{
			Fail("object " + std::to_string(Slot) + " starts sequence " + std::to_string(Sequence) + " of object " + std::to_string(OwnerSlot) + "'s, past its tables ("
				+ std::to_string(Pack != nullptr && Pack->ScriptEntries >= 2 ? Pack->ScriptEntries - 2 : 0) + " sequences)");
			return;
		}
		if (*Script == Pack->Scripts)
		{
			// An empty entry points at the table itself once relocated: the original runs its words, pointers among them.
			Fail("object " + std::to_string(Slot) + " starts sequence " + std::to_string(Sequence) + ", which its table leaves empty");
			return;
		}
		Target->Pc = {Pack, *Script};
		Target->Loops = 0;
		Target->WaitTicks = 0;
		Target->OnNear = {};
		Target->OnTimer = {};
		Target->OnLanding = {};
		Target->bExtensionPending = false;
		Target->Targets = PlayTargets;
		BattleObjectMachine::RunSequence(*this, Slot, -1, 1, 0);
	}

	void FBattleObjects::Perform(int32 Slot, uint16 Targets, int32 Sequence)
	{
		Performer = static_cast<uint16>(Slot);
		PlayTargets = Targets;
		FBattleObject* Found = Object(Slot & 0xFFFF);
		if (Found == nullptr)
		{
			// The original clears the signal of whatever the empty slot's pointer reaches.
			Fail("a sequence is performed by the empty slot " + std::to_string(Slot));
			return;
		}
		Found->Signal = 0;
		StartSequence(Slot, Slot, Sequence);
	}

	void FBattleObjects::PerformSignalled(int32 Slot, uint16 Targets, int32 Sequence)
	{
		Performer = static_cast<uint16>(Slot);
		PlayTargets = Targets;
		bSignalled = true;
		FBattleObject* Found = Object(Slot & 0xFFFF);
		if (Found == nullptr)
		{
			Fail("a signalled sequence is performed by the empty slot " + std::to_string(Slot));
			return;
		}
		Found->Signal = 1;
		const uint32 Target = BattleObjectMachine::FirstTarget(*this);
		Services->BeginEventPerformance(Slot & 0xFFFF, static_cast<int32>(Target));
		// The first record's result for the first target is a plain hit; past the eleven it lands on the record's bytes after the results.
		FBattlePerformance& Record = Context->Performances[0];
		const std::size_t Offset = ResultsOffset + Target;
		if (Target < Record.Results.size())
		{
			Record.Results[Target] = 0;
		}
		else if (Offset == 0x23)
		{
			Record.Actor = 0;
		}
		else
		{
			const std::size_t Total = (Offset - 0x24) / 2;
			const uint32 Kept = Offset % 2 == 0 ? 0xFFFFFF00u : 0xFFFF00FFu;
			Record.Totals[Total] = static_cast<uint16>(Record.Totals[Total] & Kept);
			// The shown total loses the same byte, and goes with the total when that is gone.
			Record.WholeTotals[Total] = Record.Totals[Total] == 0 ? 0 : Record.WholeTotals[Total] & Kept;
		}
		StartSequence(Slot, Slot, Sequence);
	}

	void FBattleObjects::React(int32 Slot, uint16 FromSlots, int32 Sequence)
	{
		const uint16 SavedPerformer = Performer;
		const uint16 SavedTargets = PlayTargets;
		Performer = static_cast<uint16>(Slot);
		PlayTargets = FromSlots;
		FBattleObject* Found = Object(Slot);
		if (Found == nullptr)
		{
			Fail("an empty slot " + std::to_string(Slot) + " reacts");
			return;
		}
		Found->Signal = 0;
		if (static_cast<uint32>(Slot) != BattleObjectMachine::FirstTarget(*this))
		{
			StartSequence(Slot, Slot, Sequence);
		}
		Performer = SavedPerformer;
		PlayTargets = SavedTargets;
	}

	void FBattleObjects::ReactFrom(int32 OwnerSlot, int32 Slot, uint16 From, int32 Sequence)
	{
		Performer = static_cast<uint16>(Slot);
		PlayTargets = From;
		FBattleObject* Found = Object(Slot);
		if (Found == nullptr)
		{
			Fail("an empty slot " + std::to_string(Slot) + " reacts");
			return;
		}
		Found->Signal = 0;
		if (static_cast<uint32>(Slot) != BattleObjectMachine::FirstTarget(*this))
		{
			StartSequence(Slot, OwnerSlot, Sequence);
		}
	}

	void FBattleObjects::StartRemovingOtherParty(int32 Kept)
	{
		for (int32 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
		{
			if (Slot != Kept)
			{
				StartSequence(Slot, Slot, 5);
			}
		}
	}

	bool FBattleObjects::OtherPartyLeaving(int32 Kept) const
	{
		for (int32 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
		{
			const FBattleObject* Found = Slot != Kept ? Object(Slot) : nullptr;
			if (Found != nullptr && (Found->bVisible != 0 || Found->Playing != 0))
			{
				return true;
			}
		}
		return false;
	}

	void FBattleObjects::FreeOtherParty(int32 Kept)
	{
		for (int32 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
		{
			if (Slot != Kept)
			{
				Free(Slot);
			}
		}
	}
}
