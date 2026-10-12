#include "Graphics/AffineBlend.h"

#include <cmath>

namespace Weltall
{
	namespace
	{
		float Lerp(float From, float To, float Alpha)
		{
			return From + (To - From) * Alpha;
		}

		float Length(const std::array<float, 3>& Vector)
		{
			return std::sqrt(Vector[0] * Vector[0] + Vector[1] * Vector[1] + Vector[2] * Vector[2]);
		}
	}

	std::array<float, 3> MoveViewPoint(const FViewMove& Move, const std::array<float, 3>& Point)
	{
		std::array<float, 3> Result{};
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			Result[Row] = Move[Row][0] * Point[0] + Move[Row][1] * Point[1] + Move[Row][2] * Point[2] + Move[Row][3];
		}
		return Result;
	}

	FAffine FAffine::From(const FGteMatrix& Matrix)
	{
		FAffine Result;
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				Result.M[Row][Column] = static_cast<float>(Matrix.M[Row][Column]) / static_cast<float>(FGteMatrix::One);
			}
			Result.T[Row] = static_cast<float>(Matrix.T[Row]);
		}
		return Result;
	}

	std::array<float, 3> FAffine::Turn(const std::array<float, 3>& Point) const
	{
		std::array<float, 3> Result{};
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			Result[Row] = M[Row][0] * Point[0] + M[Row][1] * Point[1] + M[Row][2] * Point[2];
		}
		return Result;
	}

	bool FAffine::Invert(FAffine& Inverse) const
	{
		const float Determinant = M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0])
			+ M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
		if (std::fabs(Determinant) < 1e-6f)
		{
			return false;
		}
		const float Scale = 1.0f / Determinant;
		Inverse.M[0] = {(M[1][1] * M[2][2] - M[1][2] * M[2][1]) * Scale, (M[0][2] * M[2][1] - M[0][1] * M[2][2]) * Scale,
			(M[0][1] * M[1][2] - M[0][2] * M[1][1]) * Scale};
		Inverse.M[1] = {(M[1][2] * M[2][0] - M[1][0] * M[2][2]) * Scale, (M[0][0] * M[2][2] - M[0][2] * M[2][0]) * Scale,
			(M[0][2] * M[1][0] - M[0][0] * M[1][2]) * Scale};
		Inverse.M[2] = {(M[1][0] * M[2][1] - M[1][1] * M[2][0]) * Scale, (M[0][1] * M[2][0] - M[0][0] * M[2][1]) * Scale,
			(M[0][0] * M[1][1] - M[0][1] * M[1][0]) * Scale};
		const std::array<float, 3> Moved = Inverse.Turn(T);
		Inverse.T = {-Moved[0], -Moved[1], -Moved[2]};
		return true;
	}

	FAffine FAffine::After(const FAffine& Inner) const
	{
		FAffine Result;
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				Result.M[Row][Column] = M[Row][0] * Inner.M[0][Column] + M[Row][1] * Inner.M[1][Column] + M[Row][2] * Inner.M[2][Column];
			}
		}
		const std::array<float, 3> Moved = Turn(Inner.T);
		Result.T = {Moved[0] + T[0], Moved[1] + T[1], Moved[2] + T[2]};
		return Result;
	}

	FViewMove FAffine::ToMove() const
	{
		FViewMove Move{};
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			Move[Row] = {M[Row][0], M[Row][1], M[Row][2], T[Row]};
		}
		return Move;
	}

	bool BlendAffine(const FAffine& From, const FAffine& To, float Alpha, FAffine& Blended)
	{
		for (std::size_t Column = 0; Column < 3; ++Column)
		{
			const std::array<float, 3> A{From.M[0][Column], From.M[1][Column], From.M[2][Column]};
			const std::array<float, 3> B{To.M[0][Column], To.M[1][Column], To.M[2][Column]};
			if (A[0] * B[0] + A[1] * B[1] + A[2] * B[2] < 0.0f)
			{
				return false;
			}
			std::array<float, 3> Mixed{Lerp(A[0], B[0], Alpha), Lerp(A[1], B[1], Alpha), Lerp(A[2], B[2], Alpha)};
			const float MixedLength = Length(Mixed);
			if (MixedLength > 0.0f)
			{
				const float Wanted = Lerp(Length(A), Length(B), Alpha);
				for (float& Value : Mixed)
				{
					Value *= Wanted / MixedLength;
				}
			}
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				Blended.M[Row][Column] = Mixed[Row];
			}
		}
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			Blended.T[Row] = Lerp(From.T[Row], To.T[Row], Alpha);
		}
		return true;
	}

	FViewMove BlendedViewMove(const FAffine& From, const FAffine& To, float Alpha)
	{
		FAffine Blended;
		FAffine Inverse;
		if (!BlendAffine(From, To, Alpha, Blended) || !To.Invert(Inverse))
		{
			return UnmovedView;
		}
		return Blended.After(Inverse).ToMove();
	}
}
