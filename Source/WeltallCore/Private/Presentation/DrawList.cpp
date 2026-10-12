#include "Presentation/DrawList.h"

#include <algorithm>
#include <utility>

namespace Weltall
{
	namespace
	{
		/** The corners of a rectangle in the order a four-point polygon takes them: top left, top right, bottom left, bottom right. */
		std::array<FDrawVertex, 4> RectCorners(float X, float Y, float Width, float Height)
		{
			std::array<FDrawVertex, 4> Corners{};
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				Corners[Corner].X = X + (Corner & 1 ? Width : 0.0f);
				Corners[Corner].Y = Y + (Corner & 2 ? Height : 0.0f);
			}
			return Corners;
		}
	}

	FDrawPrimitive MakeGouraudRect(const FScreenRect& Rect, const std::array<FDrawColor, 4>& Colors)
	{
		FDrawPrimitive Primitive;
		Primitive.Corners = RectCorners(Rect.X, Rect.Y, Rect.Width, Rect.Height);
		for (std::size_t Corner = 0; Corner < 4; ++Corner)
		{
			Primitive.Corners[Corner].Color = Colors[Corner];
		}
		Primitive.bGouraud = true;
		return Primitive;
	}

	std::vector<int32> MatchBlendKeys(const FDrawList& Earlier, const FDrawList& Later)
	{
		std::vector<int32> Matches;
		std::vector<std::pair<uint32, int32>> Keyed;
		MatchBlendKeys(Earlier, Later, Matches, Keyed);
		return Matches;
	}

	void MatchBlendKeys(const FDrawList& Earlier, const FDrawList& Later, std::vector<int32>& Matches, std::vector<std::pair<uint32, int32>>& Keyed)
	{
		Keyed.clear();
		for (std::size_t Index = 0; Index < Earlier.Primitives.size(); ++Index)
		{
			if (Earlier.Primitives[Index].BlendKey != 0)
			{
				Keyed.emplace_back(Earlier.Primitives[Index].BlendKey, static_cast<int32>(Index));
			}
		}
		std::sort(Keyed.begin(), Keyed.end());
		Matches.assign(Later.Primitives.size(), -1);
		for (std::size_t Index = 0; Index < Later.Primitives.size(); ++Index)
		{
			const FDrawPrimitive& Primitive = Later.Primitives[Index];
			if (Primitive.BlendKey == 0)
			{
				continue;
			}
			const auto Found = std::lower_bound(Keyed.begin(), Keyed.end(), std::pair<uint32, int32>(Primitive.BlendKey, -1));
			if (Found != Keyed.end() && Found->first == Primitive.BlendKey
				&& Earlier.Primitives[static_cast<std::size_t>(Found->second)].CornerCount == Primitive.CornerCount)
			{
				Matches[Index] = Found->second;
			}
		}
	}

	FDrawPrimitive MakeSprite(int32 X, int32 Y, int32 Width, int32 Height, const FDrawTexture& Texture, uint8 U, uint8 V, FDrawColor Color)
	{
		FDrawPrimitive Primitive;
		Primitive.Corners = RectCorners(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Width), static_cast<float>(Height));
		for (FDrawVertex& Corner : Primitive.Corners)
		{
			Corner.Color = Color;
			Corner.U = U + (Corner.X - X);
			Corner.V = V + (Corner.Y - Y);
		}
		Primitive.Texture = Texture;
		return Primitive;
	}
}
