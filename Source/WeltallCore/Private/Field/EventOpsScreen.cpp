#include "EventOps.h"

#include <array>

namespace Weltall::EventOps
{
	namespace
	{
		uint8 ReadByte(FEventContext& Context, int32 Offset)
		{
			return static_cast<uint8>(ReadValue(Context, Offset));
		}

		/** E7: the colour the screen is cleared to. */
		void SetClearColor(FEventContext& Context)
		{
			FMdlColor& Color = Context.Render.BackgroundColor;
			Color.R = ReadByte(Context, 1);
			Color.G = ReadByte(Context, 3);
			Color.B = ReadByte(Context, 5);
			Advance(Context, 7);
		}

		/** FE 8E: how far past the screen the original's culling keeps models. */
		void SetCullMargins(FEventContext& Context)
		{
			Context.Render.CullMarginX = ReadValue(Context, 1);
			Context.Render.CullMarginY = ReadValue(Context, 3);
			Advance(Context, 5);
		}

		/** FE 9E: the rectangle of the screen the frame's drawing is clipped to. */
		void SetDrawClip(FEventContext& Context)
		{
			FVramRect& Clip = Context.Render.DrawClip;
			Clip.X = static_cast<int16>(ReadValue(Context, 1));
			Clip.Y = static_cast<int16>(ReadValue(Context, 3));
			Clip.Width = static_cast<int16>(ReadValue(Context, 5));
			Clip.Height = static_cast<int16>(ReadValue(Context, 7));
			Advance(Context, 9);
		}

		/** FE 6A: the farthest ordering-table entry drawn, where the backdrop is drawn too. */
		void SetFarBucket(FEventContext& Context)
		{
			Context.Render.FarBucket = static_cast<uint16>(ReadValue(Context, 1));
			Advance(Context, 3);
		}

		/** A fade's ticks from the value operand at Offset: with the fix, no ticks takes one. */
		int32 FadeTicks(FEventContext& Context, int32 Offset)
		{
			const int32 Ticks = ReadValue(Context, Offset);
			return Ticks == 0 && Context.Options.bZeroFrameFadeSnaps ? 1 : Ticks;
		}

		/**
		 * F1: fades the scripts' colour layer toward a colour over a number of ticks, blended by a GPU
		 * mode. No ticks takes one when the fix is on; in the original it divides by zero unchecked, and
		 * the layer then stays as it is.
		 */
		void ScreenFade(FEventContext& Context)
		{
			const int32 Ticks = FadeTicks(Context, 9);
			const std::array<int32, 3> Target = {ReadValue(Context, 3), ReadValue(Context, 5), ReadValue(Context, 7)};
			const int32 Mode = ReadValue(Context, 1);
			Context.Render.Fades[1].Start(Target, Ticks, static_cast<uint16>(Mode));
			Advance(Context, 11);
		}

		/** B3: the map's layer cleared, and the screen faded back in from black if it is faded out. */
		void FadeIn(FEventContext& Context)
		{
			Context.Render.Fades[0].Reset();
			Context.Render.StartFadeIn(FadeTicks(Context, 1));
			Advance(Context, 3);
		}

		/** B4: the screen faded out to black, unless it is. */
		void FadeOut(FEventContext& Context)
		{
			Context.Render.StartFadeOut(FadeTicks(Context, 1));
			Advance(Context, 3);
		}

		/**
		 * E1: copies a rectangle of VRAM to another place, or with the source's corner at (0, 0) clears
		 * the rectangle that the other operands give to black; the renderer makes the change.
		 */
		void ChangeVram(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 13);
			const int32 X = ReadOperand(Context, 1, Flags, 0x80);
			const int32 Y = ReadOperand(Context, 3, Flags, 0x40);
			std::array<int32, 4> Rest{};
			for (std::size_t Operand = 0; Operand < Rest.size(); ++Operand)
			{
				Rest[Operand] = ReadOperand(Context, 5 + 2 * static_cast<int32>(Operand), Flags, static_cast<uint8>(0x20 >> Operand));
			}
			FFieldVramOperation Operation;
			const bool bClear = X == 0 && Y == 0;
			Operation.Kind = bClear ? EFieldVramChange::Clear : EFieldVramChange::Copy;
			if (bClear)
			{
				Operation.Rect = {static_cast<int16>(Rest[0]), static_cast<int16>(Rest[1]), static_cast<int16>(Rest[2]), static_cast<int16>(Rest[3])};
			}
			else
			{
				Operation.Rect = {static_cast<int16>(X), static_cast<int16>(Y), static_cast<int16>(Rest[0]), static_cast<int16>(Rest[1])};
				Operation.ToX = static_cast<int16>(Rest[2]);
				Operation.ToY = static_cast<int16>(Rest[3]);
			}
			// The library refuses a copy with no width or height before the GPU sees it.
			if (bClear || (Operation.Rect.Width != 0 && Operation.Rect.Height != 0))
			{
				Context.Render.VramOperations.push_back(Operation);
				Context.Render.WroteVram(bClear ? Operation.Rect : FVramRect{Operation.ToX, Operation.ToY, Operation.Rect.Width, Operation.Rect.Height});
			}
			Advance(Context, 14);
		}

		/**
		 * FE DF: 0 clears VRAM (the library's clear of 1280 x 512, which it holds to all but the last column
		 * and row) and widens the drawing area and the display to 640 x 224; 1 shows the still the scripts put
		 * in VRAM, 2 hides it; any other mode does nothing.
		 */
		void SetHiResStill(FEventContext& Context)
		{
			constexpr FVramRect Everything{0, 0, 0x500, 0x200};
			constexpr FVramRect WideScreen{0, 0, 0x280, 0xE0};
			FFieldRenderState& Render = Context.Render;
			switch (ReadValue(Context, 1))
			{
			case 0:
			{
				FFieldVramOperation Clear;
				Clear.Kind = EFieldVramChange::Clear;
				Clear.Rect = Everything;
				Render.VramOperations.push_back(Clear);
				Render.WroteVram(Everything);
				Render.DrawClip = WideScreen;
				Render.bHiResDisplay = true;
				break;
			}
			case 1:
				Render.bHiResStill = true;
				break;
			case 2:
				Render.bHiResStill = false;
				break;
			default:
				break;
			}
			Advance(Context, 3);
		}

		/** E5: turns fog on with its colours and depths, and puts every model in its fog draw mode. */
		void SetFog(FEventContext& Context)
		{
			FFieldFog& Fog = Context.Render.Fog;
			for (std::size_t Channel = 0; Channel < 3; ++Channel)
			{
				Fog.Color[Channel] = ReadByte(Context, 1 + 2 * static_cast<int32>(Channel));
			}
			for (std::size_t Channel = 0; Channel < 3; ++Channel)
			{
				Fog.FarColor[Channel] = ReadByte(Context, 7 + 2 * static_cast<int32>(Channel));
			}
			Fog.Near = static_cast<int16>(ReadValue(Context, 13));
			Fog.Far = static_cast<int16>(ReadValue(Context, 15));
			Fog.bOn = true;
			SetFieldModelDrawModes(Context.Actors, true);
			Advance(Context, 17);
		}

		/** FE 0C: six words that nothing in the field reads; the instruction only moves on. */
		void SetUnreadWords(FEventContext& Context)
		{
			Advance(Context, 13);
		}

		/** FE 1D: how far the drifting models move each tick; drifting models then escape the culling. */
		void SetSceneryDriftStep(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 7);
			FFieldRenderState& Render = Context.Render;
			Render.DriftStep[0] = static_cast<int16>(ReadOperand(Context, 1, Flags, 0x80));
			Render.DriftStep[2] = static_cast<int16>(ReadOperand(Context, 3, Flags, 0x40));
			Render.DriftStep[1] = static_cast<int16>(ReadOperand(Context, 5, Flags, 0x20));
			Render.SceneryDrift |= FieldSceneryDrift::DrawnOffScreen;
			Advance(Context, 8);
		}

		/** FE 70: whether the scenery drifts, and whether the drifting models escape the culling. */
		void SetSceneryDrift(FEventContext& Context)
		{
			Context.Render.SceneryDrift = static_cast<uint8>(ReadValue(Context, 1) - 0x80);
			Advance(Context, 3);
		}

		/** FE 80: the backdrop's texture and scroll, for the backdrop the map makes after its init. */
		void SetPanorama(FEventContext& Context)
		{
			FFieldPanorama& Panorama = Context.Render.Panorama;
			Panorama.VramX = static_cast<int16>(ReadU16(Context, 1));
			Panorama.VramY = static_cast<int16>(ReadU16(Context, 3));
			Panorama.Width = static_cast<int16>(ReadU16(Context, 5));
			if (Panorama.Width == 0)
			{
				Panorama.Width = 1;
			}
			Panorama.Height = static_cast<int16>(ReadU16(Context, 7));
			Panorama.ClutX = 0;
			Panorama.ClutY = static_cast<int16>(ReadU16(Context, 9));
			Panorama.TexturePage = static_cast<int16>(ReadU16(Context, 11));
			Panorama.Scroll = static_cast<int16>(ReadU16(Context, 13));
			Advance(Context, 15);
		}

		/** FE 81: the backdrop's horizon point: x, its distance along the view, then its height. */
		void SetPanoramaPosition(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 7);
			std::array<int32, 3>& Position = Context.Render.Panorama.Position;
			Position[0] = ReadOperand(Context, 1, Flags, 0x80);
			Position[2] = ReadOperand(Context, 3, Flags, 0x40);
			Position[1] = ReadOperand(Context, 5, Flags, 0x20);
			Advance(Context, 8);
		}

		/** FE 82: the backdrop's colour bands and fade; the map then makes a backdrop after its init. */
		void SetPanoramaColors(FEventContext& Context)
		{
			FFieldPanorama& Panorama = Context.Render.Panorama;
			for (std::size_t Band = 0; Band < 3; ++Band)
			{
				for (std::size_t Channel = 0; Channel < 3; ++Channel)
				{
					Panorama.Colors[Band][Channel] = ReadByte(Context, 1 + 2 * static_cast<int32>(3 * Band + Channel));
				}
			}
			Panorama.BandScale = static_cast<int16>(ReadValue(Context, 19));
			Panorama.DepthDivisor = static_cast<int16>(ReadValue(Context, 21));
			Panorama.DepthOffset = static_cast<int16>(ReadValue(Context, 23));
			Panorama.bEnabled = true;
			Advance(Context, 25);
		}

		/**
		 * FE 3D and FE 3E: one of the gears' lights' direction (x, y, z) or colour (r, g, b). A light
		 * past the three writes where the original's does, as long as that is within the two matrices.
		 */
		template <int32 (*Word)(int32, int32)>
		void SetGearLight(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 9);
			const int32 Light = ReadOperand(Context, 1, Flags, 0x80);
			std::array<int16, 2 * FFieldGearLights::MatrixWords>& Words = Context.Render.GearLights.Words;
			for (int32 Component = 0; Component < 3; ++Component)
			{
				const int32 At = Word(Light, Component);
				if (At < 0 || static_cast<std::size_t>(At) >= Words.size())
				{
					Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Light));
					return;
				}
			}
			constexpr std::array<uint8, 3> Bits = {0x40, 0x20, 0x10};
			for (int32 Component = 0; Component < 3; ++Component)
			{
				const int16 Value = static_cast<int16>(ReadOperand(Context, 3 + 2 * Component, Flags, Bits[static_cast<std::size_t>(Component)]));
				Words[static_cast<std::size_t>(Word(Light, Component))] = Value;
			}
			Advance(Context, 10);
		}

		/** FE 3F: the gears' back colour. */
		void SetAmbientColor(FEventContext& Context)
		{
			std::array<uint8, 3>& Ambient = Context.Render.GearLights.Ambient;
			for (std::size_t Channel = 0; Channel < 3; ++Channel)
			{
				Ambient[Channel] = ReadByte(Context, 1 + 2 * static_cast<int32>(Channel));
			}
			Advance(Context, 7);
		}
	}

	void RegisterScreen(FBaseTable& Base, FExtendedTable& Extended)
	{
		Base[0xE1] = &ChangeVram;
		Base[0xE5] = &SetFog;
		Base[0xE7] = &SetClearColor;
		Base[0xF1] = &ScreenFade;
		Base[0xB3] = &FadeIn;
		Base[0xB4] = &FadeOut;
		Extended[0x0C] = &SetUnreadWords;
		Extended[0x3D] = &SetGearLight<&FFieldGearLights::DirectionWord>;
		Extended[0x3E] = &SetGearLight<&FFieldGearLights::ColorWord>;
		Extended[0x3F] = &SetAmbientColor;
		Extended[0x6A] = &SetFarBucket;
		Extended[0x1D] = &SetSceneryDriftStep;
		Extended[0x70] = &SetSceneryDrift;
		Extended[0x80] = &SetPanorama;
		Extended[0x81] = &SetPanoramaPosition;
		Extended[0x82] = &SetPanoramaColors;
		Extended[0x8E] = &SetCullMargins;
		Extended[0x9E] = &SetDrawClip;
		Extended[0xDF] = &SetHiResStill;
	}
}
