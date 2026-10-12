#include "Support/FieldPlaySession.h"

#include "Media/Iso9660.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <vector>

namespace Weltall::Testing
{
	void FFieldPlaySession::Run(int32 Ticks, EFieldCulling Culling)
	{
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			const std::vector<FPadState> Held(static_cast<std::size_t>(Play->VblanksPerTick()));
			if (auto Error = Play->Tick(Held, Culling, Frame))
			{
				FAIL(Error->Message);
			}
		}
	}

	std::unique_ptr<FFieldPlaySession> EnterFieldMap(const FIso9660& Disc, int32 Map)
	{
		return EnterFieldMap(Disc, Map, {});
	}

	std::unique_ptr<FFieldPlaySession> EnterFieldMap(const FIso9660& Disc, int32 Map, const std::function<void(FGameState&)>& Stage)
	{
		auto Session = std::make_unique<FFieldPlaySession>();
		auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		Session->Executable = std::make_unique<FPsxExecutable>(std::move(*Executable));
		auto Index = FGameFileIndex::Read(*Session->Executable);
		REQUIRE(Index);
		Session->Index = std::make_unique<FGameFileIndex>(std::move(*Index));
		auto Play = FFieldPlay::NewGame(Disc.Image(), *Session->Index, *Session->Executable, EDiscRelease::UsaDisc1, Session->Songs, nullptr);
		if (!Play)
		{
			FAIL(Play.Error().Message);
		}
		Session->Play = std::move(*Play);
		if (Stage)
		{
			Stage(Session->Play->Game());
		}
		if (auto Error = Session->Play->EnterMap(Map))
		{
			FAIL(Error->Message);
		}
		return Session;
	}

	int32 FieldDrawEntry(const FFieldFrame& Frame, const FFieldDrawItem& Item)
	{
		switch (Item.Kind)
		{
		case EFieldDrawKind::Gear:
			return Item.Gear == EFieldGearDraw::Cloth ? Frame.Gears.Cloths[Item.Index].Bucket : Frame.Gears.Polygons[Item.Index].Bucket;
		case EFieldDrawKind::Character:
			return Frame.Quads[Item.Index].Quad.OtIndex;
		case EFieldDrawKind::Particle:
			return Frame.Particles[Item.Index].OtIndex;
		case EFieldDrawKind::Backdrop:
			return Frame.Backdrop.Entry;
		case EFieldDrawKind::SpriteModel:
			return Frame.SpriteModels.Polygons[Item.Index].Bucket;
		case EFieldDrawKind::Scenery:
			break;
		}
		return Frame.Scenery.Polygons[Item.Index].Bucket;
	}

	int32 FieldDrawRank(const FFieldDrawItem& Item)
	{
		switch (Item.Kind)
		{
		case EFieldDrawKind::Gear:
			return 0;
		case EFieldDrawKind::Particle:
			return 1;
		case EFieldDrawKind::Character:
			return 2;
		case EFieldDrawKind::SpriteModel:
			return 3;
		default:
			return 4;
		}
	}
}
