#include "Battle/Show/BattleShowNumbers.h"

#include "Battle/BattleContext.h"
#include "Game/PsxRandom.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 PartRoom = 16;
		constexpr int32 DigitFrames = 0x72;
		constexpr int32 MissFrame = 0x7C;
		constexpr int32 FuelFrame = 0x7F;
		constexpr int32 EtherFrame = 0x80;
		constexpr int32 FuelGainFrame = 0x91;
		/** Where a number of so many characters starts, to centre it: its digits are 10 apart. */
		constexpr std::array<int16, 7> CentredStarts{0, -4, -8, -12, -16, -20, 0};
		constexpr int32 DigitSpacing = 10;
		constexpr int32 Unit = 0x2000;
		constexpr int32 SlideSteps = 8;
		constexpr int32 HoldSteps = 0x10;
		constexpr int32 FadeSteps = 0x10;
		constexpr int32 MissSteps = 0x40;
		constexpr int32 BurstSteps = 0x20;
		constexpr int32 BurstGrowth = 0x330;
		constexpr uint32 Powers[] = {10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
		/** The digits the original draws (its amounts are 16 bits), and an amount drawn whole, past 16 bits. */
		constexpr int32 Digits = 5;
		constexpr int32 WholeDigits = 6;
		constexpr uint32 Largest16 = 0xFFFF;

		/** An amount's text: as the original's five digits, or whole when it is past 16 bits. */
		std::array<uint8, 8> AmountText(uint32 Value)
		{
			return FormatBattleNumber(Value, Value > Largest16 ? WholeDigits : Digits);
		}

		/** Where an amount's text starts, centred: the original's table, and six characters as it spaces the others. */
		int32 CentredStart(uint8 Characters)
		{
			return Characters == WholeDigits ? -4 * WholeDigits : CentredStarts[Characters];
		}

		uint8 Darken(uint8 Value, int32 By)
		{
			return static_cast<uint8>(std::clamp(Value - By, 0, 255));
		}

		/** The colour of the total and its burst by the result's class: EP heals magenta, heals green, fuel gains blue. */
		std::array<uint8, 3> ClassColor(uint8 Class)
		{
			switch (Class)
			{
			case BattleResults::EpHeal:
				return {0x80, 0, 0x80};
			case BattleResults::HpHeal:
				return {0, 0x80, 0};
			case BattleResults::FuelGain:
				return {0, 0, 0x80};
			default:
				return {0x80, 0x80, 0x80};
			}
		}
	}

	std::array<uint8, 8> FormatBattleNumber(uint32 Value, int32 Width)
	{
		std::array<uint8, 8> Out{};
		std::size_t At = 1;
		uint8 Count = 0;
		if (static_cast<int32>(Value) < 0)
		{
			Out[At++] = '-';
			Value = 0u - Value;
			--Width;
			Count = 1;
		}
		bool bLeading = false;
		for (;;)
		{
			Value %= Powers[static_cast<std::size_t>(Width)];
			if (Width == 0)
			{
				break;
			}
			--Width;
			const uint32 Digit = Value / Powers[static_cast<std::size_t>(Width)];
			bLeading = bLeading || (Digit & 0xFF) != 0;
			if (bLeading)
			{
				Out[At++] = static_cast<uint8>(Digit);
				++Count;
			}
		}
		Out[At] = static_cast<uint8>(Value);
		Out[0] = static_cast<uint8>(Count + 1);
		return Out;
	}

	void FBattleNumbers::Reserve()
	{
		for (FBattleNumber& Number : Numbers)
		{
			Number.Parts.reserve(PartRoom);
		}
		for (FBattleTotalBurst& Burst : Bursts)
		{
			Burst.Parts.reserve(PartRoom);
		}
		Total.Parts.reserve(PartRoom);
	}

	void FBattleNumbers::AddDigits(uint32 Value, int32 Y, std::vector<FBattleSheetPart>& Parts)
	{
		const std::array<uint8, 8> Text = AmountText(Value);
		int32 X = CentredStart(Text[0]);
		for (uint8 Character = 0; Character < Text[0]; ++Character)
		{
			bFault = bFault || !Sheet->BuildParts(Text[1 + Character] + DigitFrames, X, Y, Parts);
			X += DigitSpacing;
		}
	}

	void FBattleNumbers::Pop(const FSprite& Sprite, uint32 Amount, uint8 Result)
	{
		if (!bPlaying)
		{
			return;
		}
		OpenTotal();
		if (GearPressed != nullptr)
		{
			*GearPressed = false;
		}
		for (FBattleNumber& Older : Numbers)
		{
			if (Older.bLive && Older.Over == &Sprite)
			{
				End(Older);
			}
		}
		const auto Free = std::find_if(Numbers.begin(), Numbers.end(), [](const FBattleNumber& Number) { return !Number.bLive; });
		if (Free == Numbers.end())
		{
			bFault = true;
			return;
		}
		FBattleNumber& Number = *Free;
		Number.bLive = true;
		Number.Serial = Made++ & 0xFFFFFF;
		Number.Numbers = this;
		Number.Over = &Sprite;
		Number.Phase = EBattleNumberPhase::Slide;
		Number.Count = SlideSteps;
		Number.Position = Sprite.Position;
		Number.Scale = {Unit, Unit, Unit};
		Number.Code = 0x2D;
		Number.Rotation = {};
		Number.Color = {0x80, 0x80, 0x80};
		Number.Parts.clear();
		Number.bRight = (Sprite.TimeFlags & SpriteTimeFlags::FacingMirrored) != 0;
		Sprites->LinkTask(Number);

		int32 X = 0;
		if (Result != BattleResults::Miss)
		{
			X = CentredStart(AmountText(Amount)[0]);
		}
		switch (Result)
		{
		case BattleResults::EpDamage:
			bFault = bFault || !Sheet->BuildParts(EtherFrame, X, -0x10, Number.Parts);
			X += 0x20;
			break;
		case BattleResults::HpHeal:
			Number.Color = {0, 0x80, 0};
			Number.Code &= 0xFE;
			break;
		case BattleResults::EpHeal:
			bFault = bFault || !Sheet->BuildParts(EtherFrame, X, -0x10, Number.Parts);
			Number.Color = {0x80, 0, 0x80};
			Number.Code &= 0xFE;
			X += 0x20;
			break;
		case BattleResults::Miss:
			Number.Phase = EBattleNumberPhase::MissFade;
			Number.Count = MissSteps;
			bFault = bFault || !Sheet->BuildParts(MissFrame, -0x14, -0x10, Number.Parts);
			Number.Code = static_cast<uint8>((Number.Code & 0xFE) | 2);
			return;
		case BattleResults::HalvedDamage:
			Number.Color = {0x80, 0, 0};
			Number.Code &= 0xFE;
			break;
		case BattleResults::FuelLoss:
			bFault = bFault || !Sheet->BuildParts(FuelFrame, X, 0, Number.Parts);
			break;
		case BattleResults::FuelGain:
			bFault = bFault || !Sheet->BuildParts(FuelGainFrame, X, 0, Number.Parts);
			Number.Color = {0, 0, 0x80};
			Number.Code &= 0xFE;
			break;
		default:
			break;
		}
		const std::array<uint8, 8> Text = AmountText(Amount);
		for (uint8 Character = 0; Character < Text[0]; ++Character)
		{
			bFault = bFault || !Sheet->BuildParts(Text[1 + Character] + DigitFrames, X, -0x10, Number.Parts);
			X += DigitSpacing;
		}
	}

	void FBattleNumbers::ShowSlot(FBattlePerformance& Record, int32 Slot, const FSprite* Over)
	{
		const std::size_t Index = static_cast<std::size_t>(Slot);
		if (Record.Results[Index] == BattleResults::NoResult || Record.Amounts[Index] == 0xFFFF || Over == nullptr)
		{
			return;
		}
		const bool bWhole = Record.bShownWhole;
		RunningTotal = bWhole ? Record.WholeTotals[Index] : Record.Totals[Index];
		TotalClass = bWhole ? Record.WholeTotalKinds[Index] : Record.TotalKinds[Index];
		Pop(*Over, bWhole ? Record.WholeAmounts[Index] : Record.Amounts[Index], Record.Results[Index]);
		Record.Amounts[Index] = 0xFFFF;
	}

	void FBattleNumbers::ShowAll(FBattlePerformance& Record, const std::array<const FSprite*, 11>& SlotSprites, const FSprite* Actor)
	{
		bool bCountered = false;
		for (int32 Slot = 0; Slot < FBattlePerformance::SlotCount; ++Slot)
		{
			const FSprite* Over = SlotSprites[static_cast<std::size_t>(Slot)];
			bCountered = bCountered || Record.Results[static_cast<std::size_t>(Slot)] == BattleResults::Counterattack;
			if (Over != nullptr && (Over != Actor || !bCountered))
			{
				ShowSlot(Record, Slot, Over);
			}
		}
	}

	void FBattleNumbers::OpenTotal()
	{
		if (Total.bOpen || bWithoutTotal)
		{
			return;
		}
		Total.bOpen = true;
		Sprites->LinkTask(Total);
		Total.Place = {0x90, 0x2A};
		Total.Parts.clear();
		LastTotal = 0xFFFFFFFF;
	}

	void FBattleNumbers::CloseTotal()
	{
		if (Total.bOpen)
		{
			Total.bOpen = false;
			Sprites->UnlinkTask(Total);
		}
	}

	int32 FBattleNumbers::Pending() const
	{
		return Sprites->CountedTasks - (Total.bOpen ? 1 : 0);
	}

	void FBattleNumbers::UpdateTotal()
	{
		if (!Total.bOpen || bWithoutTotal || RunningTotal == LastTotal)
		{
			return;
		}
		LastTotal = RunningTotal;
		Burst(RunningTotal);
		Total.Scale = {Unit, Unit, Unit};
		Total.Code = 0x2C;
		Total.Rotation = {};
		Total.Color = ClassColor(TotalClass);
		Total.Parts.clear();
		AddDigits(RunningTotal, -8, Total.Parts);
	}

	void FBattleNumbers::Burst(uint32 Value)
	{
		const auto Free = std::find_if(Bursts.begin(), Bursts.end(), [](const FBattleTotalBurst& Burst) { return !Burst.bLive; });
		if (Free == Bursts.end())
		{
			bFault = true;
			return;
		}
		FBattleTotalBurst& Burst = *Free;
		Burst.bLive = true;
		Burst.Serial = Made++ & 0xFFFFFF;
		Burst.Numbers = this;
		Sprites->LinkTask(Burst);
		Burst.Spin = ((Random->Next() & 3) - 2) * -8;
		if (Burst.Spin == 0)
		{
			Burst.Spin = 6;
		}
		Burst.Count = BurstSteps;
		Burst.Code = 0x2E;
		Burst.Scale = {Unit, Unit, Unit};
		Burst.Rotation = {};
		Burst.Color = ClassColor(TotalClass);
		Burst.Parts.clear();
		AddDigits(Value, -8, Burst.Parts);
		// The ghosts' parts are a texel smaller each way than the panel's.
		for (FBattleSheetPart& Part : Burst.Parts)
		{
			--Part.Width;
			--Part.Height;
		}
	}

	void FBattleNumbers::End(FBattleNumber& Number)
	{
		Number.bLive = false;
		Sprites->UnlinkTask(Number);
	}

	void FBattleNumbers::End(FBattleTotalBurst& Burst)
	{
		Burst.bLive = false;
		Sprites->UnlinkTask(Burst);
	}

	void FBattleNumber::Update()
	{
		Numbers->UpdateTotal();
		switch (Phase)
		{
		case EBattleNumberPhase::Slide:
			Position[0] += bRight ? 0xC0000 : -0xC0000;
			if (--Count < 0)
			{
				Count = HoldSteps;
				Phase = EBattleNumberPhase::Hold;
			}
			break;
		case EBattleNumberPhase::Hold:
			if (--Count < 0)
			{
				Count = FadeSteps;
				Code = static_cast<uint8>((Code & 0xFE) | 2);
				Phase = EBattleNumberPhase::Fade;
			}
			break;
		case EBattleNumberPhase::Fade:
			// Each channel fades from the red one's new value, so a coloured number turns grey as it goes.
			Color[0] = Darken(Color[0], 8);
			Color[1] = Darken(Color[0], 8);
			Color[2] = Darken(Color[0], 8);
			if (--Count < 0 || (Color[0] == 0 && Color[1] == 0 && Color[2] == 0))
			{
				Numbers->End(*this);
			}
			break;
		case EBattleNumberPhase::MissFade:
			for (uint8& Channel : Color)
			{
				Channel = static_cast<uint8>(Channel - 2);
			}
			if (--Count < 0)
			{
				Numbers->End(*this);
			}
			break;
		}
	}

	void FBattleTotalBurst::Update()
	{
		Rotation[2] = static_cast<int16>(Rotation[2] + Spin);
		for (int32& Axis : Scale)
		{
			Axis += BurstGrowth;
		}
		for (uint8& Channel : Color)
		{
			Channel = Darken(Channel, 4);
		}
		if (--Count == 0)
		{
			Numbers->End(*this);
		}
	}
}
