#include "Gears/GearPackets.h"

#include "Gears/GearSkeleton.h"
#include "Graphics/Vram.h"

#include <string>

namespace Weltall
{
	namespace
	{
		/** The page bits a texture-page base overrides: the page's column and its row of 256. */
		constexpr uint16 PagePlaceBits = 0x1F;
		/** libgpu's bit for a page from row 512 on, which the console's GPU ignores: set for a place in VRAM's port rows. */
		constexpr uint16 PortRowsBit = 0x800;
		/** The CLUT bits an overriding CLUT keeps from the command: its column within 256 units. */
		constexpr uint16 ClutColumnBits = 0xF;

		/** libgte's GetTPage(0, 0, X, Y), the bits a base keeps. */
		uint16 PagePlace(int16 X, int16 Y)
		{
			const uint16 Column = static_cast<uint16>((static_cast<uint16>(X) & 0x3FF) >> 6);
			const uint16 Row = static_cast<uint16>((static_cast<uint16>(Y) & 0x100) >> 4);
			return static_cast<uint16>((Column | Row) & PagePlaceBits);
		}

		/** libgte's GetClut(X, Y). */
		uint16 ClutAttribute(int16 X, int16 Y)
		{
			return static_cast<uint16>(static_cast<uint16>(Y) << 6 | ((static_cast<uint16>(X) >> 4) & 0x3F));
		}

		/** A polygon the drawing never lights again keeps the build's light: the Gouraud kinds its draw mode passes by. */
		bool KeepsBuildLight(const FMdlPrimitive& Primitive, const FMdlPacket& Packet)
		{
			return Primitive.Traits().bGouraud && Packet.Relight == EMdlRelight::None;
		}
	}

	FMdlTextureState PlaceGearTexture(const FMdlPrimitive& Primitive, const FGearTexturePlace* Place)
	{
		if (Place == nullptr)
		{
			return {Primitive.TexturePage, Primitive.Clut};
		}
		const uint16 PortRows = Place->Y >= FVram::Height ? PortRowsBit : 0;
		return {static_cast<uint16>((Primitive.TexturePage & ~PagePlaceBits) | PagePlace(Place->X, Place->Y) | PortRows),
			static_cast<uint16>((Primitive.Clut & ClutColumnBits) | (ClutAttribute(Place->ClutX, Place->ClutY) & ~ClutColumnBits))};
	}

	TResult<std::vector<FGearBonePackets>> BuildGearPackets(const FMdlModel& Model, std::span<const FGearBone> Bones, int32 LightMode,
		const FGearTexturePlace* Place, const std::optional<FGte>& Lighting)
	{
		std::vector<FGearBonePackets> Built;
		const FGte Unlit;
		for (std::size_t Index = 1; Index < Bones.size(); ++Index)
		{
			const FGearBone& Bone = Bones[Index];
			if (Bone.Part == FGearBone::NoPart)
			{
				continue;
			}
			const std::string Where = "bone " + std::to_string(Index) + "'s part " + std::to_string(Bone.Part) + ": ";
			if (Bone.Part >= Model.Parts.size())
			{
				return MakeError(Where + "the model has no such part");
			}
			const FMdlPart& Part = Model.Parts[Bone.Part];
			TResult<std::vector<FMdlPacket>> Packets = BuildMdlPackets(Part, LightMode, Lighting ? *Lighting : Unlit);
			if (!Packets)
			{
				return MakeError(Where + Packets.Error().Message);
			}
			FGearBonePackets& Out = Built.emplace_back();
			Out.Bone = static_cast<uint16>(Index);
			Out.Part = Bone.Part;
			Out.Textures.reserve(Part.Primitives.size());
			for (std::size_t Number = 0; Number < Part.Primitives.size(); ++Number)
			{
				const FMdlPrimitive& Primitive = Part.Primitives[Number];
				const FMdlPacket& Packet = (*Packets)[Number];
				if (KeepsBuildLight(Primitive, Packet) && !Lighting)
				{
					return MakeError(Where + "a polygon keeps the light of a build whose lighting the previous frame's drawing left");
				}
				if (LightMode != 0 && RelightReadsLightBuffer(Packet.Relight) && Part.LightBufferBytes == 0)
				{
					return MakeError(Where + "a polygon is relit from light data its part keeps no room for");
				}
				if (Primitive.Type == EMdlPrimitiveType::EnvironmentMappedTriangle && Place == nullptr)
				{
					return MakeError(Where + "an environment-mapped polygon takes its page and CLUT from an earlier build's place");
				}
				if (Primitive.Traits().bTextured && (Primitive.bInheritsTexturePage || Primitive.bInheritsClut))
				{
					return MakeError(Where + "a polygon takes its page or CLUT from an earlier build");
				}
				Out.Textures.push_back(Primitive.Traits().bTextured ? PlaceGearTexture(Primitive, Place) : FMdlTextureState{});
			}
			Out.Packets = std::move(*Packets);
		}
		return Built;
	}
}
