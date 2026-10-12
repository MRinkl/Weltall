#pragma once

#include "Foundation/Types.h"
#include "Graphics/GteMatrix.h"

#include <array>

namespace Weltall
{
	/** A vertex as the GTE takes it (libgte's SVECTOR). */
	using FGteVector = std::array<int16, 3>;

	/** A point through a GTE matrix into view space, without the GTE's rounding. */
	inline std::array<float, 3> ViewPoint(const FGteMatrix& Matrix, const FGteVector& Local)
	{
		std::array<float, 3> View{};
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			const float Turned = static_cast<float>(Matrix.M[Row][0]) * Local[0] + static_cast<float>(Matrix.M[Row][1]) * Local[1]
				+ static_cast<float>(Matrix.M[Row][2]) * Local[2];
			View[Row] = Turned / static_cast<float>(FGteMatrix::One) + static_cast<float>(Matrix.T[Row]);
		}
		return View;
	}

	/** The GTE's flag register bits that game code reads. */
	namespace GteFlag
	{
		constexpr uint32 Ir1Saturated = 1u << 24;
		constexpr uint32 Ir2Saturated = 1u << 23;
		/** With sf = 1, set once a view depth passes 7FFFh; not part of the error bit. */
		constexpr uint32 Ir3Saturated = 1u << 22;
		/** SZ3 or OTZ outside 0..FFFFh: behind the eye, or too far. */
		constexpr uint32 DepthSaturated = 1u << 18;
		constexpr uint32 DivideOverflow = 1u << 17;
		constexpr uint32 ScreenXSaturated = 1u << 14;
		constexpr uint32 ScreenYSaturated = 1u << 13;
		constexpr uint32 DepthCueSaturated = 1u << 12;
		/** Set whenever any of bits 30-23 or 18-13 is. */
		constexpr uint32 Error = 1u << 31;
	}

	/** A color as the GTE's color FIFO holds it: 0-255 per channel and the primitive's GPU code. */
	struct FGteColor
	{
		uint8 R = 0;
		uint8 G = 0;
		uint8 B = 0;
		uint8 Code = 0;

		bool operator==(const FGteColor&) const = default;
	};

	/** What RTPS leaves for one vertex. */
	struct FGteProjection
	{
		/** SX2, SY2: screen coordinates relative to the offset registers' origin. */
		int16 ScreenX = 0;
		int16 ScreenY = 0;
		/** SZ3. */
		uint16 ScreenZ = 0;
		/** IR0, the depth-cue fraction (4096 = fully fogged). */
		int16 DepthCue = 0;
		uint32 Flag = 0;
		/** MAC1-3: the vertex in view space, before any saturation. */
		std::array<int32, 3> View{};
	};

	/**
	 * The GTE's commands as psx-spx specifies them, with sf = 1 as libgte issues them: exact integer
	 * arithmetic, the division unit's approximation and the flags included.
	 */
	struct FGte
	{
		/** RT and TR, as SetRotMatrix and SetTransMatrix load them. */
		FGteMatrix Transform;
		/** OFX, OFY in 16.16. */
		int32 OffsetX = 0;
		int32 OffsetY = 0;
		/** H. */
		uint16 ProjectionDistance = 0;
		/** DQA, DQB. */
		int16 DepthCueA = 0;
		int32 DepthCueB = 0;
		/** RFC, GFC, BFC: the far color depth cueing fades toward, a 0-255 color times 16 as SetFarColor loads it. */
		std::array<int32, 3> FarColor{};
		/** LLM: row i is light i's direction, 4.12. Only the rotation part is used. */
		FGteMatrix LightMatrix;
		/** LCM: column i is light i's color, 4.12. Only the rotation part is used. */
		FGteMatrix ColorMatrix;
		/** BK, the light every surface receives, in 4096ths (SetBackColor loads a 0-255 color times 16). */
		std::array<int32, 3> BackColor{};
		/** ZSF3, ZSF4: the average-depth scales. */
		int16 AverageScale3 = 0;
		int16 AverageScale4 = 0;

		/** RTPS: one vertex through the perspective transformation. */
		WELTALLCORE_API FGteProjection Rtps(const FGteVector& Vertex) const;

		/** NCS (lm = 1): the color the lights and the back color give a surface with this normal (4.12). */
		WELTALLCORE_API FGteColor NormalColor(const FGteVector& Normal) const;

		/** NCCS (lm = 1): NormalColor's light multiplied by Color, whose code it keeps. */
		WELTALLCORE_API FGteColor NormalColorColor(const FGteVector& Normal, const FGteColor& Color) const;

		/** DPCS: Color faded toward the far color by DepthCue (IR0, 4096 fully), keeping Color's code. */
		WELTALLCORE_API FGteColor DepthCueColor(const FGteColor& Color, int16 DepthCue) const;

		/**
		 * libgte's SetFogNearFar: DQA and DQB so that RTPS's depth cue runs from 0 at depth Near to
		 * 4096 at Far for a projection distance H; nothing changes unless Far lies more than 99 past Near.
		 * False, changing nothing, where the library divides by an H of 0, which traps.
		 */
		WELTALLCORE_API bool SetFogNearFar(int32 Near, int32 Far, int32 H);

		/** AVSZ3: OTZ from three screen depths, clamped to 0..FFFFh. */
		WELTALLCORE_API uint16 AverageDepth3(uint16 Depth1, uint16 Depth2, uint16 Depth3) const;

		/** AVSZ4: OTZ from four screen depths, clamped to 0..FFFFh. */
		WELTALLCORE_API uint16 AverageDepth4(uint16 Depth0, uint16 Depth1, uint16 Depth2, uint16 Depth3) const;
	};

	/** NCLIP: twice the signed area of a screen triangle, positive when it runs clockwise on screen. */
	WELTALLCORE_API int32 Nclip(int16 X0, int16 Y0, int16 X1, int16 Y1, int16 X2, int16 Y2);

	/** libgte's rsin: round(4096 * sin(2 pi * Angle / 4096)), which its table holds exactly. */
	WELTALLCORE_API int32 GteSin(int32 Angle);
	WELTALLCORE_API int32 GteCos(int32 Angle);

	/** RotMatrixZXY (or RotMatrix) for a rotation about y alone: [[c, 0, s], [0, 1, 0], [-s, 0, c]]. */
	WELTALLCORE_API FGteMatrix RotationY(int32 Angle);

	/** libgte's RotMatrixX of the identity: [[1, 0, 0], [0, c, -s], [0, s, c]]. */
	WELTALLCORE_API FGteMatrix RotationX(int32 Angle);

	/** libgte's RotMatrixZ of the identity: [[c, -s, 0], [s, c, 0], [0, 0, 1]]. */
	WELTALLCORE_API FGteMatrix RotationZ(int32 Angle);

	/** libgte's MulMatrix: A * B on the GTE, each entry (sum >> 12) saturated to 16 bits; T is A's. */
	WELTALLCORE_API FGteMatrix MulMatrix(const FGteMatrix& A, const FGteMatrix& B);

	/** libgte's ApplyMatrixLV: M * V >> 12 for a 32-bit vector, computed in two halves as libgte does. */
	WELTALLCORE_API std::array<int32, 3> ApplyMatrixLV(const FGteMatrix& Matrix, const std::array<int32, 3>& Vector);

	WELTALLCORE_API FGteMatrix Transpose(const FGteMatrix& Matrix);

	/**
	 * libgte's ScaleMatrixL: multiplies row i by Scale[i] (4.12), keeping 16 bits of each product
	 * shifted right by 12.
	 */
	WELTALLCORE_API FGteMatrix ScaleRows(const FGteMatrix& Matrix, const std::array<int32, 3>& Scale);
}
