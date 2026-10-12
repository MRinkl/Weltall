#include "Chd/ChdCdTracks.h"

#include "Chd/ChdMetadata.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <string_view>
#include <vector>

namespace Chd
{
	namespace
	{
		/** The frames a long read takes from the image at a time: hunks enough to be unpacked on several threads. */
		constexpr uint64 FramesPerPiece = 2048;

		struct FTrackKind
		{
			std::string_view Name;
			ECdTrackType Type;
			uint32 SectorBytes;
		};

		/** The names a track's type goes by in its metadata: chdman's own, and the older ones that give the size. */
		constexpr std::array<FTrackKind, 15> TrackKinds{{
			{"MODE1", ECdTrackType::Mode1, 2048},
			{"MODE1/2048", ECdTrackType::Mode1, 2048},
			{"MODE1_RAW", ECdTrackType::Mode1Raw, 2352},
			{"MODE1/2352", ECdTrackType::Mode1Raw, 2352},
			{"MODE2", ECdTrackType::Mode2, 2336},
			{"MODE2/2336", ECdTrackType::Mode2, 2336},
			{"MODE2_FORM1", ECdTrackType::Mode2Form1, 2048},
			{"MODE2/2048", ECdTrackType::Mode2Form1, 2048},
			{"MODE2_FORM2", ECdTrackType::Mode2Form2, 2324},
			{"MODE2/2324", ECdTrackType::Mode2Form2, 2324},
			{"MODE2_FORM_MIX", ECdTrackType::Mode2FormMix, 2336},
			{"MODE2_RAW", ECdTrackType::Mode2Raw, 2352},
			{"MODE2/2352", ECdTrackType::Mode2Raw, 2352},
			{"CDI/2352", ECdTrackType::Mode2Raw, 2352},
			{"AUDIO", ECdTrackType::Audio, 2352},
		}};

		/** Tracks lie in the CHD each padded to a whole number of four frames. */
		constexpr uint32 TrackPadding = 4;

		bool ReadNumber(std::string_view Text, uint32& Value)
		{
			const auto [End, Error] = std::from_chars(Text.data(), Text.data() + Text.size(), Value);
			return Error == std::errc() && End == Text.data() + Text.size();
		}

		/** A track from its metadata's text: "TRACK:1 TYPE:MODE2_RAW SUBTYPE:NONE FRAMES:1234 PREGAP:150 PGTYPE:VAUDIO ...". */
		bool ParseTrack(std::string_view Text, FCdTrack& Track)
		{
			bool bHasNumber = false;
			bool bHasType = false;
			bool bHasFrames = false;
			while (!Text.empty())
			{
				const std::size_t End = std::min(Text.find(' '), Text.size());
				const std::string_view Field = Text.substr(0, End);
				Text.remove_prefix(std::min(End + 1, Text.size()));
				const std::size_t Colon = Field.find(':');
				if (Colon == std::string_view::npos)
				{
					continue;
				}
				const std::string_view Key = Field.substr(0, Colon);
				const std::string_view Value = Field.substr(Colon + 1);
				if (Key == "TRACK")
				{
					bHasNumber = ReadNumber(Value, Track.Number);
				}
				else if (Key == "TYPE")
				{
					const auto Kind = std::find_if(TrackKinds.begin(), TrackKinds.end(), [Value](const FTrackKind& Known) { return Known.Name == Value; });
					bHasType = Kind != TrackKinds.end();
					if (bHasType)
					{
						Track.Type = Kind->Type;
						Track.SectorBytes = Kind->SectorBytes;
					}
				}
				else if (Key == "FRAMES")
				{
					bHasFrames = ReadNumber(Value, Track.Frames);
				}
				else if (Key == "PREGAP" && !ReadNumber(Value, Track.Pregap))
				{
					return false;
				}
				else if (Key == "PGTYPE")
				{
					// A pregap whose sectors the image keeps has its type marked with a V.
					Track.bPregapInImage = !Value.empty() && Value.front() == 'V';
				}
			}
			return bHasNumber && bHasType && bHasFrames;
		}
	}

	TResult<std::vector<FCdTrack>> ReadCdTracks(const FFile& File)
	{
		std::vector<FCdTrack> Tracks;
		for (const FMetadata& Entry : File.Metadata())
		{
			if (Entry.Tag != MetadataTag::CdTrack && Entry.Tag != MetadataTag::CdTrackOld)
			{
				continue;
			}
			const std::string_view Text(reinterpret_cast<const char*>(Entry.Data.data()), Entry.Data.size());
			FCdTrack& Track = Tracks.emplace_back();
			if (!ParseTrack(Text.substr(0, Text.find('\0')), Track))
			{
				return EError::Corrupt;
			}
		}
		if (Tracks.empty())
		{
			return Tracks;
		}
		std::sort(Tracks.begin(), Tracks.end(), [](const FCdTrack& A, const FCdTrack& B) { return A.Number < B.Number; });

		const FHeader& Header = File.Header();
		if (Header.UnitBytes != CdFrameBytes)
		{
			return EError::Corrupt;
		}
		const uint64 FramesInImage = Header.LogicalBytes / CdFrameBytes;
		uint64 Next = 0;
		for (FCdTrack& Track : Tracks)
		{
			Track.FirstFrame = Next;
			Next += (static_cast<uint64>(Track.Frames) + TrackPadding - 1) / TrackPadding * TrackPadding;
			if ((Track.bPregapInImage && Track.Pregap > Track.Frames) || Track.FirstFrame + Track.Frames > FramesInImage)
			{
				return EError::Corrupt;
			}
		}
		return Tracks;
	}

	FCdTrackSectors::FCdTrackSectors(std::shared_ptr<const FImage> InImage, FCdTrack InTrack) : Image(std::move(InImage)), CdTrack(InTrack)
	{
	}

	bool FCdTrackSectors::Read(uint64 Offset, std::span<uint8> Destination) const
	{
		if (Offset > Size() || Destination.size() > Size() - Offset)
		{
			return false;
		}
		if (Destination.empty())
		{
			return true;
		}
		// The sectors' whole frames are read a piece at a time, so that a long read unpacks whole hunks on several threads.
		const uint64 FirstSector = Offset / CdTrack.SectorBytes;
		const uint64 EndSector = (Offset + Destination.size() - 1) / CdTrack.SectorBytes + 1;
		std::vector<uint8> Frames;
		std::size_t Done = 0;
		for (uint64 Piece = FirstSector; Piece < EndSector; Piece += FramesPerPiece)
		{
			const uint64 Count = std::min(FramesPerPiece, EndSector - Piece);
			Frames.resize(static_cast<std::size_t>(Count * CdFrameBytes));
			if (!Image->Read((CdTrack.FirstSectorFrame() + Piece) * CdFrameBytes, Frames))
			{
				return false;
			}
			for (uint64 Sector = Piece; Sector < Piece + Count; ++Sector)
			{
				const uint32 Within = static_cast<uint32>(Offset + Done - Sector * CdTrack.SectorBytes);
				const std::size_t Take = std::min<std::size_t>(CdTrack.SectorBytes - Within, Destination.size() - Done);
				const uint8* Frame = Frames.data() + (Sector - Piece) * CdFrameBytes;
				if (!CdTrack.IsAudio())
				{
					std::memcpy(Destination.data() + Done, Frame + Within, Take);
				}
				else
				{
					// Each sample's two bytes trade places.
					for (std::size_t Byte = 0; Byte < Take; ++Byte)
					{
						Destination[Done + Byte] = Frame[(Within + Byte) ^ 1];
					}
				}
				Done += Take;
			}
		}
		return true;
	}
}
