#include "Sound/SoundDriverMath.h"
#include "Support/SoundTestFiles.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

TEST_CASE("Sound driver pitches convert to SPU steps as the original converts them", "[Sound]")
{
	const FSoundDriverTables Tables = Testing::MakeSoundDriverTables();
	struct FVector
	{
		int16 Pitch;
		int16 Step;
	};
	const FVector Vectors[] = {{0x0000, 0x0080}, {0x0100, 0x0087}, {0x3000, 0x0800}, {0x3B80, 0x0F8B}, {0x3C00, 0x1000}, {0x3C01, 0x1001},
		{0x3C80, 0x1078}, {0x3CFF, 0x10F2}, {0x4800, 0x2000}, {0x4B00, 0x260E}, {0x5400, 0x4000}, {0x5A00, 0x5A82}, {0x5FFF, 0x7FF8}};
	for (const FVector& Vector : Vectors)
	{
		INFO("pitch " << Vector.Pitch);
		CHECK(SoundDriverMath::PitchToStep(Tables, Vector.Pitch) == Vector.Step);
	}
}

TEST_CASE("Notes above the SPU's range wrap as in the original, or stay at the top with the fix", "[Sound]")
{
	const FSoundDriverTables Tables = Testing::MakeSoundDriverTables();
	CHECK(SoundDriverMath::PitchRegister(Tables, 0x5400, false) == 0x0000);
	CHECK(SoundDriverMath::PitchRegister(Tables, 0x5A00, false) == 0x1A82);
	CHECK(SoundDriverMath::PitchRegister(Tables, 0x5FFF, false) == 0x3FF8);
	CHECK(SoundDriverMath::PitchRegister(Tables, 0x5400, true) == 0x3FFF);
	CHECK(SoundDriverMath::PitchRegister(Tables, 0x7FFF, true) == 0x3FFF);
	// Below key 84 both agree.
	CHECK(SoundDriverMath::PitchRegister(Tables, 0x53FF, true) == SoundDriverMath::PitchRegister(Tables, 0x53FF, false));
	CHECK(SoundDriverMath::PitchRegister(Tables, 0x3C00, true) == 0x1000);
}

TEST_CASE("Voice volumes follow the driver's pan law", "[Sound]")
{
	struct FVector
	{
		int32 Volume;
		uint16 Expression;
		uint16 Pan;
		uint16 Left;
		uint16 Right;
	};
	// Sequence volume 7F000000h, no LFO.
	const FVector Vectors[] = {{0x7F, 0x7F00, 0x4000, 0x2BF3, 0x2BF3}, {0x7F, 0x7F00, 0, 0x3E04, 0}, {0x7F, 0x7F00, 0x7F00, 0x00AF, 0x3DBC},
		{0x7F, 0x6000, 0x4000, 0x2139, 0x2139}, {0x40, 0x5A00, 0x2000, 0x12EC, 0x07D9}, {0x7F, 0x7F00, 0x6000, 0x15F9, 0x34FC}};
	for (const FVector& Vector : Vectors)
	{
		INFO("volume " << Vector.Volume << ", expression " << Vector.Expression << ", pan " << Vector.Pan);
		const SoundDriverMath::FVoiceMix Mix{0x7F000000, 0, Vector.Volume << 24, Vector.Expression, Vector.Pan, 0, 0};
		const SoundDriverMath::FVoiceVolumes Stereo = SoundDriverMath::VoiceVolumes(Mix, true);
		CHECK(Stereo.Left == Vector.Left);
		CHECK(Stereo.Right == Vector.Right);
	}
	const SoundDriverMath::FVoiceVolumes Mono = SoundDriverMath::VoiceVolumes({0x7F000000, 0, 0x7F000000, 0x7F00, 0, 0, 0}, false);
	CHECK(Mono.Left == 0x2BF3);
	CHECK(Mono.Right == 0x2BF3);
	// An auto-pan past the edge stops there.
	CHECK(SoundDriverMath::VoiceVolumes({0x7F000000, 0, 0x7F000000, 0x7F00, 0x7F00, 0, 0x2000}, true).Right == 0x3DBC);
}

TEST_CASE("Envelope registers take each field in its place and keep the others", "[Sound]")
{
	// Attack rate 7Fh exponential, decay 0Fh, sustain level 0Fh, sustain rate 7Fh exponential
	// decrease (ADSR2 bit 13 is unused), release 1Fh exponential.
	uint16 Adsr1 = 0;
	Adsr1 = SoundDriverMath::WithAttack(Adsr1, 0x7F, 5);
	Adsr1 = SoundDriverMath::WithDecay(Adsr1, 0x0F);
	Adsr1 = SoundDriverMath::WithSustainLevel(Adsr1, 0x0F);
	CHECK(Adsr1 == 0xFFFF);
	uint16 Adsr2 = 0;
	Adsr2 = SoundDriverMath::WithSustainRate(Adsr2, 0x7F, 7);
	Adsr2 = SoundDriverMath::WithRelease(Adsr2, 0x1F, 7);
	CHECK(Adsr2 == 0xDFFF);
	// Linear modes leave their bits clear; the other fields' bits survive a rewrite.
	CHECK(SoundDriverMath::WithAttack(0x00AB, 0x10, 1) == 0x10AB);
	CHECK(SoundDriverMath::WithDecay(0xFFFF, 0x3) == 0xFF3F);
	CHECK(SoundDriverMath::WithRelease(0xFFFF, 6, 3) == 0xFFC6);
	CHECK(SoundDriverMath::WithSustainRate(0xFFFF, 0, 1) == 0x003F);
}
