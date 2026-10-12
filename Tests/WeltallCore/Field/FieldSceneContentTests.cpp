#include "Field/FieldCameraFloor.h"
#include "Field/FieldMap.h"
#include "Field/FieldScene.h"
#include "Field/FieldSceneFrame.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Support/OracleDigest.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <string>

using namespace Weltall;
using Testing::FOracleDigest;
using Testing::FXorShift32;

// The digests come from the game's own code run under the oracle harness on the same maps
// for the walkmesh's triangle search on every layer, the kernel's packets built for every
// placed model as the map's load builds them, the models drawn for a camera at rest, and the scripts'
// texture scroll (FE 1B) on the models it can scroll.

namespace
{
	struct FGameMaps
	{
		const FIso9660& Disc;
		FGameFileIndex Index;

		FFieldMap Load(int32 Number) const
		{
			auto Map = LoadFieldMap(Disc.Image(), Index, Number);
			if (!Map)
			{
				FAIL(Map.Error().Message);
			}
			return std::move(*Map);
		}

		FFieldScene Scene(const FFieldMap& Map) const
		{
			auto Scene = BuildFieldScene(Map);
			if (!Scene)
			{
				FAIL(Scene.Error().Message);
			}
			return std::move(*Scene);
		}
	};

	FGameFileIndex ReadIndex(const FIso9660& Disc)
	{
		auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		return std::move(*Index);
	}

	/** The kinds whose color the builder never writes; the maps draw them raw. */
	bool ColorUndefined(EMdlPrimitiveType Type, int32 LightMode)
	{
		return Type == EMdlPrimitiveType::StaticTexturedTriangle
			|| (LightMode == 0 && (Type == EMdlPrimitiveType::TexturedTriangle || Type == EMdlPrimitiveType::TexturedQuad));
	}

	int32 ColorCount(const FMdlPrimitive& Primitive)
	{
		return Primitive.Traits().bGouraud ? Primitive.Traits().Corners : 1;
	}

	/**
	 * A packet as the oracle digests it: code, corners when drawn, colors (a fog handler writes one where
	 * the builder writes none), then UVs (scrolled by Scroll) with the CLUT and page beside the first two.
	 */
	void AddPacket(FOracleDigest& Digest, const FFieldScene& Scene, const FFieldMap& Map, uint32 Index, const FFieldPolygonView* Drawn, FMdlUv Scroll = {},
		bool bFogged = false)
	{
		const FFieldScenePolygon& Polygon = Scene.Polygons[Index];
		const FFieldSceneObject& Object = Scene.Objects[Polygon.Object];
		const FMdlPrimitive& Primitive = Scene.PrimitiveOf(Polygon);
		// A fog handler clears the raw bit: the texels are modulated by its colour.
		Digest.U8(bFogged ? Polygon.Packet.Code & 0xFE : Polygon.Packet.Code);
		if (Drawn != nullptr)
		{
			for (int32 Corner = 0; Corner < Primitive.Traits().Corners; ++Corner)
			{
				Digest.I16(Drawn->ScreenX[static_cast<std::size_t>(Corner)], Drawn->ScreenY[static_cast<std::size_t>(Corner)]);
			}
		}
		const auto& Colors = Drawn != nullptr ? Drawn->Colors : Polygon.Packet.Colors;
		if (bFogged || !ColorUndefined(Primitive.Type, Map.Actors[Object.Actor].LightMode()))
		{
			for (int32 Corner = 0; Corner < ColorCount(Primitive); ++Corner)
			{
				const FMdlColor& Color = Colors[static_cast<std::size_t>(Corner)];
				Digest.U8(Color.R, Color.G, Color.B);
			}
		}
		if (Primitive.Traits().bTextured)
		{
			const auto ObjectOnMap = std::find_if(Map.ModelObjects.begin(), Map.ModelObjects.end(),
				[&Object](const FFieldModelObject& Candidate) { return Candidate.Actor == Object.Actor; });
			const FMdlTextureState Texture = Primitive.Texture(ObjectOnMap->TextureAtBuild);
			for (int32 Corner = 0; Corner < Primitive.Traits().Corners; ++Corner)
			{
				const FMdlUv& Uv = Primitive.Uvs[static_cast<std::size_t>(Corner)];
				Digest.I16(static_cast<uint8>(Uv.U + Scroll.U) | static_cast<uint8>(Uv.V + Scroll.V) << 8);
				if (Corner < 2)
				{
					Digest.I16(Corner == 0 ? Texture.Clut : Texture.TexturePage);
				}
			}
		}
	}

	std::string FloorDigest(const FFieldMap& Map, int32 Number)
	{
		FXorShift32 Random(0xF100 + static_cast<uint32>(Number));
		FOracleDigest Digest;
		for (int32 Case = 0; Case < 256; ++Case)
		{
			const FWalkmeshLayer& Layer = Map.Walkmesh.Layers[Random.Next() % Map.Walkmesh.Layers.size()];
			const auto [LowX, HighX] = std::minmax_element(Layer.Vertices.begin(), Layer.Vertices.end(), [](const FGteVector& A, const FGteVector& B) { return A[0] < B[0]; });
			const auto [LowZ, HighZ] = std::minmax_element(Layer.Vertices.begin(), Layer.Vertices.end(), [](const FGteVector& A, const FGteVector& B) { return A[2] < B[2]; });
			const int32 X = (*LowX)[0] - 64 + static_cast<int32>(Random.Next() % static_cast<uint32>((*HighX)[0] - (*LowX)[0] + 129));
			const int32 Z = (*LowZ)[2] - 64 + static_cast<int32>(Random.Next() % static_cast<uint32>((*HighZ)[2] - (*LowZ)[2] + 129));
			const FFieldFloor Floor = FindFieldFloor(Layer, X, Z);
			Digest.I32(Floor.Triangle);
			Digest.I16(Floor.Height);
			Digest.I32(Floor.Normal[0], Floor.Normal[1], Floor.Normal[2]);
		}
		return Digest.Hex();
	}

	std::string PacketDigest(const FFieldScene& Scene, const FFieldMap& Map)
	{
		FOracleDigest Digest;
		for (uint32 Index = 0; Index < Scene.Polygons.size(); ++Index)
		{
			Digest.I32(Scene.Objects[Scene.Polygons[Index].Object].Actor);
			AddPacket(Digest, Scene, Map, Index, nullptr);
		}
		return Digest.Hex();
	}

	/** The camera at rest looking at (X, the floor under it on layer 0 less 32, Z), the yaw in 45-degree steps. */
	FFieldCamera RestingCamera(const FFieldMap& Map, int32 X, int32 Z, int32 YawStep)
	{
		const FFieldFloor Floor = FindFieldFloor(Map.Walkmesh.Layers.front(), X, Z);
		FFieldCamera Camera;
		Camera.Rotation = {0, static_cast<int16>(YawStep * FFieldCamera::YawStep), 0};
		Camera.Target = FollowTarget({X * 0x10000, Floor.Height * 0x10000, Z * 0x10000});
		Camera.Eye = KeepEyeAboveCameraLayer(Map.Walkmesh, FollowEye(Camera, Camera.Target));
		return Camera;
	}
}

TEST_CASE("Every map of the first disc builds its scene", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FGameMaps Maps{*Disc, ReadIndex(*Disc)};
	std::size_t Objects = 0;
	std::size_t Polygons = 0;
	std::size_t Relit = 0;
	std::size_t Background = 0;
	std::size_t FacingScreen = 0;
	FFieldSceneFrame Frame;
	for (int32 Number = 0; Number < FieldMapCount; ++Number)
	{
		INFO("map " << Number);
		const FFieldMap Map = Maps.Load(Number);
		const FFieldScene Scene = Maps.Scene(Map);
		Objects += Scene.Objects.size();
		Polygons += Scene.Polygons.size();
		for (const FFieldSceneObject& Object : Scene.Objects)
		{
			Relit += Object.DrawMode == EFieldDrawMode::Relit;
			Background += Object.bBackground;
			FacingScreen += Object.MatrixMode == EFieldMatrixMode::FacingScreen;
		}
		EvaluateFieldScene(Scene, BuildFieldView(RestingCamera(Map, 0, 0, 4)), EFieldCulling::Original, Frame);
		CHECK(Frame.DrawOrder.size() <= Scene.Polygons.size());
	}
	CHECK(Objects == 20526);
	CHECK(Polygons == 1322676);
	CHECK(Relit == 184);
	CHECK(Background == 1055);
	CHECK(FacingScreen == 736);
}

TEST_CASE("Walkmesh floors are found as the game finds them", "[Field][Content][Oracle]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FGameMaps Maps{*Disc, ReadIndex(*Disc)};
	const std::pair<int32, const char*> Digests[] = {
		{1, "f035424b29d3ad31d9510c6745b7232488c9a795"},
		{59, "dcc9ae858869164d93ac46c78afa01bd77fdd1ae"},
		{62, "43085d13f68a2b046f9f8af74161612982a94031"},
		{63, "a1604c1cb56611b0bb0274416268a08004de4f5f"},
		{101, "67697a3f449307b7ba51e01501817f262eb581f3"},
		{182, "3e1aa7b8191fd9056cd47ceb3d64655b812d1e76"},
		{184, "eb92ee71987052a562dbcef37293a9f5fae0d229"},
		{280, "41cf82db2e314b9626d989f69b9ee34561db1979"},
		{514, "d2d7ee07cdf55b23b7882451a44ec96319ec021a"},
	};
	for (const auto& [Number, Expected] : Digests)
	{
		INFO("map " << Number);
		CHECK(FloorDigest(Maps.Load(Number), Number) == Expected);
	}
}

TEST_CASE("Placed models are built as the game builds them", "[Field][Content][Oracle]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FGameMaps Maps{*Disc, ReadIndex(*Disc)};
	// Lahan, and the maps with lit models: relit flat and Gouraud kinds, static Gouraud kinds lit
	// once, degenerate triangles, models built twice.
	const std::pair<int32, const char*> Digests[] = {
		{1, "e112823be5e7b09d3370a6e25e9cea8f86004ee0"},
		{59, "87a1e1cb7e9a559663d07de8011232ecc787b87b"},
		{62, "9b52749b5987a025b5f6ddd4c1bf99f3670d83e8"},
		{63, "afa20227e7995ff1cd4a8e2f17a130c65c4b3893"},
		{101, "041e416aa9febe6b94b64ed8bea56fd2cb9019cb"},
		{182, "dba7dd5ef27da7a1dbbd8e8e5a34deb6aefdc872"},
		{184, "2550a9761310bf9c91dfcee04ed2730c9c016f94"},
		{280, "5ac4e41bc5389013233789d4b12852a0ffbb02a8"},
		{514, "c21a4986f3d11d571a02d9f2ffb6683e36dd6bad"},
	};
	for (const auto& [Number, Expected] : Digests)
	{
		INFO("map " << Number);
		const FFieldMap Map = Maps.Load(Number);
		CHECK(PacketDigest(Maps.Scene(Map), Map) == Expected);
	}
}

TEST_CASE("Frames of the scenery match the game's drawing", "[Field][Content][Oracle]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FGameMaps Maps{*Disc, ReadIndex(*Disc)};
	struct FView
	{
		int32 Map;
		int32 X;
		int32 Z;
		int32 YawStep;
		std::size_t Drawn;
		const char* Digest;
	};
	const FView Views[] = {
		{1, 0, 0, 4, 338, "64a6dea9162c8fabb376c7a022779449d9573b4f"},
		{1, -600, 400, 2, 154, "f217e4ca7447c1d3c123a59f93108a090e8c34af"},
		{15, 200, -300, 4, 234, "c461477513ce184ed2322c50a4b3c8531d64c66a"},
		{59, 0, 0, 4, 280, "a3be906056c9241d37411cd9ef98896ebaf329d0"},
		{62, 0, 0, 4, 340, "02f6ae927ac022c07177c26c44d0ffd7b64390fe"},
		{101, 0, 0, 6, 120, "1afd4fa6a7450e180884f5c8f06e260d7edbc07c"},
		{182, 0, 0, 4, 135, "101a7d119b9d5fea0f600ace1cd0daed6baf878d"},
		{234, 0, 0, 4, 360, "bcf2e2665984b7996bc77c525b47336388a2f1da"},
		{280, 0, 0, 4, 228, "c65a46c01722625b78eb97594a0b0a314f0cc865"},
		{514, 0, 0, 4, 1467, "a07cc47278e524c17c72581a7f16480ec097aca4"},
		{22, 0, 0, 4, 299, "c174ba036a717fc83b2b6309c4b333a37702b26b"},
		{22, 300, 300, 1, 321, "60e3c7dc2c1418cda2342fa07f7628593579e28c"},
	};
	FFieldSceneFrame Frame;
	for (const FView& View : Views)
	{
		INFO("map " << View.Map << " at " << View.X << ", " << View.Z);
		const FFieldMap Map = Maps.Load(View.Map);
		const FFieldScene Scene = Maps.Scene(Map);
		EvaluateFieldScene(Scene, BuildFieldView(RestingCamera(Map, View.X, View.Z, View.YawStep)), EFieldCulling::Original, Frame);
		FOracleDigest Digest;
		for (const uint32 Index : Frame.DrawOrder)
		{
			const FFieldScenePolygon& Polygon = Scene.Polygons[Index];
			Digest.I32(Scene.Objects[Polygon.Object].Actor, Polygon.Primitive);
			AddPacket(Digest, Scene, Map, Index, &Frame.Polygons[Index]);
		}
		CHECK(Frame.DrawOrder.size() == View.Drawn);
		CHECK(Digest.Hex() == View.Digest);
	}
}

TEST_CASE("Frames of the scenery in fog match the game's drawing", "[Field][Content][Oracle]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FGameMaps Maps{*Disc, ReadIndex(*Disc)};
	struct FView
	{
		int32 Map;
		int32 X;
		int32 Z;
		int32 YawStep;
		FFieldFog Fog;
		std::size_t Drawn;
		const char* Digest;
	};
	const FView Views[] = {
		{2, 0, 0, 4, {true, {128, 128, 128}, {60, 60, 80}, 4000, 20000}, 321, "08b21d9e16e7b6325c4c79944e6f0e828d446605"},
		{1, 0, 0, 4, {true, {255, 219, 163}, {100, 56, 20}, 5900, 10000}, 338, "794fe8c9664529e4b2d9d754fff48a4ed181800b"},
		{22, 300, 300, 1, {true, {128, 128, 128}, {0, 40, 80}, 1600, 8000}, 321, "8df8fa03c6f7e6b2e532524b84efe98a212db05d"},
	};
	FFieldSceneFrame Frame;
	for (const FView& View : Views)
	{
		INFO("map " << View.Map << " at " << View.X << ", " << View.Z);
		const FFieldMap Map = Maps.Load(View.Map);
		FFieldScene Scene = Maps.Scene(Map);
		// E5 puts every model in its fog draw mode: sorted by its farthest corner or by its average.
		Scene.State.Fog = View.Fog;
		for (FFieldSceneObject& Object : Scene.Objects)
		{
			Object.bFogged = true;
			Object.DrawMode = (Map.Actors[Object.Actor].Flags & FieldActorFlag::SortByFarthestVertex) != 0 ? EFieldDrawMode::Farthest : EFieldDrawMode::Average;
		}
		EvaluateFieldScene(Scene, BuildFieldView(RestingCamera(Map, View.X, View.Z, View.YawStep)), EFieldCulling::Original, Frame);
		FOracleDigest Digest;
		for (const uint32 Index : Frame.DrawOrder)
		{
			const FFieldScenePolygon& Polygon = Scene.Polygons[Index];
			Digest.I32(Scene.Objects[Polygon.Object].Actor, Polygon.Primitive);
			AddPacket(Digest, Scene, Map, Index, &Frame.Polygons[Index], {}, Frame.Polygons[Index].bFogged);
		}
		CHECK(Frame.DrawOrder.size() == View.Drawn);
		CHECK(Digest.Hex() == View.Digest);
	}
}

TEST_CASE("The scripts' texture scrolls move the models' coordinates as the game's FE 1B moves their packets'", "[Field][Content][Oracle]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FGameMaps Maps{*Disc, ReadIndex(*Disc)};
	// Maps whose scripts scroll models: every placed model of flat-textured polygons alone is scrolled
	// at random, then its packets in both of the original's buffers are digested.
	const std::pair<int32, const char*> Digests[] = {
		{74, "43e4fecb6d20188dd0ad94cc15acb5c3cd1a22de"},
		{273, "fa7ee831f844f24082f39a484ba95b1cbcb2b132"},
		{367, "228ce7af8e62122c336605d80080f918f8aee42d"},
		{570, "9466b4dd5f80f667cce28643decb0cdbf4c58341"},
	};
	for (const auto& [Number, Expected] : Digests)
	{
		INFO("map " << Number);
		const FFieldMap Map = Maps.Load(Number);
		const FFieldScene Scene = Maps.Scene(Map);
		std::vector<const FFieldSceneObject*> Scrolled;
		for (const FFieldSceneObject& Object : Scene.Objects)
		{
			const std::vector<FMdlPrimitive>& Primitives = Scene.PartOf(Object).Primitives;
			if (std::all_of(Primitives.begin(), Primitives.end(), [](const FMdlPrimitive& Primitive) { return Primitive.Traits().bTextured && !Primitive.Traits().bGouraud; }))
			{
				Scrolled.push_back(&Object);
			}
		}
		REQUIRE_FALSE(Scrolled.empty());
		FFieldRenderState Render;
		FXorShift32 Random(0x1B00 + static_cast<uint32>(Number));
		for (int32 Step = 0; Step < 24; ++Step)
		{
			const FFieldSceneObject& Object = *Scrolled[Random.Next() % Scrolled.size()];
			const uint32 U = Random.Next();
			const uint32 V = Random.Next();
			Render.ScrollModelUv(Object.Actor, {static_cast<uint8>(U), static_cast<uint8>(V)});
		}
		FOracleDigest Digest;
		for (const FFieldSceneObject* Object : Scrolled)
		{
			for (int32 Buffer = 0; Buffer < 2; ++Buffer)
			{
				for (uint32 Index = Object->FirstPolygon; Index < Object->FirstPolygon + Object->PolygonCount; ++Index)
				{
					Digest.I32(Object->Actor);
					AddPacket(Digest, Scene, Map, Index, nullptr, Render.ModelUvScroll(Object->Actor));
				}
			}
		}
		CHECK(Digest.Hex() == Expected);
	}
}
