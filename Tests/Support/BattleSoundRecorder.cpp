#include "Support/BattleSoundRecorder.h"

#include "Support/BattleObjectServices.h"

namespace Weltall::Testing
{
	void FBattleSoundRecorder::RegisterEffects(std::span<const uint8> Seds)
	{
		const uint16 Id = SoundFileId(Seds);
		Registered.insert(Id);
		Events.push_back({"register", {Id}});
	}

	void FBattleSoundRecorder::UnregisterEffects(std::span<const uint8> Seds)
	{
		const uint16 Id = SoundFileId(Seds);
		Registered.erase(Id);
		Events.push_back({"unregister", {Id}});
	}

	void FBattleSoundRecorder::StopEffects(std::span<const uint8> Seds)
	{
		Events.push_back({"stop file", {SoundFileId(Seds)}});
	}

	void FBattleSoundRecorder::Digest(FOracleDigest& One)
	{
		for (const FEvent& Event : Events)
		{
			One.U8(0xEE, Event.Values.size() + 1);
			One.Raw(std::span<const uint8>(reinterpret_cast<const uint8*>(Event.Name.data()), Event.Name.size()));
			for (const uint32 Value : Event.Values)
			{
				One.I32(Value);
			}
		}
		Events.clear();
	}
}
