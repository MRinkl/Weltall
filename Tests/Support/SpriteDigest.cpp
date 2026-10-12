#include "Support/SpriteDigest.h"

namespace Weltall::Testing
{
	void DigestSprite(FOracleDigest& Digest, const FSprite& Sprite, bool bDrawn)
	{
		Digest.I32(Sprite.Position[0], Sprite.Position[1], Sprite.Position[2], Sprite.Velocity[0], Sprite.Velocity[1], Sprite.Velocity[2], Sprite.Speed, Sprite.Gravity);
		Digest.I32(Sprite.Red | Sprite.Green << 8 | Sprite.Blue << 16 | static_cast<uint32>(Sprite.Code) << 24);
		Digest.I16(Sprite.Scale, Sprite.OtIndex, Sprite.OtBias, Sprite.Heading, Sprite.Frame, Sprite.FrameTop, Sprite.FrameHalfHeight, Sprite.SizeFactor);
		Digest.I32(bDrawn ? Sprite.DrawFlags & ~SpriteDrawFlags::MatrixDirty : Sprite.DrawFlags, Sprite.BuildFlags);
		Digest.I32(Sprite.Pc, Sprite.Header, Sprite.List);
		Digest.I16(Sprite.Facing, Sprite.Weight, Sprite.FloorY);
		Digest.U8(Sprite.StackPointer, Sprite.ParentAnimation);
		for (const uint8 Byte : Sprite.Stack)
		{
			Digest.U8(Byte);
		}
		Digest.I16(Sprite.Wait, Sprite.SavedPoint[0], Sprite.SavedPoint[1], Sprite.SavedPoint[2]);
		Digest.I32(Sprite.AnimationFlags, Sprite.TimeFlags, Sprite.NextWord);
		const bool bModel = Sprite.ModelFile != nullptr;
		if (Sprite.bHasTransform)
		{
			Digest.I16(Sprite.Rotation[0], Sprite.Rotation[1], Sprite.Rotation[2], Sprite.TransformScale[0], Sprite.TransformScale[1], Sprite.TransformScale[2]);
			Digest.U8(Sprite.PivotX, Sprite.PivotY);
			if (bModel)
			{
				Digest.U8(2);
				Digest.I32(Sprite.Model);
			}
			else if (Sprite.bHasAnchors)
			{
				Digest.U8(1);
				for (const FSpriteAnchor& Anchor : Sprite.Anchors)
				{
					Digest.U8(Anchor.OffsetX, Anchor.OffsetY);
					Digest.I16(Anchor.RotationX, Anchor.RotationY, Anchor.RotationZ);
				}
			}
			else
			{
				Digest.U8(0);
			}
		}
		if (Sprite.Task == nullptr)
		{
			Digest.I32(Sprite.Extra.PresetVerticalSpeed, Sprite.Extra.PresetGravity);
			Digest.I16(Sprite.Extra.Signal);
		}
		if (Sprite.bHasTransform && !bModel && Sprite.Layout() == SpriteDrawFlags::LayoutParts)
		{
			for (int32 Index = 0; Index < Sprite.PartCount(); ++Index)
			{
				const FSpritePart& Part = Sprite.Parts[static_cast<std::size_t>(Index)];
				Digest.I16(Part.X, Part.Y);
				Digest.U8(Part.U, Part.V, Part.Width, Part.Height, Part.GrowWidth, Part.GrowHeight);
				Digest.I16(Part.TexturePage, Part.Clut);
				Digest.I32(Part.ColorCode, Part.Flags & 0x3F);
			}
		}
	}
}
