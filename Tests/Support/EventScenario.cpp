#include "Support/EventScenario.h"

#include "Foundation/LittleEndian.h"
#include "Support/EventScenarioProgram.h"

#include <algorithm>

namespace Weltall::Testing
{
	namespace
	{
		using namespace LittleEndian;
		using namespace EventScenarioShape;

		constexpr uint32 TriangleCount = 8;
		constexpr uint32 Grid = 4;
		struct FInventoryLayout
		{
			std::size_t Counts;
			std::size_t Items;
			uint32 Size;
		};
		constexpr std::array<FInventoryLayout, 5> Inventory = {{{0x1F90, 0x2026, 150}, {0x1D38, 0x1D9C, 100}, {0x1E00, 0x1EC8, 200}, {0x20BC, 0x2120, 100}, {0x2184, 0x221A, 150}}};

		/**
		 * A grid of squares over -500h..500h, two triangles each, wound so that the field's NCLIP tests
		 * hold inside; random heights and attribute indices (the camera's layer is flat).
		 */
		FWalkmeshLayer GenerateWalkLayer(FScenarioRandom& G, bool bFlat)
		{
			constexpr int32 Step = 0xA00 / static_cast<int32>(Grid);
			FWalkmeshLayer Layer;
			for (uint32 Row = 0; Row <= Grid; ++Row)
			{
				for (uint32 Column = 0; Column <= Grid; ++Column)
				{
					const int32 Height = bFlat ? 0 : static_cast<int32>(G.Below(0x100)) - 0x80;
					Layer.Vertices.push_back({static_cast<int16>(-0x500 + Step * static_cast<int32>(Column)), static_cast<int16>(Height), static_cast<int16>(-0x500 + Step * static_cast<int32>(Row))});
				}
			}
			for (uint32 Row = 0; Row < Grid; ++Row)
			{
				for (uint32 Column = 0; Column < Grid; ++Column)
				{
					const uint16 A = static_cast<uint16>(Row * (Grid + 1) + Column);
					const uint16 B = static_cast<uint16>(A + 1);
					const uint16 C = static_cast<uint16>((Row + 1) * (Grid + 1) + Column + 1);
					const uint16 D = static_cast<uint16>(C - 1);
					for (const std::array<uint16, 3>& Corners : {std::array<uint16, 3>{A, B, C}, std::array<uint16, 3>{A, C, D}})
					{
						const FGteVector& P0 = Layer.Vertices[Corners[0]];
						const FGteVector& P1 = Layer.Vertices[Corners[1]];
						const FGteVector& P2 = Layer.Vertices[Corners[2]];
						const int32 Area = P0[2] * P1[0] + P1[2] * P2[0] + P2[2] * P0[0] - P0[2] * P2[0] - P1[2] * P0[0] - P2[2] * P1[0];
						FWalkmeshTriangle Triangle;
						Triangle.Vertices = Area >= 0 ? Corners : std::array<uint16, 3>{Corners[0], Corners[2], Corners[1]};
						Triangle.Neighbours = {-1, -1, -1};
						Triangle.Attribute = static_cast<uint8>(G.Below(8));
						Layer.Triangles.push_back(Triangle);
					}
				}
			}
			return Layer;
		}
	}

	FEventScenario GenerateEventScenario(uint32 Seed, EEventScenarioFamily Family)
	{
		FScenarioRandom G(Seed);
		const uint32 Actors = 3 + G.Below(4);
		const uint32 Planned = 48 + G.Below(32);
		std::vector<uint32> Offsets;
		const std::vector<uint8> Code = GenerateEventProgram(G, Actors, Planned, Offsets, Family);
		auto Somewhere = [&]() { return static_cast<uint16>(Offsets[G.Below(Planned)]); };

		FEventScenario Scenario;
		Scenario.bStandInSprites = Family != EEventScenarioFamily::Machine;
		std::vector<uint8>& Section = Scenario.Section;
		auto Append = [&Section](uint32 Value, std::size_t Bytes)
		{
			for (std::size_t Byte = 0; Byte < Bytes; ++Byte)
			{
				Section.push_back(static_cast<uint8>(Value >> (8 * Byte)));
			}
		};
		for (int32 Word = 0; Word < 32; ++Word)
		{
			Append(G.Next(), 4);
		}
		Append(Actors, 4);
		for (uint32 Entry = 0; Entry < Actors * 32; ++Entry)
		{
			// The moves' inits end at once: the init ignores yields, so a move there would spin to the run
			// limit and leave its stage to the next script of its thread.
			const bool bInit = Family != EEventScenarioFamily::Machine && Entry % 32 == 0;
			Append(bInit ? Offsets.back() : Somewhere(), 2);
		}
		Section.insert(Section.end(), Code.begin(), Code.end());

		for (uint32 Actor = 0; Actor < Actors; ++Actor)
		{
			std::array<uint8, 0x5C> Record{};
			const std::span<uint8> Bytes(Record);
			uint16 Flags = 0;
			if (G.Below(8) != 0)
			{
				Flags = static_cast<uint16>(0x200 | (G.Below(2) == 0 ? 0x20 : 0));
			}
			// Flags that choose a model's draw mode: sorted by the farthest vertex, lit, the alternate mode.
			constexpr std::array<uint16, 4> DrawBits = {0, 0x10, 0x04, 0x4000};
			Flags |= DrawBits[G.Below(4)];
			// Half have a model with a MIMe animation, whose keys' weights the scripts set.
			if (G.Below(2) == 0)
			{
				Flags |= 0x2000;
			}
			WriteU16(Bytes, 0x58, Flags);
			for (const std::size_t At : {0x20, 0x40})
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					WriteU32(Bytes, At + 4 * Axis, static_cast<uint32>(static_cast<int32>(G.Below(0x800)) - 0x400));
				}
			}
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				WriteU16(Bytes, 0x50 + 2 * Axis, static_cast<uint16>(G.Next()));
			}
			Scenario.Records.push_back(Record);
		}
		for (uint32 Actor = 0; Actor < Actors; ++Actor)
		{
			std::array<uint8, EventStateBytes> State{};
			const std::span<uint8> Bytes(State);
			uint32 Flags0 = G.Next() & ~1u;
			Flags0 |= G.Below(8) == 0 ? 1u : 0u;
			uint32 Flags1 = G.Next() & ~0x100000u;
			Flags1 |= G.Below(10) == 0 ? 0x100000u : 0u;
			WriteU32(Bytes, 0x00, Flags0);
			WriteU32(Bytes, 0x04, Flags1);
			for (std::size_t Layer = 0; Layer < 4; ++Layer)
			{
				WriteU16(Bytes, 0x08 + 2 * Layer, static_cast<uint16>(G.Below(TriangleCount)));
			}
			WriteU16(Bytes, 0x10, static_cast<uint16>(G.Below(WalkingLayers)));
			WriteU16(Bytes, 0x1A, static_cast<uint16>(G.Below(0x80)));
			constexpr std::array<int32, 3> Reaches = {0x400, 0x100, 0x400};
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				const int32 Whole = static_cast<int32>(G.Below(static_cast<uint32>(2 * Reaches[Axis]))) - Reaches[Axis];
				WriteU32(Bytes, 0x20 + 4 * Axis, static_cast<uint32>(Whole) << 16 | (G.Next() & 0xFFFF));
			}
			const uint32 Depth = G.Below(5);
			for (std::size_t Entry = 0; Entry < 4; ++Entry)
			{
				WriteU16(Bytes, 0x78 + 2 * Entry, Somewhere());
			}
			WriteU32(Bytes, 0x12C, Depth << 6);
			for (std::size_t Slot = 0; Slot < 8; ++Slot)
			{
				const std::size_t At = 0x8C + 8 * Slot;
				uint16 Pc = 0xFFFF;
				uint8 Wait = 0;
				uint8 Script = 0xFF;
				uint32 Word = 0xFFFF | 0xFu << 18;
				if (G.Below(3) == 0)
				{
					Pc = Somewhere();
					const bool bWaits = G.Below(4) == 0;
					Wait = static_cast<uint8>(bWaits ? G.Below(3) : 0);
					Script = static_cast<uint8>(G.Below(32));
					Word = 0xFFFF | G.Below(8) << 18;
				}
				if (G.Below(8) == 0)
				{
					Word |= 0x400000;
				}
				if (G.Below(6) == 0)
				{
					Word |= G.Below(4) << 16;
				}
				WriteU16(Bytes, At, Pc);
				State[At + 2] = Wait;
				State[At + 3] = Script;
				WriteU32(Bytes, At + 4, Word);
			}
			State[0xCE] = static_cast<uint8>(G.Below(8));
			State[0xCF] = static_cast<uint8>(G.Below(8));
			Scenario.States.push_back(State);
		}
		for (uint16& Variable : Scenario.Variables)
		{
			Variable = static_cast<uint16>(G.Next());
		}
		const std::array<FGteVector, 4> Square = {FGteVector{-0x500, 0, -0x500}, FGteVector{0x500, 0, -0x500}, FGteVector{0x500, 0, 0x500}, FGteVector{-0x500, 0, 0x500}};
		FFieldZone Clockwise;
		Clockwise.Corners = Square;
		FFieldZone Counterclockwise;
		std::reverse_copy(Square.begin(), Square.end(), Counterclockwise.Corners.begin());
		FFieldZone Scattered;
		for (FGteVector& Corner : Scattered.Corners)
		{
			const int32 X = static_cast<int32>(G.Below(0x400)) - 0x200;
			const int32 Y = static_cast<int32>(G.Below(0x100)) - 0x80;
			const int32 Z = static_cast<int32>(G.Below(0x400)) - 0x200;
			Corner = {static_cast<int16>(X), static_cast<int16>(Y), static_cast<int16>(Z)};
		}
		Scenario.Zones = {Clockwise, Counterclockwise, Scattered};
		for (uint32 Layer = 0; Layer <= WalkingLayers; ++Layer)
		{
			Scenario.Walkmesh.Layers.push_back(GenerateWalkLayer(G, Layer == WalkingLayers));
		}
		for (int32 Word = 0; Word < 8; ++Word)
		{
			Scenario.Walkmesh.Attributes.push_back(G.Next());
		}
		for (uint8& Character : Scenario.PartyCharacters)
		{
			const bool bEmpty = G.Below(4) == 0;
			Character = static_cast<uint8>(bEmpty ? 0xFF : G.Below(11));
		}
		Scenario.PartyActors = {0, 1, 0xFF};
		Scenario.Leader = 0;
		Scenario.PadHeld = static_cast<uint16>(G.Next());
		Scenario.PadLatched = static_cast<uint16>(G.Next());
		Scenario.RandomSeed = G.Next();
		Scenario.bMovieOverField = G.Below(8) == 0;
		Scenario.bPartyWalking = G.Below(4) == 0;
		Scenario.ClockTicks = static_cast<int32>(G.Below(40));
		Scenario.ClockMode = ClockModes[G.Below(static_cast<uint32>(ClockModes.size()))];
		std::vector<uint8>& Game = Scenario.GameState;
		Game.resize(0x2358);
		const std::span<uint8> GameBytes(Game);
		WriteU32(GameBytes, 0x1924, G.Below(10000000));
		WriteU16(GameBytes, 0x1D30, static_cast<uint16>(G.Next()));
		for (const FInventoryLayout& List : Inventory)
		{
			for (uint32 Slot = 0; Slot < List.Size; ++Slot)
			{
				const bool bNearCap = G.Below(8) == 0;
				Game[List.Counts + Slot] = static_cast<uint8>(bNearCap ? 97 + G.Below(3) : G.Below(4));
				Game[List.Items + Slot] = static_cast<uint8>(G.Below(8));
			}
		}
		for (std::size_t Character = 0; Character < 11; ++Character)
		{
			for (std::size_t Points = 0; Points < 4; ++Points)
			{
				WriteU16(GameBytes, 0x26C + 0xA4 * Character + 0x4C + 2 * Points, static_cast<uint16>(G.Below(1000)));
			}
		}
		WriteU16(GameBytes, 0x1D32, static_cast<uint16>(G.Next()));
		WriteU16(GameBytes, 0x2318, static_cast<uint16>(G.Next()));
		for (std::size_t Character = 0; Character < 11; ++Character)
		{
			Game[0x26C + 0xA4 * Character + 0x77] = static_cast<uint8>(G.Below(100));
			Game[0x26C + 0xA4 * Character + 0x78] = static_cast<uint8>(G.Below(100));
		}
		for (std::size_t Gear = 0; Gear < 20; ++Gear)
		{
			const std::size_t Record = 0x978 + 0xA4 * Gear;
			WriteU16(GameBytes, Record + 0x38, static_cast<uint16>(G.Below(1000)));
			WriteU16(GameBytes, Record + 0x3A, static_cast<uint16>(G.Below(1000)));
			WriteU32(GameBytes, Record + 0x60, G.Below(100000));
			WriteU32(GameBytes, Record + 0x64, G.Below(100000));
		}
		Scenario.MapNumber = static_cast<int32>(G.Below(0x4000));
		for (uint32 Actor = 0; Actor < Actors; ++Actor)
		{
			if (G.Below(3) == 0)
			{
				Scenario.Resets.push_back(static_cast<int32>(Actor));
			}
		}
		Scenario.bMusicLoaded = G.Below(2) == 0;
		for (int32& Axis : Scenario.CameraEye)
		{
			Axis = static_cast<int32>(G.Next());
		}
		for (int32& Axis : Scenario.CameraTarget)
		{
			Axis = static_cast<int32>(G.Next());
		}
		if (Family != EEventScenarioFamily::Machine)
		{
			for (std::size_t Actor = 0; Actor < Scenario.States.size(); ++Actor)
			{
				Scenario.StandInGravity.push_back(static_cast<int32>(G.Below(0x20000)));
			}
		}
		for (FFieldActorSoundChannel& Channel : Scenario.ActorSoundChannels)
		{
			const uint32 Owner = G.Below(Actors + 2);
			if (Owner < Actors)
			{
				Channel = {static_cast<uint16>(Owner), static_cast<uint16>(1 + G.Below(0x100))};
			}
		}
		Scenario.PlayingEffects = G.Next() & 0xFFFF;
		return Scenario;
	}
}
