#include "Battle/Show/BattleModelTextures.h"

#include "Battle/Show/BattleObjectFiles.h"
#include "Battle/Show/BattleSceneGraphics.h"
#include "Battle/Show/BattleSceneModels.h"
#include "Battle/Show/BattleStageBackdrop.h"
#include "Battle/Show/BattleStageFiles.h"
#include "Gears/GearTextureRegions.h"

#include <string>

namespace Weltall
{
	namespace
	{
		constexpr int16 EnemyTextureY = 0x100;
		/** The CLUT rows of the party's gears and of the enemies' models, a slot each from here; the enemies' from the party's three on. */
		constexpr int16 SlotClutRows = 0x1C0;
		constexpr int16 FirstEnemySlot = 3;
		constexpr FGearTexturePlace SpecialPlace{0x300, 0x100, 0, 0x1DB};

		/**
		 * A model loaded as an object whose making put its textures at Place, unless the model's are absolute, and Over (a
		 * variant's images or its parts' textures) after it at OverPlace: what Create and CreateFromFiles upload and seed.
		 */
		void AddModel(FVram& Vram, const FGearModelPack& Pack, const FGearTexturePlace* Place, std::vector<FTexturePictureSeed>& Seeds,
			std::span<const FGearTextureBlock> Over = {}, const FGearTexturePlace* OverPlace = nullptr)
		{
			const std::vector<FTexturePictureSeed> Made = UploadGearModelTextures(Vram, Pack, Place, Over, OverPlace);
			Seeds.insert(Seeds.end(), Made.begin(), Made.end());
		}

		/** A model's files made into an object, a gear's variant after it: its images over the gear's, or its parts' model at the gear's place. */
		void AddFiles(FVram& Vram, const FBattleModelFiles& Files, const FGearTexturePlace& Place, std::vector<FTexturePictureSeed>& Regions)
		{
			const FBattleGearVariant* Variant = Files.Variant ? &*Files.Variant : nullptr;
			if (Variant != nullptr && Variant->Parts.empty())
			{
				AddModel(Vram, Files.Model, &Place, Regions, Variant->Images, &Place);
				return;
			}
			if (Variant == nullptr || !Variant->Model)
			{
				AddModel(Vram, Files.Model, &Place, Regions);
				return;
			}
			// The parts' textures go over the gear's: its pictures are named as both leave them.
			const FGearModelPack& Parts = *Variant->Model;
			const bool bPartsPlaced = (Parts.Header.Flags & GearModelFlags::AbsoluteTextures) == 0;
			AddModel(Vram, Files.Model, &Place, Regions, Parts.Textures, bPartsPlaced ? &Place : nullptr);
			AddModel(Vram, Parts, &Place, Regions);
		}
	}

	TResult<std::vector<FTexturePictureSeed>> LoadBattleModelTextures(const FCdImage& Image, const FGameFileIndex& Index, const FBattleObjectTables& Tables,
		EBattleModelSource Source, int32 Number, int32 Variant, FVram& Vram)
	{
		std::vector<FTexturePictureSeed> Regions;
		switch (Source)
		{
		case EBattleModelSource::Stage:
		{
			// The stage is a still object: its textures lie where its files say, and its backdrop draws from them too.
			TResult<FBattleStageFiles> Stage = FBattleStageFiles::Load(Image, Index, Number);
			if (!Stage)
			{
				return Stage.Error();
			}
			AddModel(Vram, Stage->Model.Pack, nullptr, Regions);
			const std::vector<FTexturePictureSeed> Backdrop = BattleBackdropTextureSeeds(MakeBattleStageBackdrop(Stage->Record), Stage->Model.Pack);
			Regions.insert(Regions.end(), Backdrop.begin(), Backdrop.end());
			break;
		}
		case EBattleModelSource::SceneEnemies:
		{
			TResult<FBattleSceneGraphics> Scene = FBattleSceneGraphics::Load(Image, Index, Number);
			if (!Scene)
			{
				return Scene.Error();
			}
			const std::optional<std::vector<int16>> Columns = BattleSceneColumns(*Scene);
			if (!Columns)
			{
				return MakeError("scene " + std::to_string(Number) + "'s image sets run past its file");
			}
			for (std::size_t Entry = 0; Entry < Scene->Entries().size() && Entry < Columns->size(); ++Entry)
			{
				const FBattleSceneEntry& Found = Scene->Entries()[Entry];
				if (Found.IsSprite() || !Found.HasOwnImages())
				{
					continue;
				}
				TResult<FBattleModelFiles> Files = BattleObjectFiles::SceneModel(*Scene, static_cast<int32>(Entry));
				if (!Files)
				{
					return Files.Error();
				}
				// Each type's CLUT on a row of its own, as each takes its first slot's.
				const FGearTexturePlace Place{(*Columns)[Entry], EnemyTextureY, 0, static_cast<int16>(SlotClutRows + FirstEnemySlot + Entry)};
				AddModel(Vram, Files->Model, &Place, Regions);
			}
			break;
		}
		case EBattleModelSource::Gear:
		{
			TResult<FBattleModelFiles> Files = BattleObjectFiles::ReadGear(Image, Index, Tables, Number, Variant);
			if (!Files)
			{
				return Files.Error();
			}
			FGearTexturePlace Place = Tables.GearPlaces[0];
			Place.ClutX = 0;
			Place.ClutY = SlotClutRows;
			AddFiles(Vram, *Files, Place, Regions);
			break;
		}
		case EBattleModelSource::Special:
		{
			TResult<FBattleModelFiles> Files = BattleObjectFiles::ReadSpecialModel(Image, Index, Number);
			if (!Files)
			{
				return Files.Error();
			}
			AddFiles(Vram, *Files, SpecialPlace, Regions);
			break;
		}
		}
		return Regions;
	}
}
