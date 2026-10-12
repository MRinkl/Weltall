#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * The kernel's polygon kinds, by their number in a model. The first four of each shape are
	 * lightable: built with the lights when their model object has a light mode, and relit every frame
	 * in the draw mode that relights. The static ones keep the colours they are built with. Every
	 * Gouraud kind takes its colours from the part's normals and the lights when it is built.
	 */
	enum class EMdlPrimitiveType : uint8
	{
		FlatTriangle,
		TexturedTriangle,
		GouraudTriangle,
		GouraudTexturedTriangle,
		StaticFlatTriangle,
		StaticTexturedTriangle,
		StaticGouraudTriangle,
		StaticGouraudTexturedTriangle,
		FlatQuad,
		TexturedQuad,
		GouraudQuad,
		GouraudTexturedQuad,
		StaticFlatQuad,
		StaticTexturedQuad,
		StaticGouraudQuad,
		StaticGouraudTexturedQuad,
		/**
		 * A static raw-textured triangle whose UVs come from its normals every frame, on an 8-bit page
		 * and a CLUT the drawing's texture base places; the model stores nothing for it.
		 */
		EnvironmentMappedTriangle,
	};

	inline constexpr int32 MdlPrimitiveTypeCount = 17;

	/** What the kernel's primitive table says about a kind. */
	struct FMdlPrimitiveTraits
	{
		/** 3 or 4; a quad is drawn as the triangles 0-1-2 and 1-3-2. */
		uint8 Corners = 3;
		bool bTextured = false;
		bool bGouraud = false;
		bool bLightable = false;
		/** Bytes of the model's packet source each primitive reads, texture commands aside. */
		uint8 SourceBytes = 4;
	};

	WELTALLCORE_API const FMdlPrimitiveTraits& MdlPrimitiveTraits(EMdlPrimitiveType Type);

	struct FMdlColor
	{
		uint8 R = 0;
		uint8 G = 0;
		uint8 B = 0;

		bool operator==(const FMdlColor&) const = default;
	};

	struct FMdlUv
	{
		uint8 U = 0;
		uint8 V = 0;

		bool operator==(const FMdlUv&) const = default;
	};

	/** The texture page and CLUT the packet builder holds: the last its texture commands set. */
	struct FMdlTextureState
	{
		uint16 TexturePage = 0;
		uint16 Clut = 0;

		bool operator==(const FMdlTextureState&) const = default;
	};

	/** One polygon as the packet builder reads it from the model. */
	struct FMdlPrimitive
	{
		EMdlPrimitiveType Type = EMdlPrimitiveType::FlatTriangle;
		/** The GPU command byte: bit 1 semi-transparent (with the page's blend mode), bit 0 raw texture. */
		uint8 Code = 0;
		/**
		 * The colour stored with untextured kinds and textured quads (textured triangles store none and
		 * keep 0). Flat kinds built without light and the static textured quad draw with it as stored;
		 * lit untextured kinds scale their light by it; lit textured kinds ignore it.
		 */
		FMdlColor Color;
		/**
		 * Textured kinds before their part's first texture page or CLUT command take that value from
		 * the builder's state, which the previously built model left.
		 */
		bool bInheritsTexturePage = false;
		bool bInheritsClut = false;
		/** Vertex (and normal) numbers, in the GPU's order; the fourth only for quads. */
		std::array<uint16, 4> Vertices{};
		/** Texture coordinates of each corner, for the kinds the model gives them. */
		std::array<FMdlUv, 4> Uvs{};
		/** The texture page and CLUT attributes, unless inherited. */
		uint16 TexturePage = 0;
		uint16 Clut = 0;

		const FMdlPrimitiveTraits& Traits() const { return MdlPrimitiveTraits(Type); }
		bool IsSemiTransparent() const { return (Code & 2) != 0; }
		bool IsRawTexture() const { return (Code & 1) != 0; }

		/** The page and CLUT it samples when its model is built with the builder in State. */
		FMdlTextureState Texture(const FMdlTextureState& State) const
		{
			return {bInheritsTexturePage ? State.TexturePage : TexturePage, bInheritsClut ? State.Clut : Clut};
		}
	};

	/** A vertex's movement in one key of a MIMe animation. */
	struct FMdlVertexDelta
	{
		FGteVector Delta{};
		uint16 Vertex = 0;
	};

	struct FMdlMimeKey
	{
		std::vector<FMdlVertexDelta> VertexDeltas;
		/** Only for parts whose MIMe moves their normals. */
		std::vector<FMdlVertexDelta> NormalDeltas;
	};

	/**
	 * MIMe vertex animation: every update resets the listed vertices to the model's and adds each
	 * key's deltas scaled by its weight (4096 = 1.0). Normals are never reset.
	 */
	struct FMdlMime
	{
		std::vector<FMdlMimeKey> Keys;
		std::vector<uint16> ResetVertices;
	};

	struct FMdlPart
	{
		std::vector<FGteVector> Vertices;
		/** One per vertex for smooth shading; empty when the part has none of its own. */
		std::vector<FGteVector> Normals;
		/** In the order the kernel builds and draws them. */
		std::vector<FMdlPrimitive> Primitives;
		/** The box the kernel culls the part by, as stored: the vertices' extremes in every field model. */
		FGteVector BoundsMin{};
		FGteVector BoundsMax{};
		/**
		 * Room for what lit polygons keep between frames (face normals, stored colors); a part
		 * without it cannot have its flat or untextured lit polygons lit again when drawn.
		 */
		uint32 LightBufferBytes = 0;
		bool bMimeMovesNormals = false;
		std::optional<FMdlMime> Mime;

		/** The builder's state after it built this part from State. */
		WELTALLCORE_API FMdlTextureState TextureStateAfter(const FMdlTextureState& State) const;
	};

	/** A static polygon model in the kernel's MDL format (the field's scenery, among others). */
	struct FMdlModel
	{
		std::vector<FMdlPart> Parts;
	};

	/** Reads a model whose offsets are relative to its start, as stored. */
	WELTALLCORE_API TResult<FMdlModel> ParseMdlModel(std::span<const uint8> Bytes);

	/** Reads a part on its own, its offsets relative to its start, as a sprite's script embeds one. */
	WELTALLCORE_API TResult<FMdlPart> ParseMdlPart(std::span<const uint8> Bytes);

	/** Reads a list of models: a u32 count, then each model's offset from the list's start. */
	WELTALLCORE_API TResult<std::vector<FMdlModel>> ParseMdlModelList(std::span<const uint8> Bytes);
}
