#include "Battle/Show/BattleExtensionDrawing.h"

#include "BattleShowDrawing.h"
#include "Graphics/GteVectors.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		using namespace BattleShowDrawing;

		constexpr uint32 ScreenSpace = 0x1000000;
		constexpr uint32 FarEntry = 0x2000000;
		constexpr uint32 ParentsEntry = 0x20000000;
		/** A quad is not linked when the GTE's flags of its corners hold this bit. */
		constexpr uint32 DropFlag = 0x8000;
		constexpr int32 StripHeight = 4;
		constexpr uint32 FlippedAcross = 0x10;
		constexpr uint32 FlippedDown = 0x20;
		/** EXT4 draws copies until the screen's right edge is passed; EXT5 draws this many. */
		constexpr int32 ScreenRight = 0x140;
		constexpr int32 ScrollCopies = 16;
		/** The copies' offset is 16 bits wide: a scroll that has not left the screen after this many copies never does. */
		constexpr int32 MostCopies = 0x10000;
		constexpr int16 BoundsStart = 0x400;

		/** The parts' bounds in texels: the least x and y, the greatest x + width and y + height. */
		struct FPartBounds
		{
			int16 Left = BoundsStart;
			int16 Top = BoundsStart;
			int16 Right = -BoundsStart;
			int16 Bottom = -BoundsStart;

			explicit FPartBounds(const FSprite& Sprite)
			{
				for (int32 Number = 0; Number < Sprite.PartCount(); ++Number)
				{
					const FSpritePart& Part = Sprite.Parts[static_cast<std::size_t>(Number)];
					Left = std::min(Left, Part.X);
					Top = std::min(Top, Part.Y);
					if (Right < Part.X + Part.Width)
					{
						Right = static_cast<int16>(Part.X + Part.Width);
					}
					if (Bottom < Part.Y + Part.Height)
					{
						Bottom = static_cast<int16>(Part.Y + Part.Height);
					}
				}
			}

			int32 Width() const { return Right - Left; }
		};

		/** A texel coordinate cut to 16 bits, then scaled by the sprite's size shift into 16 bits. */
		int16 Scaled(int32 Value, int32 Shift)
		{
			return static_cast<int16>(int32{static_cast<int16>(Value)} << Shift);
		}
	}

	bool FBattleExtensionDrawing::SetUp(FSprite& Sprite, int32& Entry, FGteMatrix& Matrix, bool& bFault)
	{
		bFault = false;
		Entry = SpriteEntry(*Gte, Sprite, *View);
		Sprite.OtIndex = static_cast<int16>(Entry);
		if ((Sprite.DrawFlags & ScreenSpace) == 0)
		{
			if ((Sprite.DrawFlags & ParentsEntry) != 0)
			{
				bFault = true;
				return false;
			}
			if (!InTable(Entry))
			{
				return false;
			}
			UpdateMatrix(Sprite);
			Matrix = SpriteDrawMatrix(Sprite, SpriteModelMatrix(Sprite), *View);
			return true;
		}
		// In screen space: its own matrix at its place, without the view, in its bias's entry or the last.
		UpdateMatrix(Sprite);
		Matrix = SpriteModelMatrix(Sprite);
		Entry = (Sprite.DrawFlags & FarEntry) != 0 ? static_cast<int32>(LastEntry) : Sprite.OtBias;
		return InTable(Entry);
	}

	bool FBattleExtensionDrawing::DrawWave(FSprite& Sprite, const FBattleWave& Wave)
	{
		int32 Entry = 0;
		FGteMatrix Matrix;
		bool bFault = false;
		if (!SetUp(Sprite, Entry, Matrix, bFault))
		{
			return !bFault;
		}
		if (Sprite.PartCapacity == 0)
		{
			return false;
		}
		const FSpritePart& Part = Sprite.Parts[0];
		const int32 Shift = Sprite.SizeShift();
		const auto Shifted = [](int32 Phase, int32 Amplitude) { return GteSin(Phase) * Amplitude / 0x1000; };
		const int32 Left = static_cast<uint16>(Part.X);
		const int32 Top = static_cast<uint16>(Part.Y);
		int32 Phase = Wave.Phase;
		int32 Amplitude = Wave.Amplitude << 8;
		int32 Frequency = Wave.Frequency << 8;
		int32 Below = Shifted(Phase, Wave.Amplitude);
		Gte->Transform = Matrix;
		for (int32 Row = Part.Y + Part.Height - StripHeight; Part.Y < Row; Row -= StripHeight)
		{
			const int32 Above = Shifted(Phase, Amplitude >> 8);
			const int32 Y = Top + Part.Height + Row;
			FSpriteQuad Quad;
			Quad.Matrix = Matrix;
			Quad.Corners[0] = {Scaled(Left + Above, Shift), Scaled(Y, Shift), 0};
			Quad.Corners[1] = {Scaled(Part.Width + Left + Above, Shift), Scaled(Y, Shift), 0};
			Quad.Corners[2] = {Scaled(Left + Below, Shift), Scaled(Y + StripHeight, Shift), 0};
			Quad.Corners[3] = {Scaled(Part.Width + Left + Below, Shift), Scaled(Y + StripHeight, Shift), 0};
			Quad.U0 = Part.U;
			Quad.U1 = static_cast<uint8>(Part.U + Part.Width);
			Quad.V0 = static_cast<uint8>(Part.V + Part.Height + Row);
			Quad.V1 = static_cast<uint8>(Part.V + Part.Height + Row + StripHeight);
			Quad.TexturePage = Part.TexturePage;
			Quad.Clut = Part.Clut;
			Quad.ColorCode = Part.ColorCode;
			Quad.OtIndex = Entry;
			uint32 Flag = 0;
			for (const FGteVector& Corner : Quad.Corners)
			{
				Flag |= Gte->Rtps(Corner).Flag;
			}
			if ((Flag & DropFlag) == 0)
			{
				Quads->push_back(Quad);
			}
			Phase += Frequency >> 8;
			Frequency += Wave.FrequencyStep;
			Amplitude += Wave.AmplitudeStep;
			Below = Above;
		}
		return true;
	}

	void FBattleExtensionDrawing::AddCopy(const FSprite& Sprite, const FGteMatrix& Matrix, int32 Dx, int32 Dy, int32 Entry)
	{
		const int32 Shift = Sprite.SizeShift();
		const bool bByDepth = Entry < 0;
		for (int32 Number = 0; Number < Sprite.PartCount(); ++Number)
		{
			const FSpritePart& Part = Sprite.Parts[static_cast<std::size_t>(Number)];
			const int16 Near = Part.X;
			const int16 Far = static_cast<int16>(Part.X + Part.Width);
			const int16 Upper = Part.Y;
			const int16 Lower = static_cast<int16>(Part.Y + Part.Height);
			const bool bAcross = (Part.Flags & FlippedAcross) != 0;
			const bool bDown = (Part.Flags & FlippedDown) != 0;
			int16 First = Scaled(static_cast<int16>((bAcross ? Far : Near) + Dx), Shift);
			const int16 Second = Scaled(static_cast<int16>((bAcross ? Near : Far) + Dx), Shift);
			const int16 Above = Scaled(static_cast<int16>((bDown ? Lower : Upper) + Dy), Shift);
			const int16 Beneath = Scaled(static_cast<int16>((bDown ? Upper : Lower) + Dy), Shift);
			if (bByDepth)
			{
				// A part wholly before the sprite's origin is left out, one across it cut there with its whole picture.
				if (Second < 0)
				{
					continue;
				}
				First = std::max<int16>(First, 0);
			}
			FSpriteQuad Quad;
			Quad.Matrix = Matrix;
			Quad.Corners[0] = {First, Above, 0};
			Quad.Corners[1] = {Second, Above, 0};
			Quad.Corners[2] = {First, Beneath, 0};
			Quad.Corners[3] = {Second, Beneath, 0};
			Quad.U0 = Part.U;
			Quad.U1 = static_cast<uint8>(Part.U + Part.Width - 1);
			Quad.V0 = Part.V;
			Quad.V1 = static_cast<uint8>(Part.V + Part.Height - 1);
			Quad.TexturePage = Part.TexturePage;
			Quad.Clut = Part.Clut;
			Quad.ColorCode = Part.ColorCode;
			Quad.OtIndex = Entry;
			if (bByDepth)
			{
				std::array<FGteProjection, 4> Points;
				uint32 Flag = 0;
				for (std::size_t Corner = 0; Corner < Points.size(); ++Corner)
				{
					Points[Corner] = Gte->Rtps(Quad.Corners[Corner]);
					Flag |= Points[Corner].Flag;
				}
				Quad.OtIndex = (Gte->AverageDepth4(Points[0].ScreenZ, Points[1].ScreenZ, Points[2].ScreenZ, Points[3].ScreenZ) >> EntryShift) + Sprite.OtBias;
				if ((Flag & DropFlag) != 0 || !InTable(Quad.OtIndex))
				{
					continue;
				}
			}
			Quads->push_back(Quad);
		}
	}

	bool FBattleExtensionDrawing::DrawScroll(FSprite& Sprite, const FBattleScroll& Scroll)
	{
		const int32 Start = static_cast<int16>(Scroll.Scroll[0] >> 16);
		if (Scroll.Module != 4)
		{
			// EXT5: the sprite back where it began, its own matrix there under the view.
			Sprite.Position = Scroll.Held;
			const FPartBounds Bounds(Sprite);
			UpdateMatrix(Sprite);
			if (Bounds.Width() == 0)
			{
				return false;
			}
			const FGteMatrix Matrix = CompMatrix(*View, SpriteModelMatrix(Sprite));
			Gte->Transform = Matrix;
			int32 Offset = Start % Bounds.Width() - Bounds.Width();
			for (int32 Copy = 0; Copy < ScrollCopies; ++Copy)
			{
				AddCopy(Sprite, Matrix, static_cast<int16>(Offset), 0, -1);
				Offset += Bounds.Width();
			}
			return true;
		}
		int32 Entry = 0;
		FGteMatrix Matrix;
		bool bFault = false;
		if (!SetUp(Sprite, Entry, Matrix, bFault))
		{
			return !bFault;
		}
		const FPartBounds Bounds(Sprite);
		const int32 Dy = static_cast<int16>(Scroll.Scroll[1] >> 16);
		Gte->Transform = Matrix;
		// Rightward while the copy's left edge is on the screen, then leftward from the same place while its right edge is.
		const auto CopiesToward = [this, &Bounds, Start](int32 Step, int16 Edge, bool bRightward)
		{
			int32 Offset = Start;
			for (int32 Copies = 0; Copies < MostCopies; ++Copies, Offset += Step)
			{
				const int16 X = Gte->Rtps({static_cast<int16>(Edge + static_cast<int16>(Offset)), Bounds.Top, 0}).ScreenX;
				if (bRightward ? X > ScreenRight : X < 0)
				{
					return Copies;
				}
			}
			return -1;
		};
		const int32 Rightward = CopiesToward(Bounds.Width(), Bounds.Left, true);
		const int32 Leftward = CopiesToward(-Bounds.Width(), Bounds.Right, false);
		if (Rightward < 0 || Leftward < 0)
		{
			return false;
		}
		for (int32 Copy = 0; Copy < Rightward; ++Copy)
		{
			AddCopy(Sprite, Matrix, static_cast<int16>(Start + Copy * Bounds.Width()), Dy, Entry);
		}
		for (int32 Copy = 0; Copy < Leftward; ++Copy)
		{
			AddCopy(Sprite, Matrix, static_cast<int16>(Start - Copy * Bounds.Width()), Dy, Entry);
		}
		return true;
	}
}
