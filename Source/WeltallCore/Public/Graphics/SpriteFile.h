#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * Where a sprite file's three tables start, as its header gives them: the animations, the frames
	 * and the palettes. A sprite takes them when it takes its file and reads the file through them
	 * from then on, as the kernel keeps them as addresses: a file loaded later into the same memory
	 * is read through the tables of the one that was there.
	 */
	struct FSpriteTables
	{
		uint32 Animations = 0;
		uint32 Frames = 0;
		uint32 Palettes = 0;
	};

	/**
	 * An animated sprite's file as the kernel reads it: animations (each a header with its facing
	 * directions' frame lists and a script), frames made of parts, and palettes. A streamed sprite
	 * carries its texels and uploads each frame's cells when it is shown; a resident one names places
	 * in texture pages that are already in VRAM.
	 *
	 * A frame is a head byte (bits 0-5 its part count, bit 7 two-byte placements), its half height,
	 * right extent and height above the origin in texels, for a streamed frame its VRAM area's width
	 * and height, then one entry per part (streamed: a u16 cell index and a u16 place in the area;
	 * resident: a u16 cell offset), then the placement stream: per part any number of command bytes
	 * (bit 7 set) and the part's attributes and position.
	 *
	 * The tables are read through the FSpriteTables given, the file's own (Tables) or those a sprite
	 * took from the memory the file now occupies.
	 */
	class FSpriteFile
	{
	public:
		/** Reads a file and checks that every animation header, frame, cell and placement lies within it. */
		WELTALLCORE_API static TResult<FSpriteFile> Parse(std::span<const uint8> Bytes);
		/**
		 * The same for a file whose header counts further sections after its three (Battle's: sounds,
		 * banks and more sprites), whose offsets follow the three's; the kernel reads only the three.
		 */
		WELTALLCORE_API static TResult<FSpriteFile> ParseWithSections(std::span<const uint8> Bytes);

		uint32 Size() const { return static_cast<uint32>(Bytes.size()); }
		/** The file's own tables, from its header. */
		const FSpriteTables& Tables() const { return Own; }

		/** A byte of the file, 0 past its end: scripts and frame lists are read as the kernel reads memory. */
		uint8 Byte(uint32 Offset) const { return Offset < Bytes.size() ? Bytes[Offset] : 0; }
		uint16 Word(uint32 Offset) const { return static_cast<uint16>(Byte(Offset) | Byte(Offset + 1) << 8); }
		/** Writes a byte of the file, as a script writes its variables into the file in memory; nothing past its end. */
		void SetByte(uint32 Offset, uint8 Value)
		{
			if (Offset < Bytes.size())
			{
				Bytes[Offset] = Value;
			}
		}
		/** The file's bytes from Offset on, for uploads of its texels and palettes. */
		std::span<const uint8> From(uint32 Offset) const { return Offset < Bytes.size() ? std::span<const uint8>(Bytes).subspan(Offset) : std::span<const uint8>(); }

		int32 AnimationCount(const FSpriteTables& At) const { return Word(At.Animations) & 0x3F; }
		/** Where animation Index's header is: the table's offsets count from the table. */
		uint32 AnimationHeader(const FSpriteTables& At, int32 Index) const { return At.Animations + Word(At.Animations + 2 + 2 * static_cast<uint32>(Index)); }
		/** The bytes after the animation offsets, which a script's frames may be looked up through. */
		uint32 ByteTable(const FSpriteTables& At) const { return At.Animations + 2 + 2 * static_cast<uint32>(AnimationCount(At)); }

		bool IsResident(const FSpriteTables& At) const { return (Word(At.Frames) & 0x8000) != 0; }
		int32 FrameCount(const FSpriteTables& At) const { return Word(At.Frames) & 0x1FF; }
		/** The most parts one frame has: the part records a sprite keeps. */
		int32 MaxParts(const FSpriteTables& At) const { return Word(At.Frames) >> 9 & 0x3F; }
		/** Where frame Number is, as the kernel finds it: frames count from 1, and 0 reads the table's own word. */
		uint32 Frame(const FSpriteTables& At, int32 Number) const { return At.Frames + Word(At.Frames + 2 * static_cast<uint32>(Number)); }

		/** Palettes of 16 colours per set. */
		int32 PalettesPerSet(const FSpriteTables& At) const { return Word(At.Palettes); }
		/** Where the colours of palette set Set start. */
		uint32 PaletteSet(const FSpriteTables& At, int32 Set) const { return At.Palettes + 4 + static_cast<uint32>(Set * PalettesPerSet(At) * 32); }

	private:
		static TResult<FSpriteFile> ParseTables(std::span<const uint8> Bytes);

		std::vector<uint8> Bytes;
		FSpriteTables Own;
	};
}
