#pragma once

#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/MdlModel.h"
#include "Graphics/Sprite.h"

#include <array>
#include <vector>

namespace Weltall
{
	class FSpriteFile;

	/**
	 * A primitive of a model a battle sprite's script attaches (an object of a TMD inside the script's file), as the
	 * battle's packet builder and its drawing take it: a triangle or a quad, flat or graded, textured or not, lit by
	 * the GTE's lights as it is drawn or coloured by its file.
	 */
	struct FBattleTmdPrimitive
	{
		/** The GPU's code as the file gives it; the packet's takes its raw-texel and blend bits from the sprite. */
		uint8 Mode = 0;
		uint8 CornerCount = 3;
		bool bTextured = false;
		bool bGouraud = false;
		/** How many of its colours the lights give as it is drawn: none, the flat one, or one a corner. */
		uint8 LitColors = 0;
		/** Whether a sprite's added colour reaches its packet (not a lit one without texture). */
		bool bTinted = false;
		std::array<uint16, 4> Vertices{};
		std::array<uint16, 3> Normals{};
		/** The packet's colours as built: the file's, or mid grey for a lit textured one. */
		std::array<FMdlColor, 4> Colors{};
		std::array<FMdlUv, 4> Uvs{};
		uint16 Clut = 0;
		uint16 TexturePage = 0;
	};

	/** A battle model as its file holds it now (a script scales its vertices and sets its pages' blend modes in place). */
	struct FBattleTmdModel
	{
		std::vector<FGteVector> Vertices;
		std::vector<FGteVector> Normals;
		std::vector<FBattleTmdPrimitive> Primitives;

		/**
		 * The object at Object in File. False for one that runs past its file, names a vertex or a normal it lacks, or
		 * holds a kind of primitive no model of the game has (the lit graded ones without texture and the lit graded
		 * textured quad, which the original draws from the wrong words).
		 */
		WELTALLCORE_API bool Read(const FSpriteFile& File, uint32 Object);
	};

	/**
	 * The builder's change to the file: every textured primitive's page takes the blend mode of a sprite that blends
	 * (Blend 1 to 7: the mode is its low two bits less one). False when the object runs past its file.
	 */
	WELTALLCORE_API bool SetBattleTmdBlend(FSpriteFile& File, uint32 Object, uint8 Blend);

	/** The colour and code of a primitive's packet corner as built for the sprite's packets and tinted by its added colour. */
	WELTALLCORE_API uint32 BattleTmdColorCode(const FBattleTmdPrimitive& Primitive, std::size_t Corner, const FSpriteModelPackets& Packets);
}
