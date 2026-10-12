#include "Battling/BattlingHud.h"

#include "Battling/BattlingFight.h"
#include "BattlingFightOps.h"

#include <algorithm>
#include <span>

namespace Weltall
{
	namespace
	{
		using FPoint = std::array<int16, 2>;

		/** Points of the HUD's gauge (FBattlingArenaTables::HudPoints) by what they are. */
		enum EHudPoint : std::size_t
		{
			LeftEnd,
			RightEnd,
			LowerRight,
			LowerMiddle,
			BendTop,
			LowerLeft,
			SecondPieceEnd,
			FirstPieceEnd,
		};

		/** Player 1's gauge is placed from its points' origin; player 2's turned about the screen. */
		constexpr FPoint PlayerOneOrigin{0x67, 6};
		constexpr FPoint PlayerTwoTurn{0xDA, 0x20};
		/** The gauge's pieces end at these lengths of its 7Fh. */
		constexpr int32 FirstPieceLength = 0x38;
		constexpr int32 SecondPieceLength = 0x48;
		/** The outline's left end reaches under the gear's name. */
		constexpr std::array<int16, 2> OutlineLeftX{0x1D, 0x121};
		/** The screen's width, which player 2's win marks are mirrored across. */
		constexpr int16 MirrorWidth = 0x140;

		constexpr uint8 StateReplay = 4;
		constexpr uint8 StateResult = 8;

		constexpr uint8 FlatTriangle = 0x20;
		constexpr uint8 RawTexturedQuad = MenuGpu::TexturedQuad | MenuGpu::RawTexture;
		constexpr uint8 OutlineLine = 0x4C;
		constexpr uint8 RawSprite = MenuGpu::Sprite | MenuGpu::RawTexture;
		/** The drawing-mode command's dithering bit, which the HUD asks SetDrawTPage for. */
		constexpr uint16 Dither = 0x200;

		constexpr FDrawColor Black{0, 0, 0};
		constexpr FDrawColor Red{0xFF, 0, 0};
		constexpr FDrawColor Yellow{0xFF, 0xFF, 0};
		constexpr FDrawColor Green{0, 0xFF, 0};
		constexpr FDrawColor Grey{0x60, 0x60, 0x80};

		/** The heat shown is the heat over 64: its top blinks from 57 up. */
		constexpr int32 HeatShift = 6;
		constexpr int32 HeatBlinksAbove = 0x38;
		constexpr int16 HeatBottom = 0x60;
		constexpr int32 HeatRows = 0x40;

		struct FSidePlaces
		{
			FPoint Tube;
			FPoint Heat;
			FPoint Icon;
			uint8 IconU;
			FPoint Name;
			std::array<FPoint, 4> NameBox;
			std::array<FPoint, 4> Lamp;
		};

		constexpr std::array<FSidePlaces, 2> Places{{
			{{6, 7}, {6, 0x20}, {7, 9}, 0, {0x1E, 0x0B}, {{{0x1E, 9}, {0x63, 9}, {0x1E, 0x14}, {0x5E, 0x14}}},
				{{{6, 0x32}, {0x0A, 0x36}, {6, 0x4C}, {0x0A, 0x48}}}},
			{{0x13A, 7}, {0x13A, 0x20}, {0x123, 9}, 0x20, {0xE3, 0x15}, {{{0x122, 0x13}, {0xE3, 0x13}, {0x122, 0x1E}, {0xDE, 0x1E}}},
				{{{0x13A, 0x32}, {0x136, 0x36}, {0x13A, 0x4C}, {0x136, 0x48}}}},
		}};

		/** Player 1's win marks, three triangles; player 2's are mirrored, the second's first corner a row higher and its last a column to the left. */
		constexpr std::array<std::array<FPoint, 3>, 3> WinMarks{{
			{{{0x14, 0x20}, {0x1C, 0x20}, {0x1C, 0x28}}},
			{{{0x13, 0x20}, {0x13, 0x29}, {0x1B, 0x29}}},
			{{{0x13, 0x32}, {0x13, 0x2A}, {0x1B, 0x2A}}},
		}};

		int16 GaugeX(int32 Side, int32 Length)
		{
			return static_cast<int16>(Side == 0 ? PlayerOneOrigin[0] + Length : PlayerTwoTurn[0] - Length);
		}

		int16 GaugeY(int32 Side, int32 Height)
		{
			return static_cast<int16>(Side == 0 ? PlayerOneOrigin[1] + Height : PlayerTwoTurn[1] - Height);
		}

		void SetCorner(FMenuPrimitive& Primitive, std::size_t Corner, int32 X, int32 Y, int32 U = 0, int32 V = 0)
		{
			FMenuCorner& At = Primitive.Corners[Corner];
			At.X = static_cast<int16>(X);
			At.Y = static_cast<int16>(Y);
			At.U = static_cast<uint8>(U);
			At.V = static_cast<uint8>(V);
		}

		FMenuPrimitive MakeFlat(uint8 Code, const FDrawColor& Colour, std::span<const FPoint> Points)
		{
			FMenuPrimitive Primitive;
			Primitive.Code = Code;
			Primitive.Corners[0].Color = Colour;
			for (std::size_t Corner = 0; Corner < Points.size(); ++Corner)
			{
				SetCorner(Primitive, Corner, Points[Corner][0], Points[Corner][1]);
			}
			return Primitive;
		}

		/** libgpu's SetDrawTPage alone: the page, and no texture window (a GPU NOP in its place). */
		FMenuPrimitive MakeTexturePage(uint16 Page)
		{
			FMenuPrimitive Primitive;
			Primitive.Code = MenuGpu::DrawModeCommand;
			Primitive.Words = {MenuGpu::DrawMode(Page) | Dither, 0};
			return Primitive;
		}

		/**
		 * A picture of 4-bit pixels placed at its own size, as the HUD's load makes one:
		 * from (X, Y) rightwards for player 1, or ending at X for player 2, whose texels it takes one to the left.
		 */
		FMenuPrimitive MakePictureQuad(int32 Side, const FPoint& At, const FVramRect& Pixels, uint16 Clut)
		{
			constexpr int32 PixelsPerUnit = 4;
			const int32 Width = Pixels.Width * PixelsPerUnit;
			const int32 Left = Side == 0 ? At[0] : At[0] - Width;
			const int32 U = Pixels.X * PixelsPerUnit - (Side == 0 ? 0 : 1);
			const int32 V = Pixels.Y;
			FMenuPrimitive Quad;
			Quad.Code = RawTexturedQuad;
			Quad.Corners[0].Color = Black;
			const int32 NearX = Side == 0 ? Left : Left + Width;
			const int32 FarX = Side == 0 ? Left + Width : Left;
			SetCorner(Quad, 0, NearX, At[1], U, V);
			SetCorner(Quad, 1, FarX, At[1], U + Width, V);
			SetCorner(Quad, 2, NearX, At[1] + Pixels.Height, U, V + Pixels.Height);
			SetCorner(Quad, 3, FarX, At[1] + Pixels.Height, U + Width, V + Pixels.Height);
			Quad.Clut = Clut;
			Quad.Page = MenuGpu::TexturePage(0, 0, Pixels.X, Pixels.Y);
			return Quad;
		}

		FMenuPrimitive MakeRawSprite(const FPoint& At, uint8 U, uint8 V, uint16 Clut, int16 Width, int16 Height)
		{
			FMenuPrimitive Sprite;
			Sprite.Code = RawSprite;
			Sprite.Corners[0].Color = Black;
			SetCorner(Sprite, 0, At[0], At[1], U, V);
			Sprite.Clut = Clut;
			Sprite.Width = Width;
			Sprite.Height = Height;
			return Sprite;
		}
	}

	FBattlingHud::FBattlingHud(const FBattlingArenaTables& InTables, const FBattlingArenaTextures& InTextures)
		: HeatV(static_cast<uint8>(InTextures.HudHeat.Y))
	{
		const auto& Points = InTables.HudPoints;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			FSide& Into = Sides[static_cast<std::size_t>(Side)];
			const FSidePlaces& At = Places[static_cast<std::size_t>(Side)];
			const auto Place = [&](EHudPoint Point) {
				return FPoint{GaugeX(Side, Points[Point][0]), GaugeY(Side, Points[Point][1])};
			};

			const std::array<std::array<EHudPoint, 4>, 3> PieceCorners{{
				{LeftEnd, FirstPieceEnd, LowerLeft, BendTop},
				{FirstPieceEnd, SecondPieceEnd, BendTop, LowerMiddle},
				{SecondPieceEnd, RightEnd, LowerMiddle, LowerRight},
			}};
			for (std::size_t Piece = 0; Piece < PieceCorners.size(); ++Piece)
			{
				std::array<FPoint, 4> Corners{};
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					Corners[Corner] = Place(PieceCorners[Piece][Corner]);
				}
				Into.Gauge[Piece] = MakeFlat(MenuGpu::FlatQuad, Red, Corners);
				Into.Grey[Piece] = MakeFlat(MenuGpu::FlatQuad, Grey, Corners);
			}

			FPoint OutlineStart = Place(LeftEnd);
			OutlineStart[0] = OutlineLeftX[static_cast<std::size_t>(Side)];
			const std::array<FPoint, 4> Upper{OutlineStart, Place(RightEnd), Place(LowerRight), Place(LowerMiddle)};
			const std::array<FPoint, 4> Lower{Place(LowerMiddle), Place(BendTop), Place(LowerLeft), Place(LeftEnd)};
			for (std::size_t Line = 0; Line < 2; ++Line)
			{
				FMenuPrimitive& Outline = Into.Outline[Line] = MakeFlat(OutlineLine, Black, Line == 0 ? Upper : Lower);
				Outline.LinePoints = 4;
			}

			Into.NameBox = MakeFlat(MenuGpu::FlatQuad, Black, At.NameBox);
			Into.Lamp = MakeFlat(MenuGpu::FlatQuad, Red, At.Lamp);

			Into.Tube = MakePictureQuad(Side, At.Tube, InTextures.HudTubes, InTextures.HudClut);
			Into.Heat = MakePictureQuad(Side, At.Heat, InTextures.HudHeat, InTextures.HudClut);
			Into.Heat.Code = MenuGpu::TexturedQuad;

			Into.Icon = MakeRawSprite(At.Icon, At.IconU, 0, MenuGpu::Clut(0, BattlingIconPaletteRow + Side), BattlingGears::IconSize, BattlingGears::IconSize);
			constexpr int16 NameWidth = 0x40;
			constexpr int16 NameRows = 8;
			const uint8 NameU = static_cast<uint8>((BattlingNamesPlace.X - BattlingIconsPlace.X) * 4);
			Into.Name = MakeRawSprite(At.Name, NameU, static_cast<uint8>(NameRows * Side),
				MenuGpu::Clut(BattlingNamePalettePlace.X, BattlingNamePalettePlace.Y), NameWidth, NameRows);

			for (std::size_t Mark = 0; Mark < WinMarks.size(); ++Mark)
			{
				std::array<FPoint, 3> Corners = WinMarks[Mark];
				if (Side == 1)
				{
					for (FPoint& Corner : Corners)
					{
						Corner[0] = static_cast<int16>(MirrorWidth - Corner[0]);
					}
					if (Mark == 1)
					{
						--Corners[0][1];
						--Corners[2][0];
					}
				}
				Into.WinMarks[Mark] = MakeFlat(FlatTriangle, Green, Corners);
			}
		}
	}

	bool FBattlingHud::IsDrawn(const FBattlingFight& Fight)
	{
		return Fight.LatchedState != StateReplay && Fight.LatchedState != StateResult
			&& Fight.Settings.Mode != EBattlingMode::Practice;
	}

	void FBattlingHud::AddGauge(int32 Side, int32 Length, const FDrawColor& Colour, bool bDamage, std::vector<FMenuPrimitive>& Out) const
	{
		FPieces Pieces = Sides[static_cast<std::size_t>(Side)].Gauge;
		int32 Count = 1;
		if (Length < FirstPieceLength)
		{
			Pieces[0].Corners[1].X = GaugeX(Side, Length);
			Pieces[0].Corners[3].X = GaugeX(Side, Length - 4);
		}
		else if (Length < SecondPieceLength)
		{
			// The second piece's lower end runs down the bend as the gauge grows.
			const int32 IntoBend = Length - FirstPieceLength;
			Pieces[1].Corners[1].X = GaugeX(Side, Length);
			Pieces[1].Corners[3].X = GaugeX(Side, Length - 4 - (IntoBend >> 2));
			Pieces[1].Corners[3].Y = GaugeY(Side, IntoBend / 2 + 8);
			Count = 2;
		}
		else
		{
			Pieces[2].Corners[1].X = GaugeX(Side, Length);
			Pieces[2].Corners[3].X = GaugeX(Side, Length - 8);
			Count = 3;
		}
		for (int32 Piece = Count - 1; Piece >= 0; --Piece)
		{
			FMenuPrimitive& Drawn = Out.emplace_back(Pieces[static_cast<std::size_t>(Piece)]);
			Drawn.Corners[0].Color = Colour;
			const uint32 Key = static_cast<uint32>(Side);
			Drawn.BlendKey = bDamage ? BattlingHudBlendKeys::Damage(Key, static_cast<uint32>(Piece)) : BattlingHudBlendKeys::Hp(Key, static_cast<uint32>(Piece));
		}
	}

	void FBattlingHud::AddHeat(int32 Side, const FBattlingFighter& Fighter, uint32 VBlank, std::vector<FMenuPrimitive>& Out) const
	{
		const int32 Shown = Fighter.Heat >> HeatShift;
		FMenuPrimitive& Heat = Out.emplace_back(Sides[static_cast<std::size_t>(Side)].Heat);
		Heat.Corners[0].Color = {static_cast<uint8>(Shown > HeatBlinksAbove ? VBlank << 3 : 0xFF), 0, 0};
		const int16 Top = static_cast<int16>(HeatBottom - Shown);
		const uint8 TopV = static_cast<uint8>(HeatV + HeatRows - static_cast<uint8>(Shown));
		for (std::size_t Corner = 0; Corner < 2; ++Corner)
		{
			Heat.Corners[Corner].Y = Top;
			Heat.Corners[Corner].V = TopV;
		}
	}

	void FBattlingHud::Draw(const FBattlingFight& Fight, std::vector<FMenuPrimitive>& Out) const
	{
		const auto& Fighters = Fight.Fighters;
		Out.push_back(MakeTexturePage(MenuGpu::TexturePage(0, 2, 0, 0)));
		for (int32 Side = 1; Side >= 0; --Side)
		{
			const FPieces& GreyPieces = Sides[static_cast<std::size_t>(Side)].Grey;
			Out.insert(Out.end(), GreyPieces.rbegin(), GreyPieces.rend());
		}
		for (int32 Side = 1; Side >= 0; --Side)
		{
			// The HP the last hits took, reddening as it fades.
			const FBattlingFighter& Fighter = Fighters[static_cast<std::size_t>(Side)];
			if (Fighter.GaugeFlash != 0)
			{
				const int32 Faded = 0x100 - Fighter.GaugeFlash;
				const uint8 Level = static_cast<uint8>(Faded * 3 >> 3);
				const FDrawColor Colour{static_cast<uint8>(Level + (Fighter.GaugeFlash * 0xFF >> 8)), Level, static_cast<uint8>(Faded >> 1)};
				AddGauge(Side, Fighter.PreviousHpGauge, Colour, true, Out);
			}
		}
		for (int32 Side = 1; Side >= 0; --Side)
		{
			AddGauge(Side, Fighters[static_cast<std::size_t>(Side)].HpGauge, Red, false, Out);
		}
		for (int32 Side = 1; Side >= 0; --Side)
		{
			const auto& Outline = Sides[static_cast<std::size_t>(Side)].Outline;
			Out.insert(Out.end(), Outline.begin(), Outline.end());
		}
		Out.push_back(Sides[1].NameBox);
		Out.push_back(Sides[0].NameBox);
		AddHeat(0, Fighters[0], Fight.FrameVBlank, Out);
		AddHeat(1, Fighters[1], Fight.FrameVBlank, Out);
		Out.push_back(MakeTexturePage(MenuGpu::TexturePage(0, 0, BattlingIconsPlace.X, BattlingIconsPlace.Y)));
		Out.push_back(Sides[1].Name);
		Out.push_back(Sides[0].Name);
		Out.push_back(MakeTexturePage(MenuGpu::TexturePage(1, 0, BattlingIconsPlace.X, BattlingIconsPlace.Y)));
		Out.push_back(Sides[1].Icon);
		Out.push_back(Sides[0].Icon);
		Out.push_back(Sides[1].Tube);
		Out.push_back(Sides[0].Tube);
		for (int32 Side = 1; Side >= 0; --Side)
		{
			// The lamp lights when Ether would overheat the gear: yellow while the heat may still run over, red when not even that.
			const FBattlingFighter& Fighter = Fighters[static_cast<std::size_t>(Side)];
			if (Fighter.Heat >= 0x1000 - Fighter.EtherHeat)
			{
				FMenuPrimitive& Lamp = Out.emplace_back(Sides[static_cast<std::size_t>(Side)].Lamp);
				Lamp.Corners[0].Color = BattlingFightOps::HeatCanAdd(Fighter, Fighter.EtherHeat) ? Yellow : Red;
			}
		}
		for (int32 Side = 1; Side >= 0; --Side)
		{
			const auto& Marks = Sides[static_cast<std::size_t>(Side)].WinMarks;
			for (int32 Mark = std::min<int32>(Fighters[static_cast<std::size_t>(Side)].RoundsWon, 3) - 1; Mark >= 0; --Mark)
			{
				Out.push_back(Marks[static_cast<std::size_t>(Mark)]);
			}
		}
	}
}
