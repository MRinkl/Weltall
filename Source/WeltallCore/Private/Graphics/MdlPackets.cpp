#include "Graphics/MdlPackets.h"

#include "Graphics/GteVectors.h"

namespace Weltall
{
	namespace
	{
		FMdlColor ColorOf(const FGteColor& Color)
		{
			return {Color.R, Color.G, Color.B};
		}

		FGteColor StoredColor(const FMdlPrimitive& Primitive)
		{
			return {Primitive.Color.R, Primitive.Color.G, Primitive.Color.B, Primitive.Code};
		}

		const FGteVector& CornerNormal(const FMdlPart& Part, const FMdlPrimitive& Primitive, std::size_t Corner)
		{
			return Part.Normals[Primitive.Vertices[Corner]];
		}

		FGteVector FaceNormalOf(const FMdlPart& Part, const FMdlPrimitive& Primitive)
		{
			return TriangleNormal(Part.Vertices[Primitive.Vertices[0]], Part.Vertices[Primitive.Vertices[1]], Part.Vertices[Primitive.Vertices[2]]);
		}

		/** The Gouraud kinds light their corners with NCT (textured) or NCCT, and a quad's fourth with NCS or NCCS. */
		void LightCorners(FMdlPacket& Packet, const FMdlPrimitive& Primitive, const FMdlPart& Part, const FGte& Lighting)
		{
			const bool bTimesColor = !Primitive.Traits().bTextured;
			for (std::size_t Corner = 0; Corner < Primitive.Traits().Corners; ++Corner)
			{
				const FGteVector& Normal = CornerNormal(Part, Primitive, Corner);
				Packet.Colors[Corner] = ColorOf(bTimesColor ? Lighting.NormalColorColor(Normal, StoredColor(Primitive)) : Lighting.NormalColor(Normal));
			}
		}

		TResult<FMdlPacket> BuildPacket(const FMdlPart& Part, const FMdlPrimitive& Primitive, bool bLit, const FGte& Lighting)
		{
			FMdlPacket Packet;
			Packet.Code = Primitive.Code;
			switch (Primitive.Type)
			{
			case EMdlPrimitiveType::FlatTriangle:
			case EMdlPrimitiveType::FlatQuad:
				Packet.Relight = EMdlRelight::FaceTimesColor;
				Packet.FaceNormal = FaceNormalOf(Part, Primitive);
				Packet.Colors[0] = bLit ? ColorOf(Lighting.NormalColorColor(Packet.FaceNormal, StoredColor(Primitive))) : Primitive.Color;
				break;
			case EMdlPrimitiveType::TexturedTriangle:
			case EMdlPrimitiveType::TexturedQuad:
				Packet.Relight = EMdlRelight::Face;
				Packet.FaceNormal = FaceNormalOf(Part, Primitive);
				if (bLit)
				{
					Packet.Colors[0] = ColorOf(Lighting.NormalColor(Packet.FaceNormal));
				}
				else if (!Primitive.IsRawTexture())
				{
					return MakeError("a lightable textured polygon built without light is drawn modulated by a color nothing wrote");
				}
				break;
			case EMdlPrimitiveType::GouraudTriangle:
				Packet.Relight = EMdlRelight::CornersTimesColor;
				LightCorners(Packet, Primitive, Part, Lighting);
				break;
			case EMdlPrimitiveType::GouraudTexturedTriangle:
				Packet.Relight = EMdlRelight::Corners;
				LightCorners(Packet, Primitive, Part, Lighting);
				break;
			case EMdlPrimitiveType::StaticGouraudTriangle:
			case EMdlPrimitiveType::StaticGouraudTexturedTriangle:
			case EMdlPrimitiveType::GouraudQuad:
			case EMdlPrimitiveType::GouraudTexturedQuad:
			case EMdlPrimitiveType::StaticGouraudQuad:
			case EMdlPrimitiveType::StaticGouraudTexturedQuad:
				LightCorners(Packet, Primitive, Part, Lighting);
				break;
			case EMdlPrimitiveType::StaticFlatTriangle:
			case EMdlPrimitiveType::StaticFlatQuad:
			case EMdlPrimitiveType::StaticTexturedQuad:
				Packet.Colors[0] = Primitive.Color;
				break;
			case EMdlPrimitiveType::StaticTexturedTriangle:
				if (!Primitive.IsRawTexture())
				{
					return MakeError("a static textured triangle is drawn modulated by a color nothing wrote");
				}
				break;
			case EMdlPrimitiveType::EnvironmentMappedTriangle:
				// Raw: its texels come from its normals as it is drawn.
				break;
			}
			return Packet;
		}
	}

	TResult<std::vector<FMdlPacket>> BuildMdlPackets(const FMdlPart& Part, int32 LightMode, const FGte& Lighting)
	{
		std::vector<FMdlPacket> Packets;
		Packets.reserve(Part.Primitives.size());
		for (const FMdlPrimitive& Primitive : Part.Primitives)
		{
			TResult<FMdlPacket> Packet = BuildPacket(Part, Primitive, LightMode != 0, Lighting);
			if (!Packet)
			{
				return Packet.Error();
			}
			Packets.push_back(*Packet);
		}
		return Packets;
	}

	void RelightMdlPacket(FMdlPacket& Packet, const FMdlPrimitive& Primitive, const FMdlPart& Part, const FGte& Lighting)
	{
		switch (Packet.Relight)
		{
		case EMdlRelight::None:
			break;
		case EMdlRelight::FaceTimesColor:
			Packet.Colors[0] = ColorOf(Lighting.NormalColorColor(Packet.FaceNormal, StoredColor(Primitive)));
			break;
		case EMdlRelight::Face:
			Packet.Colors[0] = ColorOf(Lighting.NormalColor(Packet.FaceNormal));
			break;
		case EMdlRelight::CornersTimesColor:
		case EMdlRelight::Corners:
			LightCorners(Packet, Primitive, Part, Lighting);
			break;
		}
	}

	bool RelightReadsLightBuffer(EMdlRelight Relight)
	{
		return Relight == EMdlRelight::FaceTimesColor || Relight == EMdlRelight::Face || Relight == EMdlRelight::CornersTimesColor;
	}

	std::array<FMdlUv, 3> MdlEnvironmentTexels(const FMdlPart& Part, const FMdlPrimitive& Primitive, const FGteMatrix& Matrix, const FMdlEnvironmentMapping& Mapping)
	{
		const std::vector<FGteVector>& Normals = Part.Normals.empty() ? Part.Vertices : Part.Normals;
		std::array<FMdlUv, 3> Texels{};
		for (std::size_t Corner = 0; Corner < Texels.size(); ++Corner)
		{
			const FGteLongVector Turned = ApplyMatrix(Matrix, Normals[Primitive.Vertices[Corner]]);
			Texels[Corner] = {static_cast<uint8>((static_cast<uint32>(Turned[0]) >> Mapping.ShiftU) + Mapping.OffsetU),
				static_cast<uint8>((static_cast<uint32>(Turned[1]) >> Mapping.ShiftV) + Mapping.OffsetV)};
		}
		return Texels;
	}
}
