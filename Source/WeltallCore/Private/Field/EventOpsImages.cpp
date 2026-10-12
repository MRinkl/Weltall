#include "EventOps.h"

#include <utility>

// The scripts' images in VRAM: TIMs read from the disc and uploaded, the effects' textures kept aside
// while large images cover them, and CLUT rows faded.
namespace Weltall::EventOps
{
	namespace
	{
		constexpr int32 ImageDirectory = 4;
		/** FE 77's images are files of directory 4 from this one on. */
		constexpr int32 FirstScriptImage = 0x7FB;
		/** A CLUT row operand that leaves the CLUT where the image's header puts it; the others count from row 232. */
		constexpr int32 ClutKept = 0xFF;
		constexpr int32 FirstClutRow = 0xE8;
		/** An image placed right of this x and below this y reaches the effects' textures, which are kept aside first. */
		constexpr int32 EffectTexturesReachedX = 0x2BF;
		constexpr int32 EffectTexturesReachedY = 0xFF;
		/** What FE DD's two modes that take operands grant the running actor: the rows' work takes many instructions' time. */
		constexpr int32 ClutFadeBudget = 0x20;
		constexpr int32 ClutRowWidth = 0x100;

		/** Reads a file of directory 4 as a TIM, or reports why it cannot. */
		std::optional<FTim> ReadImage(FEventContext& Context, int32 File)
		{
			std::vector<uint8> Bytes;
			if (Context.Files == nullptr || !Context.Files->ReadFile(ImageDirectory, File, Bytes))
			{
				Fail(Context, EEventFault::ImageUnreadable, Context.Files == nullptr ? 0 : static_cast<uint32>(File));
				return std::nullopt;
			}
			TResult<FTim> Image = ParseTim(Bytes);
			if (!Image)
			{
				Fail(Context, EEventFault::ImageUnreadable, static_cast<uint32>(File));
				return std::nullopt;
			}
			return std::move(*Image);
		}

		/**
		 * Uploads an image as the field's TIM loader does: its CLUT first, moved to (0, ClutY) unless
		 * ClutY is -1, then its pixels at (X, Y). The loader moves the rectangles in the image's own
		 * header, so a later upload of the same image starts from them.
		 */
		void PlaceImage(FEventContext& Context, FTim& Image, int16 X, int16 Y, int16 ClutY)
		{
			if (Image.Clut)
			{
				if (ClutY != -1)
				{
					Image.Clut->Rect.X = 0;
					Image.Clut->Rect.Y = ClutY;
				}
				Context.Render.Upload(*Image.Clut);
			}
			Image.Image.Rect.X = X;
			Image.Image.Rect.Y = Y;
			Context.Render.Upload(Image.Image);
		}

		/**
		 * FE 77: mode 0 reads an image, 1 uploads it, another gives it up; each ends the turn. The port
		 * reads at once, so the instruction never waits for the drive as the original's may.
		 */
		void LoadImage(FEventContext& Context)
		{
			const uint8 Mode = CodeByte(Context, 1);
			if (Mode == 0)
			{
				// The file's number lies past this instruction's three bytes: in the upload (FE 77 01) that
				// the scripts always put next, whose operand at +3 holds it and whose flags make it a constant.
				const int32 File = ReadOperand(Context, 5, CodeByte(Context, 13), 0x80);
				std::optional<FTim> Image = ReadImage(Context, FirstScriptImage + File);
				if (!Image)
				{
					return;
				}
				Context.ScriptImage = std::move(*Image);
				Advance(Context, 2);
			}
			else if (Mode == 1)
			{
				const uint8 Flags = CodeByte(Context, 10);
				const int32 Clut = ReadOperand(Context, 8, Flags, 0x10);
				const int16 ClutY = Clut == ClutKept ? int16{-1} : static_cast<int16>(Clut + FirstClutRow);
				const int32 X = ReadOperand(Context, 4, Flags, 0x40);
				const int32 Y = ReadOperand(Context, 6, Flags, 0x20);
				if (!Context.ScriptImage)
				{
					Fail(Context, EEventFault::ImageNotLoaded, 0);
					return;
				}
				if (Y > EffectTexturesReachedY && X > EffectTexturesReachedX)
				{
					Context.Render.SaveEffectTextures();
				}
				PlaceImage(Context, *Context.ScriptImage, static_cast<int16>(X), static_cast<int16>(Y), ClutY);
				Advance(Context, 11);
			}
			else
			{
				Context.ScriptImage.reset();
				Advance(Context, 2);
			}
			Context.Run.bYield = true;
		}

		/** FE A3: keeps the effects' textures aside (0) or puts them back (another). */
		void KeepEffectTextures(FEventContext& Context)
		{
			if (CodeByte(Context, 1) == 0)
			{
				Context.Render.SaveEffectTextures();
			}
			else
			{
				Context.Render.RestoreEffectTextures();
			}
			Advance(Context, 2);
		}

		/**
		 * FE DD: mode 0 keeps whole CLUT rows aside (from row y, h of them), 1 fades one of them into
		 * their working copy by a factor out of 32, which the tick's frame then loads back, 2 gives them
		 * up and 3 only ends the turn; another mode leaves its second byte to run as an instruction.
		 */
		void FadeClutRows(FEventContext& Context)
		{
			FFieldClutFade& Fade = Context.Render.ClutFade;
			FFieldVramOperation Operation;
			switch (CodeByte(Context, 1))
			{
			case 0:
			{
				Context.Run.Budget += ClutFadeBudget;
				const int32 Y = ReadValue(Context, 2);
				const int32 Rows = ReadValue(Context, 4);
				if (Rows < 0)
				{
					Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(Rows));
					return;
				}
				Fade.bKept = true;
				Fade.Rect = {0, static_cast<int16>(Y), ClutRowWidth, static_cast<int16>(Rows)};
				Operation.Kind = EFieldVramChange::SaveClutRows;
				Operation.Rect = Fade.Rect;
				Context.Render.VramOperations.push_back(Operation);
				Context.Run.bYield = true;
				Advance(Context, 6);
				return;
			}
			case 1:
			{
				Context.Run.Budget += ClutFadeBudget;
				const int32 Row = ReadValue(Context, 2);
				const int32 Factor = ReadValue(Context, 4);
				if (!Fade.bKept || Row < 0 || Row >= Fade.Rect.Height)
				{
					Fail(Context, EEventFault::ImageNotLoaded, static_cast<uint32>(Row));
					return;
				}
				Operation.Kind = EFieldVramChange::FadeClutRow;
				Operation.Rect = Fade.Rect;
				Operation.Row = Row;
				Operation.Factor = Factor;
				Context.Render.VramOperations.push_back(Operation);
				Fade.bLoadPending = true;
				Advance(Context, 6);
				return;
			}
			case 2:
				Fade.bKept = false;
				break;
			case 3:
				break;
			default:
				return;
			}
			Context.Run.bYield = true;
			Advance(Context, 2);
		}

		/** FE B1: puts the gear cockpit's instruments up, their image read and uploaded (the CLUT to row 232). */
		void ShowCockpitHud(FEventContext& Context)
		{
			std::optional<FTim> Image = ReadImage(Context, FFieldCockpitHud::ImageFile);
			if (!Image)
			{
				return;
			}
			PlaceImage(Context, *Image, FFieldCockpitHud::ImageX, FFieldCockpitHud::ImageY, FFieldCockpitHud::ClutY);
			Context.Render.CockpitHud.Show();
			Advance(Context, 1);
		}
	}

	void RegisterImages(FBaseTable&, FExtendedTable& Extended)
	{
		Extended[0x77] = &LoadImage;
		Extended[0xA3] = &KeepEffectTextures;
		Extended[0xB1] = &ShowCockpitHud;
		Extended[0xDD] = &FadeClutRows;
	}
}
