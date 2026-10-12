#include "Chd/ChdCdSector.h"

#include <algorithm>
#include <array>

namespace Chd
{
	namespace
	{
		constexpr std::array<uint8, 12> Sync{0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
		/** The parity covers the sector from its header on: 2064 bytes for P, those and P's 172 for Q. */
		constexpr uint32 CoveredFrom = 12;
		constexpr uint32 ModeOffset = 15;
		constexpr uint32 POffset = 0x81C;
		constexpr uint32 PColumns = 86;
		constexpr uint32 PRows = 24;
		constexpr uint32 QOffset = POffset + 2 * PColumns;
		constexpr uint32 QDiagonals = 52;
		constexpr uint32 QRows = 43;
		constexpr uint32 QCovered = 2236;

		/** Multiplication by x in GF(2^8) with the polynomial x^8 + x^4 + x^3 + x^2 + 1, and division by 1 + x. */
		struct FTables
		{
			std::array<uint8, 256> Times2{};
			std::array<uint8, 256> DivideBy3{};
		};

		constexpr FTables MakeTables()
		{
			FTables Tables;
			for (uint32 Value = 0; Value < 256; ++Value)
			{
				const uint32 Doubled = (Value << 1) ^ ((Value & 0x80) != 0 ? 0x11D : 0);
				Tables.Times2[Value] = static_cast<uint8>(Doubled);
				Tables.DivideBy3[Value ^ static_cast<uint8>(Doubled)] = static_cast<uint8>(Value);
			}
			return Tables;
		}

		constexpr FTables Tables = MakeTables();

		/** Computes one pair of parity bytes over Count bytes of the covered area, the first at Start, Step apart. */
		void Parity(std::span<uint8, CdSectorBytes> Sector, uint32 Start, uint32 Step, uint32 Count, uint32 Wrap, uint8& First, uint8& Second)
		{
			const bool bMode2 = Sector[ModeOffset] == 2;
			uint8 A = 0;
			uint8 B = 0;
			for (uint32 Index = 0, At = Start; Index < Count; ++Index, At = (At + Step) % Wrap)
			{
				const uint8 Byte = bMode2 && At < 4 ? 0 : Sector[CoveredFrom + At];
				A = Tables.Times2[A ^ Byte];
				B ^= Byte;
			}
			First = Tables.DivideBy3[Tables.Times2[A] ^ B];
			Second = static_cast<uint8>(First ^ B);
		}
	}

	void RestoreSyncAndEcc(std::span<uint8, CdSectorBytes> Sector)
	{
		std::copy(Sync.begin(), Sync.end(), Sector.begin());
		for (uint32 Column = 0; Column < PColumns; ++Column)
		{
			Parity(Sector, Column, PColumns, PRows, QCovered, Sector[POffset + Column], Sector[POffset + PColumns + Column]);
		}
		for (uint32 Diagonal = 0; Diagonal < QDiagonals; ++Diagonal)
		{
			Parity(Sector, (Diagonal >> 1) * PColumns + (Diagonal & 1), PColumns + 2, QRows, QCovered, Sector[QOffset + Diagonal],
				Sector[QOffset + QDiagonals + Diagonal]);
		}
	}
}
