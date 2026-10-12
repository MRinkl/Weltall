#pragma once

#include "Foundation/Types.h"
#include "Graphics/Gte.h"

#include <array>

namespace Weltall
{
	/** A 32-bit vector (libgte's VECTOR); the GTE takes only 16 bits of each component. */
	using FGteLongVector = std::array<int32, 3>;

	/**
	 * libgte's VectorNormal: V scaled to length 4096 by the library's square-root table and the GTE,
	 * as this game's copy computes it. Each component counts only its low 16 bits, and their squares
	 * must sum below 2^31 (the library's addition traps above that).
	 */
	WELTALLCORE_API FGteLongVector VectorNormal(const FGteLongVector& Vector);

	/** libgte's VectorNormalS: VectorNormal kept to 16 bits a component. */
	WELTALLCORE_API FGteVector VectorNormalS(const FGteLongVector& Vector);

	/** The kernel's largest component: the component of the largest magnitude, ties to the first; signed as it is. */
	WELTALLCORE_API int32 LargestComponent(const FGteLongVector& Vector);

	/** libgte's SquareRoot0: the square root of a non-negative value from the library's table, rounded down. */
	WELTALLCORE_API int32 SquareRoot0(int32 Value);

	/** libgte's OuterProduct12: A x B >> 12 through the GTE's outer product, 16 bits of each input component. */
	WELTALLCORE_API FGteLongVector OuterProduct12(const FGteLongVector& A, const FGteLongVector& B);

	/** libgte's OuterProduct0: A x B without the shift, 16 bits of each input component. */
	WELTALLCORE_API FGteLongVector OuterProduct0(const FGteLongVector& A, const FGteLongVector& B);

	/** libgte's ApplyMatrix: M * V >> 12, without the translation. */
	WELTALLCORE_API FGteLongVector ApplyMatrix(const FGteMatrix& Matrix, const FGteVector& Vector);

	/** libgte's RotTrans: M * V >> 12 + T, the GTE's MAC values, and its flag register when asked. */
	WELTALLCORE_API FGteLongVector RotTrans(const FGteMatrix& Matrix, const FGteVector& Vector, uint32* Flag = nullptr);

	/** libgte's ApplyMatrixSV: M * V >> 12 as the GTE's IR registers keep it, each component saturated to 16 bits. */
	WELTALLCORE_API FGteVector ApplyMatrixSV(const FGteMatrix& Matrix, const FGteVector& Vector);

	/** libgte's RotTransSV: M * V >> 12 + T as the GTE's IR registers keep it, each component saturated to 16 bits. */
	WELTALLCORE_API FGteVector RotTransSV(const FGteMatrix& Matrix, const FGteVector& Vector);

	/**
	 * libgte's CompMatrix: A * B for the rotations (as MulMatrix), and A's rotation applied to B's
	 * translation plus A's: B's translation counts only 16 bits of each component.
	 */
	WELTALLCORE_API FGteMatrix CompMatrix(const FGteMatrix& A, const FGteMatrix& B);

	/**
	 * The game's own rotation matrix from three angles (4096 = a turn): Rx * Ry * Rz, each product
	 * truncated in the order the kernel's routine takes them. The translation is left zero.
	 */
	WELTALLCORE_API FGteMatrix RotationXyz(const FGteVector& Angles);

	/**
	 * libgte's RotMatrixZYX: Rz * Ry * Rx from three angles, each product truncated as the library
	 * takes them; a negative angle's sine is the negated sine of its magnitude. No translation.
	 */
	WELTALLCORE_API FGteMatrix RotationZyx(const FGteVector& Angles);

	/**
	 * libgte's RotMatrixYXZ: Ry * Rx * Rz from three angles, each product truncated as the library
	 * takes them; a negative angle's sine is the negated sine of its magnitude. No translation.
	 */
	WELTALLCORE_API FGteMatrix RotationYxz(const FGteVector& Angles);

	/**
	 * The game's own face normal for a triangle: (P2 - P0) x (P1 - P0), divided down by the square
	 * root of its largest component and normalized. A degenerate triangle divides by zero, which the
	 * R3000 answers with -1 for each component.
	 */
	WELTALLCORE_API FGteVector TriangleNormal(const FGteVector& P0, const FGteVector& P1, const FGteVector& P2);

	/**
	 * Whether three projected corners wind clockwise on the screen (NCLIP above zero), found from where they lie in view
	 * space, so it holds for corners the GTE cannot project too (behind the eye).
	 */
	WELTALLCORE_API bool WindsClockwiseFromEye(const FGteProjection& A, const FGteProjection& B, const FGteProjection& C);
}
