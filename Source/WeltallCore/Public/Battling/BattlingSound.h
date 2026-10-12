#pragma once

#include "Battling/BattlingData.h"
#include "Battling/BattlingSkeleton.h"
#include "Graphics/Gte.h"

#include <array>

namespace Weltall
{
	class FPsxRandom;

	/** What Battling's sound channels ask of the sound driver: effects of its own effect file, two voices each. */
	class IBattlingSoundDriver
	{
	public:
		virtual ~IBattlingSoundDriver() = default;
		virtual void PlayEffect(uint32 Effect, uint8 Voice, int16 Volume, int16 Pan) = 0;
		virtual void SetPan(uint8 Voice, int16 Pan) = 0;
		virtual void SetVolume(uint8 Voice, int16 Volume) = 0;
		virtual void StopVoice(uint8 Voice) = 0;
		virtual void StopAll() = 0;
		/** Stops the song the field left playing. */
		virtual void StopSong() = 0;
		/** The song's tempo scaled (100h as written). */
		virtual void SetSongTempo(int32 Scale) = 0;
		/** The voices (as a bit mask) still playing Effect. */
		virtual uint32 PlayingVoices(uint32 Effect) = 0;
	};

	/** How a channel's sound is placed: in the middle at full volume, at a point copied as it starts, or following a point. */
	enum class EBattlingSoundPlace : uint8
	{
		Centred,
		Fixed,
		Following,
	};

	/** Battling's four effect channels: each a pair of voices, the oldest taken when none is free. */
	class FBattlingSoundChannels
	{
	public:
		static constexpr int32 ChannelCount = 4;
		/** Battling's effects are the program's effect file's, numbered from here. */
		static constexpr uint32 EffectBase = 0x60000;

		explicit FBattlingSoundChannels(IBattlingSoundDriver& InDriver);

		WELTALLCORE_API void Reset();

		/** Every effect stopped at the driver; the channels keep their records until they are polled. */
		void StopAll() { Driver.StopAll(); }

		void StopSong() { Driver.StopSong(); }

		void SetSongTempo(int32 Scale) { Driver.SetSongTempo(Scale); }

		/** A system sound: centred, full volume, tagged with the vertical blank's count. */
		WELTALLCORE_API void Play(int32 Effect, uint32 VBlank);

		/** An effect of the program's file at a point, tagged by the fighter's side and the effect. */
		WELTALLCORE_API void PlayAt(int32 Side, int32 Effect, const FBattlingPoint* Point, EBattlingSoundPlace Place);

		/** A sound of the fighter's set (index 0 a random one of the six hits). */
		WELTALLCORE_API void PlayFighter(const FBattlingProgramTables& Tables, const FBattlingSoundSet& Set, int32 Side, int32 Index,
			const FBattlingPoint* Point, EBattlingSoundPlace Place, FPsxRandom& Random);

		WELTALLCORE_API void StopFighter(const FBattlingProgramTables& Tables, const FBattlingSoundSet& Set, int32 Side, int32 Index);

		/** Drops the channels whose voices have finished, and ages them; once a pass after the frame. */
		WELTALLCORE_API void Poll();

		/** Places the point sounds by the frame's view (pan by the screen x, volume by the depth) and ages the channels. */
		WELTALLCORE_API void UpdatePositions(const FGte& View, const FBattlingPoint& Origin);

		struct FChannel
		{
			EBattlingSoundPlace Place = EBattlingSoundPlace::Centred;
			bool bActive = false;
			uint8 Voice = 0;
			uint8 Tag = 0;
			uint16 Age = 0;
			uint32 Effect = 0;
			uint32 VoiceMask = 0;
			FBattlingPoint Point{};
			const FBattlingPoint* Following = nullptr;
		};

		std::array<FChannel, ChannelCount> Channels{};

	private:
		void Start(uint32 Effect, EBattlingSoundPlace Place, const FBattlingPoint* Point, uint8 Tag);
		void StopTag(uint8 Tag);
		void Age();

		IBattlingSoundDriver& Driver;
	};
}
