
#include "BattleObjectMachine.h"

#include "Graphics/ClutFade.h"

#include <algorithm>
#include <span>
#include <string>

namespace Weltall::BattleObjectMachine
{
	namespace
	{
		constexpr uint32 ResultBuffer = 0x100;
		constexpr uint32 SecondBuffer = 0x200;
		constexpr uint32 SourceBuffer = 0x400;
		constexpr uint32 FromVram = 1;
		constexpr uint32 FromColour = 2;
		constexpr int16 WholeSide = 0x100;
		constexpr int32 FullLevel = 32;

		/** A 15-bit colour of three channels as the start packs it (the third unmasked). */
		uint16 PackColour(uint16 Red, uint16 Green, uint16 Blue)
		{
			return static_cast<uint16>((Blue * 0x400 + (Green & 0x1F) * 0x20) | (Red & 0x1F));
		}

		/** The colour matrix's element at (Column, Row), counting past its rows as the original's pointer does. */
		int16* MatrixElement(FBattleObjects& Objects, int32 Column, int32 Row)
		{
			const int32 Index = Row * 3 + Column;
			if (Index < 0 || Index >= 9)
			{
				Objects.Fail("a CLUT animation reaches the colour matrix's element " + std::to_string(Index) + ", past its nine");
				return nullptr;
			}
			return &Objects.ColourMatrix.M[static_cast<std::size_t>(Index / 3)][static_cast<std::size_t>(Index % 3)];
		}

		/** A source of a VRAM animation: read from VRAM, or filled with a colour. */
		void FillVramSource(FBattleObjects& Objects, std::vector<uint16>& Buffer, uint32 Kind, const FVramRect& Rect, const std::array<uint16, 3>& Values)
		{
			if (Kind == FromVram)
			{
				Objects.Services->StoreImage({static_cast<int16>(Values[0]), static_cast<int16>(Values[1]), Rect.Width, Rect.Height}, Buffer);
			}
			else if (Kind == FromColour)
			{
				std::fill(Buffer.begin(), Buffer.end(), PackColour(Values[0], Values[1], Values[2]));
			}
		}

		/** A source of a colour matrix animation: a block of the matrix, or a channel per row by the row's place. */
		void FillMatrixSource(FBattleObjects& Objects, std::vector<uint16>& Buffer, uint32 Kind, const FVramRect& Rect, const std::array<uint16, 3>& Values)
		{
			for (int32 Row = 0; Row < Rect.Height && !Objects.Faulted(); ++Row)
			{
				for (int32 Column = 0; Column < Rect.Width; ++Column)
				{
					uint16& Into = Buffer[static_cast<std::size_t>(Row * Rect.Width + Column)];
					if (Kind == FromVram)
					{
						const int16* Element = MatrixElement(Objects, static_cast<int16>(Values[0]) + Column, static_cast<int16>(Values[1]) + Row);
						if (Element == nullptr)
						{
							return;
						}
						Into = static_cast<uint16>(*Element);
					}
					else if (Kind == FromColour)
					{
						const int32 Channel = (Rect.Y + Row) % 3;
						if (Channel < 0)
						{
							Objects.Fail("a CLUT animation fills a matrix row with a stale register's colour");
							return;
						}
						Into = Values[static_cast<std::size_t>(Channel)];
					}
				}
			}
		}

		int32 CurveLevel(FBattleObjects& Objects, const FBattleClutAnimation& Animation)
		{
			switch (Animation.Curve)
			{
			case EBattleClutCurve::Cosine:
				return static_cast<int16>(Animation.Base + GearMachine::Divide(Objects, GteCos(Animation.Phase) + 0x1000, Animation.Divisor));
			case EBattleClutCurve::Linear:
			{
				const int32 Level = static_cast<int16>(Animation.Base + GearMachine::Divide(Objects, Animation.Phase, Animation.Divisor));
				return Level <= FullLevel ? Level : -1;
			}
			case EBattleClutCurve::Down:
				return static_cast<int16>(Animation.Base - GearMachine::Divide(Objects, Animation.Phase, Animation.Divisor));
			case EBattleClutCurve::Up:
			default:
			{
				const int16 Level = static_cast<int16>(FullLevel - GearMachine::Divide(Objects, Animation.Phase, Animation.Divisor));
				return Level < Animation.Base ? Animation.Base : Level;
			}
			}
		}

		/** Types 4 and 5: the matrix block scaled by the level, or blended from the second source to the first. */
		void WriteMatrix(FBattleObjects& Objects, const FBattleClutAnimation& Animation, int32 Level)
		{
			std::size_t At = 0;
			for (int32 Row = 0; Row < Animation.Rect.Height && !Objects.Faulted(); ++Row)
			{
				for (int32 Column = 0; Column < Animation.Rect.Width; ++Column, ++At)
				{
					int16* Element = MatrixElement(Objects, Animation.Rect.X + Column, Animation.Rect.Y + Row);
					if (Element == nullptr)
					{
						return;
					}
					if (Animation.Type == 4)
					{
						const int32 Product = Animation.Source[At] * Level;
						*Element = static_cast<int16>((Product < 0 ? Product + 0x1F : Product) >> 5);
					}
					else
					{
						const int32 Product = (static_cast<int32>(Animation.Source[At]) - Animation.Second[At]) * Level;
						*Element = static_cast<int16>(Animation.Second[At] + ((Product < 0 ? Product + 0x1F : Product) >> 5));
					}
				}
			}
		}

		/** A buffer's colours as the heap holds them: in blocks of two words. */
		void AppendBlock(std::vector<uint16>& Memory, const std::vector<uint16>& Buffer)
		{
			Memory.insert(Memory.end(), Buffer.begin(), Buffer.end());
			Memory.resize((Memory.size() + 1) & ~std::size_t{1});
		}

		/**
		 * The memory after an animation's result, where the original's feed goes on reading: the buffers are taken
		 * from the heap one after another, so its second and first source follow, then those of the object's
		 * animations started after it, then what the heap held before, which on a console whose heap has not been
		 * used that far is zeros.
		 */
		std::vector<uint16> MemoryAfterResult(const FBattleObject& Object, const FBattleClutAnimation& From)
		{
			std::vector<uint16> Memory;
			bool bAfter = false;
			for (const FBattleClutAnimation& Animation : Object.ClutAnimations)
			{
				if (&Animation == &From)
				{
					bAfter = true;
				}
				else if (bAfter && Animation.bActive)
				{
					AppendBlock(Memory, Animation.Result);
				}
				if (bAfter && Animation.bActive)
				{
					AppendBlock(Memory, Animation.Second);
					AppendBlock(Memory, Animation.Source);
				}
			}
			return Memory;
		}

		/** The overlap of an animation's result with the animation it feeds, copied into that one's first source. */
		void Feed(FBattleObjects& Objects, const FBattleObject& Object, const FBattleClutAnimation& From, FBattleClutAnimation& To)
		{
			int16 IntoX = 0;
			int16 FromX = 0;
			int16 Width = 0;
			if (To.Rect.X < From.Rect.X)
			{
				IntoX = static_cast<int16>(From.Rect.X - To.Rect.X);
				Width = static_cast<int16>(To.Rect.X + To.Rect.Width - From.Rect.X);
			}
			else
			{
				FromX = static_cast<int16>(To.Rect.X - From.Rect.X);
				Width = static_cast<int16>(From.Rect.X + From.Rect.Width - To.Rect.X);
			}
			int16 IntoY = 0;
			int16 FromY = 0;
			int16 Height = 0;
			if (To.Rect.Y < From.Rect.Y)
			{
				IntoY = static_cast<int16>(From.Rect.Y - To.Rect.Y);
				Height = static_cast<int16>(To.Rect.Y + To.Rect.Height - From.Rect.Y);
			}
			else
			{
				FromY = static_cast<int16>(To.Rect.Y - From.Rect.Y);
				Height = static_cast<int16>(From.Rect.Y + From.Rect.Height - To.Rect.Y);
			}
			if (Width <= 0 || Height <= 0)
			{
				return;
			}
			To.bFed = true;
			if (!To.bHasSource || (To.Type < 4 ? !From.bHasResult : !From.bColourMatrix))
			{
				Objects.Fail("a CLUT animation feeds one from colours or into a source it does not have");
				return;
			}
			// The original measures the overlap from each rectangle's near edge only: a result narrower or shorter than
			// the one it feeds is read past its colours. The fix keeps to what the two share.
			const bool bSharedOnly = Objects.ShowOptions == nullptr || Objects.ShowOptions->bTintKeepsStagePalettes;
			if (bSharedOnly)
			{
				Width = std::min<int16>(Width, static_cast<int16>(From.Rect.Width - FromX));
				Height = std::min<int16>(Height, static_cast<int16>(From.Rect.Height - FromY));
				if (Width <= 0 || Height <= 0)
				{
					return;
				}
			}
			const std::size_t IntoLast = static_cast<std::size_t>((IntoY + Height - 1) * To.Rect.Width + IntoX + Width - 1);
			const std::size_t FromLast = static_cast<std::size_t>((FromY + Height - 1) * From.Rect.Width + FromX + Width - 1);
			if (IntoLast >= To.Source.size())
			{
				Objects.Fail("a CLUT animation feeds past the colours of the one it feeds");
				return;
			}
			std::vector<uint16> After;
			if (To.Type < 4 && FromLast >= From.Result.size())
			{
				After = MemoryAfterResult(Object, From);
			}
			for (int32 Row = 0; Row < Height && !Objects.Faulted(); ++Row)
			{
				for (int32 Column = 0; Column < Width; ++Column)
				{
					const std::size_t Into = static_cast<std::size_t>((IntoY + Row) * To.Rect.Width + IntoX + Column);
					if (To.Type < 4)
					{
						const std::size_t Read = static_cast<std::size_t>((FromY + Row) * From.Rect.Width + FromX + Column);
						const std::size_t Past = Read - std::min(Read, From.Result.size());
						To.Source[Into] = Read < From.Result.size() ? From.Result[Read] : Past < After.size() ? After[Past] : uint16{0};
					}
					else
					{
						const int16* Element = MatrixElement(Objects, FromX + Column, FromY + Row);
						if (Element == nullptr)
						{
							return;
						}
						To.Source[Into] = static_cast<uint16>(*Element);
					}
				}
			}
		}
	}

	void StartClutAnimation(FBattleObjects& Objects, FBattleClutAnimation& Animation, const FBattleClutStart& Start)
	{
		if (Animation.bActive)
		{
			return;
		}
		Animation.bActive = true;
		Animation.bFed = false;
		Animation.Type = Start.Type;
		Animation.Feeds = Start.Feeds;
		Animation.Phase = 0;
		Animation.Level = -1;
		Animation.Speed = Start.Speed;
		Animation.Divisor = Start.Divisor;
		Animation.Base = Start.Base;
		Animation.Curve = Start.Curve;
		Animation.bColourMatrix = Start.bColourMatrix;
		Animation.bHasResult = false;
		Animation.bHasSecond = false;
		Animation.bHasSource = false;
		uint32 Flags = Start.Flags;
		if ((Start.Type & 1) == 0)
		{
			Flags &= 0xFD0F;
		}
		if ((Start.Type & 4) != 0)
		{
			Flags &= 0xFEFF;
		}
		int16 Width = Start.Width == 0 ? WholeSide : Start.Width;
		const int16 Height = Start.Height == 0 ? WholeSide : Start.Height;
		const bool bVram = Start.Type < 2;
		if (!bVram && (Start.Type < 4 || Start.Type >= 6))
		{
			return;
		}
		if (bVram)
		{
			Width = static_cast<int16>((Width + 1) / 2 * 2);
		}
		Animation.Rect = {Start.X, Start.Y, Width, Height};
		Animation.Count = static_cast<int16>(Width * Height);
		const std::size_t Colours = static_cast<std::size_t>(std::max(0, Width * Height));
		Animation.bHasResult = (Flags & ResultBuffer) != 0;
		Animation.bHasSecond = (Flags & SecondBuffer) != 0;
		Animation.bHasSource = (Flags & SourceBuffer) != 0;
		Animation.Result.assign(Animation.bHasResult ? Colours : 0, 0);
		Animation.Second.assign(Animation.bHasSecond ? Colours : 0, 0);
		Animation.Source.assign(Animation.bHasSource ? Colours : 0, 0);
		const std::array<uint16, 3> SourceValues = {Start.Values[0], Start.Values[1], Start.Values[2]};
		const std::array<uint16, 3> SecondValues = {Start.Values[3], Start.Values[4], Start.Values[5]};
		const uint32 SourceKind = Flags & 0xF;
		const uint32 SecondKind = (Flags >> 4) & 0xF;
		if ((SourceKind == FromVram || SourceKind == FromColour) && !Animation.bHasSource)
		{
			Objects.Fail("a CLUT animation fills a first source it has no room for");
			return;
		}
		if ((SecondKind == FromVram || SecondKind == FromColour) && !Animation.bHasSecond)
		{
			Objects.Fail("a CLUT animation fills a second source it has no room for");
			return;
		}
		if (bVram)
		{
			FillVramSource(Objects, Animation.Source, SourceKind, Animation.Rect, SourceValues);
			FillVramSource(Objects, Animation.Second, SecondKind, Animation.Rect, SecondValues);
		}
		else
		{
			FillMatrixSource(Objects, Animation.Source, SourceKind, Animation.Rect, SourceValues);
			FillMatrixSource(Objects, Animation.Second, SecondKind, Animation.Rect, SecondValues);
		}
	}

	void StopClutAnimation(FBattleObjects& Objects, FBattleClutAnimation& Animation)
	{
		if (!Animation.bActive)
		{
			return;
		}
		if (Animation.bHasSource && Animation.Type < 4)
		{
			Objects.Services->LoadImage(Animation.Rect, Animation.Source);
		}
		Animation.bHasSource = false;
		Animation.bHasSecond = false;
		Animation.bHasResult = false;
		Animation.Source.clear();
		Animation.Second.clear();
		Animation.Result.clear();
		Animation.bActive = false;
	}

	void StepClutAnimation(FBattleObjects& Objects, FBattleObject& Object, FBattleClutAnimation& Animation, int32 CatchUp)
	{
		if (!Animation.bActive)
		{
			return;
		}
		Animation.Phase = static_cast<int16>(Animation.Phase + Animation.Speed * (CatchUp + 1));
		const int32 Level = CurveLevel(Objects, Animation);
		if (Level < 0)
		{
			StopClutAnimation(Objects, Animation);
			return;
		}
		if (Level == static_cast<uint16>(Animation.Level))
		{
			return;
		}
		Animation.Level = static_cast<int16>(Level);
		if (Animation.Type < 2)
		{
			if (!Animation.bHasResult || !Animation.bHasSource || (Animation.Type == 1 && !Animation.bHasSecond))
			{
				Objects.Fail("a CLUT animation steps without the colours it blends");
				return;
			}
			// The count is kept in 16 bits: a whole 256 by 256 block counts none.
			const std::size_t Colours = std::min<std::size_t>(static_cast<uint16>(Animation.Count), Animation.Result.size());
			const std::span<uint16> Result = std::span<uint16>(Animation.Result).first(Colours);
			if (Animation.Type == 1)
			{
				LerpClutColors(std::span<const uint16>(Animation.Second).first(Colours), Animation.Source, Level, Result);
			}
			else
			{
				FadeClutColors(std::span<const uint16>(Animation.Source).first(Colours), Level, Result);
			}
			if (Animation.Feeds == FBattleClutAnimation::FeedsNothing)
			{
				Objects.Services->LoadImage(Animation.Rect, Animation.Result);
			}
		}
		else if (Animation.Type == 4 || Animation.Type == 5)
		{
			WriteMatrix(Objects, Animation, Level);
		}
		FBattleClutAnimation* Fed = nullptr;
		if (Animation.Feeds >= 0)
		{
			Fed = &Object.ClutAnimations[static_cast<std::size_t>(Animation.Feeds)];
		}
		else if (Animation.Feeds == FBattleClutAnimation::FeedsStageTint)
		{
			Fed = &Objects.StageTintPalette;
		}
		if (Fed != nullptr && Fed->bActive)
		{
			Feed(Objects, Object, Animation, *Fed);
		}
	}
}
