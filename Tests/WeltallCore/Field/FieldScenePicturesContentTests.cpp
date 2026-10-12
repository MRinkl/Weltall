#include "Field/FieldGears.h"
#include "Field/FieldMap.h"
#include "Field/FieldRuntime.h"
#include "Field/FieldScene.h"
#include "Field/FieldScenePictures.h"
#include "Gears/GearFiles.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/FieldPlaySession.h"
#include "Support/FieldTickSession.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Support/PortStory.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

using namespace Weltall;

// The scenes the maps stage before a picture and the scene that leaves Fei waiting just off the 4:3 screen, on the real
// maps, against what the original does; and map 73's Gears, whose textures load over the sprite images its archive puts
// where they lie.

namespace
{
	/** Map 73: Fei and Bart in their Gears before the desert's picture. */
	constexpr int32 DesertMap = 73;

	/** Map 50: the ride into the night, which a new game reaches with the story's progress at 39. Fei is actor 1. */
	constexpr int32 RideMap = 50;
	constexpr uint16 RideStory = 39;
	constexpr std::size_t RideFei = 1;
	/** Where the story's document has him wait, out of every view of the scene. */
	constexpr int32 OutOfViewX = 2730;
	constexpr int32 OutOfViewZ = 2831;
	/** A message is confirmed this many ticks after it opens; the scene leaves for map 54 well within the limit. */
	constexpr int32 ReadingTicks = 40;
	constexpr int32 RideTickLimit = 6000;

	FGameFileIndex ReadIndex(const FIso9660& Disc)
	{
		auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		return std::move(*Index);
	}

	/** How the ride went: whether Fei waited out of view, and the tick the scene left for the next map. */
	struct FRide
	{
		bool bWaitedOutOfView = false;
		int32 LeftAt = -1;
	};

	FRide PlayRide(const Testing::FOracleFieldFiles& Files, const FMesResident& Resident, const FEventOptions& Options)
	{
		Testing::FOracleTickSession Session(Files, RideMap);
		Session.Game.SetSavedEventVariable(0, RideStory);
		FFieldServices Services = Session.Services(Files, Resident);
		Services.Options = Options;
		Services.Story = &Testing::PortStory();
		FFieldRuntime Runtime(Services);
		REQUIRE_FALSE(Runtime.EnterMap(RideMap));
		FRide Ride;
		int32 Reading = 0;
		for (int32 Tick = 0; Tick < RideTickLimit; ++Tick)
		{
			const FEventContext& Context = Runtime.Context();
			const bool bMessage = std::any_of(Context.Messages.Windows.begin(), Context.Messages.Windows.end(),
				[](const FFieldMessageWindow& Window) { return Window.bOpen; });
			Reading = bMessage ? Reading + 1 : 0;
			std::vector<FPadState> Vblanks(static_cast<std::size_t>(Runtime.VblanksPerTick()));
			if (Reading == ReadingTicks)
			{
				Vblanks[0].Buttons = FPadState::Bit(EPadButton::Cross);
				Reading = 0;
			}
			REQUIRE_FALSE(Runtime.Tick(Vblanks));
			if (Runtime.Context().RunningMap != RideMap)
			{
				Ride.LeftAt = Tick;
				break;
			}
			const FEventActorState& Fei = Runtime.Context().States[RideFei];
			Ride.bWaitedOutOfView = Ride.bWaitedOutOfView || (Fei.Position[0] >> 16 == OutOfViewX && Fei.Position[2] >> 16 == OutOfViewZ);
		}
		return Ride;
	}
}

TEST_CASE("The scenes staged before a picture are those of maps 4, 39, 73, 81 and 514", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FGameFileIndex Index = ReadIndex(*Disc);
	std::vector<std::pair<int32, uint16>> Found;
	for (int32 Number = 0; Number < FieldMapCount; ++Number)
	{
		INFO("map " << Number);
		auto Map = LoadFieldMap(Disc->Image(), Index, Number);
		REQUIRE(Map);
		for (const FFieldModelObject& Object : Map->ModelObjects)
		{
			if (IsScenePicture(Map->Models[Map->Actors[Object.Actor].Model].Parts.front()))
			{
				Found.emplace_back(Number, Object.Actor);
			}
		}
	}
	const std::vector<std::pair<int32, uint16>> Pictures = {{4, 14}, {39, 16}, {73, 6}, {81, 2}, {81, 3}, {81, 4}, {514, 26}, {514, 27}};
	// The rest of that form: skies (the background's in maps 178, 383, 390, 401 and 403, a wall of sky in 537) and the
	// photographs of Fei's memories that float in map 658's dark.
	const std::vector<std::pair<int32, uint16>> Others = {{178, 9}, {383, 28}, {390, 40}, {390, 41}, {401, 28}, {403, 110}, {537, 36}, {658, 17}};
	std::vector<std::pair<int32, uint16>> Expected = Pictures;
	Expected.insert(Expected.end(), Others.begin(), Others.end());
	std::sort(Expected.begin(), Expected.end());
	CHECK(Found == Expected);
	for (const int32 Number : {4, 39, 73, 81, 514})
	{
		INFO("map " << Number);
		auto Map = LoadFieldMap(Disc->Image(), Index, Number);
		REQUIRE(Map);
		auto Scene = BuildFieldScene(*Map);
		REQUIRE(Scene);
		MarkScenePictures(Number, *Scene);
		std::size_t Marked = 0;
		for (const FFieldSceneObject& Object : Scene->Objects)
		{
			const bool bListed = std::find(Pictures.begin(), Pictures.end(), std::pair{Number, Object.Actor}) != Pictures.end();
			CHECK(Object.bPicture == bListed);
			Marked += Object.bPicture ? 1 : 0;
		}
		CHECK(Marked > 0);
	}
}

TEST_CASE("Map 73's Gears load their textures over the sprite images its archive puts where they lie", "[Field][Gears][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Testing::EnterFieldMap(*Disc, DesertMap);
	Session->Run(1);
	const FEventContext& Context = Session->Play->Field().Context();
	REQUIRE(Context.Gears.Count == 2);
	// What each unit of the Gears' blocks should hold, the later block's where two cover it.
	std::map<std::pair<int32, int32>, uint16> Expected;
	for (int32 Slot = 0; Slot < Context.Gears.Count; ++Slot)
	{
		auto Files = ReadGearFiles(Disc->Image(), *Session->Index, Context.Gears.FileNumbers[static_cast<std::size_t>(Slot)] / 2);
		REQUIRE(Files);
		const FGearTexturePlace Place = FieldGearTexturePlace(Context.Gears, Slot);
		const bool bAbsolute = (Files->Model.Header.Flags & GearModelFlags::AbsoluteTextures) != 0;
		for (const FGearTextureBlock& Block : Files->Model.Textures)
		{
			const int32 X = (bAbsolute ? Block.X : Block.bClut ? Place.ClutX : Place.X) + Block.OffsetX;
			const int32 Y = (bAbsolute ? Block.Y : Block.bClut ? Place.ClutY : Place.Y) + Block.OffsetY;
			for (int32 Row = 0; Row < Block.Height; ++Row)
			{
				for (int32 Column = 0; Column < Block.Width; ++Column)
				{
					Expected[{X + Column, Y + Row}] = Block.Pixels[static_cast<std::size_t>(Row * Block.Width + Column)];
				}
			}
		}
	}
	// The map's sprite slots 1 to 9 take images from its archive at (576, 256) and below, the first Gear's place.
	const FFieldMap& Map = Session->Play->Field().Map();
	const bool bSlotsUnderGear = std::any_of(Map.TextureUploads.begin(), Map.TextureUploads.begin() + static_cast<std::ptrdiff_t>(Map.ArchiveUploads),
		[&Expected](const FVramImage& Image) { return Expected.contains({Image.Rect.X, Image.Rect.Y}); });
	CHECK(bSlotsUnderGear);
	const FVram& Vram = Session->Play->Drawing().Vram();
	std::size_t Wrong = 0;
	for (const auto& [At, Unit] : Expected)
	{
		Wrong += Vram.At(At.first, At.second) != Unit ? 1 : 0;
	}
	CHECK(Wrong == 0);
}

TEST_CASE("Map 50's ride into the night has Fei wait out of view, or in the original just off the 4:3 screen, the scene's time the same", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const FRide Staged = PlayRide(*Files, Resident, FEventOptions());
	FEventOptions Original;
	Original.bWideViewStaging = false;
	const FRide AsOriginal = PlayRide(*Files, Resident, Original);
	CHECK(Staged.bWaitedOutOfView);
	CHECK_FALSE(AsOriginal.bWaitedOutOfView);
	REQUIRE(Staged.LeftAt > 0);
	CHECK(Staged.LeftAt == AsOriginal.LeftAt);
}
