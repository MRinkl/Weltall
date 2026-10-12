#include "Battle/Show/BattleStageBackdrop.h"

#include "Battle/Show/BattleStageFiles.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 PanoramaSlots = 2;
		constexpr int32 GridCells = 8;
		constexpr int32 GridSide = GridCells + 1;
		constexpr int16 ScreenWidth = 320;
		constexpr int16 ScreenHeight = 240;
		/** The horizon's unclamped place counts for the glow only within twice the screen's height. */
		constexpr uint16 GlowReach = 480;
		/** The kernel's page for a draw mode: page 0, 4-bit, blend mode 1 (adding). */
		constexpr uint16 AddingPage = 0x20;
		constexpr uint16 BlendModeAdding = 1;

		/** The record's colours as the stage's build passes them: the sky's, the graded band's top and the ground's. */
		constexpr std::size_t ColoursOffset = 0x458;
		constexpr std::size_t ColourStride = 4;

		namespace Words
		{
			constexpr std::size_t VramX = 0;
			constexpr std::size_t VramY = 1;
			/** The panorama's width, the dome's tile. */
			constexpr std::size_t Width = 2;
			/** The panorama's height, how far the dome's grid lies above a quarter of the eye's height. */
			constexpr std::size_t Height = 3;
			constexpr std::size_t ClutX = 5;
			constexpr std::size_t ClutY = 6;
			/** The panorama's page, the dome's grid spacing. */
			constexpr std::size_t Page = 7;
			/** The panorama's scroll, the dome's glow scale. */
			constexpr std::size_t Scroll = 8;
			/** The second kind of panorama's band scale, the dome's band scale. */
			constexpr std::size_t BandScale = 9;
			constexpr std::size_t DepthDivisor = 10;
			constexpr std::size_t DepthOffset = 11;
		}

		/** A division by 1 << Shift that rounds toward zero, as the compiler divides a signed value by a power of two. */
		int32 Divide(int32 Value, int32 Shift)
		{
			return (Value < 0 ? Value + ((1 << Shift) - 1) : Value) >> Shift;
		}

		int32 Wrap(int64 Value)
		{
			return static_cast<int32>(static_cast<uint32>(Value));
		}

		std::array<uint8, 3> RecordColour(const FBattleStageRecord& Record, std::size_t Index)
		{
			const std::span<const uint8> Bytes = Record.Bytes();
			const std::size_t At = ColoursOffset + ColourStride * Index;
			return {Bytes[At], Bytes[At + 1], Bytes[At + 2]};
		}

		/** A panorama as the stage's build makes it from the element: a panorama of the second kind has the record's colours. */
		FFieldPanorama MakePanorama(const FBattleStageElement& Element, bool bColoured, const FBattleStageRecord& Record)
		{
			FFieldPanorama Panorama;
			Panorama.bEnabled = true;
			Panorama.VramX = Element.Words[Words::VramX];
			Panorama.VramY = Element.Words[Words::VramY];
			Panorama.Width = Element.Words[Words::Width];
			Panorama.Height = Element.Words[Words::Height];
			Panorama.ClutX = Element.Words[Words::ClutX];
			Panorama.ClutY = Element.Words[Words::ClutY];
			Panorama.TexturePage = Element.Words[Words::Page];
			Panorama.Scroll = Element.Words[Words::Scroll];
			Panorama.Position = {Element.Point[0], Element.Point[1], Element.Point[2]};
			Panorama.bBands = bColoured;
			if (bColoured)
			{
				for (std::size_t Band = 0; Band < Panorama.Colors.size(); ++Band)
				{
					Panorama.Colors[Band] = RecordColour(Record, Band);
				}
				Panorama.BandScale = Element.Words[Words::BandScale];
			}
			Panorama.DepthDivisor = Element.Words[Words::DepthDivisor];
			Panorama.DepthOffset = Element.Words[Words::DepthOffset];
			return Panorama;
		}

		/** libgte's GetTPage for a 4-bit page adding. */
		uint16 AddingPageAt(int32 X, int32 Y)
		{
			return static_cast<uint16>(BlendModeAdding << 5 | (Y & 0x100) >> 4 | (X & 0x3FF) >> 6 | (Y & 0x200) << 2);
		}

		/** The setup's dome from the element. */
		FBattleFloorDome MakeDome(const FBattleStageElement& Element, const FBattleStageRecord& Record)
		{
			FBattleFloorDome Dome;
			const std::array<int16, 4> Grid = Record.DomeGrid();
			Dome.ScrollStep = {Grid[2], Grid[3]};
			Dome.ProjectionDistance = Grid[0];
			Dome.TiltScale = Grid[1];
			Dome.Tile = Element.Words[Words::Width];
			Dome.Lift = Element.Words[Words::Height];
			Dome.GlowScale = Element.Words[Words::Scroll];
			Dome.BandScale = Element.Words[Words::BandScale];
			Dome.HorizonHeight = static_cast<int16>(Element.Point[1]);
			Dome.HorizonDistance = static_cast<int16>(Element.Point[2]);
			const int16 Spacing = Element.Words[Words::Page];
			for (int32 Row = 0; Row < GridSide; ++Row)
			{
				for (int32 Column = 0; Column < GridSide; ++Column)
				{
					Dome.Points[static_cast<std::size_t>(Row * GridSide + Column)] = {static_cast<int16>((Column - GridCells / 2) * Spacing), 0,
						static_cast<int16>((Row - GridCells / 2) * Spacing)};
				}
			}
			for (std::size_t Band = 0; Band < Dome.Colors.size(); ++Band)
			{
				Dome.Colors[Band] = RecordColour(Record, Band);
			}
			for (std::size_t Channel = 0; Channel < Dome.Glow[0].size(); ++Channel)
			{
				Dome.Glow[0][Channel] = static_cast<uint8>(0xFF - Dome.Colors[0][Channel]);
			}
			const int32 X = Element.Words[Words::VramX];
			const int32 Y = Element.Words[Words::VramY];
			const int32 PageX = Divide(X, 6) << 6;
			const int32 PageY = Divide(Y, 8) << 8;
			Dome.TexturePage = AddingPageAt(PageX, PageY);
			Dome.Clut = static_cast<uint16>(Element.Words[Words::ClutY] << 6 | ((Element.Words[Words::ClutX] >> 4) & 0x3F));
			Dome.WindowX = static_cast<int16>(X - PageX);
			Dome.WindowY = static_cast<int16>(Y - PageY);
			return Dome;
		}

		FBattleDomePacket DrawMode(const std::array<int16, 4>& Window)
		{
			FBattleDomePacket Packet;
			Packet.bDrawMode = true;
			Packet.DrawPage = AddingPage;
			Packet.Window = Window;
			return Packet;
		}

		/** A band across the screen from Top to Bottom, of the top's colour and the bottom's. */
		FBattleDomePacket Band(int16 Top, int16 Bottom, const std::array<uint8, 3>& TopColour, const std::array<uint8, 3>& BottomColour, bool bGraded)
		{
			FBattleDomePacket Packet;
			FFieldBackdropQuad& Quad = Packet.Quad;
			Quad.Kind = bGraded ? FFieldBackdropQuad::EKind::Graded : FFieldBackdropQuad::EKind::Flat;
			Quad.Corners = {{{0, Top}, {ScreenWidth, Top}, {0, Bottom}, {ScreenWidth, Bottom}}};
			Quad.Colors = {TopColour, TopColour, BottomColour, BottomColour};
			return Packet;
		}

		/** The point Scale (8.8) times the horizon's point Ahead of Target, at Scale times Height. */
		FGteVector Scaled(const std::array<int32, 2>& Ahead, int16 Height, int32 Scale, const FGteVector& Target)
		{
			return {static_cast<int16>(static_cast<int16>(Divide(Ahead[0] * Scale, 8)) + Target[0]), static_cast<int16>(Divide(Height * Scale, 8)),
				static_cast<int16>(static_cast<int16>(Divide(Ahead[1] * Scale, 8)) + Target[2])};
		}

		/** The floor: the dome's packets in the order it links them. */
		void DrawDome(FBattleFloorDome& Dome, const FGteVector& Eye, const FGteVector& Target, const FGte& Gte, const FGteArctangents& Arctangents,
			std::vector<FBattleDomePacket>& Packets)
		{
			Packets.push_back(DrawMode({}));
			const int32 AcrossX = Target[0] - Eye[0];
			const int32 AcrossZ = Target[2] - Eye[2];
			const FGteVector Heading = VectorNormalS({AcrossX, 0, AcrossZ});
			const std::array<int32, 2> Ahead{Divide(Heading[0] * Dome.HorizonDistance, 12), Divide(Heading[2] * Dome.HorizonDistance, 12)};
			const FGteVector Horizon{static_cast<int16>(static_cast<int16>(Ahead[0]) + Target[0]), Dome.HorizonHeight,
				static_cast<int16>(static_cast<int16>(Ahead[1]) + Target[2])};
			const int16 HorizonY = Gte.Rtps(Horizon).ScreenY;
			const int16 ClampedY = std::min<int16>(HorizonY, ScreenHeight);

			// The graded band from the horizon down to its end, and the ground below it.
			int16 BandY = Gte.Rtps(Scaled(Ahead, Dome.HorizonHeight, Dome.BandScale, Target)).ScreenY;
			if (BandY >= 0)
			{
				if (ClampedY < ScreenHeight)
				{
					Packets.push_back(Band(ClampedY, BandY, Dome.Colors[1], Dome.Colors[2], true));
				}
			}
			else
			{
				BandY = 0;
			}
			if (BandY < ScreenHeight)
			{
				Packets.push_back(Band(BandY, ScreenHeight, Dome.Colors[2], Dome.Colors[2], false));
			}

			// The glow above the horizon, added, from its top at the horizon to its bottom at the glow point's mirror.
			const int16 GlowY = Gte.Rtps(Scaled(Ahead, Dome.HorizonHeight, Dome.GlowScale, Target)).ScreenY;
			const int16 GlowEnd = static_cast<int16>(ClampedY * 2 - GlowY - 8);
			if (static_cast<uint16>(HorizonY) < GlowReach && GlowEnd < ScreenHeight)
			{
				FBattleDomePacket Packet = Band(HorizonY, GlowEnd, Dome.Glow[0], Dome.Glow[1], true);
				Packet.bSemiTransparent = true;
				Packets.push_back(Packet);
			}

			// The grid: turned to the camera's heading, tilted with its pitch, a little ahead of the eye, under its own projection distance.
			const int32 Length = SquareRoot0(Wrap(static_cast<int64>(AcrossX) * AcrossX + static_cast<int64>(AcrossZ) * AcrossZ));
			const int32 Pitch = GteRatan2(Target[1] - Eye[1], Length, Arctangents);
			const int32 Tilt = Divide(Divide((Pitch - 0x100) * Dome.TiltScale, 9) * (0x400 - std::abs(Pitch)), 10);
			const int16 Turn = static_cast<int16>(GteRatan2(AcrossX, AcrossZ, Arctangents));
			const FGteMatrix Unturned = RotationXyz({0, static_cast<int16>(-Turn), 0});
			FGteMatrix Grid = RotationYxz({static_cast<int16>(Tilt), Turn, 0});
			Grid.T = {Eye[0] + Heading[0] * 2, Divide(Eye[1], 2) - Dome.Lift, Eye[2] + Heading[2] * 2};
			FGte Projection = Gte;
			Projection.Transform = CompMatrix(Gte.Transform, CompMatrix(Grid, Unturned));
			Projection.ProjectionDistance = static_cast<uint16>(Dome.ProjectionDistance);

			Dome.Scroll[0] = static_cast<int16>(Dome.Scroll[0] + Dome.ScrollStep[0]);
			Dome.Scroll[1] = static_cast<int16>(Dome.Scroll[1] + Dome.ScrollStep[1]);
			const int32 Mask = static_cast<uint16>(Dome.Tile) - 1;
			const int32 FirstU = (Divide(Dome.Scroll[0], 4) + Grid.T[0] / 12) & Mask;
			const int32 FirstV = (Divide(Dome.Scroll[1], 4) + Grid.T[2] / 12) & Mask;
			const int32 Half = Dome.Tile / 2;
			for (int32 Row = 0; Row < GridCells; ++Row)
			{
				const uint8 Top = static_cast<uint8>((Row & 1) * Half + FirstV);
				const uint8 Bottom = static_cast<uint8>(Top + Half - 1);
				for (int32 Column = 0; Column < GridCells; ++Column)
				{
					const std::size_t At = static_cast<std::size_t>(Row * GridSide + Column);
					const FGteProjection A = Projection.Rtps(Dome.Points[At]);
					const FGteProjection B = Projection.Rtps(Dome.Points[At + 1]);
					const FGteProjection C = Projection.Rtps(Dome.Points[At + GridSide]);
					if (Nclip(A.ScreenX, A.ScreenY, B.ScreenX, B.ScreenY, C.ScreenX, C.ScreenY) < 0)
					{
						continue;
					}
					const FGteProjection D = Projection.Rtps(Dome.Points[At + GridSide + 1]);
					const uint8 Left = static_cast<uint8>((Column & 1) * Half + FirstU);
					const uint8 Right = static_cast<uint8>(Left + Half - 1);
					FBattleDomePacket& Packet = Packets.emplace_back();
					FFieldBackdropQuad& Quad = Packet.Quad;
					Quad.Kind = FFieldBackdropQuad::EKind::Textured;
					Quad.Corners = {{{A.ScreenX, A.ScreenY}, {B.ScreenX, B.ScreenY}, {C.ScreenX, C.ScreenY}, {D.ScreenX, D.ScreenY}}};
					Quad.Texels = {{{Left, Top}, {Right, Top}, {Left, Bottom}, {Right, Bottom}}};
					Quad.TexturePage = Dome.TexturePage;
					Quad.Clut = Dome.Clut;
				}
			}

			if (ClampedY >= 0)
			{
				Packets.push_back(Band(0, ClampedY, Dome.Colors[0], Dome.Colors[0], false));
			}
			Packets.push_back(DrawMode({Dome.WindowX, Dome.WindowY, Dome.Tile, Dome.Tile}));
		}
	}

	FBattleStageBackdrop MakeBattleStageBackdrop(const FBattleStageRecord& Record)
	{
		FBattleStageBackdrop Backdrop;
		int32 Panoramas = 0;
		for (int32 Index = 0; Index < FBattleStageRecord::ElementCount; ++Index)
		{
			const FBattleStageElement Element = Record.Element(Index);
			const int16 Kind = Element.Kind();
			if (Kind == BattleStageElementKinds::Panorama || Kind == BattleStageElementKinds::PanoramaOverClear)
			{
				if (Panoramas < PanoramaSlots)
				{
					Backdrop.Panoramas[static_cast<std::size_t>(Panoramas)] = MakePanorama(Element, Kind == BattleStageElementKinds::PanoramaOverClear, Record);
				}
				++Panoramas;
			}
			else if (Kind == BattleStageElementKinds::FloorDome && !Backdrop.Dome)
			{
				Backdrop.Dome = MakeDome(Element, Record);
			}
		}
		return Backdrop;
	}

	void DrawBattleStageBackdrop(FBattleStageBackdrop& Backdrop, const FGteVector& Eye, const FGteVector& Target, const FGte& Gte,
		const FGteArctangents& Arctangents, FBattleBackdropFrame& Frame)
	{
		for (std::size_t Slot = 0; Slot < Frame.Panoramas.size(); ++Slot)
		{
			Frame.Panoramas[Slot] = Backdrop.Panoramas[Slot] ? PlacePanorama(*Backdrop.Panoramas[Slot], Eye, Target, Gte, Arctangents) : FFieldBackdrop{};
		}
		Frame.Dome.clear();
		if (Backdrop.Dome)
		{
			DrawDome(*Backdrop.Dome, Eye, Target, Gte, Arctangents, Frame.Dome);
			std::reverse(Frame.Dome.begin(), Frame.Dome.end());
		}
	}
}
