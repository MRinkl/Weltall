#include "Graphics/Mdec.h"

#include <algorithm>
#include <limits>
#include <string>

namespace Weltall
{
	namespace
	{
		constexpr std::array<uint8, 64> IntraQuant = {
			2, 16, 16, 19, 16, 19, 22, 22, 22, 22, 22, 22, 26, 24, 26, 27, 27, 27, 26, 26, 26, 26, 27, 27, 27, 29, 29, 29, 34, 34, 34, 29,
			29, 29, 27, 27, 29, 29, 32, 32, 34, 34, 37, 38, 37, 35, 35, 34, 35, 38, 38, 40, 40, 40, 48, 48, 46, 46, 56, 56, 58, 69, 69, 83,
		};

		constexpr std::array<int16, 64> StandardScale = {
			0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82,
			0x7D8A, 0x6A6D, 0x471C, 0x18F8, -0x18F9, -0x471D, -0x6A6E, -0x7D8B,
			0x7641, 0x30FB, -0x30FC, -0x7642, -0x7642, -0x30FC, 0x30FB, 0x7641,
			0x6A6D, -0x18F9, -0x7D8B, -0x471D, 0x471C, 0x7D8A, 0x18F8, -0x6A6E,
			0x5A82, -0x5A83, -0x5A83, 0x5A82, 0x5A82, -0x5A83, -0x5A83, 0x5A82,
			0x471C, -0x7D8B, 0x18F8, 0x6A6D, -0x6A6E, -0x18F9, 0x7D8A, -0x471D,
			0x30FB, -0x7642, 0x7641, -0x30FC, -0x30FC, 0x7641, -0x7642, 0x30FB,
			0x18F8, -0x471D, 0x6A6D, -0x7D8B, 0x7D8A, -0x6A6E, 0x471C, -0x18F9,
		};

		/** Where each coefficient of the zigzag order lies in the 8x8 block, row by row. */
		constexpr std::array<uint8, 64> ZigzagToRaster = {
			0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
			35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
		};

		constexpr uint16 EndOfBlock = 0xFE00;
		/** The inverse DCT's two passes multiply by two 1.14 basis values: 2^28 per 8-bit step, less the extra bits kept. */
		constexpr int32 IdctShift = 32 - MdecSampleFractionBits;

		using FBlock = std::array<int32, 64>;

		int32 SignExtend10(uint16 Code)
		{
			return static_cast<int16>(static_cast<uint16>(Code << 6)) >> 6;
		}

		class FCodeReader
		{
		public:
			explicit FCodeReader(std::span<const uint16> InCodes) : Codes(InCodes) {}

			bool Next(uint16& Code)
			{
				if (Position >= Codes.size())
				{
					return false;
				}
				Code = Codes[Position++];
				return true;
			}

		private:
			std::span<const uint16> Codes;
			std::size_t Position = 0;
		};

		/**
		 * Expands one block's codes into dequantized coefficients, row by row, as the MDEC does: FE00h
		 * before a block is padding; a run that passes the last coefficient ends the block, which is how
		 * the end code FE00h ends it; each value saturates to 11 bits. Scale 0 stores values doubled and
		 * in plain order, without the tables. The block's scale goes to QuantScale.
		 */
		bool ExpandBlock(FCodeReader& Reader, const std::array<uint8, 64>& Quant, FBlock& Block, int32& QuantScale)
		{
			Block.fill(0);
			uint16 Code = EndOfBlock;
			while (Code == EndOfBlock)
			{
				if (!Reader.Next(Code))
				{
					return false;
				}
			}
			QuantScale = Code >> 10;
			int32 Index = 0;
			int32 Value = SignExtend10(Code) * Quant[0];
			for (;;)
			{
				if (QuantScale == 0)
				{
					Value = SignExtend10(Code) * 2;
				}
				const std::size_t Slot = static_cast<std::size_t>(QuantScale == 0 ? Index : ZigzagToRaster[static_cast<std::size_t>(Index)]);
				Block[Slot] = std::clamp(Value, -0x400, 0x3FF);
				if (Index >= 63)
				{
					return true;
				}
				if (!Reader.Next(Code))
				{
					return false;
				}
				Index += (Code >> 10) + 1;
				if (Index > 63)
				{
					return true;
				}
				Value = (SignExtend10(Code) * Quant[static_cast<std::size_t>(Index)] * QuantScale + 4) / 8;
			}
		}

		/** The inverse DCT of a block into Out (8x8 at Stride), keeping MdecSampleFractionBits of fraction. */
		void InverseDct(const FBlock& Block, const std::array<int16, 64>& Scale, int16* Out, int32 Stride)
		{
			std::array<int64, 64> Rows{};
			for (std::size_t Row = 0; Row < 8; ++Row)
			{
				for (std::size_t X = 0; X < 8; ++X)
				{
					int64 Sum = 0;
					for (std::size_t Column = 0; Column < 8; ++Column)
					{
						Sum += static_cast<int64>(Block[Row * 8 + Column]) * Scale[Column * 8 + X];
					}
					Rows[Row * 8 + X] = Sum;
				}
			}
			for (std::size_t Y = 0; Y < 8; ++Y)
			{
				for (std::size_t X = 0; X < 8; ++X)
				{
					int64 Sum = 0;
					for (std::size_t Row = 0; Row < 8; ++Row)
					{
						Sum += Rows[Row * 8 + X] * Scale[Row * 8 + Y];
					}
					const int64 Sample = (Sum + (int64{1} << (IdctShift - 1))) >> IdctShift;
					Out[static_cast<std::ptrdiff_t>(Y) * Stride + static_cast<std::ptrdiff_t>(X)] = static_cast<int16>(
						std::clamp<int64>(Sample, std::numeric_limits<int16>::min(), std::numeric_limits<int16>::max()));
				}
			}
		}

		uint8 ToUnsigned8(int32 Sample)
		{
			const int32 Rounded = (Sample + (1 << (MdecSampleFractionBits - 1))) >> MdecSampleFractionBits;
			return static_cast<uint8>(std::clamp(Rounded, -128, 127) + 128);
		}
	}

	const FMdecTables& FMdecTables::Standard()
	{
		static const FMdecTables Tables{IntraQuant, IntraQuant, StandardScale};
		return Tables;
	}

	TResult<FMdecPicture> DecodeMdecPicture(std::span<const uint16> Codes, int32 Width, int32 Height, const FMdecTables& Tables)
	{
		if (Width <= 0 || Height <= 0 || Width % 16 != 0 || Height % 16 != 0)
		{
			return MakeError("an MDEC picture is made of whole 16x16 macroblocks");
		}
		FMdecPicture Picture;
		Picture.Width = Width;
		Picture.Height = Height;
		const int32 ChromaWidth = Width / 2;
		Picture.Luma.resize(static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height));
		Picture.ChromaBlue.resize(Picture.Luma.size() / 4);
		Picture.ChromaRed.resize(Picture.Luma.size() / 4);

		FCodeReader Reader(Codes);
		FBlock Block;
		int32 BlockScale = 0;
		for (int32 MacroX = 0; MacroX < Width / 16; ++MacroX)
		{
			for (int32 MacroY = 0; MacroY < Height / 16; ++MacroY)
			{
				const std::ptrdiff_t Chroma = static_cast<std::ptrdiff_t>(MacroY) * 8 * ChromaWidth + MacroX * 8;
				int16* const Planes[2] = {Picture.ChromaRed.data() + Chroma, Picture.ChromaBlue.data() + Chroma};
				for (int16* Plane : Planes)
				{
					if (!ExpandBlock(Reader, Tables.ChromaQuant, Block, BlockScale))
					{
						return MakeError("the codes end inside macroblock " + std::to_string(MacroX) + "," + std::to_string(MacroY));
					}
					Picture.QuantScale = std::max(Picture.QuantScale, BlockScale);
					InverseDct(Block, Tables.Scale, Plane, ChromaWidth);
				}
				for (int32 Quarter = 0; Quarter < 4; ++Quarter)
				{
					if (!ExpandBlock(Reader, Tables.LumaQuant, Block, BlockScale))
					{
						return MakeError("the codes end inside macroblock " + std::to_string(MacroX) + "," + std::to_string(MacroY));
					}
					Picture.QuantScale = std::max(Picture.QuantScale, BlockScale);
					const std::ptrdiff_t Top = static_cast<std::ptrdiff_t>(MacroY * 16 + (Quarter / 2) * 8) * Width;
					InverseDct(Block, Tables.Scale, Picture.Luma.data() + Top + MacroX * 16 + (Quarter % 2) * 8, Width);
				}
			}
		}
		return Picture;
	}

	namespace
	{
		/** Each pixel's R, G and B as the MDEC converts it: each chroma sample covers 2x2 pixels. */
		template <typename TVisit>
		void ForEachRgb(const FMdecPicture& Picture, TVisit&& Visit)
		{
			const int32 ChromaWidth = Picture.Width / 2;
			for (int32 Y = 0; Y < Picture.Height; ++Y)
			{
				for (int32 X = 0; X < Picture.Width; ++X)
				{
					const std::size_t Chroma = static_cast<std::size_t>((Y / 2) * ChromaWidth + X / 2);
					const int32 Luma = Picture.Luma[static_cast<std::size_t>(Y * Picture.Width + X)];
					const int32 Blue = Picture.ChromaBlue[Chroma];
					const int32 Red = Picture.ChromaRed[Chroma];
					// BT.601 weights in 256ths: 1.402 Cr, -0.344 Cb - 0.714 Cr, 1.772 Cb.
					Visit(ToUnsigned8(Luma + ((359 * Red + 128) >> 8)), ToUnsigned8(Luma + ((-88 * Blue - 183 * Red + 128) >> 8)),
						ToUnsigned8(Luma + ((454 * Blue + 128) >> 8)));
				}
			}
		}
	}

	std::vector<uint8> ToRgb24(const FMdecPicture& Picture)
	{
		std::vector<uint8> Rgb(static_cast<std::size_t>(Picture.Width) * static_cast<std::size_t>(Picture.Height) * 3);
		std::size_t Out = 0;
		ForEachRgb(Picture, [&](uint8 R, uint8 G, uint8 B) {
			Rgb[Out++] = R;
			Rgb[Out++] = G;
			Rgb[Out++] = B;
		});
		return Rgb;
	}

	void ToRgb15(const FMdecPicture& Picture, std::span<uint16> Units)
	{
		std::size_t Out = 0;
		ForEachRgb(Picture, [&](uint8 R, uint8 G, uint8 B) {
			if (Out < Units.size())
			{
				Units[Out] = static_cast<uint16>(R >> 3 | (G >> 3) << 5 | (B >> 3) << 10);
			}
			++Out;
		});
	}
}
