#include "Graphics/MdlMime.h"

namespace Weltall
{
	void AnimateMdlMime(const FMdlPart& Part, std::span<const int32> Weights, std::span<FGteVector> Vertices)
	{
		const FMdlMime& Mime = *Part.Mime;
		for (const uint16 Vertex : Mime.ResetVertices)
		{
			Vertices[Vertex] = Part.Vertices[Vertex];
		}
		for (std::size_t Key = 0; Key < Mime.Keys.size(); ++Key)
		{
			const int32 Weight = Key < Weights.size() ? Weights[Key] : 0;
			if (Weight == 0)
			{
				continue;
			}
			for (const FMdlVertexDelta& Delta : Mime.Keys[Key].VertexDeltas)
			{
				FGteVector& Moved = Vertices[Delta.Vertex];
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					const int32 Step = static_cast<int32>(static_cast<uint32>(Delta.Delta[Axis]) * static_cast<uint32>(Weight)) >> 12;
					Moved[Axis] = static_cast<int16>(Moved[Axis] + static_cast<int16>(Step));
				}
			}
		}
	}
}
