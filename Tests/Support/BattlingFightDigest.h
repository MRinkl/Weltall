#pragma once

#include "Battling/BattlingFight.h"
#include "Support/OracleDigest.h"

#include <array>
#include <string>
#include <vector>

namespace Weltall::Testing
{
	/** A sound driver that plays nothing and keeps every call, as the oracle's digests take them. */
	class FBattlingSoundRecorder final : public IBattlingSoundDriver
	{
	public:
		void PlayEffect(uint32 Effect, uint8 Voice, int16 Volume, int16 Pan) override;
		void SetPan(uint8 Voice, int16 Pan) override;
		void SetVolume(uint8 Voice, int16 Volume) override;
		void StopVoice(uint8 Voice) override;
		void StopAll() override;
		void StopSong() override;
		void SetSongTempo(int32 Scale) override;
		uint32 PlayingVoices(uint32 Effect) override;

		/** The calls since the last clear, as the oracle encodes them. */
		std::vector<uint8> Calls;
	};

	/** The fight's state as the fight's oracle digests it, part by part. */
	enum class EBattlingFightPart
	{
		Fighter1,
		Fighter2,
		Joints1,
		Joints2,
		Ai,
		Fight,
		Effects,
		Particles,
		Sound,
		/** The script machine's, in the tutorial's and the scripted bout's cases. */
		Script,
		/** The versus screen's fire and the fade in through it, in the cases that begin under it. */
		Fire,
	};

	inline constexpr std::array<EBattlingFightPart, 9> BattlingFightParts{EBattlingFightPart::Fighter1, EBattlingFightPart::Fighter2,
		EBattlingFightPart::Joints1, EBattlingFightPart::Joints2, EBattlingFightPart::Ai, EBattlingFightPart::Fight, EBattlingFightPart::Effects,
		EBattlingFightPart::Particles, EBattlingFightPart::Sound};

	/** One part's bytes; a run of whole sets adds the rounds' screens' state to the fight's, a run with menus their state too. */
	std::vector<uint8> BattlingFightPartBytes(const FBattlingFight& Fight, EBattlingFightPart Part, const FBattlingSoundRecorder& Sound,
		bool bRounds = false, bool bMenus = false);

	/**
	 * The arena's menus after a pass as the menus' oracle digests them: the menus, the gear choice, the
	 * settings, the idle countdown, the generator's seed, the fire and the sound calls.
	 */
	std::vector<uint8> BattlingArenaMenusBytes(const FBattlingFight& Fight, const FBattlingSoundRecorder& Sound);
}
