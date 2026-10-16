#include "Battle/Show/BattleModelTextures.h"
#include "Battle/Show/BattleObjectFiles.h"
#include "Battle/Show/BattleSceneGraphics.h"
#include "Field/FieldGearDrawing.h"
#include "Field/FieldGearStandIns.h"
#include "Field/FieldGears.h"
#include "Field/FieldPlay.h"
#include "Field/FieldPresenter.h"
#include "Gears/GearFiles.h"
#include "Gears/GearTextureRegions.h"
#include "Graphics/Texture.h"
#include "Graphics/TextureRegion.h"
#include "Support/FieldPlaySession.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <set>
#include <span>
#include <vector>

using namespace Weltall;

// The gears a field map draws in 3D, its own and the stand-ins for its gears' sprites, name their pictures by the keys the
// texture export gives them.

namespace
{
	/** The mountain path, where four of the field's own gears stand. */
	constexpr int32 MountainPathMap = 16;
	constexpr int32 MountainPathGears = 4;
	/** A hangar the party walks on foot or in its gears (L2 and R2 together board them), other gears parked in it. */
	constexpr int32 HangarMap = 260;
	constexpr int32 BoardAt = 40;
	constexpr int32 BoardFor = 4;
	constexpr int32 SettledTicks = 40;

	/** A model's picture's key as its load left it, else as VRAM holds it. */
	uint64 SeedKey(const FVram& Vram, const FTexturePictureSeed& Seed)
	{
		if (Seed.Key != 0)
		{
			return Seed.Key;
		}
		return Seed.Palette ? TextureRegionKeyWith(ReadTextureRegionTexels(Vram, Seed.Region), *Seed.Palette) : TextureRegionKey(Vram, Seed.Region);
	}

	/** The keys as VRAM holds the pictures, through their loaded colours: what a seed's loaded key stands for while nothing has changed them. */
	std::set<uint64> DrawnKeys(const FVram& Vram, std::span<const FTexturePictureSeed> Seeds)
	{
		std::set<uint64> Keys;
		for (const FTexturePictureSeed& Seed : Seeds)
		{
			Keys.insert(Seed.Palette ? TextureRegionKeyWith(ReadTextureRegionTexels(Vram, Seed.Region), *Seed.Palette) : TextureRegionKey(Vram, Seed.Region));
		}
		return Keys;
	}

	std::set<uint64> SeedKeys(const FVram& Vram, std::span<const FTexturePictureSeed> Seeds)
	{
		std::set<uint64> Keys;
		for (const FTexturePictureSeed& Seed : Seeds)
		{
			Keys.insert(SeedKey(Vram, Seed));
		}
		return Keys;
	}

	/** The keys of every picture the texture export takes from the field's gear models. */
	std::set<uint64> ExportedFieldGearKeys(const FCdImage& Image, const FGameFileIndex& Index)
	{
		std::set<uint64> Keys;
		const FGearTexturePlace Place = FieldGearTexturePlace(FFieldGears{}, 0);
		for (int32 GearModel = 0; GearModel < FGearFileNumbers::ModelCount; ++GearModel)
		{
			const TResult<FGearFiles> Files = ReadGearFiles(Image, Index, GearModel);
			REQUIRE(Files);
			FVram Vram;
			const std::set<uint64> Made = SeedKeys(Vram, UploadGearModelTextures(Vram, Files->Model, &Place));
			Keys.insert(Made.begin(), Made.end());
		}
		return Keys;
	}

	/** The keys of every picture the texture export takes from Battle's models: its gears with their variants, and each scene's model enemies. */
	std::set<uint64> ExportedBattleModelKeys(const FCdImage& Image, const FGameFileIndex& Index)
	{
		const TResult<FBattleObjectTables> Tables = FBattleObjectTables::Load(Image, Index);
		REQUIRE(Tables);
		std::set<uint64> Keys;
		const auto Take = [&](EBattleModelSource Source, int32 Number, int32 Variant)
		{
			FVram Vram;
			const auto Seeds = LoadBattleModelTextures(Image, Index, *Tables, Source, Number, Variant, Vram);
			REQUIRE(Seeds);
			const std::set<uint64> Made = SeedKeys(Vram, *Seeds);
			Keys.insert(Made.begin(), Made.end());
		};
		for (int32 Gear = 0; Gear < FBattleObjectTables::GearCount; ++Gear)
		{
			for (int32 Variant = 0; Variant <= Tables->Gears[static_cast<std::size_t>(Gear)].LastVariant; ++Variant)
			{
				Take(EBattleModelSource::Gear, Gear, Variant);
			}
		}
		for (const int32 Scene : FBattleSceneGraphics::Scenes(Index))
		{
			Take(EBattleModelSource::SceneEnemies, Scene, 0);
		}
		return Keys;
	}

	/** How many of the textured polygons a frame's gears lay out read, and how many of those read texels inside a region of its seeds. */
	struct FCoverage
	{
		int32 Textured = 0;
		int32 Covered = 0;
	};

	FCoverage SeedCoverage(const FFieldGearsFrame& Gears)
	{
		FCoverage Coverage;
		for (const FFieldGearLayoutItem& Item : Gears.Items)
		{
			if (Item.Part == nullptr)
			{
				continue;
			}
			for (std::size_t Number = 0; Number < Item.Part->Primitives.size(); ++Number)
			{
				const FMdlPrimitive& Primitive = Item.Part->Primitives[Number];
				if (!Primitive.Traits().bTextured)
				{
					continue;
				}
				const FMdlTextureState& Texture = Item.Packets->Textures[Number];
				const uint32 View = Item.bPortRows
					? PackTextureView(FTexturePage::FromPortAttribute(Texture.TexturePage), FClutPosition::FromPortAttribute(Texture.Clut))
					: PackTextureView(FTexturePage::FromAttribute(Texture.TexturePage), FClutPosition::FromAttribute(Texture.Clut));
				const std::span<const FMdlUv> Uvs(Primitive.Uvs.data(), Primitive.Traits().Corners);
				++Coverage.Textured;
				Coverage.Covered += std::any_of(Gears.TextureSeeds.begin(), Gears.TextureSeeds.end(), [View, Uvs](const FTexturePictureSeed& Seed)
				{
					return Seed.Region.View == View
						&& std::all_of(Uvs.begin(), Uvs.end(), [&Seed](const FMdlUv& Uv) { return Seed.Region.Contains(Uv.U, Uv.V); });
				}) ? 1 : 0;
			}
		}
		return Coverage;
	}
}

TEST_CASE("The field's own gears draw from regions whose pictures the texture export keys", "[Field][Content][TextureRegion]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Testing::EnterFieldMap(*Disc, MountainPathMap);
	Session->Run(SettledTicks);
	const FEventContext& Context = Session->Play->Field().Context();
	REQUIRE(Context.Gears.Models);
	REQUIRE(Context.Gears.Count == MountainPathGears);
	const FFieldGearsFrame& Gears = Session->Frame.Gears;
	std::size_t Seeds = 0;
	for (int32 Slot = 0; Slot < MountainPathGears; ++Slot)
	{
		const FGearModel* Model = Context.Gears.Models->Model(Slot);
		REQUIRE(Model != nullptr);
		REQUIRE(Model->TextureSeeds);
		Seeds += Model->TextureSeeds->size();
	}
	CHECK(Gears.TextureSeeds.size() == Seeds);
	// Every textured polygon reads its texels inside a model's region, so none falls back to its whole page.
	const FCoverage Coverage = SeedCoverage(Gears);
	CHECK(Coverage.Textured > 100);
	CHECK(Coverage.Covered == Coverage.Textured);
	// Each slot puts its gear's textures elsewhere; the keys are those of the gears' models loaded at the first slot's place,
	// and the field's VRAM holds the pictures as they were loaded.
	const std::set<uint64> Drawn = SeedKeys(Session->Play->Drawing().Vram(), Gears.TextureSeeds);
	CHECK(DrawnKeys(Session->Play->Drawing().Vram(), Gears.TextureSeeds) == Drawn);
	const std::set<uint64> Exported = ExportedFieldGearKeys(Disc->Image(), *Session->Index);
	std::vector<uint64> Missing;
	std::set_difference(Drawn.begin(), Drawn.end(), Exported.begin(), Exported.end(), std::back_inserter(Missing));
	INFO("drawn " << Drawn.size() << ", exported " << Exported.size() << ", not exported " << Missing.size());
	CHECK(Drawn.size() > 10);
	CHECK(Missing.empty());
}

TEST_CASE("The stand-ins for a map's gear sprites draw from regions whose pictures the battle export keys", "[Field][Content][TextureRegion]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Testing::EnterFieldMap(*Disc, HangarMap);
	Session->Play->Drawing().SetGearStandIns(EFieldGearModels::Everywhere);
	const FFieldGearStandIns* StandIns = Session->Play->Drawing().GearStandIns();
	REQUIRE(StandIns != nullptr);
	for (int32 Tick = 0; Tick < BoardAt + BoardFor + SettledTicks; ++Tick)
	{
		std::vector<FPadState> Held(static_cast<std::size_t>(Session->Play->VblanksPerTick()));
		for (FPadState& Vblank : Held)
		{
			Vblank.Buttons = Tick >= BoardAt && Tick < BoardAt + BoardFor ? FPadState::Bit(EPadButton::L2) | FPadState::Bit(EPadButton::R2) : 0;
		}
		REQUIRE_FALSE(Session->Play->Tick(Held, EFieldCulling::InRange, Session->Frame));
	}
	const FEventContext& Context = Session->Play->Field().Context();
	REQUIRE(StandIns->IsShown(Context.LeaderActor));
	const FFieldGearsFrame& Gears = Session->Frame.Gears;
	const FCoverage Coverage = SeedCoverage(Gears);
	CHECK(Coverage.Textured > 100);
	CHECK(Coverage.Covered == Coverage.Textured);
	// Their textures lie in the port's rows, read with the high y bits; their keys are their battle models' (the party's gears,
	// and the other gears the hangar shows, which are scenes' model enemies).
	CHECK(std::all_of(Gears.TextureSeeds.begin(), Gears.TextureSeeds.end(), [](const FTexturePictureSeed& Seed)
	{
		return TextureViewPage(Seed.Region.View).Y >= FVram::Height;
	}));
	const std::set<uint64> Drawn = SeedKeys(Session->Play->Drawing().Vram(), Gears.TextureSeeds);
	CHECK(DrawnKeys(Session->Play->Drawing().Vram(), Gears.TextureSeeds) == Drawn);
	const std::set<uint64> Exported = ExportedBattleModelKeys(Disc->Image(), *Session->Index);
	std::vector<uint64> Missing;
	std::set_difference(Drawn.begin(), Drawn.end(), Exported.begin(), Exported.end(), std::back_inserter(Missing));
	INFO("drawn " << Drawn.size() << ", exported " << Exported.size() << ", not exported " << Missing.size());
	CHECK(Drawn.size() > 10);
	CHECK(Missing.empty());
}
