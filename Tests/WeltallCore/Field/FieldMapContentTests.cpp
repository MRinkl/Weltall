#include "Field/FieldMap.h"
#include "Foundation/Sha1.h"
#include "Graphics/Texture.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <string>

using namespace Weltall;

namespace
{
	TResult<FFieldMap> Load(const FIso9660& Disc, int32 Number)
	{
		const auto Executable = ReadBootExecutable(Disc);
		if (!Executable)
		{
			return Executable.Error();
		}
		const auto Index = FGameFileIndex::Read(*Executable);
		if (!Index)
		{
			return Index.Error();
		}
		return LoadFieldMap(Disc.Image(), *Index, Number);
	}

	std::string VramDigest(const FFieldMap& Map)
	{
		FVram Vram;
		UploadFieldTextures(Map, Vram, false);
		std::vector<uint8> Bytes;
		Bytes.reserve(static_cast<std::size_t>(FVram::Width) * FVram::Height * 2);
		for (int32 Y = 0; Y < FVram::Height; ++Y)
		{
			for (int32 X = 0; X < FVram::Width; ++X)
			{
				const uint16 Unit = Vram.At(X, Y);
				Bytes.push_back(static_cast<uint8>(Unit));
				Bytes.push_back(static_cast<uint8>(Unit >> 8));
			}
		}
		return FSha1::ToHex(FSha1::Hash(Bytes));
	}

	std::size_t PrimitiveCount(const FFieldMap& Map)
	{
		std::size_t Count = 0;
		for (const FMdlModel& Model : Map.Models)
		{
			for (const FMdlPart& Part : Model.Parts)
			{
				Count += Part.Primitives.size();
			}
		}
		return Count;
	}

	std::size_t VertexCount(const FFieldMap& Map)
	{
		std::size_t Count = 0;
		for (const FMdlModel& Model : Map.Models)
		{
			for (const FMdlPart& Part : Model.Parts)
			{
				Count += Part.Vertices.size();
			}
		}
		return Count;
	}

	bool ReadsTextureCommands(const FMdlPrimitive& Primitive)
	{
		return Primitive.Traits().bTextured && Primitive.Type != EMdlPrimitiveType::EnvironmentMappedTriangle;
	}

	/**
	 * Whether any polygon's texture depends on the model builder's state from before the map. Where
	 * the only such thing is a CLUT on a direct-colour page, which reads no CLUT, it does not.
	 */
	bool DependsOnEarlierBuilds(const FFieldMap& Map)
	{
		bool bPageSet = false;
		bool bClutSet = false;
		for (const FFieldModelObject& Object : Map.ModelObjects)
		{
			const FMdlPart& Part = Map.Models[Map.Actors[Object.Actor].Model].Parts.front();
			for (const FMdlPrimitive& Primitive : Part.Primitives)
			{
				if (!ReadsTextureCommands(Primitive))
				{
					continue;
				}
				const FTexturePage Page = FTexturePage::FromAttribute(Primitive.Texture(Object.TextureAtBuild).TexturePage);
				if ((Primitive.bInheritsTexturePage && !bPageSet) || (Primitive.bInheritsClut && !bClutSet && Page.Depth != ETextureDepth::Direct15))
				{
					return true;
				}
				bPageSet = bPageSet || !Primitive.bInheritsTexturePage;
				bClutSet = bClutSet || !Primitive.bInheritsClut;
			}
		}
		return false;
	}

	struct FMapFacts
	{
		int32 Map;
		std::size_t Actors;
		std::size_t ModelObjects;
		std::size_t Models;
		std::size_t Vertices;
		std::size_t Primitives;
		std::vector<std::size_t> Triangles;
		std::vector<std::size_t> WalkVertices;
		std::size_t Zones;
		std::size_t Uploads;
		std::size_t ScriptBytes;
		std::size_t MessageBytes;
		std::size_t SpriteSetBytes;
		std::size_t EncounterBytes;
	};

	/** Totals over a range of maps, as an independent reader counts them. */
	struct FCensus
	{
		std::size_t Actors = 0;
		std::size_t ModelObjects = 0;
		std::size_t Models = 0;
		std::size_t Vertices = 0;
		std::size_t Primitives = 0;
		std::size_t MimeParts = 0;
		std::size_t Layers = 0;
		std::size_t Triangles = 0;
		std::size_t WalkVertices = 0;
		std::size_t Zones = 0;
		std::size_t Uploads = 0;
		std::size_t ScriptBytes = 0;
		std::size_t MessageBytes = 0;
		std::size_t SpriteSetBytes = 0;
		std::size_t EncounterBytes = 0;
	};

	/** Loads maps First to End - 1 of the first disc, checking what holds for every map, and counts them. */
	void CheckCensus(int32 First, int32 End, const FCensus& Expected)
	{
		REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
		const auto Executable = ReadBootExecutable(*Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		FCensus Census;
		for (int32 Number = First; Number < End; ++Number)
		{
			const auto Map = LoadFieldMap(Disc->Image(), *Index, Number);
			if (!Map)
			{
				FAIL(Map.Error().Message);
			}
			INFO("map " << Number);
			Census.Actors += Map->Actors.size();
			Census.ModelObjects += Map->ModelObjects.size();
			Census.Models += Map->Models.size();
			Census.Vertices += VertexCount(*Map);
			Census.Primitives += PrimitiveCount(*Map);
			for (const FMdlModel& Model : Map->Models)
			{
				for (const FMdlPart& Part : Model.Parts)
				{
					Census.MimeParts += Part.Mime.has_value();
					// Every box is exactly its vertices' extremes.
					for (std::size_t Axis = 0; Axis < 3 && !Part.Vertices.empty(); ++Axis)
					{
						const auto [Low, High] = std::minmax_element(Part.Vertices.begin(), Part.Vertices.end(),
							[Axis](const FGteVector& A, const FGteVector& B) { return A[Axis] < B[Axis]; });
						CHECK(((*Low)[Axis] == Part.BoundsMin[Axis] && (*High)[Axis] == Part.BoundsMax[Axis]));
					}
				}
			}
			Census.Layers += Map->Walkmesh.Layers.size();
			for (const FWalkmeshLayer& Layer : Map->Walkmesh.Layers)
			{
				Census.Triangles += Layer.Triangles.size();
				Census.WalkVertices += Layer.Vertices.size();
			}
			Census.Zones += Map->Zones.size();
			Census.Uploads += Map->TextureUploads.size();
			Census.ScriptBytes += Map->EventScripts.size();
			Census.MessageBytes += Map->Messages.size();
			Census.SpriteSetBytes += Map->SpriteSets.size();
			Census.EncounterBytes += Map->Encounters.size();
			CHECK_FALSE(DependsOnEarlierBuilds(*Map));
		}
		CHECK(Census.Actors == Expected.Actors);
		CHECK(Census.ModelObjects == Expected.ModelObjects);
		CHECK(Census.Models == Expected.Models);
		CHECK(Census.Vertices == Expected.Vertices);
		CHECK(Census.Primitives == Expected.Primitives);
		CHECK(Census.MimeParts == Expected.MimeParts);
		CHECK(Census.Layers == Expected.Layers);
		CHECK(Census.Triangles == Expected.Triangles);
		CHECK(Census.WalkVertices == Expected.WalkVertices);
		CHECK(Census.Zones == Expected.Zones);
		CHECK(Census.Uploads == Expected.Uploads);
		CHECK(Census.ScriptBytes == Expected.ScriptBytes);
		CHECK(Census.MessageBytes == Expected.MessageBytes);
		CHECK(Census.SpriteSetBytes == Expected.SpriteSetBytes);
		CHECK(Census.EncounterBytes == Expected.EncounterBytes);
	}
}

// Four quarters of the first disc's 730 maps, so the reads run in parallel.
TEST_CASE("Field maps 0 to 182 load as an independent reader reads them", "[Field][Content]")
{
	CheckCensus(0, 183, {9547, 5881, 5236, 365378, 322417, 51, 439, 82467, 48705, 342, 20676, 975496, 354228, 2840596, 85536});
}

TEST_CASE("Field maps 183 to 365 load as an independent reader reads them", "[Field][Content]")
{
	CheckCensus(183, 366, {8617, 5143, 3916, 332187, 286526, 23, 447, 42082, 27850, 173, 19301, 1111204, 601406, 2817808, 83424});
}

TEST_CASE("Field maps 366 to 548 load as an independent reader reads them", "[Field][Content]")
{
	CheckCensus(366, 549, {8098, 4611, 3735, 309267, 265823, 24, 488, 46329, 33445, 171, 20009, 1188688, 230902, 2912972, 73920});
}

TEST_CASE("Field maps 549 to 729 load as an independent reader reads them", "[Field][Content]")
{
	CheckCensus(549, 730, {7920, 4891, 3365, 332443, 300986, 59, 441, 41964, 28223, 79, 17716, 1176472, 270527, 2853668, 92400});
}

TEST_CASE("Field maps hold what an independent reader finds in them", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMapFacts Expected[] = {
		// The debug room, Lahan, the mountain path, a room whose walkmesh keeps 160 attribute words, a
		// map with a negative sprite image count, a model inheriting its texture, the lit map, and the
		// developers' room built on Lahan's scenery.
		{0, 27, 5, 1, 4, 2, {23, 2}, {24, 4}, 0, 8, 6780, 3488, 20680, 528},
		{1, 143, 96, 75, 4467, 4626, {2652, 802, 40}, {1412, 408, 25}, 12, 245, 12056, 9193, 27528, 528},
		{15, 109, 67, 29, 2440, 3258, {1936, 195, 48}, {1054, 103, 27}, 17, 164, 15372, 3024, 11856, 528},
		{41, 1, 0, 0, 0, 0, {2, 2}, {4, 4}, 0, 12, 396, 8, 2920, 528},
		{164, 81, 49, 49, 3362, 2562, {42, 91, 2}, {44, 62, 4}, 0, 48, 4676, 2384, 90568, 528},
		{234, 42, 14, 13, 4441, 4025, {37, 2}, {35, 4}, 0, 198, 3968, 1523, 17236, 528},
		{280, 17, 9, 5, 970, 1386, {2, 2}, {4, 4}, 0, 41, 2920, 179, 3996, 528},
		{720, 146, 96, 75, 4469, 4619, {2652, 802, 40}, {1412, 408, 25}, 12, 246, 4688, 1507, 30552, 528},
	};
	for (const FMapFacts& Facts : Expected)
	{
		INFO("map " << Facts.Map);
		const auto Map = Load(*Disc, Facts.Map);
		if (!Map)
		{
			FAIL(Map.Error().Message);
		}
		CHECK(Map->Actors.size() == Facts.Actors);
		CHECK(Map->ModelObjects.size() == Facts.ModelObjects);
		CHECK(Map->Models.size() == Facts.Models);
		CHECK(VertexCount(*Map) == Facts.Vertices);
		CHECK(PrimitiveCount(*Map) == Facts.Primitives);
		std::vector<std::size_t> Triangles;
		std::vector<std::size_t> WalkVertices;
		for (const FWalkmeshLayer& Layer : Map->Walkmesh.Layers)
		{
			Triangles.push_back(Layer.Triangles.size());
			WalkVertices.push_back(Layer.Vertices.size());
		}
		CHECK(Triangles == Facts.Triangles);
		CHECK(WalkVertices == Facts.WalkVertices);
		CHECK(Map->Zones.size() == Facts.Zones);
		CHECK(Map->TextureUploads.size() == Facts.Uploads);
		CHECK(Map->EventScripts.size() == Facts.ScriptBytes);
		CHECK(Map->Messages.size() == Facts.MessageBytes);
		CHECK(Map->SpriteSets.size() == Facts.SpriteSetBytes);
		CHECK(Map->Encounters.size() == Facts.EncounterBytes);
	}
}

TEST_CASE("Field map details: lights, MIMe keys, inherited textures, short attribute tables", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Lit = Load(*Disc, 280);
	REQUIRE(Lit);
	CHECK(Lit->Lights.Directional[0].Direction == FGteVector{-50, 500, 80});
	CHECK(Lit->Lights.Directional[0].Color == FGteVector{100, 100, 100});
	CHECK(Lit->Lights.Directional[1].Color == FGteVector{0, 0, 0});
	CHECK(Lit->Lights.Ambient == FGteVector{50, 50, 50});

	// Lahan's actor 59 animates model 63: two keys of six deltas, six vertices reset.
	const auto Lahan = Load(*Disc, 1);
	REQUIRE(Lahan);
	CHECK((Lahan->Actors[59].Flags & FieldActorFlag::VertexAnimation) != 0);
	CHECK(Lahan->Actors[59].Model == 63);
	CHECK(Lahan->Lights.Ambient == FGteVector{30, 30, 30});
	const FMdlPart& Animated = Lahan->Models[63].Parts[0];
	CHECK(Animated.Vertices.size() == 8);
	REQUIRE(Animated.Mime.has_value());
	REQUIRE(Animated.Mime->Keys.size() == 2);
	CHECK(Animated.Mime->Keys[1].VertexDeltas.size() == 6);
	CHECK((Animated.Mime->Keys[0].VertexDeltas[0].Delta == FGteVector{-1, -3, 8} && Animated.Mime->Keys[0].VertexDeltas[0].Vertex == 0));
	CHECK(Animated.Mime->ResetVertices.size() == 6);
	CHECK(Animated.Mime->Keys[0].NormalDeltas.empty());

	const auto Path = Load(*Disc, 15);
	REQUIRE(Path);
	const FMdlPart& Waving = Path->Models[0].Parts[0];
	REQUIRE(Waving.Mime.has_value());
	CHECK(Waving.Mime->Keys.size() == 2);
	CHECK(Waving.Mime->Keys[0].VertexDeltas.size() == 130);
	CHECK((Waving.Mime->Keys[0].VertexDeltas[0].Delta == FGteVector{0, 3, 0} && Waving.Mime->Keys[0].VertexDeltas[0].Vertex == 17));

	// Actor 28's model has no texture commands: it is built with model 6's last page and CLUT.
	const auto Inheriting = Load(*Disc, 234);
	REQUIRE(Inheriting);
	const auto Object = std::find_if(Inheriting->ModelObjects.begin(), Inheriting->ModelObjects.end(), [](const FFieldModelObject& Candidate) { return Candidate.Actor == 28; });
	REQUIRE(Object != Inheriting->ModelObjects.end());
	CHECK(Object->TextureAtBuild == FMdlTextureState{0x88, 0x7B40});
	CHECK(Inheriting->Models[Inheriting->Actors[28].Model].Parts[0].Primitives[0].bInheritsTexturePage);

	const auto Short = Load(*Disc, 41);
	REQUIRE(Short);
	CHECK(Short->Walkmesh.Attributes.size() == 160);
}

TEST_CASE("Field maps fill VRAM as an independent reader does", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const std::pair<int32, const char*> Digests[] = {
		{0, "d716a4f404f6d67f11fe7a212ea3d317201373d7"},
		{1, "460f1647301a0b654589ecc80ca5a67d410ad71e"},
		{164, "b505406504050fc9538dec4b5b4bd29a42f9df2a"},
		{280, "d842ca12d183f65aa41c9fc78bf5527e4aea5fb4"},
	};
	for (const auto& [Number, Digest] : Digests)
	{
		INFO("map " << Number);
		const auto Map = Load(*Disc, Number);
		REQUIRE(Map);
		CHECK(VramDigest(*Map) == Digest);
	}
}

TEST_CASE("The second disc holds a placeholder for the maps it never loads", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const auto Missing = Load(*Disc, 1);
	REQUIRE_FALSE(Missing);
	CHECK(Missing.Error().Message.find("placeholder") != std::string::npos);
	const auto Path = Load(*Disc, 15);
	REQUIRE(Path);
	CHECK(PrimitiveCount(*Path) == 3258);
	CHECK(VramDigest(*Path) == "44a496e0838537b4810568c347eda94f53408c6f");
}
