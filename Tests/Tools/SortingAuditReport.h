#pragma once

#include "Support/SortingDrawing.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Weltall::Testing
{
	/** The player's framing a frame was drawn under: the view's zoom, tilt and turn as the program shows them. */
	struct FSortingFraming
	{
		float Zoom = 1.0f;
		float Tilt = 0.0f;
		float Turn = 0.0f;
	};

	/**
	 * What the audit writes of one place's frames (a field map, a world map's arrival, a save): a row for each pair of
	 * pieces that disagree in a frame, and for each class the frame where it covers the most pixels seen.
	 */
	class FSortingAuditReport
	{
	public:
		/** Rows into Rows as Place's; pairs of fewer than LeastPixels pixels are left out; with bPictures, the worst frames kept. */
		FSortingAuditReport(std::ofstream& InRows, std::string InPlace, uint32 InLeastPixels, bool bInPictures)
			: Rows(InRows), Place(std::move(InPlace)), LeastPixels(InLeastPixels), bPictures(bInPictures)
		{
		}

		/** A frame's audit, labelled Label, drawn under Framing; Describe names a piece of the frame by its number. */
		void Add(const std::string& Label, const FSortingFraming& Framing, const FSortingFrameAudit& Audit, const std::function<std::string(uint32)>& Describe);

		/**
		 * The worst frame of each class, by its pixels seen, about where they lie: the painter's order, the same with the
		 * class's pixels tinted, and the frame by depth, side by side (Folder/PLACE-CLASS-LABEL.bmp).
		 */
		void WritePictures(const std::filesystem::path& Folder) const;

	private:
		/** A frame in the painter's order and by depth, and where a class's conflicts lie in it. */
		struct FPicture
		{
			uint32 Covered = 0;
			std::string Label;
			int32 Width = 0;
			int32 Height = 0;
			std::vector<uint32> Painted;
			std::vector<uint32> Depth;
			std::vector<uint8> Mask;
			int32 Left = 0;
			int32 Top = 0;
			int32 Right = 0;
			int32 Bottom = 0;
		};

		std::ofstream& Rows;
		std::string Place;
		uint32 LeastPixels = 0;
		bool bPictures = false;
		std::map<std::string, FPicture> Pictures;
	};
}
