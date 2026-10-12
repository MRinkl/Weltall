#include "Game/PsxRandom.h"
#include "Graphics/Gte.h"
#include "Graphics/GteMath.h"
#include "Graphics/GteMatrix.h"
#include "Graphics/GteVectors.h"
#include "Graphics/SpriteFile.h"
#include "Graphics/SpriteSystem.h"
#include "SpriteRules.h"

#include <algorithm>

namespace Weltall
{
	using namespace SpriteRules;

	namespace
	{
		/** Animation 63 treats a hold as the end. */
		constexpr int8 HoldEndsAnimation = '?';

		/** A script variable: a stack byte, or a byte of the file's own data. */
		struct FVariable
		{
			bool bValid = false;
			/** The stack byte's index from the stack's first, when it is no byte of a file's (File). */
			int32 StackIndex = 0;
			const FSpriteFile* File = nullptr;
			uint32 At = 0;
		};

		void CallListener(FSprite& Sprite)
		{
			if (Sprite.Listener != nullptr)
			{
				Sprite.Listener->OnAnimationEvent(Sprite);
			}
		}

		/** A speed from a script's byte: in the mode's ticks and the sprite's weight, 16.16. */
		int32 ScriptSpeed(const FSpriteSystem& System, const FSprite& Sprite, int32 Value)
		{
			return DivideByPower(int64{Value} * 0x10 * (System.TickExtraFrames + 1) * Sprite.Weight, 12);
		}

		void SetState(FSprite& Sprite, uint32 State)
		{
			Sprite.AnimationFlags = (Sprite.AnimationFlags & ~SpriteAnimationFlags::StateMask) | State << SpriteAnimationFlags::StateShift;
		}
	}

	class FSpriteOperations
	{
	public:
		FSpriteOperations(FSpriteSystem& InSystem, FSprite& InSprite) : System(InSystem), Sprite(InSprite) {}

		FVariable Resolve(uint8 Operand)
		{
			FVariable Variable;
			if ((Operand & 0x80) == 0)
			{
				Variable.StackIndex = Sprite.StackPointer + Operand;
				uint8 Unused = 0;
				Variable.bValid = System.ReadStackByte(Sprite, Variable.StackIndex, Unused);
			}
			else if (!Sprite.bHasVariables)
			{
				System.Report(ESpriteFault::NoVariables, Operand);
			}
			else
			{
				Variable.bValid = true;
				Variable.File = Sprite.ScriptFile;
				Variable.At = Sprite.Variables + (Operand & 0x7F);
			}
			return Variable;
		}

		/** A byte of a variable, or of what follows it, as an operation reads its operands there. */
		bool Get(const FVariable& Variable, int32 Offset, uint8& Value)
		{
			if (Variable.File != nullptr)
			{
				Value = Variable.File->Byte(Variable.At + static_cast<uint32>(Offset));
				return true;
			}
			Value = 0;
			return System.ReadStackByte(Sprite, Variable.StackIndex + Offset, Value);
		}

		uint8 Get(const FVariable& Variable)
		{
			uint8 Value = 0;
			Get(Variable, 0, Value);
			return Value;
		}

		/**
		 * Writes a variable: the stack's, or the file's own data, which every sprite of the file reads; the
		 * port writes a file only where a mode keeps it in memory to be written (Battle's).
		 */
		void Set(const FVariable& Variable, int32 Value, int32 Offset = 0)
		{
			if (Variable.File == nullptr)
			{
				System.WriteStackByte(Sprite, Variable.StackIndex + Offset, static_cast<uint8>(Value));
				return;
			}
			FSpriteFile* Writable = System.BattleMode != nullptr ? System.BattleMode->WritableFile(*Variable.File) : nullptr;
			if (Writable == nullptr)
			{
				System.Report(ESpriteFault::NoVariables, Variable.At);
				return;
			}
			Writable->SetByte(Variable.At + static_cast<uint32>(Offset), static_cast<uint8>(Value));
		}

		void Execute(uint8 Opcode, const FSpriteOperands& Args);

	private:
		void SetPositionFrom(uint8 Mode);
		void Rotate(const FSpriteOperands& Args, int32 Axis, bool bMirrors);
		void UploadImage(uint32 At);
		bool NeedsTransform() const { return Sprite.bHasTransform; }
		void MarkDirty() { Sprite.DrawFlags |= SpriteDrawFlags::MatrixDirty; }

		FSpriteSystem& System;
		FSprite& Sprite;
	};

	void FSpriteSystem::ExecuteKernelInstruction(FSprite& Sprite, uint8 Opcode, const FSpriteOperands& Args)
	{
		FSpriteOperations(*this, Sprite).Execute(Opcode, Args);
	}

	std::optional<FSpriteOperands> FSpriteSystem::VariableOperands(FSprite& Sprite, uint8 Operand)
	{
		FSpriteOperations Operations(*this, Sprite);
		const FVariable Variable = Operations.Resolve(Operand);
		if (!Variable.bValid)
		{
			return std::nullopt;
		}
		FSpriteOperands Args;
		Args.bInScript = false;
		for (int32 Byte = 0; Byte < 3; ++Byte)
		{
			if (!Operations.Get(Variable, Byte, Args.Bytes[static_cast<std::size_t>(Byte)]))
			{
				return std::nullopt;
			}
		}
		return Args;
	}

	void FSpriteSystem::Step(FSprite& Sprite)
	{
		if (BattleMode != nullptr)
		{
			BattleMode->Step(Sprite);
			return;
		}
		FSpriteOperations Operations(*this, Sprite);
		for (;;)
		{
			if (Sprite.Wait != 0 || FirstFault != ESpriteFault::None)
			{
				return;
			}
			const FSpriteFile& File = *Sprite.ScriptFile;
			const uint32 At = Sprite.Pc;
			if (At >= File.Size())
			{
				Report(ESpriteFault::PcOutsideFile, At);
				return;
			}
			const uint8 Opcode = File.Byte(At);
			const int32 TimeScale = static_cast<int32>(Sprite.TimeScale());
			if (Opcode < 0x80)
			{
				Sprite.Pc = At + 1;
				if (Opcode >= 0x40 && !StrayWait)
				{
					Report(ESpriteFault::UnsupportedInstruction, Opcode);
					return;
				}
				if (Opcode < 0x10)
				{
					SetFrame(Sprite, Sprite.Frame + 1);
				}
				else if (Opcode < 0x20)
				{
					Sprite.AnimationFlags = (Sprite.AnimationFlags & ~SpriteAnimationFlags::ListStepMask) | ((Sprite.ListStep() + 1) & 0x3F) << SpriteAnimationFlags::ListStepShift;
					ApplyDirectionFrame(Sprite);
				}
				else if (Opcode < 0x30)
				{
					SetFrame(Sprite, Sprite.Frame - 1);
				}
				const int32 Units = Opcode < 0x40 ? (Opcode & 0xF) + 1 : *StrayWait;
				int32 Wait = DivideByPower(int64{Units} * TimeScale, 8);
				Sprite.Wait = static_cast<uint16>(Sprite.Wait + (Wait == 0 ? 1 : Wait));
				CountStep(Sprite);
				return;
			}
			switch (Opcode)
			{
			case 0x81:
				if (Sprite.Animation() != HoldEndsAnimation)
				{
					Sprite.Wait = 0;
					CallListener(Sprite);
					SetState(Sprite, SpriteAnimationFlags::StateHeld);
					return;
				}
				[[fallthrough]];
			case 0x80:
				SetState(Sprite, 0);
				if (Sprite.Listener != nullptr)
				{
					Sprite.Listener->OnAnimationEvent(Sprite);
					return;
				}
				if (Sprite.NextAnimation() >= 0)
				{
					PlayAnimation(Sprite, Sprite.NextAnimation());
				}
				SetState(Sprite, 0);
				return;
			case 0x82:
			{
				CallListener(Sprite);
				const int32 VerticalSpeed = Sprite.Velocity[1];
				PlayAnimation(Sprite, Sprite.Animation());
				Sprite.Velocity[1] = VerticalSpeed;
				Sprite.Wait = 0;
				Step(Sprite);
				return;
			}
			case 0x85:
			{
				uint32 Address = 0;
				if (!PopAddress(Sprite, Address))
				{
					return;
				}
				Sprite.Pc = Address;
				continue;
			}
			case 0x86:
				if (Sprite.Velocity[1] < 0)
				{
					Sprite.Wait = 1;
					return;
				}
				break;
			case 0x87:
				if (static_cast<int16>(Sprite.Position[1] >> 16) < Sprite.FloorY)
				{
					Sprite.Wait = 1;
					return;
				}
				break;
			case 0x8E:
				Sprite.Pc = FSprite::Stopped;
				return;
			case 0x98:
				if (Sprite.Parent != nullptr)
				{
					Sprite.Wait = 1;
					if (Sprite.Parent->Animation() == Sprite.ParentAnimation && Sprite.Parent->State() == SpriteAnimationFlags::StateRunning)
					{
						return;
					}
				}
				break;
			case 0xA7:
			{
				Sprite.Pc = At + TableLength(Opcode);
				const uint8 Operand = File.Byte(At + 1);
				if ((Operand & 0x80) != 0)
				{
					HitStop = (Operand & 0x7F) + 1;
					Sprite.Wait = static_cast<uint16>(Sprite.Wait + 1);
					return;
				}
				const int32 Wait = static_cast<int32>(static_cast<uint32>((Operand + 2) * TimeScale) >> 8);
				Sprite.Wait = static_cast<uint16>(Sprite.Wait + (Wait == 0 ? 1 : Wait));
				return;
			}
			case 0xBE:
			{
				const int32 Word = static_cast<int16>(File.Word(At + 1));
				const int32 Scaled = Multiply(((Word >> 11) & 0xF) + 1, TimeScale);
				const int32 Wait = (Scaled >> 8) == 0 ? 1 : static_cast<int16>(static_cast<uint32>(Scaled) >> 8);
				int32 Frame = Word & 0x1FF;
				if (Sprite.Layout() == SpriteDrawFlags::LayoutParts)
				{
					if (Word < 0 && Frame != 0)
					{
						if (Sprite.ByteTableFile == nullptr)
						{
							Report(ESpriteFault::NoVariables, At);
							return;
						}
						Frame = Sprite.ByteTableFile->Byte(Sprite.ByteTable + static_cast<uint32>(Frame) - 1);
					}
					const bool bFlipped = (Word & 0x200) != 0;
					Sprite.TimeFlags = bFlipped ? Sprite.TimeFlags | SpriteTimeFlags::ListEntryFlipped : Sprite.TimeFlags & ~SpriteTimeFlags::ListEntryFlipped;
					const bool bMirrored = bFlipped != Sprite.IsMirroredFacing();
					Sprite.DrawFlags = (Sprite.DrawFlags & ~(SpriteDrawFlags::Mirrored | SpriteDrawFlags::FlippedVertically))
						| (bMirrored ? SpriteDrawFlags::Mirrored : 0) | ((Word & 0x400) != 0 ? SpriteDrawFlags::FlippedVertically : 0);
					SetFrame(Sprite, Frame);
				}
				else
				{
					Sprite.Frame = static_cast<uint16>(Frame);
				}
				Sprite.Wait = static_cast<uint16>(Sprite.Wait + Wait);
				Sprite.Pc += 3;
				return;
			}
			case 0xC8:
			{
				const FVariable Variable = Operations.Resolve(File.Byte(At + 2));
				FSpriteOperands Args;
				Args.bInScript = false;
				for (int32 Byte = 0; Byte < 3 && Variable.bValid; ++Byte)
				{
					if (!Operations.Get(Variable, Byte, Args.Bytes[static_cast<std::size_t>(Byte)]))
					{
						return;
					}
				}
				if (!Variable.bValid)
				{
					return;
				}
				Operations.Execute(File.Byte(At + 1), Args);
				break;
			}
			case 0xD4:
				Sprite.Pc = At + static_cast<uint32>(static_cast<int16>(File.Word(At + 1)));
				CallListener(Sprite);
				continue;
			case 0xE1:
				Sprite.Pc = At + static_cast<uint32>(static_cast<int16>(File.Word(At + 1)));
				continue;
			case 0xE2:
				if (!PushAddress(Sprite, At + 3))
				{
					return;
				}
				Sprite.Pc = At + static_cast<uint32>(static_cast<int16>(File.Word(At + 1)));
				continue;
			case 0xE4:
			{
				uint8 Count = 0;
				if (!PopByte(Sprite, Count))
				{
					return;
				}
				if (Count != 0)
				{
					if (!PushByte(Sprite, static_cast<uint8>(Count - 1)))
					{
						return;
					}
					Sprite.Pc = At + static_cast<uint32>(static_cast<int16>(File.Word(At + 1)));
					continue;
				}
				break;
			}
			case 0xFA:
			{
				const FVariable Variable = Operations.Resolve(File.Byte(At + 1));
				if (!Variable.bValid)
				{
					return;
				}
				if (Operations.Get(Variable) != 0)
				{
					Sprite.Pc = At + static_cast<uint32>(static_cast<int16>(File.Word(At + 2)));
					continue;
				}
				break;
			}
			default:
			{
				FSpriteOperands Args;
				Args.At = At + 1;
				for (int32 Byte = 0; Byte < 3; ++Byte)
				{
					Args.Bytes[static_cast<std::size_t>(Byte)] = File.Byte(At + 1 + static_cast<uint32>(Byte));
				}
				Operations.Execute(Opcode, Args);
				break;
			}
			}
			Sprite.Pc += TableLength(Opcode);
		}
	}

	void FSpriteOperations::Execute(uint8 Opcode, const FSpriteOperands& Args)
	{
		const int32 TimeScale = static_cast<int32>(Sprite.TimeScale());
		const bool bExtra = (Sprite.AnimationFlags & SpriteAnimationFlags::HasExtra) != 0;
		const bool bMirrored = Sprite.IsMirroredFacing();
		switch (Opcode)
		{
		case 0x8A:
			Sprite.Velocity[0] = 0;
			Sprite.Velocity[2] = 0;
			Sprite.Speed = 0;
			return;
		case 0x8C:
		{
			if (Sprite.Target == nullptr || System.Arctangents == nullptr)
			{
				System.Report(ESpriteFault::UnsupportedInstruction, Opcode);
				return;
			}
			const int32 DeltaZ = static_cast<int16>(Sprite.Target->Position[2] >> 16) - static_cast<int16>(Sprite.Position[2] >> 16);
			const int32 DeltaX = static_cast<int16>(Sprite.Target->Position[0] >> 16) - static_cast<int16>(Sprite.Position[0] >> 16);
			const int32 Heading = static_cast<int16>(-GteRatan2(DeltaZ, DeltaX, *System.Arctangents) & 0xFFF);
			System.SetDirection(Sprite, Heading);
			System.SetFacing(Sprite, Heading);
			return;
		}
		case 0x8D:
			System.ModelTextureX = Sprite.Resource->TextureX;
			System.ModelTextureY = Sprite.Resource->TextureY;
			System.bModelTextureOverride = true;
			return;
		case 0x90:
			if (Sprite.CurrentFile == Sprite.PrimaryFile)
			{
				System.SetResource(Sprite, Sprite.AlternateFile);
				Sprite.NextWord |= SpriteNextFlags::AlternateFile;
			}
			else
			{
				System.SetResource(Sprite, Sprite.PrimaryFile);
				Sprite.NextWord &= ~SpriteNextFlags::AlternateFile;
			}
			return;
		case 0x91:
			Sprite.Code &= 0xFE;
			System.ApplyColorBlend(Sprite);
			return;
		case 0x92:
			Sprite.Code |= 1;
			System.ApplyColorBlend(Sprite);
			return;
		case 0x93:
			System.CopyParentAnchors(Sprite);
			return;
		case 0x94:
			if (Sprite.Layout() != SpriteDrawFlags::LayoutTransform)
			{
				return;
			}
			if (Sprite.Parent == nullptr)
			{
				System.Report(ESpriteFault::UnsupportedInstruction, Opcode);
				return;
			}
			Sprite.Rotation[1] = Sprite.Parent->Heading;
			MarkDirty();
			return;
		case 0x96:
			if (System.ReachesTask(Sprite, Opcode))
			{
				System.KillChildren(Sprite);
			}
			return;
		case 0xA0:
			Sprite.Speed = Multiply(ScriptSpeed(System, Sprite, Args.S8(0)), 0x100);
			System.UpdateVelocity(Sprite);
			return;
		case 0xA1:
		{
			int32 Speed = bExtra ? Sprite.Extra.PresetVerticalSpeed : 0;
			if (Speed == 0)
			{
				Speed = Multiply(ScriptSpeed(System, Sprite, Args.S8(0)), 0x100);
			}
			if (TimeScale == 0)
			{
				System.Report(ESpriteFault::UnsupportedInstruction, Opcode);
				return;
			}
			Sprite.Velocity[1] = Multiply(Speed, 0x100) / TimeScale;
			return;
		}
		case 0xA2:
			Sprite.DrawFlags = (Sprite.DrawFlags & 0xFFFF00FF) | static_cast<uint32>(Args.U8(0)) << 8;
			return;
		case 0xA3:
		{
			if (bExtra && Sprite.Extra.PresetGravity != 0)
			{
				Sprite.Gravity = Sprite.Extra.PresetGravity;
				return;
			}
			if (TimeScale == 0)
			{
				System.Report(ESpriteFault::UnsupportedInstruction, Opcode);
				return;
			}
			const uint32 PerTick = 0x10000u / static_cast<uint32>(TimeScale);
			int32 Gravity = Multiply(DivideByPower(int64{Args.S8(0)} * 0x40 * Sprite.Weight, 12), 0x20);
			Gravity = DivideByPower(int64{Gravity} * DivideByPower(int64{PerTick} * PerTick, 8), 8);
			const int32 Ticks = System.TickExtraFrames + 1;
			Sprite.Gravity = Multiply(int64{Gravity} * Ticks, Ticks);
			return;
		}
		case 0xA4:
			if (Sprite.Target == nullptr)
			{
				System.Report(ESpriteFault::UnsupportedInstruction, Opcode);
				return;
			}
			System.SetAnimation(*Sprite.Target, Args.S8(0));
			return;
		case 0xA5:
			Sprite.Speed = Wrap(int64{Sprite.Speed} + Multiply(ScriptSpeed(System, Sprite, Args.S8(0)), 0x100));
			System.UpdateVelocity(Sprite);
			return;
		case 0xA6:
			if (!bExtra)
			{
				if (TimeScale == 0)
				{
					System.Report(ESpriteFault::UnsupportedInstruction, Opcode);
					return;
				}
				Sprite.Velocity[1] = Wrap(int64{Sprite.Velocity[1]} + Multiply(ScriptSpeed(System, Sprite, Args.S8(0)), 0x10000) / TimeScale);
			}
			return;
		case 0xA7:
			if ((Args.U8(0) & 0x80) == 0)
			{
				const int32 Scaled = (Args.U8(0) + 1) * TimeScale;
				const int32 Wait = (Scaled >> 8) == 0 ? 1 : static_cast<int16>(static_cast<uint32>(Scaled) >> 8);
				Sprite.Wait = static_cast<uint16>(Sprite.Wait + Wait);
			}
			else
			{
				System.HitStop = (Args.U8(0) & 0x7F) + 1;
			}
			return;
		case 0xA8:
			Sprite.Heading = static_cast<int16>(Sprite.Heading + Args.S8(0) * 16);
			System.UpdateVelocity(Sprite);
			return;
		case 0xA9:
		case 0xAA:
		case 0xAB:
		{
			const int32 Distance = ScaleBySize(Sprite, DivideByPower(int64{Args.S8(0)} * Sprite.Scale, 12));
			const int32 Axis = Opcode - 0xA9;
			const int32 Moved = Multiply(Distance, Axis == 0 && bMirrored ? -0x10000 : 0x10000);
			Sprite.Position[static_cast<std::size_t>(Axis)] = Wrap(int64{Sprite.Position[static_cast<std::size_t>(Axis)]} + Moved);
			return;
		}
		case 0xAC:
		{
			const int32 Random = System.NextRandom();
			const int32 Turn = ((static_cast<int32>(Random & 0xFF) * Args.U8(0)) >> 8) - (Args.U8(0) >> 1);
			System.SetDirection(Sprite, static_cast<int16>(Sprite.Heading + Turn * 16));
			return;
		}
		case 0xAD:
			Sprite.AnimationFlags = (Sprite.AnimationFlags & 0xFFFFF801) | static_cast<uint32>(Args.U8(0)) << 1;
			return;
		case 0xAE:
		case 0xAF:
		{
			if (!NeedsTransform())
			{
				return;
			}
			const int32 Angle = Args.S8(0) * (bMirrored ? -16 : 16);
			Sprite.Rotation[2] = static_cast<int16>(Opcode == 0xAE ? Sprite.Rotation[2] + Angle : Angle);
			MarkDirty();
			return;
		}
		case 0xB3:
			Sprite.AnimationFlags = (Sprite.AnimationFlags & ~SpriteAnimationFlags::ListStepMask) | (static_cast<uint32>(Args.S8(0)) & 0x3F) << SpriteAnimationFlags::ListStepShift;
			return;
		case 0xB4:
			System.PushByte(Sprite, Args.U8(0));
			return;
		case 0xB5:
			if (Sprite.Layout() != 0)
			{
				System.SetScale(Sprite, Args.S8(0) * 0x100);
			}
			return;
		case 0xB6:
		case 0xB7:
			if (!NeedsTransform())
			{
				return;
			}
			Sprite.Rotation[static_cast<std::size_t>(Opcode - 0xB6)] = static_cast<int16>(Sprite.Rotation[static_cast<std::size_t>(Opcode - 0xB6)] + Args.S8(0) * 16);
			MarkDirty();
			return;
		case 0xB8:
			Sprite.StackPointer = static_cast<int8>(Sprite.StackPointer - Args.U8(0));
			return;
		case 0xBA:
			System.SetBlend(Sprite, Args.U8(0));
			return;
		case 0xBB:
			Sprite.OtBias = static_cast<int16>(Sprite.OtBias + Args.S8(0));
			return;
		case 0xBC:
			SetPositionFrom(Args.U8(0));
			return;
		case 0xBF:
			Sprite.FrameTop = Args.U8(0);
			return;
		case 0xC0:
		{
			const int32 Distance = DivideByPower(int64{((System.NextRandom() & 0xFF) * Args.U8(0)) >> 8} * Sprite.Scale, 12);
			const int32 Angle = System.NextRandom();
			Sprite.Position[0] = Wrap(int64{Sprite.Position[0]} + Multiply(int64{ScaleBySize(Sprite, GteCos(Angle))} * Distance, 0x10));
			Sprite.Position[2] = Wrap(int64{Sprite.Position[2]} + Multiply(int64{ScaleBySize(Sprite, GteSin(Angle))} * Distance, -0x10));
			return;
		}
		case 0xC5:
		{
			int32 Steps = Args.U8(0) / (System.TickExtraFrames + 1);
			while (Steps-- > 0)
			{
				System.Move(Sprite);
			}
			return;
		}
		case 0xC6:
			if (bExtra)
			{
				Sprite.Extra.Signal = Args.U8(0);
			}
			return;
		case 0xC9:
			if (bExtra)
			{
				Sprite.Extra.Signal = static_cast<uint16>(Args.U8(0) | Args.U8(1) << 8);
			}
			return;
		case 0xCC:
			Sprite.bHasVariables = true;
			Sprite.Variables = Sprite.Pc + static_cast<uint32>(Args.S16());
			return;
		case 0xCD:
			Rotate(Args, 0, false);
			return;
		case 0xCE:
			Rotate(Args, 1, true);
			return;
		case 0xCF:
			Rotate(Args, 2, true);
			return;
		case 0xD0:
		case 0xD3:
		case 0xDD:
		case 0xDE:
		case 0xD1:
		case 0xD2:
		case 0xD5:
		{
			const FVariable Left = Resolve(Args.U8(0));
			const FVariable Right = Resolve(Args.U8(1));
			if (!Left.bValid || !Right.bValid)
			{
				return;
			}
			const int32 A = Get(Left);
			const int32 B = Get(Right);
			if (Opcode == 0xD1)
			{
				Set(Left, A * B);
			}
			else if (Opcode == 0xD2 || Opcode == 0xD5)
			{
				if (B == 0)
				{
					System.Report(ESpriteFault::UnsupportedInstruction, Opcode);
					return;
				}
				Set(Left, A / B);
			}
			else
			{
				Set(Left, A + B);
			}
			return;
		}
		case 0xD6:
		case 0xD7:
		case 0xD8:
		case 0xD9:
		case 0xDA:
		case 0xDB:
		case 0xDC:
		case 0xDF:
		case 0xE5:
		case 0xE6:
		{
			const FVariable Variable = Resolve(Args.U8(0));
			if (!Variable.bValid)
			{
				return;
			}
			const int32 Value = Get(Variable);
			const int32 Shift = Args.S8(1) & 0x1F;
			switch (Opcode)
			{
			case 0xD6:
				Set(Variable, Value + Args.U8(1));
				return;
			case 0xD7:
				Set(Variable, Value * Args.U8(1));
				return;
			case 0xD8:
				if (Args.S8(1) == 0)
				{
					System.Report(ESpriteFault::UnsupportedInstruction, Opcode);
					return;
				}
				Set(Variable, Value / Args.S8(1));
				return;
			case 0xD9:
				Set(Variable, Value << Shift);
				return;
			case 0xDA:
				Set(Variable, static_cast<int8>(Value) >> Shift);
				return;
			case 0xE5:
				Set(Variable, ((System.NextRandom() & 0xFF) * Args.U8(1)) >> 8);
				return;
			case 0xDF:
				Set(Variable, Args.U8(1));
				return;
			default:
			{
				// The 16-bit ones reach the byte after the variable, which is another variable or the file's data.
				uint8 High = 0;
				if (!Get(Variable, 1, High))
				{
					return;
				}
				const uint32 Word = static_cast<uint32>(Value | High << 8);
				const uint32 Result = Opcode == 0xDB ? Word << Shift : Opcode == 0xDC ? Word >> Shift : Args.U8(1);
				Set(Variable, static_cast<int32>(Result & 0xFF));
				Set(Variable, static_cast<int32>(Result >> 8 & 0xFF), 1);
				return;
			}
			}
		}
		case 0xE0:
			if (!Args.bInScript)
			{
				System.Report(ESpriteFault::UnsupportedInstruction, Opcode);
				return;
			}
			System.SpawnChild(Sprite, *Sprite.ScriptFile, Args.At + static_cast<uint32>(Args.S16()), Sprite.Resource);
			return;
		case 0xE7:
			System.SetScale(Sprite, Sprite.Scale + Args.S16() * 2);
			return;
		case 0xE9:
		case 0xEA:
		case 0xEB:
			if (!NeedsTransform())
			{
				return;
			}
			Sprite.TransformScale[static_cast<std::size_t>(Opcode - 0xE9)] = static_cast<int16>(Sprite.TransformScale[static_cast<std::size_t>(Opcode - 0xE9)] + Args.S16() * 2);
			MarkDirty();
			return;
		case 0xED:
			Sprite.Position[0] = Multiply(Args.S16(), 0x10000);
			return;
		case 0xEE:
			Sprite.Position[1] = Multiply(int64{Sprite.FloorY} + DivideByPower(int64{Args.S16()} * Sprite.Scale, 12), 0x10000);
			return;
		case 0xEF:
			Sprite.Position[2] = Multiply(Args.S16(), 0x10000);
			return;
		case 0xF1:
			Sprite.Red = Args.U8(0);
			Sprite.Green = Args.U8(1);
			Sprite.Blue = Args.U8(2);
			if (Sprite.Layout() == SpriteDrawFlags::LayoutTransform)
			{
				Sprite.TransformColor = {static_cast<int16>(Args.U8(0)), static_cast<int16>(Args.U8(1)), static_cast<int16>(Args.U8(2))};
			}
			if (Sprite.Layout() == SpriteDrawFlags::LayoutParts)
			{
				System.ApplyColorBlend(Sprite);
			}
			return;
		case 0xF2:
		{
			const auto AddClamped = [](uint8 Channel, int32 Change) { return static_cast<uint8>(std::clamp(Channel + Change, 0, 0xFF)); };
			Sprite.Red = AddClamped(Sprite.Red, Args.S8(0));
			Sprite.Green = AddClamped(Sprite.Green, Args.S8(1));
			Sprite.Blue = AddClamped(Sprite.Blue, Args.S8(2));
			if (Sprite.Layout() == SpriteDrawFlags::LayoutTransform)
			{
				for (std::size_t Channel = 0; Channel < 3; ++Channel)
				{
					Sprite.TransformColor[Channel] = static_cast<int16>(Sprite.TransformColor[Channel] + Args.S8(static_cast<int32>(Channel)));
				}
			}
			if (Sprite.Layout() == SpriteDrawFlags::LayoutParts)
			{
				System.ApplyColorBlend(Sprite);
			}
			// A model sprite's packets take the colour, when it holds a model and was not told to keep its own.
			if (Sprite.Kind() == 15 && Sprite.ModelFile != nullptr && (Sprite.BuildFlags & 2) == 0)
			{
				Sprite.ModelPackets.Tint = Sprite.TransformColor;
				if (System.BattleMode != nullptr)
				{
					System.BattleMode->ModelTinted(Sprite);
				}
			}
			return;
		}
		case 0xF5:
			if (!Args.bInScript || !NeedsTransform())
			{
				System.Report(ESpriteFault::UnsupportedInstruction, Opcode);
				return;
			}
			// The part's packets replace the part records, and the part replaces the anchor slots.
			Sprite.ModelFile = Sprite.ScriptFile;
			Sprite.Model = Args.At + static_cast<uint32>(Args.S24());
			Sprite.bModelTextureOverride = System.bModelTextureOverride;
			Sprite.ModelTextureX = System.ModelTextureX;
			Sprite.ModelTextureY = System.ModelTextureY;
			// The build's texture override lasts for that one build.
			System.bModelTextureOverride = false;
			Sprite.bHasAnchors = false;
			Sprite.PartCapacity = 0;
			return;
		case 0xFC:
			if (!Args.bInScript)
			{
				System.Report(ESpriteFault::UnsupportedInstruction, Opcode);
				return;
			}
			UploadImage(Args.At + static_cast<uint32>(Args.S24()));
			return;
		case 0xB0:
		case 0xB9:
			if (System.BattleMode == nullptr)
			{
				// The sounds of other modes' sets: no field sprite plays them.
				System.Report(ESpriteFault::UnsupportedInstruction, Opcode);
				return;
			}
			System.BattleMode->PlaySound(Sprite, Args.U8(0), Opcode == 0xB0);
			return;
		case 0xBD:
		{
			if (System.BattleMode == nullptr)
			{
				// The battle's common effects.
				System.Report(ESpriteFault::UnsupportedInstruction, Opcode);
				return;
			}
			const FSpriteResource& Common = System.BattleMode->KindResource(5);
			if (Common.File == nullptr)
			{
				System.Report(ESpriteFault::UnsupportedInstruction, Opcode);
				return;
			}
			System.SpawnChild(Sprite, *Common.File, Common.File->AnimationHeader(Common.Tables, Args.U8(0)), Sprite.Resource);
			return;
		}
		case 0xC1:
		{
			// A random distance along a random direction in the x-y plane turned by a random yaw.
			const int32 Distance = DivideByPower(int64{((System.NextRandom() & 0xFF) * Args.U8(0)) >> 8} * Sprite.Scale, 12);
			const FGteVector Offset{static_cast<int16>(ScaleBySize(Sprite, Distance)), 0, 0};
			const int16 AngleX = static_cast<int16>(System.NextRandom());
			const int16 AngleY = static_cast<int16>(System.NextRandom());
			FGteMatrix Turn = RotationXyz({AngleX, AngleY, 0});
			Turn.T = {static_cast<int16>(Sprite.Position[0] >> 16), static_cast<int16>(Sprite.Position[1] >> 16), static_cast<int16>(Sprite.Position[2] >> 16)};
			const FGteVector Moved = RotTransSV(Turn, Offset);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Sprite.Position[Axis] = Multiply(Moved[Axis], 0x10000);
			}
			return;
		}
		case 0xC4:
		{
			// The velocity turned about z by a random angle within the operand.
			const int32 Random = System.NextRandom();
			const int16 Angle = static_cast<int16>((((static_cast<int32>(Random & 0xFF) * Args.U8(0)) >> 8) - (Args.U8(0) >> 1)) * 16);
			const FGteLongVector Turned = ApplyMatrixLV(RotationXyz({0, 0, Angle}), Sprite.Velocity);
			Sprite.Velocity = Turned;
			return;
		}
		case 0xF6:
		case 0xF7:
			// Models of other modes: no sprite of the field's or the battle's attaches them.
			System.Report(ESpriteFault::UnsupportedInstruction, Opcode);
			return;
		default:
			return;
		}
	}

	void FSpriteOperations::Rotate(const FSpriteOperands& Args, int32 Axis, bool bMirrors)
	{
		if (!NeedsTransform())
		{
			return;
		}
		const int32 Word = Args.Bytes[0] | Args.Bytes[1] << 8;
		const int32 Angle = static_cast<int16>((Word & 0x1FF) * (bMirrors && Sprite.IsMirroredFacing() ? -8 : 8));
		const int32 Group = Word >> 9 & 7;
		const bool bSet = (Word & 0x1000) != 0;
		if (Group != 0)
		{
			if (!Sprite.bHasAnchors)
			{
				if (Sprite.ModelFile != nullptr)
				{
					System.Report(ESpriteFault::AnchorsHoldAModel, static_cast<uint32>(Group));
				}
				return;
			}
			FSpriteAnchor& Anchor = Sprite.Anchors[static_cast<std::size_t>(Group)];
			int16& Rotation = Axis == 0 ? Anchor.RotationX : Axis == 1 ? Anchor.RotationY : Anchor.RotationZ;
			Rotation = static_cast<int16>(bSet ? Angle : Rotation + Angle);
			return;
		}
		int16& Rotation = Sprite.Rotation[static_cast<std::size_t>(Axis)];
		Rotation = static_cast<int16>(bSet ? Angle : Rotation + Angle);
		MarkDirty();
	}

	void FSpriteOperations::SetPositionFrom(uint8 Mode)
	{
		const auto HighHalves = [](const FSprite& Of) {
			return std::array<int16, 3>{static_cast<int16>(Of.Position[0] >> 16), static_cast<int16>(Of.Position[1] >> 16), static_cast<int16>(Of.Position[2] >> 16)};
		};
		// An anchor's offset from a sprite's origin, in view units at the sprite's scale.
		const auto AnchorOffset = [this](const FSprite& Of, uint32 Slot, bool bMirrored, int32& DeltaX, int32& DeltaY) {
			DeltaX = 0;
			DeltaY = 0;
			if (Of.bHasAnchors)
			{
				if (Slot >= FSprite::AnchorCount)
				{
					System.Report(ESpriteFault::UnsupportedInstruction, 0xBC);
					return false;
				}
				DeltaX = static_cast<int8>(Of.Anchors[Slot].OffsetX);
				DeltaY = static_cast<int8>(Of.Anchors[Slot].OffsetY);
			}
			else if (Of.ModelFile != nullptr)
			{
				System.Report(ESpriteFault::AnchorsHoldAModel, Slot);
				return false;
			}
			DeltaX = DivideByPower(int64{bMirrored ? -DeltaX : DeltaX} * Of.Scale, 12);
			DeltaY = DivideByPower(int64{DeltaY} * Of.Scale, 12);
			return true;
		};

		if ((Mode & 0x80) == 0)
		{
			const FSprite* Parent = Sprite.Parent;
			if (Parent == nullptr || !Parent->bHasTransform || Parent->Layout() != SpriteDrawFlags::LayoutParts)
			{
				return;
			}
			int32 DeltaX = 0;
			int32 DeltaY = 0;
			if (!AnchorOffset(*Parent, Mode, (Parent->DrawFlags & SpriteDrawFlags::Mirrored) != 0, DeltaX, DeltaY))
			{
				return;
			}
			Sprite.Position[2] = Parent->Position[2];
			Sprite.Position[0] = Wrap(int64{Parent->Position[0]} + Multiply(DeltaX, 0x10000));
			Sprite.Position[1] = Wrap(int64{Parent->Position[1]} + Multiply(DeltaY, 0x10000));
			return;
		}

		const uint32 Index = Mode & 0x3F;
		ISpriteBattleMode* Battle = System.BattleMode;
		std::array<int16, 3> Point{};
		bool bScreenSpace = (Sprite.DrawFlags & 0x1000000) != 0;
		// A sprite's point raised by its frame's heights: its top, the middle of its top and its half
		// height, its half height, or the upper half of that.
		const auto Raised = [&HighHalves](const FSprite& Of, uint32 Which) {
			std::array<int16, 3> Lifted = HighHalves(Of);
			const uint16 Top = static_cast<uint16>(Of.FrameTop);
			const uint16 Half = static_cast<uint16>(Of.FrameHalfHeight);
			const int32 Rise = Which == 0 ? static_cast<int16>(Top)
				: Which == 1 ? Top - static_cast<int16>((static_cast<int32>(Top) - Half) / 2)
				: Which == 2 ? static_cast<int16>(Half)
				: Half - (Half >> 1);
			Lifted[1] = static_cast<int16>(Lifted[1] - Rise);
			return Lifted;
		};
		// A sprite's anchor slot as a point. For a sprite not drawn from parts the kernel leaves the point as its
		// stack held it; the port takes the sprite's place, as the kernel does for a sprite without anchors.
		const auto AnchorPoint = [this, &HighHalves, &AnchorOffset](const FSprite* Of, uint32 Slot, std::array<int16, 3>& Result) {
			if (Of == nullptr)
			{
				System.Report(ESpriteFault::UnsupportedInstruction, 0xBC);
				return false;
			}
			int32 DeltaX = 0;
			int32 DeltaY = 0;
			if (Of->bHasTransform && Of->Layout() == SpriteDrawFlags::LayoutParts && !AnchorOffset(*Of, Slot, Of->IsMirroredFacing(), DeltaX, DeltaY))
			{
				return false;
			}
			Result = HighHalves(*Of);
			Result[0] = static_cast<int16>(DeltaX + Result[0]);
			Result[1] = static_cast<int16>(DeltaY + Result[1]);
			return true;
		};
		// A sprite's place through its pointer; through one nothing set, what the console's lowest memory holds
		// (and its frame's heights there are zero).
		const auto Place = [this](const FSprite* Of) -> const std::array<int32, 3>& { return Of != nullptr ? Of->Position : System.UnsetSpritePlace; };
		const auto PlaceHalves = [&Place](const FSprite* Of) {
			const std::array<int32, 3>& At = Place(Of);
			return std::array<int16, 3>{static_cast<int16>(At[0] >> 16), static_cast<int16>(At[1] >> 16), static_cast<int16>(At[2] >> 16)};
		};
		FSprite* Actor = Battle != nullptr ? Battle->Actor() : nullptr;
		switch (Index)
		{
		case 0x00:
			Point = PlaceHalves(Sprite.Target);
			break;
		case 0x01:
			Point = PlaceHalves(Actor);
			break;
		case 0x03:
		{
			// Halfway between the actor and the sprite, each sum halved toward zero before its whole part is taken.
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				const int32 Sum = Wrap(int64{Place(Actor)[Axis]} + Sprite.Position[Axis]);
				Point[Axis] = static_cast<int16>(Wrap(int64{Sum} - (Sum >> 31)) >> 17);
			}
			break;
		}
		case 0x02:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x26:
		{
			const FSpriteBattlePoint Found = Battle != nullptr ? Battle->Point(Index, Sprite) : FSpriteBattlePoint{};
			if (!Found.bValid)
			{
				System.Report(ESpriteFault::UnsupportedInstruction, 0xBC);
				return;
			}
			Point = Found.Point;
			break;
		}
		case 0x08:
		case 0x09:
		case 0x0A:
		{
			constexpr uint32 TargetSlots[3] = {2, 1, 3};
			if (!AnchorPoint(Sprite.Target, TargetSlots[Index - 0x08], Point))
			{
				return;
			}
			break;
		}
		case 0x0B:
		case 0x0C:
		case 0x0D:
		case 0x0E:
		case 0x0F:
		case 0x10:
		case 0x11:
			if (!AnchorPoint(Sprite.Parent, Index - 0x0A, Point))
			{
				return;
			}
			break;
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
			Point = Sprite.Target != nullptr ? Raised(*Sprite.Target, Index - 0x12) : PlaceHalves(nullptr);
			break;
		case 0x16:
			Point = PlaceHalves(Sprite.Parent);
			bScreenSpace = false;
			break;
		case 0x17:
		{
			const FSpriteBattlePoint Found = Battle != nullptr ? Battle->Point(Index, Sprite) : FSpriteBattlePoint{};
			if (!Found.bValid)
			{
				System.Report(ESpriteFault::UnsupportedInstruction, 0xBC);
				return;
			}
			Point = Found.Point;
			bScreenSpace = false;
			break;
		}
		case 0x19:
		case 0x1A:
		case 0x1B:
		case 0x1C:
		case 0x1D:
		case 0x1E:
		case 0x1F:
			if (!AnchorPoint(Actor, Index - 0x18, Point))
			{
				return;
			}
			break;
		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
			Point = Actor != nullptr ? Raised(*Actor, Index - 0x20) : PlaceHalves(nullptr);
			break;
		case 0x24:
		case 0x25:
		{
			if (!System.ReachesTask(Sprite, 0xBC))
			{
				return;
			}
			// A sprite without a task is its own owner: the kernel then marks the word where a task keeps its marks, its z velocity.
			uint32& Marks = Sprite.Task != nullptr ? Sprite.Task->Marks : reinterpret_cast<uint32&>(Sprite.Velocity[2]);
			Marks = Index == 0x24 ? Marks | 0x40000000 : Marks & ~0x40000000u;
			[[fallthrough]];
		}
		case 0x18:
			Point = HighHalves(Sprite);
			bScreenSpace = false;
			break;
		default:
			// The kernel leaves the point as its stack held it.
			System.Report(ESpriteFault::UnsupportedInstruction, 0xBC);
			return;
		}
		if (bScreenSpace)
		{
			if (Battle == nullptr)
			{
				// Through the view matrix, which the port's field does not hand its sprites.
				System.Report(ESpriteFault::UnsupportedInstruction, 0xBC);
				return;
			}
			const FGteMatrix& View = Battle->View();
			const FGteVector Turned = ApplyMatrixSV(View, Point);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Point[Axis] = static_cast<int16>(Turned[Axis] + static_cast<int16>(View.T[Axis]));
			}
		}
		if ((Mode & 0x40) != 0)
		{
			Sprite.SavedPoint = Point;
			return;
		}
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Sprite.Position[Axis] = Multiply(Point[Axis], 0x10000);
		}
	}

	void FSpriteOperations::UploadImage(uint32 At)
	{
		constexpr uint32 ImageBlock = 0x1100;
		constexpr uint32 PaletteBlock = 0x1101;
		const FSpriteFile& File = *Sprite.ScriptFile;
		const auto ReadU32 = [&File](uint32 Offset) { return File.Word(Offset) | static_cast<uint32>(File.Word(Offset + 2)) << 16; };
		const int32 Count = static_cast<int32>(ReadU32(At));
		uint32 Block = At + 4 + 4 * static_cast<uint32>(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const uint32 Kind = ReadU32(Block);
			FVramRect Rect;
			if (Kind == ImageBlock)
			{
				Rect.X = static_cast<int16>(Sprite.Resource->TextureX + static_cast<int16>(File.Word(Block + 8)));
				Rect.Y = static_cast<int16>(Sprite.Resource->TextureY + static_cast<int16>(File.Word(Block + 10)));
			}
			else if (Kind == PaletteBlock)
			{
				Rect.X = static_cast<int16>(File.Word(Block + 4) + File.Word(Block + 8));
				Rect.Y = static_cast<int16>(File.Word(Block + 6) + File.Word(Block + 10));
			}
			else
			{
				return;
			}
			Rect.Width = static_cast<int16>(File.Word(Block + 12));
			Rect.Height = static_cast<int16>(File.Word(Block + 14));
			const uint32 Bytes = static_cast<uint32>(Rect.Width * Rect.Height * 2);
			if (Rect.Width < 0 || Rect.Height < 0 || Block + 16 + Bytes > File.Size())
			{
				System.Report(ESpriteFault::PcOutsideFile, Block);
				return;
			}
			System.UploadNow(Rect, File, Block + 16);
			Block += 16 + Bytes;
		}
	}
}
