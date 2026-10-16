#include "Field/FieldScreenOverlays.h"

#include "Field/FieldCamera.h"
#include "Formats/Lzss.h"
#include "Foundation/LittleEndian.h"
#include "Graphics/GteMath.h"
#include "Media/GameFileIndex.h"

#include <algorithm>
#include <optional>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		/** Where a release's Field program (directory 1, unpacked) keeps the pieces' table and their images' rectangles. */
		struct FHudLayoutPlace
		{
			EDiscRelease Release;
			int32 ProgramFile;
			std::size_t Pieces;
			std::size_t Images;
		};

		constexpr int32 ProgramDirectory = 1;
		constexpr std::array<FHudLayoutPlace, 2> Places{{
			{EDiscRelease::UsaDisc1, 14, 0x3F420, 0x3F078},
			{EDiscRelease::UsaDisc2, 14, 0x3F420, 0x3F078},
		}};
		constexpr std::size_t EntryBytes = 8;
		constexpr int32 AngleMask = 0xFFF;
		/** The texel mappings the pieces' low flag bits choose: as they are, mirrored, upside down, both. */
		constexpr uint16 LastMapping = 3;

		/**
		 * The pieces each tick scrolls, in ranges: from 0 the heading's scale, then the pitch's, the
		 * heading's five digits, the pitch's, the blinking light and the counter's scale.
		 */
		constexpr int32 FirstPitchScale = 16;
		constexpr int32 FirstHeadingDigit = 29;
		constexpr int32 FirstPitchDigit = 34;
		constexpr int32 BlinkingLight = 39;
		/** The piece shown only in the first half of every 32 ticks. */
		constexpr int32 FlashingPiece = 42;
		constexpr uint32 FlashingOff = 0x10;

		uint8 ClampTexel(int32 Value)
		{
			return static_cast<uint8>(std::clamp<int32>(static_cast<int16>(Value), 0, 0xFF));
		}

		/** The texels of a piece whose rectangle starts at (U, V) and that the frame scrolls by (Du, Dv): drawn upside down. */
		void ScrolledTexels(FFieldHudQuad& Quad, const FFieldCockpitHudLayout::FImage& Image, int32 Du, int32 Dv)
		{
			const int32 U = Image.U + Du;
			const int32 Bottom = Image.V + Dv + Image.Height - 1;
			const int32 Top = Image.V + Dv - 1;
			const int32 Right = U + Image.Width;
			Quad.Texels = {{{ClampTexel(U), ClampTexel(Bottom)}, {ClampTexel(Right), ClampTexel(Bottom)}, {ClampTexel(U), ClampTexel(Top)},
				{ClampTexel(Right), ClampTexel(Top)}}};
		}

		/** The texels a piece is made with, by the mapping its flags choose. */
		void MadeTexels(FFieldHudQuad& Quad, const FFieldCockpitHudLayout::FImage& Image, uint16 Mapping)
		{
			const bool bMirrored = (Mapping & 1) != 0;
			const bool bUpsideDown = (Mapping & 2) != 0;
			const int32 Left = bMirrored ? Image.U + Image.Width - 1 : Image.U;
			const int32 Right = bMirrored ? Image.U - 1 : Image.U + Image.Width;
			const int32 Top = bUpsideDown ? Image.V + Image.Height - 1 : Image.V;
			const int32 Bottom = bUpsideDown ? Image.V - 1 : Image.V + Image.Height;
			Quad.Texels = {{{ClampTexel(Left), ClampTexel(Top)}, {ClampTexel(Right), ClampTexel(Top)}, {ClampTexel(Left), ClampTexel(Bottom)},
				{ClampTexel(Right), ClampTexel(Bottom)}}};
		}

		/** How far the tick's frame scrolls piece Piece's texels, when it does. */
		std::optional<std::array<int32, 2>> Scroll(const FFieldCockpitHud& Hud, int32 Piece)
		{
			if (Piece < FirstPitchScale)
			{
				return std::array<int32, 2>{Hud.Heading >> 4 & 0xF, 0};
			}
			if (Piece < FirstHeadingDigit)
			{
				return std::array<int32, 2>{0, Hud.Pitch >> 4 & 0xF};
			}
			if (Piece < FirstPitchDigit)
			{
				return std::array<int32, 2>{(Hud.Heading >> (2 * (Piece - FirstHeadingDigit)) & 0xF) << 3, 0};
			}
			if (Piece < BlinkingLight)
			{
				return std::array<int32, 2>{(Hud.Pitch >> (2 * (Piece - FirstPitchDigit)) & 0xF) << 3, 0};
			}
			if (Piece == BlinkingLight)
			{
				return std::array<int32, 2>{0, Hud.Blink << 3};
			}
			if (Piece < FlashingPiece)
			{
				return std::array<int32, 2>{static_cast<int32>(Hud.DrawnCounter) >> 2 & 0xF, 0};
			}
			return std::nullopt;
		}
	}

	void FFieldCockpitHud::Show()
	{
		bOn = true;
		Blink = 0;
	}

	void FFieldCockpitHud::Tick(const FFieldCamera& Camera, const FGteArctangents& Arctangents)
	{
		if (!bOn)
		{
			return;
		}
		Heading = Camera.ViewYaw & AngleMask;
		const int32 Across = FieldLength2D((Camera.Target[0] - Camera.Eye[0]) >> 16, (Camera.Target[2] - Camera.Eye[2]) >> 16);
		Pitch = GteRatan2(Across, (Camera.Target[1] - Camera.Eye[1]) >> 16, Arctangents) & AngleMask;
		if ((Counter & 0xF) == 0)
		{
			++Blink;
		}
		if (Blink > 2)
		{
			Blink = 0;
		}
		DrawnCounter = Counter;
		++Counter;
	}

	TResult<FFieldCockpitHudLayout> FFieldCockpitHudLayout::Read(const FCdImage& Image, const FGameFileIndex& Index, EDiscRelease Release)
	{
		const auto Place = std::find_if(Places.begin(), Places.end(), [Release](const FHudLayoutPlace& Candidate) { return Candidate.Release == GameplayRelease(Release); });
		if (Place == Places.end())
		{
			return MakeError("no cockpit instruments' layout is known for this release");
		}
		const FIndexRecord* Record = Index.Find(ProgramDirectory, Place->ProgramFile);
		if (Record == nullptr)
		{
			return MakeError("the disc lacks the Field program");
		}
		const TResult<std::vector<uint8>> Sectors = ReadFileSectors(Image, *Record);
		if (!Sectors)
		{
			return Sectors.Error();
		}
		const TResult<std::vector<uint8>> Program = UnpackLzss(*Sectors);
		if (!Program)
		{
			return Program.Error();
		}
		return FromProgram(*Program, Release);
	}

	TResult<FFieldCockpitHudLayout> FFieldCockpitHudLayout::FromProgram(std::span<const uint8> Bytes, EDiscRelease Release)
	{
		const auto Place = std::find_if(Places.begin(), Places.end(), [Release](const FHudLayoutPlace& Candidate) { return Candidate.Release == GameplayRelease(Release); });
		if (Place == Places.end())
		{
			return MakeError("no cockpit instruments' layout is known for this release");
		}
		if (Bytes.size() < Place->Pieces + PieceCount * EntryBytes)
		{
			return MakeError("the Field program is too short to hold the cockpit instruments' layout");
		}
		FFieldCockpitHudLayout Layout;
		uint16 Images = 0;
		for (std::size_t Piece = 0; Piece < Layout.Pieces.size(); ++Piece)
		{
			const std::size_t At = Place->Pieces + Piece * EntryBytes;
			FPiece& Entry = Layout.Pieces[Piece];
			Entry = {ReadS16(Bytes, At), ReadS16(Bytes, At + 2), ReadU16(Bytes, At + 4), ReadU16(Bytes, At + 6)};
			// A piece made with another mapping keeps whatever texels its memory held.
			if ((Entry.Flags & 0xF) > LastMapping)
			{
				return MakeError("a cockpit instrument's piece has a texel mapping the field never sets");
			}
			Images = std::max<uint16>(Images, static_cast<uint16>(Entry.Image + 1));
		}
		if (Bytes.size() < Place->Images + Images * EntryBytes)
		{
			return MakeError("the Field program is too short to hold the cockpit instruments' images");
		}
		for (uint16 Rectangle = 0; Rectangle < Images; ++Rectangle)
		{
			const std::size_t At = Place->Images + Rectangle * EntryBytes;
			Layout.Images.push_back({ReadU16(Bytes, At), ReadU16(Bytes, At + 2), ReadU16(Bytes, At + 4), ReadU16(Bytes, At + 6)});
		}
		return Layout;
	}

	void BuildCockpitHudQuads(const FFieldCockpitHudLayout& Layout, const FFieldCockpitHud& Hud, std::span<FFieldHudQuad, FFieldCockpitHudLayout::PieceCount> Quads)
	{
		// A piece whose flags name neither blend keeps the one the piece before it was made with.
		uint8 BlendMode = 0;
		for (int32 Piece = 0; Piece < FFieldCockpitHudLayout::PieceCount; ++Piece)
		{
			const FFieldCockpitHudLayout::FPiece& Entry = Layout.Pieces[static_cast<std::size_t>(Piece)];
			const FFieldCockpitHudLayout::FImage& Image = Layout.Images[Entry.Image];
			const uint16 Blend = Entry.Flags >> 4 & 0xF;
			if (Blend < 2)
			{
				BlendMode = static_cast<uint8>(Blend + 1);
			}
			FFieldHudQuad& Quad = Quads[static_cast<std::size_t>(Piece)];
			Quad.BlendMode = BlendMode;
			Quad.bDrawn = Hud.bOn && (Piece != FlashingPiece || (Hud.DrawnCounter & FlashingOff) == 0);
			const int16 Right = static_cast<int16>(Entry.X + Image.Width);
			const int16 Bottom = static_cast<int16>(Entry.Y + Image.Height);
			Quad.Corners = {{{Entry.X, Entry.Y}, {Right, Entry.Y}, {Entry.X, Bottom}, {Right, Bottom}}};
			if (const std::optional<std::array<int32, 2>> By = Scroll(Hud, Piece))
			{
				ScrolledTexels(Quad, Image, (*By)[0], (*By)[1]);
			}
			else
			{
				MadeTexels(Quad, Image, Entry.Flags & 0xF);
			}
		}
	}
}
