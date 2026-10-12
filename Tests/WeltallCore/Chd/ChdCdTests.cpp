#include "Chd/ChdCdSector.h"
#include "Chd/ChdCdTracks.h"
#include "ChdFixtures.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <memory>

using namespace Chd;
using namespace Chd::Testing;

namespace
{
	std::shared_ptr<const FImage> OpenImage(std::string_view Hex)
	{
		auto Image = FImage::Open(std::make_shared<FMemoryInput>(FromHex(Hex)));
		REQUIRE(Image);
		return std::shared_ptr<const FImage>(std::move(*Image));
	}

	std::vector<uint8> ReadTrack(const std::shared_ptr<const FImage>& Image, const FCdTrack& Track)
	{
		const FCdTrackSectors Sectors(Image, Track);
		std::vector<uint8> Bytes(static_cast<std::size_t>(Sectors.Size()));
		REQUIRE(Sectors.Read(0, Bytes));
		return Bytes;
	}
}

TEST_CASE("Each CD track unpacks to its sectors as a BIN keeps them", "[Chd]")
{
	for (const FCdFixture* Fixture : {&CdDataFixture, &CdTracksFixture, &CdCookedFixture, &CdZstdFixture})
	{
		INFO(Fixture->Name);
		const auto Image = OpenImage(Fixture->Hex);
		CHECK(Image->File().Header().UnitBytes == CdFrameBytes);
		const auto Tracks = ReadCdTracks(Image->File());
		REQUIRE(Tracks);
		REQUIRE(Tracks->size() == Fixture->TrackCount);
		for (std::size_t Index = 0; Index < Tracks->size(); ++Index)
		{
			const FCdTrack& Track = (*Tracks)[Index];
			const FCdTrackExpected& Expected = Fixture->Tracks[Index];
			INFO("track " << Expected.Number);
			CHECK(Track.Number == Expected.Number);
			CHECK(Track.SectorBytes == Expected.SectorBytes);
			REQUIRE(Track.SectorCount() == Expected.Sectors);
			CHECK(Fnv1a(ReadTrack(Image, Track)) == Expected.Hash);
		}
	}
}

TEST_CASE("A CD's tracks follow each other padded to four frames with a pregap the image keeps first", "[Chd]")
{
	const auto Image = OpenImage(CdTracksFixture.Hex);
	const auto Tracks = ReadCdTracks(Image->File());
	REQUIRE(Tracks);
	REQUIRE(Tracks->size() == 3);
	const FCdTrack& Sound = (*Tracks)[0];
	const FCdTrack& Data = (*Tracks)[1];
	const FCdTrack& After = (*Tracks)[2];
	CHECK(Sound.IsAudio());
	CHECK(Sound.FirstFrame == 0);
	CHECK(Data.Type == ECdTrackType::Mode2Raw);
	CHECK(Data.FirstFrame == 8);
	CHECK(Data.bPregapInImage);
	CHECK(Data.Pregap == 20);
	CHECK(Data.FirstSectorFrame() == 28);
	// The data track's 50 frames take 52; the last track's pregap is not in the image.
	CHECK(After.FirstFrame == 60);
	CHECK_FALSE(After.bPregapInImage);
	CHECK(After.Pregap == 10);
	CHECK(After.SectorCount() == 6);

	// A DVD has none.
	const auto Dvd = FFile::Open(std::make_shared<FMemoryInput>(FromHex(LzmaFixture.Hex)));
	REQUIRE(Dvd);
	CHECK(ReadCdTracks(*Dvd)->empty());
}

TEST_CASE("A sector's sync and error-correction bytes are made again from its header and data", "[Chd]")
{
	const auto Data = OpenImage(CdDataFixture.Hex);
	const auto Mode2 = OpenImage(CdTracksFixture.Hex);
	const auto DataTracks = ReadCdTracks(Data->File());
	const auto Mode2Tracks = ReadCdTracks(Mode2->File());
	REQUIRE(DataTracks);
	REQUIRE(Mode2Tracks);
	const std::vector<uint8> Mode1Track = ReadTrack(Data, DataTracks->front());
	const std::vector<uint8> Mode2Track = ReadTrack(Mode2, (*Mode2Tracks)[1]);
	for (const std::span<const uint8> Original :
		{std::span(Mode1Track).subspan(20 * CdSectorBytes, CdSectorBytes), std::span(Mode2Track).subspan(3 * CdSectorBytes, CdSectorBytes)})
	{
		std::array<uint8, CdSectorBytes> Sector{};
		std::copy(Original.begin(), Original.end(), Sector.begin());
		std::fill_n(Sector.begin(), 12, uint8{0});
		std::fill(Sector.begin() + 0x81C, Sector.end(), uint8{0});
		RestoreSyncAndEcc(Sector);
		CHECK(std::equal(Sector.begin(), Sector.end(), Original.begin()));
	}
}

TEST_CASE("Reads of a CD track within and across its sectors agree", "[Chd]")
{
	const auto Image = OpenImage(CdTracksFixture.Hex);
	const auto Tracks = ReadCdTracks(Image->File());
	REQUIRE(Tracks);
	for (const FCdTrack& Track : *Tracks)
	{
		INFO("track " << Track.Number);
		const std::vector<uint8> Whole = ReadTrack(Image, Track);
		const FCdTrackSectors Sectors(Image, Track);
		std::vector<uint8> Pieces(Whole.size());
		for (std::size_t At = 0; At < Pieces.size(); At += 1001)
		{
			REQUIRE(Sectors.Read(At, std::span(Pieces).subspan(At, std::min<std::size_t>(1001, Pieces.size() - At))));
		}
		CHECK(Pieces == Whole);
		std::array<uint8, 2> Past{};
		CHECK_FALSE(Sectors.Read(Sectors.Size() - 1, Past));
	}
}
