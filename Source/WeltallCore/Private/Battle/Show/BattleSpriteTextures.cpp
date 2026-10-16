#include "Battle/Show/BattleSpriteTextures.h"

#include "Battle/Show/BattleSceneGraphics.h"
#include "Battle/Show/BattleSceneModels.h"
#include "Battle/Show/BattleShowVram.h"
#include "Battle/Show/BattleSpriteFiles.h"
#include "Graphics/SpriteCells.h"
#include "Graphics/SpriteFile.h"
#include "Graphics/SpriteSystem.h"
#include "Graphics/VramStream.h"

#include <cstdio>
#include <memory>

namespace Weltall
{
	namespace
	{
		/** The setup's places: the party's sprites along row 1C0h from 100h, each combatant's CLUT on row 1C0h + its slot. */
		constexpr int32 PartyTextureX = 0x100;
		constexpr int32 PartyTextureY = 0x1C0;
		constexpr int32 SlotClutRows = 0x1C0;
		constexpr int32 FirstEnemySlot = 3;

		/** The common effects' and an effect's resources: textures and CLUT row, which their spawned sprites draw from. */
		constexpr int32 CommonEffectsTextureX = 0x380;
		constexpr int32 CommonEffectsClutY = 0x1D1;
		constexpr int32 EffectTextureX = 0x380;
		constexpr int32 EffectTextureY = 0x100;
		constexpr int32 EffectClutY = 0x1F4;

		/**
		 * Every cell of a file's frames under each of its palette sets, at Place, on the sheets Sheet names by palette set:
		 * each set in a VRAM of its own holding Base first, the sprite uploading its palette set or not (bOwnPalette).
		 */
		template <typename TSheetName>
		void VisitCells(const FSpriteFile& File, const FSpriteFilePlace& Place, TSheetName&& SheetName, const FBattleSpriteCellVisit& Visit,
			const FVram* Base = nullptr, bool bOwnPalette = true)
		{
			for (int32 Set = 0; Set < SpritePaletteSets(File); ++Set)
			{
				const auto Vram = Base != nullptr ? std::make_unique<FVram>(*Base) : std::make_unique<FVram>();
				const std::string Sheet = SheetName(Set);
				ForEachSpriteFrame(File, Place, Set, *Vram, [&Visit, &Vram, &Sheet](std::span<const FSpritePart> Parts)
				{
					for (const FSpritePart& Part : Parts)
					{
						Visit(*Vram, SpritePartCell(Part), Sheet);
					}
				}, bOwnPalette);
			}
		}

		std::string CellsSheet(int32 Set)
		{
			return "cells-" + std::to_string(Set);
		}
	}

	std::optional<FError> ForEachBattleCharacterCell(const FBattleSpriteFiles& Files, int32 Character, const FBattleSpriteCellVisit& Visit)
	{
		const TResult<FBattleSpriteFile> Sprite = Files.ReadCharacterSprite(Character);
		if (!Sprite)
		{
			return Sprite.Error();
		}
		const TResult<FBattleSpriteFile> Attacks = Files.ReadAttacks(Character);
		if (!Attacks)
		{
			return Attacks.Error();
		}
		const FSpriteFilePlace Own{0, SlotClutRows, PartyTextureX + Files.FirstTextureColumn(), PartyTextureY};
		VisitCells(Sprite->Sprite(), Own, CellsSheet, Visit);
		const FSpriteFilePlace Streamed{0, SlotClutRows, FSpriteSystem::BattleStreamX, FSpriteSystem::BattleStreamY};
		VisitCells(Attacks->Sprite(), Attacks->Sprite().IsResident(Attacks->Sprite().Tables()) ? Own : Streamed,
			[](int32 Set) { return "attacks-cells-" + std::to_string(Set); }, Visit);
		return std::nullopt;
	}

	std::optional<FError> ForEachBattleSceneSpriteCell(const FBattleSceneGraphics& Scene, const FBattleSpriteCellVisit& Visit)
	{
		const std::optional<std::vector<int16>> Columns = BattleSceneColumns(Scene);
		if (!Columns)
		{
			return MakeError("the scene's image sets run past its file");
		}
		// One VRAM for the scene: its sets lie side by side as the setup leaves them, and an entry may draw from an earlier one's.
		const auto Vram = std::make_unique<FVram>();
		FBattleSceneImageSets Sets;
		const std::vector<FBattleSceneEntry>& Entries = Scene.Entries();
		for (std::size_t Entry = 0; Entry < Entries.size() && Entry < Columns->size(); ++Entry)
		{
			const FBattleSceneEntry& Found = Entries[Entry];
			if (!Found.IsSprite())
			{
				continue;
			}
			const std::optional<int16> Column = Sets.Place(Scene, Entry, (*Columns)[Entry], *Vram);
			if (!Column || Found.Data <= 7 || Found.Data >= Scene.KeptEnd())
			{
				return MakeError("sprite entry " + std::to_string(Entry) + " cannot be placed as the setup places it");
			}
			const TResult<FBattleSpriteFile> File = FBattleSpriteFile::Parse(Scene.Bytes().subspan(Found.Data, Scene.KeptEnd() - Found.Data));
			if (!File)
			{
				return File.Error();
			}
			// Each type's CLUT on a row of its own, as each takes its first slot's.
			const FSpriteFilePlace Place{0, SlotClutRows + FirstEnemySlot + static_cast<int32>(Entry), *Column, FBattleSceneImageSets::TextureY};
			char Sheet[32];
			std::snprintf(Sheet, sizeof(Sheet), "sprite-%02d-cells-%d", static_cast<int32>(Entry), Found.PaletteSet);
			ForEachSpriteFrame(File->Sprite(), Place, Found.PaletteSet, *Vram, [&Visit, &Vram, &Sheet](std::span<const FSpritePart> Parts)
			{
				for (const FSpritePart& Part : Parts)
				{
					Visit(*Vram, SpritePartCell(Part), Sheet);
				}
			});
		}
		return std::nullopt;
	}

	std::optional<FError> ForEachBattleCommonEffectCell(const FBattleSpriteFiles& Files, const FBattleSpriteCellVisit& Visit)
	{
		const TResult<std::vector<uint8>> Images = Files.ReadCommonImages();
		if (!Images)
		{
			return Images.Error();
		}
		const TResult<FBattleSpriteFile> Common = Files.ReadCommon();
		if (!Common)
		{
			return Common.Error();
		}
		const auto Base = std::make_unique<FVram>();
		if (!UploadBattleImageList(*Base, *Images, 0))
		{
			return MakeError("the common effects' images run past their file");
		}
		const FSpriteFilePlace Place{0, CommonEffectsClutY, CommonEffectsTextureX, 0};
		VisitCells(Common->Sprite(), Place, CellsSheet, Visit, Base.get(), false);
		return std::nullopt;
	}

	std::optional<FError> ForEachBattleEffectCell(const FBattleSpriteFiles& Files, int32 Effect, const FBattleSpriteCellVisit& Visit)
	{
		if (Files.IsEffectStandIn(Effect))
		{
			return std::nullopt;
		}
		const TResult<FBattleSpriteFile> File = Files.ReadEffect(Effect);
		if (!File)
		{
			return File.Error();
		}
		const TResult<std::vector<uint8>> Stream = Files.ReadEffectStream(Effect);
		if (!Stream)
		{
			return Stream.Error();
		}
		const auto Base = std::make_unique<FVram>();
		if (!Stream->empty())
		{
			const TResult<std::vector<FVramImage>> Images = DecodeVramStream(*Stream);
			if (!Images)
			{
				return Images.Error();
			}
			for (const FVramImage& Image : *Images)
			{
				Base->Upload(Image);
			}
		}
		const FSpriteFilePlace Resource{0, EffectClutY, EffectTextureX, EffectTextureY};
		const FSpriteFile& Sprite = File->Sprite();
		if (Sprite.IsResident(Sprite.Tables()))
		{
			VisitCells(Sprite, Resource, CellsSheet, Visit, Base.get(), false);
		}
		else
		{
			// The actor plays it as its alternate file: streamed to the battle's place, through the actor's CLUT row.
			VisitCells(Sprite, {0, SlotClutRows, FSpriteSystem::BattleStreamX, FSpriteSystem::BattleStreamY}, CellsSheet, Visit, Base.get());
		}
		for (std::size_t Section = 0; Section < File->Sections().size(); ++Section)
		{
			if (const FSpriteFile* Installed = File->SectionSprite(Section))
			{
				VisitCells(*Installed, Resource, [Section](int32 Set) { return "section-" + std::to_string(Section) + "-cells-" + std::to_string(Set); }, Visit,
					Base.get(), false);
			}
		}
		return std::nullopt;
	}
}
