#include "Battle/Show/BattleSceneModels.h"

#include "Battle/Show/BattleSceneGraphics.h"
#include "Battle/Show/BattleShowObjects.h"

namespace Weltall
{
	namespace
	{
		constexpr int32 EnemyTextureX = 0x140;
		constexpr int32 SetColumns = 0x40;
		constexpr int32 LastSetColumn = 0x2C0;
		constexpr int16 EnemyTextureY = 0x100;
		constexpr int32 ClutRow = 0x1C0;
		constexpr int32 PartySlots = 3;
		constexpr int32 SlotCount = 11;
		/** The types an enemy's slot can have that name an entry of the scene. */
		constexpr uint8 SceneTypes = 8;
		/** The flags an object is made with for a model enemy: sequences it does not own; one of several parts, a later part, the last. */
		constexpr uint32 ModelFlag = 0x80;
		constexpr uint32 SeveralParts = 2;
		constexpr uint32 LaterPart = 7;
		constexpr uint32 LastPart = 5;
	}

	std::optional<std::vector<int16>> BattleSceneColumns(const FBattleSceneGraphics& Scene)
	{
		std::vector<int16> Columns;
		int32 Column = Scene.FirstColumn() * SetColumns + EnemyTextureX;
		for (const FBattleSceneEntry& Entry : Scene.Entries())
		{
			Columns.push_back(static_cast<int16>(Column));
			if (!Entry.HasOwnImages())
			{
				continue;
			}
			if (!Entry.IsSprite())
			{
				Column += SetColumns;
				continue;
			}
			TResult<std::vector<FBattleImageSetImage>> Images = ReadBattleImageSet(Scene.Bytes(), Entry.Images);
			if (!Images)
			{
				return std::nullopt;
			}
			Column += static_cast<int32>(Images->size()) * SetColumns;
			if (Column > LastSetColumn)
			{
				Column = 0;
			}
		}
		return Columns;
	}

	FBattleSceneModels CreateBattleSceneModels(FBattleObjects& Objects, const FBattleSceneGraphics& Scene, std::size_t Index, int16 Column,
		std::array<uint8, 11>& Types)
	{
		FBattleSceneModels Made;
		const std::vector<FBattleSceneEntry>& Entries = Scene.Entries();
		const FBattleSceneEntry& Entry = Entries[Index];
		// The model's parts: the slots of this type, and those of the later entries that are parts of it.
		int32 Parts = 0;
		for (int32 Slot = PartySlots; Slot < SlotCount; ++Slot)
		{
			Parts += Types[static_cast<std::size_t>(Slot)] == Index ? 1 : 0;
		}
		for (std::size_t Later = Index + 1; Later < Entries.size(); ++Later)
		{
			if (!Entries[Later].IsSprite() && Entries[Later].Images == Index)
			{
				for (int32 Slot = PartySlots; Slot < SlotCount; ++Slot)
				{
					if (Types[static_cast<std::size_t>(Slot)] == Later)
					{
						Types[static_cast<std::size_t>(Slot)] = static_cast<uint8>(Index);
						++Parts;
					}
				}
			}
		}
		const std::span<const uint8> Bytes = Scene.Bytes();
		for (int32 Slot = PartySlots; Slot < SlotCount; ++Slot)
		{
			const uint8 Type = Types[static_cast<std::size_t>(Slot)];
			if (Type >= SceneTypes || Type != Index)
			{
				continue;
			}
			Made.bOutsideFile = Entry.Data >= Scene.KeptEnd() || Entry.Images >= Bytes.size();
			if (Made.bOutsideFile)
			{
				return Made;
			}
			uint32 Flags = Parts >= 2 ? SeveralParts : 0;
			if (Made.Count != 0)
			{
				Flags = Parts == Made.Count + 1 ? LastPart : LaterPart;
			}
			Objects.CreateModel(Slot, Flags | ModelFlag, Bytes.subspan(Entry.Data, Scene.KeptEnd() - Entry.Data), Bytes.subspan(Entry.Images), Column,
				EnemyTextureY, Slot - Made.Count + ClutRow);
			Made.Slots[static_cast<std::size_t>(Made.Count++)] = static_cast<uint8>(Slot);
		}
		return Made;
	}
}
