#include "Audio/GameSoundWave.h"
#include "Misc/AutomationTest.h"
#include "Sound/GameSound.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A sound that counts its samples, left and right apart, so the wave's order and continuity show. */
	class FCountingSound final : public Weltall::IGameSound
	{
	public:
		virtual void Render(std::span<Weltall::int16> Stereo) override
		{
			for (std::size_t Index = 0; Index + 1 < Stereo.size(); Index += 2)
			{
				Stereo[Index] = static_cast<Weltall::int16>(Next);
				Stereo[Index + 1] = static_cast<Weltall::int16>(-Next);
				++Next;
			}
		}

	private:
		int32 Next = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameSoundWaveTest, "Weltall.Audio.GameSoundWavePlaysTheGamesSamplesInOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameSoundWaveTest::RunTest(const FString&)
{
	UGameSoundWave* Wave = NewObject<UGameSoundWave>();
	Wave->SetSound(std::make_shared<FCountingSound>());
	TestEqual(TEXT("Channels"), Wave->NumChannels, 2);
	TestEqual(TEXT("Sample rate"), static_cast<int32>(Wave->GetSampleRateForCurrentPlatform()), Weltall::IGameSound::SampleRate);

	// The mixer asks for interleaved samples; a second request goes on where the first stopped.
	TArray<uint8> Audio;
	int32 Expected = 0;
	for (const int32 Samples : {1024, 256})
	{
		TestEqual(TEXT("Samples made"), Wave->OnGeneratePCMAudio(Audio, Samples), Samples);
		const Weltall::int16* Data = reinterpret_cast<const Weltall::int16*>(Audio.GetData());
		for (int32 Index = 0; Index < Samples; Index += 2)
		{
			if (Data[Index] != Expected || Data[Index + 1] != -Expected)
			{
				AddError(FString::Printf(TEXT("Sample pair %d reads %d, %d"), Expected, Data[Index], Data[Index + 1]));
				return false;
			}
			++Expected;
		}
	}
	return true;
}

#endif
