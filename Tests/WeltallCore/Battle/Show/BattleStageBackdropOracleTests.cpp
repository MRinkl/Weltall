#include "Battle/Show/BattleShowCamera.h"
#include "Battle/Show/BattleShowScreen.h"
#include "Battle/Show/BattleStageBackdrop.h"
#include "Battle/Show/BattleStageFiles.h"
#include "Battle/Show/BattleStageTint.h"
#include "Graphics/Vram.h"
#include "Support/BattleFrameDigest.h"
#include "Support/BattleObjectCases.h"
#include "Support/GameDiscs.h"
#include "Support/OracleDigest.h"
#include "Support/XorShift32.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <string>

using namespace Weltall;
using Weltall::Testing::DrawSigned;
using Weltall::Testing::FBattleObjectData;
using Weltall::Testing::FOracleDigest;
using Weltall::Testing::FXorShift32;

// Battle's stage backdrop as the original draws it: each stage that makes a
// panorama or a floor dome, built whole, then frames of its camera (a target on the ground, an eye around it, both moving a
// little each frame) drawn through the camera's view; each frame's digest walks the ordering table's last entry as
// the GPU draws it, keeping the panoramas' and the dome's packets: what drew it, its code, then a draw mode's words, a
// flat quad's colour and corners, a graded quad's colours and corners, a textured quad's corners, texels, CLUT and page.
// Its part "tint" runs the tint layers over those frames, the objects stepping after each, and adds the palettes' units
// in VRAM, both draw buffers' clear colours (the port keeps one) and the tint's two flags.
using Weltall::Testing::FBattleObjectCase;

namespace
{
	constexpr uint32 StageSeed = 0x57A6E0B5;
	constexpr uint32 MostFrames = 24;
	constexpr std::array<int32, 34> Stages{1, 2, 3, 5, 6, 7, 12, 16, 18, 20, 25, 27, 31, 38, 39, 40, 41, 43, 44, 45, 46, 47, 51, 53, 58, 61, 64, 65, 66,
		68, 69, 70, 71, 72};

	constexpr uint32 TintSeed = 0x7157A6E0;
	constexpr std::array<int32, 43> TintStages{1, 2, 3, 5, 6, 7, 12, 16, 18, 20, 25, 27, 31, 38, 39, 40, 41, 43, 44, 45, 46, 47, 51, 53, 58, 61, 64, 65,
		66, 68, 69, 70, 71, 72, 0, 4, 8, 13, 19, 36, 48, 56, 74};
	/** A layer past the four, which a set leaves alone; the modes from -1 (switched off) to 5 (past the five). */
	constexpr uint32 LayersDrawn = 5;
	constexpr uint32 ModesDrawn = 7;
	constexpr uint32 LevelsDrawn = 40;

	constexpr const char* Expected[Stages.size()] = {
		"4798e9c3671bde2887ce2450d26d40bdfe87bb4f",
		"59af399a73a61aba1cbad82d0f2cde64eb99efc7",
		"c2c4b31c1f40b447f478cec115106e3cdf2247d8",
		"764c0b18fdcadbc71c4ea0b9b221fb68fb635ca5",
		"9692204525ff1ddbc26a53ca2eec426505b713f8",
		"1a6d114d8de994493e38fdafa63f8fe6a53050be",
		"7c60aa6f63a8a6e88e278982ecaf05427920b698",
		"9f6353ff704b3761e390979446b751726c6230af",
		"c7cb5ca5dc0c0c7b5bffbe64223a154320897538",
		"ca98f13417604f62053b6202ddb0cfc03dbe3b62",
		"7c61cf178baa3083d96525058bf56c30e3e869f3",
		"9e2eb7d31c5625e339ae36aa82f4cdace3acc1bc",
		"3b7f5b74d4c0143c04919dab27048c58f84a1210",
		"37a4c8883ef0db33c52cb2e100de2343d6853a86",
		"c9f34feaa7e797b177d5c9aa1787c7efa0d35beb",
		"bea036644f59bbae9725f0689ed6fba44dd77797",
		"356c9473bb2ef847832f8c5ea9fc47a45787a6e9",
		"3ad60261d3f6a637adcbff56e5e0613b2099c7f7",
		"08f310fa08c3df132fa27e981b1b7d600c5dec70",
		"190d24810be605e93bb455814b94d238408ba887",
		"3d61da7867edc18d8bfceba11eb4d72a57a160f0",
		"7e5ae862f51f931e6128c846e73fb994e73cec25",
		"d9f8eedebe7424cc1cbf41cb7d59fa34e0f1dfc4",
		"021d29480649b541408e5863cabb170e60cf4614",
		"12da98682a87e96a9ea1c486f81dcfa539017ccf",
		"2a32f73df066e69928df15ee217d92dfd9d09a90",
		"6fe73740441b2bec859a1287e85618a900cf20e8",
		"a970fde2b52fc3f88de9b9b354e94d7fec94a6e7",
		"8776bee26bf3b7a507fc47693aa9496f5f2e1335",
		"f26091aeae688fa2ac71ddf7ca25150b2c540d18",
		"faa4c82cb9e7fa8131fef88d29686e781e233cae",
		"1d527191b44106a7ef80981503fb32bd09d4ab33",
		"3bdeed80d41c46b00cf4789a50ea4701a1b74d6b",
		"e00d4b4e56e79fa450e815421837e26feb4e7d72",
	};

	constexpr const char* ExpectedTints[TintStages.size()] = {
		"fd09568c90783dea067e5932e7688b08efb9603f",
		"5e2fb6fdeba2e5932f69c1d841924186ef5c2b86",
		"8c47d1066ac98ba43bef7fc22bbbef5c702a224b",
		"1ed593fff78382e1304db5023b2d7bfa1f52e59f",
		"6e4326a13840693ce6610322af9f4311b38d49b4",
		"f430f403ab7f9e0c567fba108c9fe5a09b56c679",
		"c84f8d4fdf171b5c7ed2545b1504e145cbc88d87",
		"08dcc789f6a82cdfe419ece147354acccd08f03b",
		"aa721fbcee2e6630edb8846e030719531773bc60",
		"8a7ff506274585a91a0cd827b3f21187b1ba7f27",
		"e23fadfd2a24cbc6ca69057e80446c695466f9d0",
		"fb1a8301eff5c47f3c145320bea5b1d10d9b96d0",
		"42b6d9dedc80f8feaff1946624499699309f6d9d",
		"cfa4a367198a86a3c5c6cbe15b2db45677fd406b",
		"a54d7b8876daf14acae2f386418a5c695a2cc5da",
		"2aa956a071c8ece53f0690e6afdc851aec085baf",
		"4fc6953d8b75bd5dbac454170b81f669049b58f6",
		"724dd0c41f339d09ac9c9cc0deb9f329c29a2880",
		"d29c6b6eede87df60cde2e25b7c6a2b4ca17f5e0",
		"de87c3fe844a4870e952d27b5d51425b41b1266a",
		"b4cac46576ee813cff4d8a10374232c808208efe",
		"1073edccdc49c261724a545ef0d2f9939052cd87",
		"b48972f09ec0b980aa70a3d55f9e904766b52dca",
		"5781a0ec4a514b18b655a7894ee94d8bfa3e3a27",
		"6a2972f7ce457e1993987875fffb654c0ce9f091",
		"5ff6825775cb49015f59b5fdddc8bfaed924136e",
		"668f58fa0a54fd4b807f63d446c6fe4ed4432a6a",
		"98b4f3a6d2300d63d4a7f13eb5d9570bf1d5605f",
		"a39b3023feebcde3b7782210062e30beb46ccba8",
		"06c054decc6c9932701829ee3eb244876ece67d7",
		"b251d81f9375bda7dc533e370a0cc0c01e18900a",
		"86dabd69ad2de71177f48c87c581cb972e79a4b8",
		"8b74933cb9c605300e2d1e5a711bc65c9f24df9e",
		"2969169f9e33cd44a7f40cbc4ceaef576c655685",
		"0f8a307766ea91b361aa5d0ac07095d9d541ca76",
		"d1b4a9aabd0622e6a82b7c0344b546943a04d1f2",
		"8e84a8ca30cbf0da79ee64ade13a8d811d050b8a",
		"08ec8355552c34ae03e0882c0c9e31f3c1879a7c",
		"e7097e0e5736441ec28c0430f1c40deb2ac25064",
		"54ff9f564d118d47e2ecca963b83c6c77c13159f",
		"7a6fc915f384525a580207a3862f654436099c8a",
		"8522b7d9d0b2966a0c231461ba731dc7c919b065",
		"2ff2197dd7cdc2a391acc8caa7b906c94af3126e",
	};

	/** The stage as the setup builds it for the tint, the objects stepping through a case's world, VRAM kept. */
	struct FTintCase
	{
		FTintCase(const FBattleObjectData& Data, int32 Stage) : World(Data), Files(Data.Stages[static_cast<std::size_t>(Stage)])
		{
			World.Services.Vram = &Vram;
			World.Gte.OffsetX = 0xA0 << 16;
			World.Gte.OffsetY = 0xA4 << 16;
			World.Gte.ProjectionDistance = 0x200;
			World.Objects.Stage = &Files.Record;
			World.Objects.Init(Files.Record);
			World.Objects.CreateStage(Files);
			World.Floor.Reset(Files.Record.FloorVertices(), Files.Record.FloorTriangles());
			Tint.Measure(Files.Model.Pack.Textures);
			Backdrop = MakeBattleStageBackdrop(Files.Record);
			if (Files.Record.ClearFlag() != 0)
			{
				Clear = Files.Record.ClearColour();
			}
		}

		/** The stage drawn through the camera (its CLUT animations, the tint, the backdrop), then the objects' step and drawing. */
		void DrawFrame(const FGteVector& Eye, const FGteVector& Target, int32 Buffer, const FGteArctangents& Arctangents)
		{
			World.Camera.Eye = Eye;
			World.Camera.Target = Target;
			World.Camera.BuildView({});
			World.Objects.StepStageClutAnimations(0);
			Tint.Apply(Layers, World.Objects, Backdrop, Clear);
			World.Objects.DrawStage(World.Camera.View, true);
			FGte Drawing = World.Gte;
			Drawing.Transform = World.Camera.View;
			DrawBattleStageBackdrop(Backdrop, Eye, Target, Drawing, Arctangents, Frame);
			World.Objects.UpdateDraw(World.Camera.View, 0, Buffer);
		}

		void DigestFrame(FOracleDigest& Digest) const
		{
			Weltall::Testing::DigestBattleBackdrop(Digest, Frame);
			const FVramRect& Palettes = Tint.Palettes();
			for (int32 Row = 0; Row < Palettes.Height; ++Row)
			{
				for (int32 Column = 0; Column < Palettes.Width; ++Column)
				{
					Digest.I32(Vram.At(Palettes.X + Column, Palettes.Y + Row));
				}
			}
			for (int32 Buffer = 0; Buffer < 2; ++Buffer)
			{
				Digest.I32(Clear[0], Clear[1], Clear[2]);
			}
			const FBattleClutAnimation& Kept = World.Objects.StageTintPalette;
			Digest.I32(Layers.bChanged || Kept.bFed, Kept.bActive);
		}

		FBattleObjectCase World;
		const FBattleStageFiles& Files;
		FVram Vram;
		FBattleTintLayers Layers;
		FBattleStageTint Tint;
		FBattleStageBackdrop Backdrop;
		std::array<uint8, 3> Clear{};
		FBattleBackdropFrame Frame;
	};

	std::string DigestTint(FXorShift32& G, const FBattleObjectData& Data, int32 Stage)
	{
		const auto Case = std::make_unique<FTintCase>(Data, Stage);
		REQUIRE_FALSE(Case->World.Objects.Faulted());
		FGteVector Target{static_cast<int16>(G.Next() % 0x2000), static_cast<int16>(-static_cast<int32>(G.Next() % 0x400)), static_cast<int16>(G.Next() % 0x2000)};
		const int16 AcrossX = static_cast<int16>(DrawSigned(G, 14));
		const int16 Height = static_cast<int16>(-static_cast<int32>(G.Next() % 0x1000) + 0x200);
		const int16 AcrossZ = static_cast<int16>(DrawSigned(G, 14));
		FGteVector Eye{static_cast<int16>(Target[0] + AcrossX), Height, static_cast<int16>(Target[2] + AcrossZ)};
		FOracleDigest Digest;
		const uint32 Frames = 1 + G.Next() % MostFrames;
		const uint32 Begin = G.Next() % Frames;
		const uint32 End = Begin + 1 + G.Next() % Frames;
		for (uint32 Number = 0; Number < Frames; ++Number)
		{
			if (Number == Begin)
			{
				// The tint's start: the layers switched off (the fade's part), the stage kept aside.
				Case->Layers.SwitchOff();
				Case->Tint.Begin(Case->Layers, Case->World.Objects, Case->Backdrop, Case->Clear);
			}
			if (Number == End)
			{
				Case->Tint.End(Case->World.Objects, Case->Backdrop, Case->Clear);
			}
			for (uint32 Set = G.Next() % 3; Set > 0; --Set)
			{
				const int32 Layer = static_cast<int32>(G.Next() % LayersDrawn);
				const int32 Mode = static_cast<int32>(G.Next() % ModesDrawn) - 1;
				const int32 Level = static_cast<int32>(G.Next() % LevelsDrawn);
				const int32 Red = DrawSigned(G, 8);
				const int32 Green = DrawSigned(G, 8);
				const int32 Blue = DrawSigned(G, 8);
				Case->Layers.Set(Layer, Mode, Level, Red, Green, Blue);
			}
			Case->DrawFrame(Eye, Target, static_cast<int32>(Number & 1), Data.Arctangents);
			REQUIRE_FALSE(Case->World.Objects.Faulted());
			Case->DigestFrame(Digest);
			for (FGteVector* Point : {&Eye, &Target})
			{
				for (int16& Axis : *Point)
				{
					Axis = static_cast<int16>(Axis + DrawSigned(G, 9));
				}
			}
		}
		if (End >= Frames)
		{
			Case->Tint.End(Case->World.Objects, Case->Backdrop, Case->Clear);
			Case->DrawFrame(Eye, Target, static_cast<int32>(Frames & 1), Data.Arctangents);
			Case->DigestFrame(Digest);
		}
		return Digest.Hex();
	}

	std::string DigestStage(FXorShift32& G, const FBattleObjectData& Data, int32 Stage)
	{
		FBattleStageBackdrop Backdrop = MakeBattleStageBackdrop(Data.Stages[static_cast<std::size_t>(Stage)].Record);
		FBattleCamera Camera;
		Camera.Init();
		FGte Gte;
		Gte.OffsetX = 0xA0 << 16;
		Gte.OffsetY = 0xA4 << 16;
		Gte.ProjectionDistance = 0x200;
		FGteVector Target{static_cast<int16>(G.Next() % 0x2000), static_cast<int16>(-static_cast<int32>(G.Next() % 0x400)), static_cast<int16>(G.Next() % 0x2000)};
		const int16 AcrossX = static_cast<int16>(DrawSigned(G, 14));
		const int16 Height = static_cast<int16>(-static_cast<int32>(G.Next() % 0x1000) + 0x200);
		const int16 AcrossZ = static_cast<int16>(DrawSigned(G, 14));
		FGteVector Eye{static_cast<int16>(Target[0] + AcrossX), Height, static_cast<int16>(Target[2] + AcrossZ)};
		FOracleDigest Digest;
		FBattleBackdropFrame Frame;
		const uint32 Frames = 1 + G.Next() % MostFrames;
		for (uint32 Number = 0; Number < Frames; ++Number)
		{
			Camera.Eye = Eye;
			Camera.Target = Target;
			Camera.BuildView({});
			Gte.Transform = Camera.View;
			DrawBattleStageBackdrop(Backdrop, Eye, Target, Gte, Data.Arctangents, Frame);
			Weltall::Testing::DigestBattleBackdrop(Digest, Frame);
			for (FGteVector* Point : {&Eye, &Target})
			{
				for (int16& Axis : *Point)
				{
					Axis = static_cast<int16>(Axis + DrawSigned(G, 9));
				}
			}
		}
		return Digest.Hex();
	}
}

TEST_CASE("Battle's stage tints recolour the palettes, the backdrop and the clear colour as the original's", "[Content][BattleObjects]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FBattleObjectData Data;
	REQUIRE(Weltall::Testing::ReadBattleObjectData(*Disc, Data));
	FXorShift32 G(TintSeed);
	for (std::size_t Case = 0; Case < TintStages.size(); ++Case)
	{
		INFO("case " << Case << ", stage " << TintStages[Case]);
		CHECK(DigestTint(G, Data, TintStages[Case]) == ExpectedTints[Case]);
	}
}

TEST_CASE("Battle's stage backdrops draw as the original draws them", "[Content][BattleObjects]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FBattleObjectData Data;
	REQUIRE(Weltall::Testing::ReadBattleObjectData(*Disc, Data));
	FXorShift32 G(StageSeed);
	for (std::size_t Case = 0; Case < Stages.size(); ++Case)
	{
		INFO("case " << Case << ", stage " << Stages[Case]);
		CHECK(DigestStage(G, Data, Stages[Case]) == Expected[Case]);
	}
}
