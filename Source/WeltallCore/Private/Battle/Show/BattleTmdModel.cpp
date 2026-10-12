#include "Battle/Show/BattleTmdModel.h"

#include "Graphics/SpriteFile.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** A TMD object's words: its vertices and normals (8 bytes each), its primitives, and their counts. */
		constexpr uint32 VertexTop = 0x00;
		constexpr uint32 VertexCount = 0x04;
		constexpr uint32 NormalTop = 0x08;
		constexpr uint32 NormalCount = 0x0C;
		constexpr uint32 PrimitiveTop = 0x10;
		constexpr uint32 PrimitiveCount = 0x14;
		constexpr uint32 ObjectSize = 0x1C;
		/** A primitive's header: its length in words less one, its flag byte (bit 0: not lit) and its mode. */
		constexpr uint32 LengthByte = 1;
		constexpr uint32 FlagByte = 2;
		constexpr uint32 ModeByte = 3;
		constexpr uint8 TexturedMode = 0x04;
		constexpr uint8 QuadMode = 0x08;
		constexpr uint8 GouraudMode = 0x10;
		constexpr uint32 Lit = 0x100;
		/** A textured primitive's texels, CLUT and page, counted from its start. */
		constexpr std::array<uint32, 4> TexelPlaces{0x04, 0x08, 0x0C, 0x10};
		constexpr uint32 ClutPlace = 0x06;
		constexpr uint32 PagePlace = 0x0A;
		constexpr uint16 PageBlendBits = 0x60;
		constexpr uint8 MidGrey = 0x80;
		constexpr uint8 RawTexels = 0x01;
		constexpr uint8 SemiTransparent = 0x02;

		/** Where a kind of primitive keeps its colours, its normals and its vertices, counted from its start. */
		struct FLayout
		{
			/** The first colour's place and how many it has (4 bytes each); none: mid grey. */
			uint8 Colors = 0;
			uint8 ColorCount = 0;
			/** The first normal's place, how many the drawing lights with, and the step from one to the next. */
			uint8 Normals = 0;
			uint8 NormalCount = 0;
			uint8 NormalStep = 0;
			/** The first vertex's place and the step from one to the next. */
			uint8 Vertices = 0;
			uint8 VertexStep = 2;
			bool bKnown = false;
		};

		FLayout LayoutOf(uint32 Type)
		{
			switch (Type)
			{
			case 0x00: return {0x04, 1, 0, 0, 0, 0x08, 2, true};
			case 0x04: return {0x10, 1, 0, 0, 0, 0x14, 2, true};
			case 0x08: return {0x04, 1, 0, 0, 0, 0x08, 2, true};
			case 0x0C: return {0x14, 1, 0, 0, 0, 0x18, 2, true};
			case 0x10: return {0x04, 3, 0, 0, 0, 0x10, 2, true};
			case 0x14: return {0x10, 3, 0, 0, 0, 0x1C, 2, true};
			case 0x18: return {0x04, 4, 0, 0, 0, 0x14, 2, true};
			case 0x1C: return {0x14, 4, 0, 0, 0, 0x24, 2, true};
			case Lit | 0x00: return {0x04, 1, 0x08, 1, 0, 0x0A, 2, true};
			case Lit | 0x04: return {0, 0, 0x10, 1, 0, 0x12, 2, true};
			case Lit | 0x08: return {0x04, 1, 0x08, 1, 0, 0x0A, 2, true};
			// The lit textured quad is drawn unlit: its normal is never read.
			case Lit | 0x0C: return {0, 0, 0, 0, 0, 0x16, 2, true};
			case Lit | 0x14: return {0, 0, 0x10, 3, 4, 0x12, 4, true};
			default: return {};
			}
		}

		bool InFile(const FSpriteFile& File, uint32 Offset, uint32 Size)
		{
			return Offset <= File.Size() && Size <= File.Size() - Offset;
		}

		uint32 ReadU32(const FSpriteFile& File, uint32 Offset)
		{
			return static_cast<uint32>(File.Word(Offset)) | static_cast<uint32>(File.Word(Offset + 2)) << 16;
		}

		bool ReadVectors(const FSpriteFile& File, uint32 At, uint32 Count, std::vector<FGteVector>& Vectors)
		{
			Vectors.clear();
			if (Count > File.Size() / 8 || !InFile(File, At, Count * 8))
			{
				return false;
			}
			Vectors.reserve(Count);
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				const uint32 Offset = At + Index * 8;
				Vectors.push_back({static_cast<int16>(File.Word(Offset)), static_cast<int16>(File.Word(Offset + 2)), static_cast<int16>(File.Word(Offset + 4))});
			}
			return true;
		}

		uint32 TypeOf(const FSpriteFile& File, uint32 Primitive)
		{
			return (File.Byte(Primitive + ModeByte) & 0x1Cu) | ((File.Byte(Primitive + FlagByte) ^ 1u) & 1u) << 8;
		}
	}

	bool FBattleTmdModel::Read(const FSpriteFile& File, uint32 Object)
	{
		Primitives.clear();
		if (!InFile(File, Object, ObjectSize) || !ReadVectors(File, Object + ReadU32(File, Object + VertexTop), ReadU32(File, Object + VertexCount), Vertices)
			|| !ReadVectors(File, Object + ReadU32(File, Object + NormalTop), ReadU32(File, Object + NormalCount), Normals))
		{
			return false;
		}
		const uint32 Count = ReadU32(File, Object + PrimitiveCount);
		if (Count > File.Size() / 4)
		{
			return false;
		}
		Primitives.reserve(Count);
		uint32 At = Object + ReadU32(File, Object + PrimitiveTop);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			const uint32 Size = (static_cast<uint32>(File.Byte(At + LengthByte)) + 1) * 4;
			if (!InFile(File, At, 4) || !InFile(File, At, Size))
			{
				return false;
			}
			const uint32 Type = TypeOf(File, At);
			const FLayout Layout = LayoutOf(Type);
			FBattleTmdPrimitive Primitive;
			Primitive.Mode = File.Byte(At + ModeByte);
			Primitive.CornerCount = (Primitive.Mode & QuadMode) != 0 ? 4 : 3;
			Primitive.bTextured = (Primitive.Mode & TexturedMode) != 0;
			Primitive.bGouraud = (Primitive.Mode & GouraudMode) != 0;
			Primitive.LitColors = Layout.NormalCount;
			Primitive.bTinted = (Type & Lit) == 0 || Primitive.bTextured;
			const uint32 Last = Layout.Vertices + uint32{Layout.VertexStep} * (Primitive.CornerCount - 1u) + 2;
			if (!Layout.bKnown || Last > Size)
			{
				return false;
			}
			for (uint32 Corner = 0; Corner < Primitive.CornerCount; ++Corner)
			{
				Primitive.Vertices[Corner] = File.Word(At + Layout.Vertices + Layout.VertexStep * Corner);
				if (Primitive.Vertices[Corner] >= Vertices.size())
				{
					return false;
				}
				const uint32 Color = Layout.ColorCount == 0 ? 0 : At + Layout.Colors + 4 * std::min<uint32>(Corner, Layout.ColorCount - 1u);
				Primitive.Colors[Corner] = Layout.ColorCount == 0 ? FMdlColor{MidGrey, MidGrey, MidGrey} : FMdlColor{File.Byte(Color), File.Byte(Color + 1), File.Byte(Color + 2)};
				if (Primitive.bTextured)
				{
					Primitive.Uvs[Corner] = {File.Byte(At + TexelPlaces[Corner]), File.Byte(At + TexelPlaces[Corner] + 1)};
				}
			}
			for (uint32 Normal = 0; Normal < Layout.NormalCount; ++Normal)
			{
				Primitive.Normals[Normal] = File.Word(At + Layout.Normals + Layout.NormalStep * Normal);
				if (Primitive.Normals[Normal] >= Normals.size())
				{
					return false;
				}
			}
			if (Primitive.bTextured)
			{
				Primitive.Clut = File.Word(At + ClutPlace);
				Primitive.TexturePage = File.Word(At + PagePlace);
			}
			Primitives.push_back(Primitive);
			At += Size;
		}
		return true;
	}

	bool SetBattleTmdBlend(FSpriteFile& File, uint32 Object, uint8 Blend)
	{
		if (!InFile(File, Object, ObjectSize))
		{
			return false;
		}
		const uint32 Count = ReadU32(File, Object + PrimitiveCount);
		const uint8 Mode = static_cast<uint8>(((Blend - 1) & 3) << 5);
		uint32 At = Object + ReadU32(File, Object + PrimitiveTop);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			if (!InFile(File, At, PagePlace + 2))
			{
				return false;
			}
			if ((File.Byte(At + ModeByte) & TexturedMode) != 0)
			{
				File.SetByte(At + PagePlace, static_cast<uint8>((File.Byte(At + PagePlace) & ~PageBlendBits) | Mode));
			}
			At += (static_cast<uint32>(File.Byte(At + LengthByte)) + 1) * 4;
		}
		return true;
	}

	uint32 BattleTmdColorCode(const FBattleTmdPrimitive& Primitive, std::size_t Corner, const FSpriteModelPackets& Packets)
	{
		const auto Channel = [&Primitive, &Packets](uint8 Value, std::size_t Index)
		{
			return static_cast<uint32>(Primitive.bTinted ? std::clamp(Value + Packets.Tint[Index], 0, 0xFF) : Value);
		};
		const FMdlColor& Color = Primitive.Colors[Corner];
		uint32 Word = Channel(Color.R, 0) | Channel(Color.G, 1) << 8 | Channel(Color.B, 2) << 16;
		if (Corner == 0)
		{
			const uint8 Code = static_cast<uint8>((Primitive.Mode & ~RawTexels) | (Packets.bRawTexture ? RawTexels : 0) | (Packets.Blend != 0 ? SemiTransparent : 0));
			Word |= static_cast<uint32>(Code) << 24;
		}
		return Word;
	}
}
