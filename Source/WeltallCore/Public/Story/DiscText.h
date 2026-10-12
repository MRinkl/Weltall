#pragma once

#include "Foundation/Types.h"

#include <memory>
#include <span>

namespace Weltall
{
	class FBattleEventArchive;
	class FCdImage;
	class FGameFileIndex;

	/**
	 * The disc's own text, for a document of the port's story that names a message of it: the words stay the disc's,
	 * read from it as the game runs, and the document says only which they are.
	 */
	class IDiscText
	{
	public:
		virtual ~IDiscText() = default;

		/** Battle event Event's message table, by the number a formation names; empty where the disc has no such event. */
		virtual std::span<const uint8> BattleEventMessages(int32 Event) const = 0;
	};

	/** The text of the disc a game plays from: its battle events' archive, read when it is first asked for. */
	class FGameDiscText final : public IDiscText
	{
	public:
		WELTALLCORE_API FGameDiscText(const FCdImage& InImage, const FGameFileIndex& InIndex);
		WELTALLCORE_API ~FGameDiscText() override;

		WELTALLCORE_API std::span<const uint8> BattleEventMessages(int32 Event) const override;

	private:
		const FCdImage& Image;
		const FGameFileIndex& Index;
		/** Null until asked for, and after that where the disc's archive cannot be read. */
		mutable std::unique_ptr<FBattleEventArchive> Events;
		mutable bool bRead = false;
	};
}
