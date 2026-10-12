#include "Menu/MenuSaveCards.h"

#include "Game/SaveStore.h"
#include "Media/MemoryCard.h"
#include "Menu/MenuSaveHeader.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace Weltall
{
	namespace
	{
		/** A save's file name: the first disc's product code and a character for its slot, '0' the first. */
		constexpr std::string_view SaveName = "BASLUS-00664";
	}

	TResult<std::vector<uint8>> ExportStoreCard(const FSaveStore& Saves, int32 Store, const FMenuResources& Resources)
	{
		if (Store < 0 || Store >= FSaveStore::StoreCount)
		{
			return MakeError("there is no save store " + std::to_string(Store));
		}
		std::vector<FCardSave> Files;
		for (int32 Slot = 0; Slot < FSaveStore::SlotsPerStore; ++Slot)
		{
			TResult<std::optional<FOriginalSave>> Save = Saves.Read(Store * FSaveStore::SlotsPerStore + Slot);
			if (!Save)
			{
				return Save.Error();
			}
			if (!*Save)
			{
				continue;
			}
			const std::span<const uint8> Data = (*Save)->Data();
			const std::array<uint8, SaveHeaderSize> Header = MenuSaveHeader(Resources, Slot, Data);
			FCardSave& File = Files.emplace_back();
			File.FileName = std::string(SaveName) + static_cast<char>('0' + Slot);
			File.Blocks.assign(Header.begin(), Header.end());
			File.Blocks.insert(File.Blocks.end(), Data.begin(), Data.end());
		}
		return WriteMemoryCardImage(Files);
	}

	TResult<std::vector<FOriginalSave>> ReadCardImageSaves(std::span<const uint8> File)
	{
		TResult<std::vector<FCardSave>> Files = ReadMemoryCardImage(File);
		if (!Files)
		{
			return Files.Error();
		}
		std::vector<FOriginalSave> Saves;
		for (const FCardSave& Each : *Files)
		{
			if (TResult<FOriginalSave> Save = FOriginalSave::FromCard(Each))
			{
				Saves.push_back(std::move(*Save));
			}
		}
		return Saves;
	}
}
