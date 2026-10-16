#pragma once

#include "Foundation/Types.h"
#include "Menu/MenuSettingsPages.h"

#include <optional>
#include <vector>

namespace Weltall
{
	namespace AssetRow
	{
		inline constexpr int32 External = 0, Capture = 1, Textures = 2, Dump = 3, Music = 4, Status = 5;
	}

	enum class EAssetExportState : uint8 { Idle, Running, Complete, Cancelled, Failed, Unavailable };

	/** What an export makes: PNGs of every texture, a native dump of the discs, or WAVs of the songs. */
	enum class EAssetExportKind : uint8 { Textures, Dump, Music };

	struct FMenuAssetProgress
	{
		EAssetExportState State = EAssetExportState::Idle;
		EAssetExportKind Kind = EAssetExportKind::Dump;
		/** Bytes for a dump, songs for music, the texture catalogue's items for textures. */
		uint64 Completed = 0;
		uint64 Total = 0;
		/** Changes when the status shown by the menu changes. */
		uint64 Revision = 0;
	};

	/** The host's incremental asset exports, one at a time, which continue while the menu is closed. */
	class IMenuAssets
	{
	public:
		virtual ~IMenuAssets() = default;
		virtual FMenuAssetProgress Progress() const = 0;
		virtual bool StartExport(EAssetExportKind Kind) = 0;
		virtual void CancelExport() = 0;
	};

	/** The export an action row starts, none for another row. */
	WELTALLCORE_API std::optional<EAssetExportKind> AssetRowExport(int32 Row);

	WELTALLCORE_API std::vector<FSettingsRow> AssetSettingsRows(const FPlayerSettings& Settings, FMenuAssetProgress Progress);
}
