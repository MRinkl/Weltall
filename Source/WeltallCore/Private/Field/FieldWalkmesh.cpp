#include "Field/FieldWalkmesh.h"

#include "Foundation/LittleEndian.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		constexpr uint32 MaxLayers = 4;
		constexpr std::size_t HeaderBytes = 0x18;
		constexpr std::size_t LayerOffsetsBytes = 8;
		constexpr std::size_t TriangleBytes = 14;
		constexpr std::size_t VertexBytes = 8;
		constexpr std::size_t AttributeCount = 256;

		bool Has(std::span<const uint8> Bytes, std::size_t Offset, std::size_t Count)
		{
			return Offset <= Bytes.size() && Count <= Bytes.size() - Offset;
		}

		TResult<FWalkmeshLayer> ReadLayer(std::span<const uint8> Section, std::size_t TrianglesAt, std::size_t TriangleCount, std::size_t VerticesAt, std::size_t AttributeCountPresent)
		{
			if (!Has(Section, TrianglesAt, TriangleCount * TriangleBytes))
			{
				return MakeError("walkmesh triangles run past the section");
			}
			FWalkmeshLayer Layer;
			Layer.Triangles.resize(TriangleCount);
			std::size_t VertexCount = 0;
			for (std::size_t Index = 0; Index < TriangleCount; ++Index)
			{
				const std::size_t At = TrianglesAt + Index * TriangleBytes;
				FWalkmeshTriangle& Triangle = Layer.Triangles[Index];
				for (std::size_t Corner = 0; Corner < 3; ++Corner)
				{
					Triangle.Vertices[Corner] = ReadU16(Section, At + Corner * 2);
					Triangle.Neighbours[Corner] = ReadS16(Section, At + 6 + Corner * 2);
					VertexCount = std::max<std::size_t>(VertexCount, Triangle.Vertices[Corner] + std::size_t{1});
					if (Triangle.Neighbours[Corner] < -1 || Triangle.Neighbours[Corner] >= static_cast<int32>(TriangleCount))
					{
						return MakeError("walkmesh triangle " + std::to_string(Index) + " names a neighbour its layer lacks");
					}
				}
				Triangle.Attribute = Section[At + 12];
				Triangle.Ceiling = static_cast<int8>(Section[At + 13]);
				if (Triangle.Attribute >= AttributeCountPresent)
				{
					return MakeError("walkmesh triangle " + std::to_string(Index) + " uses an attribute word the section lacks");
				}
			}
			// The table holds no count: the field reads each vertex a triangle names.
			if (!Has(Section, VerticesAt, VertexCount * VertexBytes))
			{
				return MakeError("walkmesh vertices run past the section");
			}
			Layer.Vertices.resize(VertexCount);
			for (std::size_t Index = 0; Index < VertexCount; ++Index)
			{
				const std::size_t At = VerticesAt + Index * VertexBytes;
				Layer.Vertices[Index] = {ReadS16(Section, At), ReadS16(Section, At + 2), ReadS16(Section, At + 4)};
			}
			return Layer;
		}
	}

	TResult<FFieldWalkmesh> ParseFieldWalkmesh(std::span<const uint8> Section)
	{
		if (!Has(Section, 0, HeaderBytes))
		{
			return MakeError("walkmesh section is shorter than its header");
		}
		const uint32 LayerCount = ReadU32(Section, 0);
		if (LayerCount < 1 || LayerCount > MaxLayers || !Has(Section, HeaderBytes, LayerCount * LayerOffsetsBytes))
		{
			return MakeError("walkmesh has " + std::to_string(LayerCount) + " layers");
		}
		const std::size_t AttributesAt = ReadU32(Section, 0x14);
		if (AttributesAt > Section.size())
		{
			return MakeError("walkmesh attribute words lie past the section");
		}
		FFieldWalkmesh Walkmesh;
		Walkmesh.Attributes.resize(std::min(AttributeCount, (Section.size() - AttributesAt) / 4));
		for (std::size_t Index = 0; Index < Walkmesh.Attributes.size(); ++Index)
		{
			Walkmesh.Attributes[Index] = ReadU32(Section, AttributesAt + Index * 4);
		}
		for (uint32 Number = 0; Number < LayerCount; ++Number)
		{
			const std::size_t Offsets = HeaderBytes + Number * LayerOffsetsBytes;
			const std::size_t TriangleCount = ReadU32(Section, 4 + Number * 4) / TriangleBytes;
			TResult<FWalkmeshLayer> Layer = ReadLayer(Section, ReadU32(Section, Offsets), TriangleCount, ReadU32(Section, Offsets + 4), Walkmesh.Attributes.size());
			if (!Layer)
			{
				return MakeError("layer " + std::to_string(Number) + ": " + Layer.Error().Message);
			}
			Walkmesh.Layers.push_back(std::move(*Layer));
		}
		return Walkmesh;
	}
}
