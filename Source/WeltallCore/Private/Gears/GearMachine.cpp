#include "GearMachine.h"

#include "Foundation/LittleEndian.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteVectors.h"

#include <limits>
#include <string>

namespace Weltall::GearMachine
{
	namespace
	{
		/** The record numbers from FEh name the header's default record. */
		constexpr uint32 DefaultRecordNumbers = 0xFE;
		constexpr uint8 DefaultRecordFlag = 0x80;
		/** Records from 40h are in a second bank, which no model has. */
		constexpr uint32 FirstSecondBankRecord = 0x40;

		bool Readable(FGearEngine& Engine, const FGearCodeRef& At, uint32 Count)
		{
			if (!At || !Engine.HoldsPack(At.Pack))
			{
				Engine.Fail("a model reads a motion pack no longer loaded");
				return false;
			}
			if (!At.Pack->Has(At.Offset, Count))
			{
				Engine.Fail("a model reads past its motion pack, at " + std::to_string(At.Offset));
				return false;
			}
			return true;
		}
	}

	uint8 ReadU8(FGearEngine& Engine, const FGearCodeRef& At)
	{
		return Readable(Engine, At, 1) ? At.Pack->Bytes[At.Offset] : 0;
	}

	uint16 ReadU16(FGearEngine& Engine, const FGearCodeRef& At)
	{
		return Readable(Engine, At, 2) ? LittleEndian::ReadU16(At.Pack->Bytes, At.Offset) : 0;
	}

	int16 ReadS16(FGearEngine& Engine, const FGearCodeRef& At)
	{
		return static_cast<int16>(ReadU16(Engine, At));
	}

	uint32 ReadU32(FGearEngine& Engine, const FGearCodeRef& At)
	{
		return Readable(Engine, At, 4) ? LittleEndian::ReadU32(At.Pack->Bytes, At.Offset) : 0;
	}

	void WriteU16(FGearEngine& Engine, const FGearCodeRef& At, uint16 Value)
	{
		if (Readable(Engine, At, 2))
		{
			LittleEndian::WriteU16(At.Pack->Bytes, At.Offset, Value);
		}
	}

	std::optional<FGearRecord> ReadRecord(FGearEngine& Engine, const FGearCodeRef& At)
	{
		if (!Readable(Engine, At, FGearRecord::HeaderBytes))
		{
			return std::nullopt;
		}
		FGearRecord Record;
		Record.At = At;
		Record.Length = ReadS16(Engine, At + 2);
		Record.Flags = ReadU16(Engine, At + 4);
		Record.bPoseOnly = ReadS16(Engine, At + 6) != 0;
		Record.Rotations = ReadU16(Engine, At + 0xC);
		Record.Translations = ReadU16(Engine, At + 0xE);
		Record.Stride = ReadS16(Engine, At + 0x10);
		Record.Events = ReadS16(Engine, At + 0x12);
		Record.EventOffset = ReadU32(Engine, At + 0x14);
		return Record;
	}

	int32 Divide(FGearEngine& Engine, int32 Dividend, int32 Divisor)
	{
		if (Divisor == 0 || (Divisor == -1 && Dividend == std::numeric_limits<int32>::min()))
		{
			Engine.Fail("a model's arithmetic divides by zero, which stops the original");
			return 0;
		}
		return Dividend / Divisor;
	}

	int32 Distance(FGearEngine& Engine, int32 X, int32 Y, int32 Z)
	{
		const int64 Squares = int64{X} * X + int64{Y} * Y + int64{Z} * Z;
		if (Squares > std::numeric_limits<int32>::max())
		{
			Engine.Fail("a model's distance has squares past 31 bits, whose root the original takes of a wrapped sum");
			return 0;
		}
		return SquareRoot0(static_cast<int32>(Squares));
	}

	int32 Ratan2(FGearEngine& Engine, int32 Y, int32 X)
	{
		if (Engine.Arctangents == nullptr)
		{
			Engine.Fail("the models have no arctangent table");
			return 0;
		}
		return GteRatan2(Y, X, *Engine.Arctangents);
	}

	FGearBone* BoneAt(FGearEngine& Engine, FGearBody& Body, int32 Bone)
	{
		if (Bone < 0 || static_cast<std::size_t>(Bone) >= Body.Bones.size())
		{
			Engine.Fail("a model's script names bone " + std::to_string(Bone) + ", which its skeleton does not have");
			return nullptr;
		}
		return &Body.Bones[static_cast<std::size_t>(Bone)];
	}

	std::optional<FGearRecord> Record(FGearModels& Models, FGearModel& Model, uint32 Number, bool& bFlag)
	{
		bFlag = false;
		Number &= 0xFF;
		if (Number >= DefaultRecordNumbers)
		{
			bFlag = (Model.DefaultRecord & DefaultRecordFlag) != 0;
			Number = Model.DefaultRecord & ~DefaultRecordFlag & 0xFF;
		}
		const std::optional<uint32> At = Number < FirstSecondBankRecord ? Model.Motion->Record(static_cast<int32>(Number)) : std::nullopt;
		if (!At)
		{
			return std::nullopt;
		}
		return ReadRecord(Models, {Model.Motion.get(), *At});
	}

	std::optional<FGearRecord> UsedRecord(FGearModels& Models, FGearModel& Model, uint32 Number, bool& bFlag)
	{
		std::optional<FGearRecord> Found = Record(Models, Model, Number, bFlag);
		if (!Found && !Models.Faulted())
		{
			Models.Fail("a gear plays record " + std::to_string(Number & 0xFF) + ", which is past its bank (or in a second bank no model has)");
		}
		return Found;
	}

	uint32 FirstMaskSlot(const FGearModels& Models)
	{
		uint32 Slot = 0;
		while (Slot < 8 && ((Models.PlayMask >> Slot) & 1) == 0)
		{
			++Slot;
		}
		return Slot;
	}

	uint32 SelectModel(FGearModels& Models, const FGearModel& Model, uint32 Selector, uint16& Mask)
	{
		Selector &= 0xFF;
		uint32 Slot = Selector;
		switch (Selector)
		{
		case 0xFF:
			Slot = FirstMaskSlot(Models);
			break;
		case 0xFE:
			Slot = Models.PlaySlot & 0xFF;
			break;
		case 0xFD:
		case 0xF9:
			Slot = Model.Slot;
			break;
		case 0xFC:
			Models.Fail("a gear's script names its model's parent, which only a clone has");
			Slot = 0;
			break;
		case 0xFA:
			Slot = 10;
			break;
		case 0xF8:
			Slot = (Model.Slot * 2u + 8) & 0xFF;
			break;
		case 0xF7:
			Slot = (Model.Slot * 2u + 9) & 0xFF;
			break;
		default:
			break;
		}
		Mask = static_cast<uint16>(1u << (Slot & 0x1F));
		return Slot;
	}

	int32 TargetDistance(FGearEngine& Engine, const FGearBody& Body)
	{
		const FGearBone& Root = Body.Bones[0];
		return Distance(Engine, Body.Target[0] - Root.Position[0], Body.Target[1] - Root.Position[1], Body.Target[2] - Root.Position[2]);
	}

	int32 Radius(const FGearModels& Models, int32 Slot)
	{
		const FGearModel* Model = Models.Model(Slot);
		if (Model == nullptr)
		{
			return 0;
		}
		const bool bFromX = (Model->Flags & GearModelFlags::RadiusFromX) != 0;
		const int32 Size = bFromX ? Model->ShadowHalfWidth : Model->ShadowHalfDepth;
		const int32 BoneScale = bFromX ? Model->Bones[0].Scale[0] : Model->Bones[0].Scale[2];
		return Size * ((Model->Scale * BoneScale) >> 12) >> 12;
	}
}
