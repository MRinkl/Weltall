#include "Chd/ChdLzma.h"

#include <algorithm>

namespace Chd
{
	namespace
	{
		constexpr uint32 ProbabilityBits = 11;
		constexpr uint16 InitialProbability = 1u << (ProbabilityBits - 1);
		constexpr uint32 AdaptShift = 5;
		constexpr uint32 TopValue = 1u << 24;
		constexpr uint32 LiteralProbabilities = 0x300;
		constexpr uint32 EndMark = 0xFFFFFFFF;
		constexpr uint32 MinMatch = 2;
		/** Distances from slot 14 on take their middle bits uncoded. */
		constexpr uint32 FirstUncodedSlot = 14;
		constexpr uint32 AlignBits = 4;

		/** LZMA's range decoder, which takes a byte whenever its range falls below 2^24. */
		class FRangeDecoder
		{
		public:
			explicit FRangeDecoder(std::span<const uint8> InBytes) : Bytes(InBytes)
			{
				bValid = NextByte() == 0;
				for (int Index = 0; Index < 4; ++Index)
				{
					Code = Code << 8 | NextByte();
				}
				bValid = bValid && Code != Range;
			}

			uint32 Bit(uint16& Probability)
			{
				const uint32 Bound = (Range >> ProbabilityBits) * Probability;
				uint32 Value = 0;
				if (Code < Bound)
				{
					Probability = static_cast<uint16>(Probability + (((1u << ProbabilityBits) - Probability) >> AdaptShift));
					Range = Bound;
				}
				else
				{
					Probability = static_cast<uint16>(Probability - (Probability >> AdaptShift));
					Code -= Bound;
					Range -= Bound;
					Value = 1;
				}
				Normalize();
				return Value;
			}

			/** Count bits of even probability, the first the highest. */
			uint32 DirectBits(uint32 Count)
			{
				uint32 Value = 0;
				for (; Count > 0; --Count)
				{
					Range >>= 1;
					const uint32 Bit = Code >= Range ? 1 : 0;
					Code -= Range & (0u - Bit);
					Value = Value << 1 | Bit;
					Normalize();
				}
				return Value;
			}

			/** A Bits-bit number coded high bit first through a tree of probabilities. */
			template <std::size_t Size>
			uint32 Tree(std::array<uint16, Size>& Probabilities, uint32 Bits)
			{
				uint32 Node = 1;
				for (uint32 Index = 0; Index < Bits; ++Index)
				{
					Node = Node << 1 | Bit(Probabilities[Node]);
				}
				return Node - (1u << Bits);
			}

			/** A Bits-bit number coded low bit first through a tree of probabilities starting at First. */
			uint32 ReverseTree(uint16* First, uint32 Bits)
			{
				uint32 Node = 1;
				uint32 Value = 0;
				for (uint32 Index = 0; Index < Bits; ++Index)
				{
					const uint32 Got = Bit(First[Node]);
					Node = Node << 1 | Got;
					Value |= Got << Index;
				}
				return Value;
			}

			/** Whether the stream began properly, has not run past its bytes and still has its range in order. */
			bool IsValid() const { return bValid && Position <= Bytes.size(); }
			/** Whether the coder ended where an encoder that flushed after the last symbol leaves it. */
			bool IsFinished() const { return Code == 0; }
			bool TookAll() const { return Position == Bytes.size(); }

		private:
			uint32 NextByte() { return Position < Bytes.size() ? Bytes[Position++] : (++Position, 0u); }

			void Normalize()
			{
				if (Range < TopValue)
				{
					Range <<= 8;
					Code = Code << 8 | NextByte();
				}
			}

			std::span<const uint8> Bytes;
			std::size_t Position = 0;
			uint32 Range = 0xFFFFFFFF;
			uint32 Code = 0;
			bool bValid = true;
		};

		uint32 AfterLiteral(uint32 State) { return State < 4 ? 0 : State < 10 ? State - 3 : State - 6; }
		uint32 AfterMatch(uint32 State) { return State < 7 ? 7 : 10; }
		uint32 AfterRep(uint32 State) { return State < 7 ? 8 : 11; }
		uint32 AfterShortRep(uint32 State) { return State < 7 ? 9 : 11; }

		template <typename TArray>
		void Reset(TArray& Probabilities)
		{
			if constexpr (std::is_same_v<std::remove_cvref_t<decltype(Probabilities[0])>, uint16>)
			{
				std::fill(std::begin(Probabilities), std::end(Probabilities), InitialProbability);
			}
			else
			{
				for (auto& Row : Probabilities)
				{
					Reset(Row);
				}
			}
		}
	}

	FLzmaDecoder::FLzmaDecoder(uint32 InLc, uint32 InLp, uint32 InPb)
		: Lc(InLc), Lp(InLp), Pb(InPb), Literals(static_cast<std::size_t>(LiteralProbabilities) << (InLc + InLp))
	{
	}

	bool FLzmaDecoder::Decode(std::span<const uint8> Packed, std::span<uint8> Destination)
	{
		for (FLengthModel* Lengths : {&Model.Length, &Model.RepLength})
		{
			Lengths->Choice = InitialProbability;
			Lengths->Choice2 = InitialProbability;
			Reset(Lengths->Low);
			Reset(Lengths->Mid);
			Reset(Lengths->High);
		}
		Reset(Model.IsMatch);
		Reset(Model.IsRep);
		Reset(Model.IsRepG0);
		Reset(Model.IsRepG1);
		Reset(Model.IsRepG2);
		Reset(Model.IsRep0Long);
		Reset(Model.DistanceSlot);
		Reset(Model.DistanceMiddle);
		Reset(Model.DistanceAlign);
		Reset(Literals);

		FRangeDecoder Coder(Packed);
		const auto DecodeLength = [&Coder](FLengthModel& Lengths, uint32 PositionState) {
			if (Coder.Bit(Lengths.Choice) == 0)
			{
				return Coder.Tree(Lengths.Low[PositionState], 3);
			}
			if (Coder.Bit(Lengths.Choice2) == 0)
			{
				return 8 + Coder.Tree(Lengths.Mid[PositionState], 3);
			}
			return 16 + Coder.Tree(Lengths.High, 8);
		};
		const auto DecodeDistance = [this, &Coder](uint32 Length) {
			const uint32 Slot = Coder.Tree(Model.DistanceSlot[std::min(Length, 3u)], 6);
			if (Slot < 4)
			{
				return Slot;
			}
			const uint32 Bits = (Slot >> 1) - 1;
			uint32 Distance = (2 | (Slot & 1)) << Bits;
			if (Slot < FirstUncodedSlot)
			{
				return Distance + Coder.ReverseTree(Model.DistanceMiddle.data() + Distance - Slot, Bits);
			}
			Distance += Coder.DirectBits(Bits - AlignBits) << AlignBits;
			return Distance + Coder.ReverseTree(Model.DistanceAlign.data(), AlignBits);
		};

		const std::size_t Size = Destination.size();
		std::size_t Position = 0;
		uint32 State = 0;
		std::array<uint32, 4> Reps{};
		while (Coder.IsValid())
		{
			// With every byte out, the stream ends here or with an end mark.
			if (Position == Size && Coder.IsFinished())
			{
				return Coder.TookAll();
			}
			const uint32 PositionState = static_cast<uint32>(Position) & ((1u << Pb) - 1);
			if (Coder.Bit(Model.IsMatch[State * MostPositionStates + PositionState]) == 0)
			{
				if (Position == Size)
				{
					return false;
				}
				const uint32 Previous = Position > 0 ? Destination[Position - 1] : 0;
				const uint32 Context = (static_cast<uint32>(Position) & ((1u << Lp) - 1)) << Lc | Previous >> (8 - Lc);
				uint16* Probabilities = Literals.data() + static_cast<std::size_t>(LiteralProbabilities) * Context;
				uint32 Symbol = 1;
				if (State >= 7)
				{
					// After a match the byte at the last distance guides the bits until one differs from it.
					uint32 Matched = Destination[Position - Reps[0] - 1];
					while (Symbol < 0x100)
					{
						const uint32 MatchedBit = Matched >> 7 & 1;
						Matched <<= 1;
						const uint32 Bit = Coder.Bit(Probabilities[((1 + MatchedBit) << 8) + Symbol]);
						Symbol = Symbol << 1 | Bit;
						if (Bit != MatchedBit)
						{
							break;
						}
					}
				}
				while (Symbol < 0x100)
				{
					Symbol = Symbol << 1 | Coder.Bit(Probabilities[Symbol]);
				}
				Destination[Position++] = static_cast<uint8>(Symbol);
				State = AfterLiteral(State);
				continue;
			}

			uint32 Length = 0;
			if (Coder.Bit(Model.IsRep[State]) != 0)
			{
				if (Position == Size || Position == 0)
				{
					return false;
				}
				if (Coder.Bit(Model.IsRepG0[State]) == 0)
				{
					if (Coder.Bit(Model.IsRep0Long[State * MostPositionStates + PositionState]) == 0)
					{
						State = AfterShortRep(State);
						Destination[Position] = Destination[Position - Reps[0] - 1];
						++Position;
						continue;
					}
				}
				else
				{
					uint32 Distance = 0;
					if (Coder.Bit(Model.IsRepG1[State]) == 0)
					{
						Distance = Reps[1];
					}
					else
					{
						if (Coder.Bit(Model.IsRepG2[State]) == 0)
						{
							Distance = Reps[2];
						}
						else
						{
							Distance = Reps[3];
							Reps[3] = Reps[2];
						}
						Reps[2] = Reps[1];
					}
					Reps[1] = Reps[0];
					Reps[0] = Distance;
				}
				Length = DecodeLength(Model.RepLength, PositionState);
				State = AfterRep(State);
			}
			else
			{
				Reps[3] = Reps[2];
				Reps[2] = Reps[1];
				Reps[1] = Reps[0];
				Length = DecodeLength(Model.Length, PositionState);
				State = AfterMatch(State);
				Reps[0] = DecodeDistance(Length);
				if (Reps[0] == EndMark)
				{
					return Position == Size && Coder.IsValid() && Coder.IsFinished() && Coder.TookAll();
				}
				if (Position == Size || Reps[0] >= Position)
				{
					return false;
				}
			}
			Length += MinMatch;
			if (Length > Size - Position)
			{
				return false;
			}
			for (const std::size_t End = Position + Length; Position < End; ++Position)
			{
				Destination[Position] = Destination[Position - Reps[0] - 1];
			}
		}
		return false;
	}
}
