#include "Story/DiscText.h"

#include "Battle/BattleEvent.h"

namespace Weltall
{
	FGameDiscText::FGameDiscText(const FCdImage& InImage, const FGameFileIndex& InIndex) : Image(InImage), Index(InIndex)
	{
	}

	FGameDiscText::~FGameDiscText() = default;

	std::span<const uint8> FGameDiscText::BattleEventMessages(int32 Event) const
	{
		if (!bRead)
		{
			bRead = true;
			if (TResult<FBattleEventArchive> Read = FBattleEventArchive::Load(Image, Index))
			{
				Events = std::make_unique<FBattleEventArchive>(std::move(*Read));
			}
		}
		return Events != nullptr && Event >= 0 && Event < Events->Count() ? Events->Messages(Event) : std::span<const uint8>();
	}
}
