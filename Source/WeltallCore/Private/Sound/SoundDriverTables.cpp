#include "Sound/SoundDriverTables.h"

#include "Foundation/LittleEndian.h"
#include "Media/PsxExecutable.h"

namespace Weltall
{
	namespace
	{
		/** Where the kernel keeps the tables (both discs of the USA release). */
		constexpr uint32 PitchAddress = 0x80050BF0;
		constexpr uint32 OctaveAddress = 0x80050B78;
		constexpr uint32 CommandLengthAddress = 0x80050824;
		constexpr uint32 ReverbAreaAddress = 0x800508E8;
		constexpr uint32 ReverbStartAddress = 0x80058E70;
		constexpr uint32 ReverbPresetAddress = 0x80058EC0;
		constexpr uint32 ReverbPresetBytes = 0x44;
		constexpr uint32 SilentBlockAddress = 0x80058E48;

		constexpr int32 NotesPerOctave = 12;
	}

	TResult<FSoundDriverTables> FSoundDriverTables::Read(const FPsxExecutable& Executable)
	{
		const auto Pitches = Executable.Read(PitchAddress, sizeof(FSoundDriverTables::Pitches));
		const auto Octaves = Executable.Read(OctaveAddress, sizeof(FSoundDriverTables::Octaves));
		const auto Lengths = Executable.Read(CommandLengthAddress, sizeof(FSoundDriverTables::CommandLengths));
		const auto Areas = Executable.Read(ReverbAreaAddress, ReverbTypes * 4);
		const auto Starts = Executable.Read(ReverbStartAddress, ReverbTypes * 4);
		const auto Presets = Executable.Read(ReverbPresetAddress, ReverbTypes * ReverbPresetBytes);
		const auto Silent = Executable.Read(SilentBlockAddress, sizeof(FSoundDriverTables::SilentBlock));
		if (Pitches.empty() || Octaves.empty() || Lengths.empty() || Areas.empty() || Starts.empty() || Presets.empty() || Silent.empty())
		{
			return MakeError("the executable holds no sound driver");
		}
		FSoundDriverTables Tables;
		for (std::size_t Index = 0; Index < Tables.Pitches.size(); ++Index)
		{
			Tables.Pitches[Index] = LittleEndian::ReadS16(Pitches, Index * 2);
		}
		for (std::size_t Index = 0; Index < Tables.Octaves.size(); ++Index)
		{
			// A note past B would index past the pitch table.
			if ((Octaves[Index] & 0xF) >= NotesPerOctave)
			{
				return MakeError("the sound driver's octave table is not the one this port knows");
			}
			Tables.Octaves[Index] = Octaves[Index];
		}
		std::copy(Lengths.begin(), Lengths.end(), Tables.CommandLengths.begin());
		std::copy(Silent.begin(), Silent.end(), Tables.SilentBlock.begin());
		for (std::size_t Type = 0; Type < ReverbTypes; ++Type)
		{
			Tables.ReverbAreaSizes[Type] = LittleEndian::ReadU32(Areas, Type * 4);
			FSpuReverbPreset& Preset = Tables.ReverbPresets[Type];
			Preset.Start = LittleEndian::ReadU16(Starts, Type * 4);
			// Each preset is a 4-byte mask of the registers it sets (the driver always sets all 32), then the registers.
			for (std::size_t Register = 0; Register < Preset.Registers.size(); ++Register)
			{
				Preset.Registers[Register] = LittleEndian::ReadU16(Presets, Type * ReverbPresetBytes + 4 + Register * 2);
			}
		}
		return Tables;
	}
}
