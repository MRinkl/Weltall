#pragma once

#include "Foundation/Types.h"
#include "Graphics/GteMatrix.h"

#include <array>

namespace Weltall
{
	/** Rows (x, y, z, then the translation) taking a point of the latest frame's view space to where it is drawn between two frames. */
	using FViewMove = std::array<std::array<float, 4>, 3>;

	inline constexpr FViewMove UnmovedView{{{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}}};

	WELTALLCORE_API std::array<float, 3> MoveViewPoint(const FViewMove& Move, const std::array<float, 3>& Point);

	/** A GTE matrix as floats: the rotation and scale in units of one, the translation in whole units. */
	struct FAffine
	{
		std::array<std::array<float, 3>, 3> M{};
		std::array<float, 3> T{};

		WELTALLCORE_API static FAffine From(const FGteMatrix& Matrix);

		/** A direction through the rotation and scale alone. */
		WELTALLCORE_API std::array<float, 3> Turn(const std::array<float, 3>& Point) const;

		/** The inverse, or false where the matrix flattens space. */
		WELTALLCORE_API bool Invert(FAffine& Inverse) const;

		/** This after Inner. */
		WELTALLCORE_API FAffine After(const FAffine& Inner) const;

		WELTALLCORE_API FViewMove ToMove() const;
	};

	/** Each column's direction and length, and the translation, Alpha of the way from From to To; false where a column turned about. */
	WELTALLCORE_API bool BlendAffine(const FAffine& From, const FAffine& To, float Alpha, FAffine& Blended);

	/**
	 * What takes a point To puts in view space to where a matrix Alpha of the way from From to To puts the same local point
	 * (BlendAffine's); the unmoved view where they do not blend or To flattens space.
	 */
	WELTALLCORE_API FViewMove BlendedViewMove(const FAffine& From, const FAffine& To, float Alpha);
}
