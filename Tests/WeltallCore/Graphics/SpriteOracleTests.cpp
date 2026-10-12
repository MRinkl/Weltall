#include "Field/FieldMap.h"
#include "Field/FieldSpriteFiles.h"
#include "Game/PsxRandom.h"
#include "Graphics/SpriteFile.h"
#include "Graphics/SpriteSystem.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Support/OracleDigest.h"
#include "Support/SpriteDigest.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using namespace Weltall;
using Testing::FOracleDigest;
using Testing::FXorShift32;

// Digests of what the kernel's sprite code does with real sprite files, run under the oracle:
// each file made into a sprite as the field
// makes an actor's, then 64 ticks of a scenario drawn from XorShift32 (facing, animation, tint, blend
// and height changes), with the sprite, its spawned children, its uploads and its end calls digested
// every tick.

namespace
{
	constexpr int32 Ticks = 128;
	/** Every twelfth map, and the ones whose sprites set frames by number or attach models. */
	constexpr int32 ExtraMaps[] = {1, 2, 47, 48, 101, 130, 334};
	constexpr int32 MapStep = 12;
	constexpr int32 ExtraSetFirst = 0x77A;
	constexpr int32 ExtraSetCount = 121;
	constexpr int32 ExtraSetGroup = 15;

	struct FGroupDigest
	{
		const char* Name;
		const char* Digest;
	};

	constexpr FGroupDigest Digests[] = {
		{"map0", "bd3924ec36e007fe9eca609b21772f2e1d334e1f"},
		{"map1", "2e9b6726bf8c7bf1d753e77434879d46958497e6"},
		{"map2", "ea2f5b77ef53360b880dfde230a707bdb6e81359"},
		{"map12", "6b87462d32785c91473328580c1cd24305b07b6c"},
		{"map24", "c007e2f90c3455518927f4b09c5bfd2f92e3a938"},
		{"map36", "82587aa2720afd174f14680bb264ec48bd634575"},
		{"map47", "4bada7a65a9b9760b11cfe7a281e701bc81c5200"},
		{"map48", "070d6c4c049d66f0db775dfefb7f003ec7f81b74"},
		{"map60", "b1dbd89c25c68b4cb5f863d8970e123ef5d1b6a2"},
		{"map72", "14c23126c4a60e70e7b9e860249b5606d7a263ba"},
		{"map84", "98f3ebde6dd56ada2eab8dc9b6dd5f71a5ab0dc1"},
		{"map96", "ef1f775f3f026ff9135590e6754d29bc3748b9c0"},
		{"map101", "98e183ec3b81abc13dda4d506382fc32246b33f4"},
		{"map108", "0d5e6db47d4be48cb82b9cc852d1dfc168bfd09d"},
		{"map120", "72d5409f6722a38b3835778cf45b9b9da896ee4b"},
		{"map130", "bba38e71527fb04665f8c41e3baa3e13fde88aed"},
		{"map132", "0fb1ed9c6d7f60f471eef65dd30d0b5086b1aced"},
		{"map144", "d1b072b1b30d14d4588eff2a25c4c73291ba9e47"},
		{"map156", "1a346db348cd040ad2750f54658ec4e1bae082cd"},
		{"map168", "84bf3c48b87cf8a174a0c94f35080b38a0a7411f"},
		{"map180", "56211a07f9d177e4e66235ad7ee53268d8da5931"},
		{"map192", "73200a98a33d6478c87374708efd80c5e4244547"},
		{"map204", "00832a98d2997e6028ca00d52509b094e0379571"},
		{"map216", "7679e59578fd40ee06137d0e44f26a49401c6f47"},
		{"map228", "85c9e423dae1008d857f621e981e740ff852ee45"},
		{"map240", "31c058fc4ae8f5ba9a278836925943084b15cf2b"},
		{"map252", "da484d5ce845b42588a0a4a03092ddf2f4ff1189"},
		{"map264", "1093dae49aff8701fc5f3a92f7e740bd09af90e0"},
		{"map276", "af73b271739181b1ddfd909776e5db44f551f0b9"},
		{"map288", "f715b420c8bcac9a9844a6138a49908917bc1af6"},
		{"map300", "664f4672737edefdfb190a12f424d27a538848f3"},
		{"map312", "fc23f356197565af847ba73d7db49e024e45d602"},
		{"map324", "61113d1bd165b18930b1fab247f62050f6929b60"},
		{"map334", "224e2124ed04b55ad19dd1b8280e3fff940aa2bb"},
		{"map336", "57d97a815c355403a49fb21537217aa4de33c8a0"},
		{"map348", "f7cf733486585a38d502daf16e04c86fc34c9fb6"},
		{"map360", "55ba13948862f856be7beba41b2c61c5531b33cb"},
		{"map372", "6b2e7bfbca6d87670c72cf30daec98fc10eabf1d"},
		{"map384", "fb53ed91fb2c2e3d66aa2cafe64da34e0d20a285"},
		{"map396", "b05d665d762f344f19c03ecc4e21f014503c096c"},
		{"map408", "cc1641a88f9f741acaa0a0cdd5cba998aaaa1214"},
		{"map420", "e8d9ae86d8309f2c66f2e8cd8bcb0c382fdc6a80"},
		{"map432", "156af9d09d3da96864468c0177bcdb002a390420"},
		{"map444", "0e9909e409f0ed09acf7e17e0e931802960d9ff0"},
		{"map456", "39d13e0adaa0fbc1e7507c80211d0d26fad94621"},
		{"map468", "87a45d8c4d04383ad1ea41d3718e4f433963f066"},
		{"map480", "6394e72cfe2471953ee504346f07fb00a127c7b8"},
		{"map492", "88d261740cfd9e43d135170a2b471037f0c8dbb5"},
		{"map504", "c58ba14b2240058941ac9678663e3f40641dca90"},
		{"map516", "bed00d714dd63097432289205ff2ab3fa8e786ef"},
		{"map528", "12906e46c6fd1d596853ab8c08ee35ebed72b4e9"},
		{"map540", "73ab8aaaa4021d79ab9e04602829990821a06b9f"},
		{"map552", "adc57d2e11fcf675c6861ef3c45657d9cc83e3cf"},
		{"map564", "e1815e4223915f3b3954428a42e3b55b2fd3ef94"},
		{"map576", "d57a90701888a4e9119b7dda1733013c5bd72c5c"},
		{"map588", "9bfa3f1872a2b7c20159841a26ed18fa5689bb73"},
		{"map600", "74932c72910656b314ed50a9d9d8bc78475bb780"},
		{"map612", "3ff298fb986da435c1064afad5f95305366544cf"},
		{"map624", "ba24243c9537b9320dea097cd50a6c2290be1cbc"},
		{"map636", "db92187b0c3e76669a3c981d06a6ed0680bb227c"},
		{"map648", "ba951ff8f126996744efe5711628ec42f47a29ed"},
		{"map660", "be355ae1ec4e7575a9b8dcace5055d3f47325e8e"},
		{"map672", "e577972d4ac2dfe69fde364dd163566bf68ffb37"},
		{"map684", "c2248f37d520faea62d6c4444cfe9e7a6ea3befe"},
		{"map696", "7b0d7a1d3be27fd15cb1e1245b47dc124bdbd329"},
		{"map708", "4adaef9fd0c2db08f2581811f65d96e25df4230c"},
		{"map720", "1869306a71cd98088f0326e9e2fd51885fb55ea3"},
		{"4/5-20", "67c84ff690b65a51822b8752b13d49fe975eced5"},
		{"4/21-37", "214c87ed0a9a4a83fd5d19bf9087c538fc951f20"},
		{"4/0x77a+", "4ad5b57ad96b5d0a5d5bcb3ccd842e504aecbf0b"},
		{"4/0x789+", "f5a28f8f75e6ef65a97f6bc9a1edf7ac31943ee7"},
		{"4/0x798+", "73b36b3c19e2b8f31066e57b333d36e966023ea4"},
		{"4/0x7a7+", "a0869e1effdd708a2896a07becc4252076f04e56"},
		{"4/0x7b6+", "74301ee510c3629ae8287ec9e2d8091e8fa41fbd"},
		{"4/0x7c5+", "7bb55dedf445b1a724686c11738b319574e72084"},
		{"4/0x7d4+", "4ad5031006d9c396f449e2bb3904a7b3b8561d69"},
		{"4/0x7e3+", "9aacf68233bd56acf5c47589e0b80fba4fc1549c"},
		{"4/0x7f2+", "8221307203496de2d0e5419e783e8a4bfc539626"},
	};

	struct FSpriteCase
	{
		FSpriteFile File;
		int32 ClutX = 0;
		int32 ClutY = 0;
		int32 TextureX = 0;
		int32 TextureY = 0;
		int32 PartRecords = 0;
	};

	struct FSpriteGroup
	{
		std::string Name;
		std::vector<FSpriteCase> Cases;
	};

	class FCountingListener final : public ISpriteListener
	{
	public:
		void OnAnimationEvent(FSprite&) override { ++Calls; }

		int32 Calls = 0;
	};

	void AddStreamedGroup(std::vector<FSpriteGroup>& Groups, const FIso9660& Disc, const FGameFileIndex& Index, std::string Name, int32 First, int32 Count)
	{
		FSpriteGroup& Group = Groups.emplace_back();
		Group.Name = std::move(Name);
		for (int32 Number = 0; Number < Count; ++Number)
		{
			auto File = ReadSpriteFile(Disc.Image(), Index, 4, First + Number);
			REQUIRE(File);
			const int32 Slot = Number % 3;
			Group.Cases.push_back({std::move(*File), 0x100, 0xE0 + Slot, 0x280, 0x100 + 0x40 * Slot, 0x20});
		}
	}

	/** The groups the oracle runs: some maps' sprites, the party's human and gear sprites, the extra animation sets. */
	std::vector<FSpriteGroup> LoadGroups(const FIso9660& Disc)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		std::vector<int32> Maps(std::begin(ExtraMaps), std::end(ExtraMaps));
		for (int32 Number = 0; Number < FieldMapCount; Number += MapStep)
		{
			Maps.push_back(Number);
		}
		std::sort(Maps.begin(), Maps.end());
		Maps.erase(std::unique(Maps.begin(), Maps.end()), Maps.end());
		std::vector<FSpriteGroup> Groups;
		for (const int32 Number : Maps)
		{
			const auto Map = LoadFieldMap(Disc.Image(), *Index, Number);
			REQUIRE(Map);
			auto Files = ParseFieldSpriteFiles(Map->SpriteSets);
			REQUIRE(Files);
			FSpriteGroup& Group = Groups.emplace_back();
			Group.Name = "map" + std::to_string(Number);
			for (std::size_t Slot = 0; Slot < Files->size(); ++Slot)
			{
				const FFieldSpriteSlot& Place = Map->SpriteSlots[Slot];
				Group.Cases.push_back({std::move((*Files)[Slot]), 0x100, 0x1E0 + static_cast<int32>(Slot), Place.X, Place.Y, 0});
			}
		}
		AddStreamedGroup(Groups, Disc, *Index, "4/5-20", 5, 16);
		AddStreamedGroup(Groups, Disc, *Index, "4/21-37", 21, 17);
		for (int32 First = 0; First < ExtraSetCount; First += ExtraSetGroup)
		{
			char Name[16];
			std::snprintf(Name, sizeof(Name), "4/%#x+", ExtraSetFirst + First);
			AddStreamedGroup(Groups, Disc, *Index, Name, ExtraSetFirst + First, std::min(ExtraSetGroup, ExtraSetCount - First));
		}
		return Groups;
	}

	void DigestTick(FOracleDigest& Digest, const FSpriteSystem& System, const FSprite& Sprite, const FCountingListener& Listener)
	{
		Testing::DigestSprite(Digest, Sprite);
		for (const FKernelTaskEntry& Entry : System.Tasks())
		{
			const FSpriteTask* Task = Entry.Sprite.get();
			const bool bOwnerMatches = Task->Owner != nullptr && (Task->Marks & 0x1FFFFFFF) == FSpriteSystem::OwnerNumber(*Task->Owner);
			Digest.U8(1, Task->Marks >> 29 & 3, bOwnerMatches ? 1 : 0, Task->bDrawn ? 1 : 0);
			Testing::DigestSprite(Digest, Task->Sprite);
		}
		Digest.U8(0);
		for (const bool bNow : {false, true})
		{
			for (const FSpriteUpload& Upload : System.Uploads())
			{
				if (Upload.bNow == bNow)
				{
					Digest.I32(Upload.File != nullptr ? Upload.Offset : 0xFFFFFFFFu);
					Digest.I16(Upload.Rect.X, Upload.Rect.Y, Upload.Rect.Width, Upload.Rect.Height);
				}
			}
			Digest.U8(0xFF);
		}
		Digest.I32(Listener.Calls);
	}

	void RunCase(FOracleDigest& Digest, const FSpriteCase& Case, uint32 Number, FVram& Vram)
	{
		FXorShift32 Rng(0x5B000000u + Number);
		FPsxRandom Random(Rng.Next());
		FSpriteSystem System;
		System.Random = &Random;
		FSprite Sprite;
		FCountingListener Listener;
		System.Create(Sprite, Case.File, Case.ClutX, Case.ClutY, Case.TextureX, Case.TextureY);
		if (Case.PartRecords != 0)
		{
			System.SetPartCapacity(Sprite, Case.PartRecords);
		}
		const FSpriteFrameSize Size = System.FirstFrameSize(Sprite);
		Digest.I32(Size.Right, Size.Top, Size.Middle);
		System.SetSizeShift(Sprite, 3);
		Sprite.Scale = 0xC00;
		Sprite.Weight = 0x2000;
		const int32 Floor = static_cast<int32>(Rng.Next() % 512) - 256;
		const int32 X = static_cast<int32>(Rng.Next() % 2048) - 1024;
		const int32 Z = static_cast<int32>(Rng.Next() % 2048) - 1024;
		Sprite.Position = {X * 0x10000, Floor * 0x10000, Z * 0x10000};
		Sprite.Gravity = 0x10000;
		Sprite.FloorY = static_cast<int16>(Floor);
		System.SetAnimation(Sprite, 0);
		System.SetDirection(Sprite, 0);
		Sprite.Listener = &Listener;
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			const uint32 Choice = Rng.Next();
			if (Choice % 8 == 0)
			{
				System.SetFacing(Sprite, static_cast<int32>(Rng.Next() & 0xFFFF));
			}
			if (Choice % 16 == 1)
			{
				System.SetAnimation(Sprite, static_cast<int32>(Rng.Next() % static_cast<uint32>(Case.File.AnimationCount(Case.File.Tables()))));
			}
			if (Choice % 16 == 3)
			{
				const int32 Red = static_cast<int32>(Rng.Next() & 0xFF);
				const int32 Green = static_cast<int32>(Rng.Next() & 0xFF);
				const int32 Blue = static_cast<int32>(Rng.Next() & 0xFF);
				System.SetTint(Sprite, Red, Green, Blue);
			}
			if (Choice % 64 == 5)
			{
				System.SetBlend(Sprite, static_cast<int32>(Rng.Next() % 5));
			}
			if (Choice % 32 == 7)
			{
				Sprite.Position[1] = (Floor - static_cast<int32>(Rng.Next() % 64)) * 0x10000;
				Sprite.Velocity[1] = static_cast<int32>(Rng.Next() & 0xFFFFF) - 0x80000;
			}
			System.TickAnimation(Sprite);
			System.RunTasks();
			System.BuildPendingFrames();
			DigestTick(Digest, System, Sprite, Listener);
			System.FlushUploads(Vram);
		}
		if (auto Error = System.FaultError())
		{
			FAIL(Error->Message);
		}
		System.EndTasks();
	}
}

TEST_CASE("The field's sprite files animate and build their frames as the kernel runs them", "[Graphics][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const std::vector<FSpriteGroup> Groups = LoadGroups(*Disc);
	REQUIRE(Groups.size() == std::size(Digests));
	FVram Vram;
	uint32 Number = 0;
	for (std::size_t Group = 0; Group < Groups.size(); ++Group)
	{
		INFO(Groups[Group].Name);
		REQUIRE(Groups[Group].Name == Digests[Group].Name);
		FOracleDigest Digest;
		for (const FSpriteCase& Case : Groups[Group].Cases)
		{
			RunCase(Digest, Case, Number++, Vram);
		}
		CHECK(Digest.Hex() == Digests[Group].Digest);
	}
}
