#include "Sound/SoundDriver.h"
#include "Sound/SoundDriverMath.h"
#include "Sound/SoundFiles.h"
#include "Support/RecordingSpu.h"
#include "Support/SoundTestFiles.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	/** Voice 0's volumes after a note slides its pan from the centre to full right. */
	SoundDriverMath::FVoiceVolumes PanAfterSlide(bool bFixed)
	{
		const FSoundDriverTables Tables = Testing::MakeSoundDriverTables();
		Testing::FRecordingSpu Spu;
		FSoundDriver Driver(Spu, Tables, {bFixed, true});
		// Track 1 (voice 0): pan 40h, a slide to 7Fh over 4 ticks, a whole note at full velocity.
		const auto File = FSoundSequenceFile::Parse(Testing::MakeSequenceFile({Testing::HexBytes("90"), Testing::HexBytes("e840 ea047f 7f01 90")}));
		REQUIRE(File);
		FSoundSequence* Sequence = Driver.LoadSequence(*File);
		REQUIRE(Sequence != nullptr);
		Driver.PlaySequence(*Sequence, 0x7F, 0);
		for (int32 Interrupt = 0; Interrupt < 40; ++Interrupt)
		{
			Driver.Interrupt();
		}
		return {Spu.Read(0x0), Spu.Read(0x2)};
	}

	SoundDriverMath::FVoiceVolumes FullNoteAt(uint16 Pan)
	{
		return SoundDriverMath::VoiceVolumes({0x7F000000, 0, 0x7F000000, 0x7F00, Pan, 0, 0}, true);
	}
}

TEST_CASE("Pan slides end on their target with the fix, and on the slide's difference as in the original", "[Sound]")
{
	const SoundDriverMath::FVoiceVolumes Fixed = PanAfterSlide(true);
	CHECK(Fixed.Left == FullNoteAt(0x7F00).Left);
	CHECK(Fixed.Right == FullNoteAt(0x7F00).Right);
	// The original stores 7Fh - 40h as the end: the pan jumps to 3F00h, just left of centre.
	const SoundDriverMath::FVoiceVolumes Original = PanAfterSlide(false);
	CHECK(Original.Left == FullNoteAt(0x3F00).Left);
	CHECK(Original.Right == FullNoteAt(0x3F00).Right);
}
