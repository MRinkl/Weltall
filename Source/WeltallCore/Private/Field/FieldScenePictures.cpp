#include "Field/FieldScenePictures.h"

#include <algorithm>
#include <limits>

namespace Weltall
{
	namespace
	{
		/** A map's picture: the actor whose model it is. */
		struct FScenePicture
		{
			int32 Map = 0;
			uint16 Actor = 0;
		};

		constexpr FScenePicture Pictures[] = {{4, 14}, {39, 16}, {73, 6}, {81, 2}, {81, 3}, {81, 4}, {514, 26}, {514, 27}};

		struct FExtent
		{
			int32 Low = std::numeric_limits<int32>::max();
			int32 High = std::numeric_limits<int32>::min();

			void Add(int32 Value)
			{
				Low = std::min(Low, Value);
				High = std::max(High, Value);
			}
			int32 Length() const { return High - Low; }
		};

		/** Whether a quad is a rectangle of its plane: its corners on two columns and two rows, two on each. */
		bool IsRectangle(const FMdlPart& Part, const FMdlPrimitive& Quad, FExtent& X, FExtent& Y)
		{
			for (const uint16 Vertex : Quad.Vertices)
			{
				X.Add(Part.Vertices[Vertex][0]);
				Y.Add(Part.Vertices[Vertex][1]);
			}
			if (X.Length() == 0 || Y.Length() == 0)
			{
				return false;
			}
			int32 OnLowColumn = 0, OnLowRow = 0;
			for (const uint16 Vertex : Quad.Vertices)
			{
				const FGteVector& At = Part.Vertices[Vertex];
				if ((At[0] != X.Low && At[0] != X.High) || (At[1] != Y.Low && At[1] != Y.High))
				{
					return false;
				}
				OnLowColumn += At[0] == X.Low ? 1 : 0;
				OnLowRow += At[1] == Y.Low ? 1 : 0;
			}
			return OnLowColumn == 2 && OnLowRow == 2;
		}
	}

	bool IsScenePicture(const FMdlPart& Part)
	{
		if (Part.Primitives.empty() || Part.Mime)
		{
			return false;
		}
		FExtent Width, Height, Plane;
		int64 Area = 0;
		for (const FMdlPrimitive& Quad : Part.Primitives)
		{
			const FMdlPrimitiveTraits& Traits = Quad.Traits();
			FExtent X, Y;
			if (Traits.Corners != 4 || !Traits.bTextured || !IsRectangle(Part, Quad, X, Y))
			{
				return false;
			}
			for (const uint16 Vertex : Quad.Vertices)
			{
				Plane.Add(Part.Vertices[Vertex][2]);
			}
			Width.Add(X.Low);
			Width.Add(X.High);
			Height.Add(Y.Low);
			Height.Add(Y.High);
			Area += static_cast<int64>(X.Length()) * Y.Length();
		}
		// Rectangles lying in one plane, none over another, filling a 4:3 rectangle between them.
		return Plane.Length() == 0 && Width.Length() * 3 == Height.Length() * 4 && Area == static_cast<int64>(Width.Length()) * Height.Length();
	}

	void MarkScenePictures(int32 Map, FFieldScene& Scene)
	{
		for (const FScenePicture& Picture : Pictures)
		{
			if (Picture.Map != Map)
			{
				continue;
			}
			for (FFieldSceneObject& Object : Scene.Objects)
			{
				if (Object.Actor == Picture.Actor && IsScenePicture(Scene.PartOf(Object)))
				{
					Object.bPicture = true;
				}
			}
		}
	}
}
