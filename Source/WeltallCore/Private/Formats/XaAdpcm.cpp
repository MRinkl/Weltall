#include "Formats/XaAdpcm.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The four filters' weights of the sample before and the one before that, in 64ths. */
		constexpr std::array<int32, 4> OldWeights = {0, 60, 115, 98};
		constexpr std::array<int32, 4> OlderWeights = {0, 0, -52, -55};
		constexpr int32 LargestShift = 12;
		/** The shifts past 12 act as 9. */
		constexpr int32 ReservedShift = 9;
		constexpr std::size_t HeaderOffset = 4;
		constexpr std::size_t WordsOffset = 16;
		constexpr std::size_t WordsPerGroup = 28;

		/**
		 * Decodes one block of a group: 28 samples, one from each of the group's data words, written to
		 * Out every Stride samples.
		 */
		void DecodeBlock(std::span<const uint8> Group, std::size_t Block, bool bEightBit, FXaChannelHistory& History,
			int16* Out, std::size_t Stride)
		{
			const uint8 Header = Group[HeaderOffset + Block];
			int32 Shift = Header & 0x0F;
			if (Shift > LargestShift)
			{
				Shift = ReservedShift;
			}
			const std::size_t Filter = (Header >> 4) & 3;
			for (std::size_t Word = 0; Word < WordsPerGroup; ++Word)
			{
				const std::size_t WordOffset = WordsOffset + Word * 4;
				int32 Expanded;
				if (bEightBit)
				{
					Expanded = static_cast<int16>(Group[WordOffset + Block] << 8);
				}
				else
				{
					const int32 Nibble = (Group[WordOffset + Block / 2] >> ((Block & 1) * 4)) & 0x0F;
					Expanded = static_cast<int16>(Nibble << 12);
				}
				int32 Sample = Expanded >> Shift;
				Sample += (History.Old * OldWeights[Filter] + History.Older * OlderWeights[Filter] + 32) >> 6;
				const int16 Clamped = static_cast<int16>(std::clamp(Sample, -0x8000, 0x7FFF));
				History.Older = History.Old;
				History.Old = Clamped;
				*Out = Clamped;
				Out += Stride;
			}
		}
	}

	FXaCodingInfo FXaCodingInfo::FromSubheader(uint8 CodingInfo)
	{
		FXaCodingInfo Info;
		Info.bStereo = (CodingInfo & 0x03) == 1;
		Info.bHalfRate = (CodingInfo & 0x0C) == 0x04;
		Info.bEightBit = (CodingInfo & 0x30) == 0x10;
		return Info;
	}

	void DecodeXaSector(std::span<const uint8> UserData, FXaCodingInfo Coding, std::array<FXaChannelHistory, 2>& History,
		std::vector<int16>& Out)
	{
		// A group holds 8 blocks of 4-bit samples or 4 of 8-bit ones, 28 samples each. In stereo, even
		// blocks are left and odd ones right, so each pair of blocks makes 28 interleaved frames.
		const std::size_t Blocks = Coding.bEightBit ? 4 : 8;
		const std::size_t GroupSamples = Blocks * WordsPerGroup;
		std::size_t Write = Out.size();
		Out.resize(Write + XaSector::GroupCount * GroupSamples);
		for (std::size_t Group = 0; Group < XaSector::GroupCount; ++Group)
		{
			const std::span<const uint8> Bytes = UserData.subspan(Group * XaSector::GroupBytes, XaSector::GroupBytes);
			for (std::size_t Block = 0; Block < Blocks; ++Block)
			{
				if (Coding.bStereo)
				{
					int16* First = Out.data() + Write + (Block / 2) * WordsPerGroup * 2 + (Block & 1);
					DecodeBlock(Bytes, Block, Coding.bEightBit, History[Block & 1], First, 2);
				}
				else
				{
					DecodeBlock(Bytes, Block, Coding.bEightBit, History[0], Out.data() + Write + Block * WordsPerGroup, 1);
				}
			}
			Write += GroupSamples;
		}
	}
}
