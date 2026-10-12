#include "Graphics/MdlModel.h"

#include "Foundation/LittleEndian.h"

#include <string>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		constexpr std::size_t ModelHeaderBytes = 0x10;
		constexpr std::size_t PartBytes = 0x38;
		constexpr uint32 RelocatedFlag = 1;
		constexpr uint16 MimeMovesNormalsFlag = 0x10;
		/** A part's flag once the kernel has turned its offsets into addresses. */
		constexpr uint16 PartRelocatedFlag = 0x20;
		constexpr uint8 TexturePageCommand = 0xC4;
		constexpr uint8 ClutCommand = 0xC8;
		constexpr std::size_t VectorBytes = 8;
		constexpr std::size_t GroupHeaderBytes = 4;
		constexpr std::size_t IndexBytes = 8;
		constexpr std::size_t MimeEntryBytes = 12;
		/** The kernel builds these with SetPolyFT3 and SetShadeTex on an 8-bit page and a CLUT at its texture base. */
		constexpr uint8 EnvironmentMappedCode = 0x25;
		constexpr uint16 EnvironmentMappedPage = 0x80;

		constexpr FMdlPrimitiveTraits Traits[MdlPrimitiveTypeCount] = {
			{3, false, false, true, 4},
			{3, true, false, true, 8},
			{3, false, true, true, 4},
			{3, true, true, true, 8},
			{3, false, false, false, 4},
			{3, true, false, false, 8},
			{3, false, true, false, 4},
			{3, true, true, false, 8},
			{4, false, false, true, 4},
			{4, true, false, true, 12},
			{4, false, true, true, 4},
			{4, true, true, true, 12},
			{4, false, false, false, 4},
			{4, true, false, false, 12},
			{4, false, true, false, 4},
			{4, true, true, false, 12},
			{3, true, false, false, 4},
		};

		bool Has(std::span<const uint8> Bytes, std::size_t Offset, std::size_t Count)
		{
			return Offset <= Bytes.size() && Count <= Bytes.size() - Offset;
		}

		/** Texture page and CLUT commands come before the textured kinds that read their own source. */
		bool ReadsTextureCommands(const FMdlPrimitiveTraits& Kind)
		{
			return Kind.bTextured && Kind.SourceBytes != 4;
		}

		FMdlUv ReadUv(std::span<const uint8> Bytes, std::size_t Offset)
		{
			return {Bytes[Offset], Bytes[Offset + 1]};
		}

		TResult<std::vector<FGteVector>> ReadVectors(std::span<const uint8> Bytes, std::size_t Offset, std::size_t Count)
		{
			if (!Has(Bytes, Offset, Count * VectorBytes))
			{
				return MakeError("MDL vectors run past the model");
			}
			std::vector<FGteVector> Vectors(Count);
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				const std::size_t At = Offset + Index * VectorBytes;
				Vectors[Index] = {ReadS16(Bytes, At), ReadS16(Bytes, At + 2), ReadS16(Bytes, At + 4)};
			}
			return Vectors;
		}

		TResult<std::vector<FMdlVertexDelta>> ReadDeltas(std::span<const uint8> Bytes, std::size_t Offset, int32 Count, std::size_t VertexCount)
		{
			if (Count < 0 || !Has(Bytes, Offset, static_cast<std::size_t>(Count) * VectorBytes))
			{
				return MakeError("MIMe deltas run past the model");
			}
			std::vector<FMdlVertexDelta> Deltas(static_cast<std::size_t>(Count));
			for (std::size_t Index = 0; Index < Deltas.size(); ++Index)
			{
				const std::size_t At = Offset + Index * VectorBytes;
				Deltas[Index] = {{ReadS16(Bytes, At), ReadS16(Bytes, At + 2), ReadS16(Bytes, At + 4)}, ReadU16(Bytes, At + 6)};
				if (Deltas[Index].Vertex >= VertexCount)
				{
					return MakeError("MIMe delta moves a vertex the part does not have");
				}
			}
			return Deltas;
		}

		TResult<FMdlMime> ReadMime(std::span<const uint8> Bytes, std::size_t Offset, std::size_t VertexCount, bool bMovesNormals)
		{
			if (!Has(Bytes, Offset, 4))
			{
				return MakeError("MIMe data lies past the model");
			}
			const int32 KeyCount = static_cast<int32>(ReadU32(Bytes, Offset));
			const std::size_t Entries = Offset + 4;
			if (KeyCount < 0 || !Has(Bytes, Entries, (static_cast<std::size_t>(KeyCount) + 1) * MimeEntryBytes))
			{
				return MakeError("MIMe key table runs past the model");
			}
			FMdlMime Mime;
			Mime.Keys.resize(static_cast<std::size_t>(KeyCount));
			for (std::size_t Key = 0; Key < Mime.Keys.size(); ++Key)
			{
				const std::size_t Entry = Entries + Key * MimeEntryBytes;
				const int32 Count = static_cast<int32>(ReadU32(Bytes, Entry));
				TResult<std::vector<FMdlVertexDelta>> Vertices = ReadDeltas(Bytes, ReadU32(Bytes, Entry + 4), Count, VertexCount);
				if (!Vertices)
				{
					return Vertices.Error();
				}
				Mime.Keys[Key].VertexDeltas = std::move(*Vertices);
				if (bMovesNormals)
				{
					TResult<std::vector<FMdlVertexDelta>> Normals = ReadDeltas(Bytes, ReadU32(Bytes, Entry + 8), Count, VertexCount);
					if (!Normals)
					{
						return Normals.Error();
					}
					Mime.Keys[Key].NormalDeltas = std::move(*Normals);
				}
			}
			const std::size_t ResetEntry = Entries + Mime.Keys.size() * MimeEntryBytes;
			const int32 ResetCount = static_cast<int32>(ReadU32(Bytes, ResetEntry));
			const std::size_t ResetAt = ReadU32(Bytes, ResetEntry + 4);
			if (ResetCount < 0 || !Has(Bytes, ResetAt, static_cast<std::size_t>(ResetCount) * 2))
			{
				return MakeError("MIMe reset list runs past the model");
			}
			for (int32 Index = 0; Index < ResetCount; ++Index)
			{
				const uint16 Vertex = ReadU16(Bytes, ResetAt + static_cast<std::size_t>(Index) * 2);
				if (Vertex >= VertexCount)
				{
					return MakeError("MIMe resets a vertex the part does not have");
				}
				Mime.ResetVertices.push_back(Vertex);
			}
			return Mime;
		}

		/** A primitive's colour, code and UVs from its packet source, as the kind's builder takes them. */
		void ReadSource(std::span<const uint8> Bytes, std::size_t At, FMdlPrimitive& Primitive)
		{
			const FMdlPrimitiveTraits& Kind = Primitive.Traits();
			if (Primitive.Type == EMdlPrimitiveType::EnvironmentMappedTriangle)
			{
				Primitive.Code = EnvironmentMappedCode;
				Primitive.TexturePage = EnvironmentMappedPage;
				return;
			}
			Primitive.Code = Bytes[At + 3];
			if (Kind.SourceBytes == 8)
			{
				Primitive.Uvs = {ReadUv(Bytes, At + 4), ReadUv(Bytes, At + 6), ReadUv(Bytes, At)};
				return;
			}
			Primitive.Color = {Bytes[At], Bytes[At + 1], Bytes[At + 2]};
			if (Kind.SourceBytes == 12)
			{
				Primitive.Uvs = {ReadUv(Bytes, At + 4), ReadUv(Bytes, At + 6), ReadUv(Bytes, At + 8), ReadUv(Bytes, At + 10)};
			}
		}

		TResult<std::vector<FMdlPrimitive>> ReadPrimitives(std::span<const uint8> Bytes, std::size_t Group, uint16 GroupCount, std::size_t Source, std::size_t VertexCount)
		{
			std::vector<FMdlPrimitive> Primitives;
			std::optional<uint16> Page;
			std::optional<uint16> Clut;
			for (uint16 GroupNumber = 0; GroupNumber < GroupCount; ++GroupNumber)
			{
				if (!Has(Bytes, Group, GroupHeaderBytes))
				{
					return MakeError("MDL primitive groups run past the model");
				}
				const uint8 Type = Bytes[Group];
				const int16 Count = ReadS16(Bytes, Group + 2);
				if (Type >= MdlPrimitiveTypeCount)
				{
					return MakeError("MDL primitive group of unknown type " + std::to_string(Type));
				}
				if (Count < 0 || !Has(Bytes, Group + GroupHeaderBytes, static_cast<std::size_t>(Count) * IndexBytes))
				{
					return MakeError("MDL primitive group runs past the model");
				}
				const FMdlPrimitiveTraits& Kind = Traits[Type];
				for (int16 Number = 0; Number < Count; ++Number)
				{
					FMdlPrimitive Primitive;
					Primitive.Type = static_cast<EMdlPrimitiveType>(Type);
					const std::size_t Indices = Group + GroupHeaderBytes + static_cast<std::size_t>(Number) * IndexBytes;
					for (std::size_t Corner = 0; Corner < 4; ++Corner)
					{
						Primitive.Vertices[Corner] = ReadU16(Bytes, Indices + Corner * 2);
						if (Corner < Kind.Corners && Primitive.Vertices[Corner] >= VertexCount)
						{
							return MakeError("MDL primitive uses a vertex the part does not have");
						}
					}
					while (ReadsTextureCommands(Kind) && Has(Bytes, Source, 4)
						&& (Bytes[Source + 3] == TexturePageCommand || Bytes[Source + 3] == ClutCommand))
					{
						(Bytes[Source + 3] == TexturePageCommand ? Page : Clut) = ReadU16(Bytes, Source);
						Source += 4;
					}
					if (!Has(Bytes, Source, Kind.SourceBytes))
					{
						return MakeError("MDL packet source runs past the model");
					}
					ReadSource(Bytes, Source, Primitive);
					Source += Kind.SourceBytes;
					if (ReadsTextureCommands(Kind))
					{
						Primitive.bInheritsTexturePage = !Page.has_value();
						Primitive.bInheritsClut = !Clut.has_value();
						Primitive.TexturePage = Page.value_or(0);
						Primitive.Clut = Clut.value_or(0);
					}
					Primitives.push_back(Primitive);
				}
				Group += GroupHeaderBytes + static_cast<std::size_t>(Count) * IndexBytes;
			}
			return Primitives;
		}

		TResult<FMdlPart> ReadPart(std::span<const uint8> Bytes, std::size_t At)
		{
			const uint16 Flags = ReadU16(Bytes, At);
			const uint16 VertexCount = ReadU16(Bytes, At + 2);
			const uint16 GroupCount = ReadU16(Bytes, At + 6);
			const uint32 VerticesAt = ReadU32(Bytes, At + 8);
			const uint32 NormalsAt = ReadU32(Bytes, At + 0xC);
			const uint32 MimeAt = ReadU32(Bytes, At + 0x1C);
			FMdlPart Part;
			Part.BoundsMin = {ReadS16(Bytes, At + 0x20), ReadS16(Bytes, At + 0x22), ReadS16(Bytes, At + 0x24)};
			Part.BoundsMax = {ReadS16(Bytes, At + 0x28), ReadS16(Bytes, At + 0x2A), ReadS16(Bytes, At + 0x2C)};
			Part.LightBufferBytes = ReadU32(Bytes, At + 0x30);
			Part.bMimeMovesNormals = (Flags & MimeMovesNormalsFlag) != 0;
			TResult<std::vector<FGteVector>> Vertices = ReadVectors(Bytes, VerticesAt, VertexCount);
			if (!Vertices)
			{
				return Vertices.Error();
			}
			Part.Vertices = std::move(*Vertices);
			// Parts without smooth shading point their normals at their vertices.
			if (NormalsAt != VerticesAt)
			{
				TResult<std::vector<FGteVector>> Normals = ReadVectors(Bytes, NormalsAt, VertexCount);
				if (!Normals)
				{
					return Normals.Error();
				}
				Part.Normals = std::move(*Normals);
			}
			TResult<std::vector<FMdlPrimitive>> Primitives = ReadPrimitives(Bytes, ReadU32(Bytes, At + 0x10), GroupCount, ReadU32(Bytes, At + 0x14), VertexCount);
			if (!Primitives)
			{
				return Primitives.Error();
			}
			Part.Primitives = std::move(*Primitives);
			for (const FMdlPrimitive& Primitive : Part.Primitives)
			{
				if (Primitive.Traits().bGouraud && Part.Normals.empty())
				{
					return MakeError("MDL part has Gouraud polygons but no normals");
				}
			}
			if (MimeAt != 0)
			{
				TResult<FMdlMime> Mime = ReadMime(Bytes, MimeAt, VertexCount, Part.bMimeMovesNormals);
				if (!Mime)
				{
					return Mime.Error();
				}
				Part.Mime = std::move(*Mime);
			}
			return Part;
		}
	}

	const FMdlPrimitiveTraits& MdlPrimitiveTraits(EMdlPrimitiveType Type)
	{
		return Traits[static_cast<uint8>(Type)];
	}

	FMdlTextureState FMdlPart::TextureStateAfter(const FMdlTextureState& State) const
	{
		for (auto Primitive = Primitives.rbegin(); Primitive != Primitives.rend(); ++Primitive)
		{
			if (ReadsTextureCommands(Primitive->Traits()))
			{
				return Primitive->Texture(State);
			}
		}
		return State;
	}

	TResult<FMdlModel> ParseMdlModel(std::span<const uint8> Bytes)
	{
		if (!Has(Bytes, 0, ModelHeaderBytes))
		{
			return MakeError("MDL model is shorter than its header");
		}
		const int32 PartCount = static_cast<int32>(ReadU32(Bytes, 0));
		if ((ReadU32(Bytes, 4) & RelocatedFlag) != 0)
		{
			return MakeError("MDL model's offsets are already relocated");
		}
		const std::size_t Count = PartCount > 0 ? static_cast<std::size_t>(PartCount) : 0;
		if (!Has(Bytes, ModelHeaderBytes, Count * PartBytes))
		{
			return MakeError("MDL model's parts run past it");
		}
		FMdlModel Model;
		for (std::size_t Part = 0; Part < Count; ++Part)
		{
			TResult<FMdlPart> Parsed = ReadPart(Bytes, ModelHeaderBytes + Part * PartBytes);
			if (!Parsed)
			{
				return MakeError("part " + std::to_string(Part) + ": " + Parsed.Error().Message);
			}
			Model.Parts.push_back(std::move(*Parsed));
		}
		return Model;
	}

	TResult<FMdlPart> ParseMdlPart(std::span<const uint8> Bytes)
	{
		if (!Has(Bytes, 0, PartBytes))
		{
			return MakeError("MDL part is shorter than its header");
		}
		if ((ReadU16(Bytes, 0) & PartRelocatedFlag) != 0)
		{
			return MakeError("MDL part's offsets are already relocated");
		}
		return ReadPart(Bytes, 0);
	}

	TResult<std::vector<FMdlModel>> ParseMdlModelList(std::span<const uint8> Bytes)
	{
		if (!Has(Bytes, 0, 4))
		{
			return MakeError("MDL model list has no count");
		}
		const uint32 Count = ReadU32(Bytes, 0);
		if (!Has(Bytes, 4, static_cast<std::size_t>(Count) * 4))
		{
			return MakeError("MDL model list's offsets run past it");
		}
		std::vector<FMdlModel> Models;
		Models.reserve(Count);
		for (uint32 Number = 0; Number < Count; ++Number)
		{
			const uint32 Offset = ReadU32(Bytes, 4 + static_cast<std::size_t>(Number) * 4);
			if (Offset > Bytes.size())
			{
				return MakeError("model " + std::to_string(Number) + " lies past its list");
			}
			TResult<FMdlModel> Model = ParseMdlModel(Bytes.subspan(Offset));
			if (!Model)
			{
				return MakeError("model " + std::to_string(Number) + ": " + Model.Error().Message);
			}
			Models.push_back(std::move(*Model));
		}
		return Models;
	}
}
