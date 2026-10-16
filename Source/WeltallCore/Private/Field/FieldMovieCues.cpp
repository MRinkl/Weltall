#include "Field/FieldMovieCues.h"

#include "Formats/Lzss.h"
#include "Foundation/LittleEndian.h"
#include "Media/GameFileIndex.h"

#include <array>

namespace Weltall
{
	namespace
	{
		/** Where a release's Field program (directory 1, unpacked) keeps the table, and how many sets it holds. */
		struct FMovieCuesLayout
		{
			EDiscRelease Release;
			int32 ProgramFile;
			std::size_t Table;
			int32 Sets;
		};

		constexpr int32 ProgramDirectory = 1;
		constexpr std::array<FMovieCuesLayout, 2> Layouts{{
			{EDiscRelease::UsaDisc1, 14, 0x3E570, 10},
			{EDiscRelease::UsaDisc2, 14, 0x3E570, 10},
		}};
		/** A cue is a frame and an effect word; a frame of FFFFh ends a set, and one heads the table. */
		constexpr std::size_t CueBytes = 4;
		constexpr uint16 EndOfSet = 0xFFFF;
		/** The effect word: the effect in its low byte, half the driver's channel in its high byte. */
		constexpr int32 ChannelShift = 7;
		constexpr int32 ChannelMask = 0xE;
	}

	TResult<FFieldMovieCues> FFieldMovieCues::Read(const FCdImage& Image, const FGameFileIndex& Index, EDiscRelease Release)
	{
		const FMovieCuesLayout* Layout = nullptr;
		for (const FMovieCuesLayout& Candidate : Layouts)
		{
			if (Candidate.Release == GameplayRelease(Release))
			{
				Layout = &Candidate;
			}
		}
		if (Layout == nullptr)
		{
			return MakeError("no movie cues are known for this release");
		}
		const FIndexRecord* Record = Index.Find(ProgramDirectory, Layout->ProgramFile);
		if (Record == nullptr)
		{
			return MakeError("the disc lacks the Field program");
		}
		const TResult<std::vector<uint8>> Sectors = ReadFileSectors(Image, *Record);
		if (!Sectors)
		{
			return Sectors.Error();
		}
		const TResult<std::vector<uint8>> Unpacked = UnpackLzss(*Sectors);
		if (!Unpacked)
		{
			return Unpacked.Error();
		}
		const std::span<const uint8> Program = *Unpacked;
		FFieldMovieCues Cues;
		std::size_t At = Layout->Table;
		for (int32 Set = 0; Set <= Layout->Sets; ++Set)
		{
			if (Set > 0)
			{
				Cues.Sets.emplace_back();
			}
			while (true)
			{
				if (At + CueBytes > Program.size())
				{
					return MakeError("the Field program ends inside its movie cues");
				}
				const uint16 Frame = LittleEndian::ReadU16(Program, At);
				const uint16 Effect = LittleEndian::ReadU16(Program, At + 2);
				At += CueBytes;
				if (Frame == EndOfSet)
				{
					break;
				}
				if (Set == 0)
				{
					return MakeError("the Field program's movie cues do not start with the end of a set");
				}
				Cues.Sets.back().push_back({Frame, static_cast<uint8>(Effect), (Effect >> ChannelShift) & ChannelMask});
			}
		}
		return Cues;
	}

	std::span<const FFieldMovieCue> FFieldMovieCues::Set(int32 Number) const
	{
		if (Number < 0 || Number >= SetCount())
		{
			return {};
		}
		return Sets[static_cast<std::size_t>(Number)];
	}
}
