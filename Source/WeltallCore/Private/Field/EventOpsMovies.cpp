#include "EventOps.h"

namespace Weltall::EventOps
{
	namespace
	{
		/** How FE 60's flags operand shows the movie (its low nibble), and the flags it keeps. */
		constexpr uint16 KindMask = 0xF;
		constexpr uint16 MovieFlags = 0xC0;
		/** FE 67's flags operand that makes the movie start again at its end. */
		constexpr uint16 RewindingFlags = 0xFE;
		constexpr int16 ScreenWidth = 0x140;
		constexpr int16 PictureHeight = 0x100;
		constexpr int16 LowerBuffer = 0x100;
		constexpr int16 OffScreenX = 0x140;
		/** FE A0's bytes from the pc (on its second byte) to its end. */
		constexpr std::size_t PlayMovieWithCuesLength = 12;

		/** FE 60: a movie, once no battle is on its way, shown by the low nibble of its flags operand. */
		void PlayMovie(FEventContext& Context)
		{
			Context.Run.bYield = true;
			if (Context.bBattleRequested)
			{
				Advance(Context, -1);
				return;
			}
			FFieldMovieRequest& Movie = Context.Requests.Movie;
			Movie.Movie = static_cast<uint16>(ReadValue(Context, 1));
			Movie.StartSector = static_cast<uint16>(ReadValue(Context, 3));
			Movie.EndFrame = static_cast<uint16>(ReadValue(Context, 5));
			const uint16 Flags = static_cast<uint16>(ReadValue(Context, 7));
			const uint16 Kind = Flags & KindMask;
			Movie.Flags = static_cast<uint8>(Flags & MovieFlags);
			Movie.Width = ScreenWidth;
			Movie.Height = PictureHeight;
			Movie.Kind = Kind;
			Movie.StartFrame = 1;
			switch (Kind)
			{
			case 0:
				Movie.Buffers = {OffScreenX, 0, OffScreenX, LowerBuffer};
				Context.MovieView = EFieldMovieView::OverField;
				Movie.bTrueColour = false;
				break;
			case 1:
				Movie.Buffers = {0, 0, 0, LowerBuffer};
				Context.MovieView = EFieldMovieView::FullScreen;
				Movie.bTrueColour = false;
				break;
			case 2:
				Movie.Buffers = {0, 0, 0, LowerBuffer};
				Context.MovieView = EFieldMovieView::FullScreen;
				Movie.bTrueColour = true;
				break;
			default:
				break;
			}
			Movie.Cues = FFieldMovieRequest::NoCues;
			Movie.bRewinds = false;
			Context.Requests.bMovie = true;
			Advance(Context, 9);
		}

		/**
		 * FE 67: a movie in the scene, decoded into a rectangle of VRAM the map's models show, the field
		 * running on; the flags operand FEh has it start again at its end. It never waits.
		 */
		void PlayMovieInScene(FEventContext& Context)
		{
			FFieldMovieRequest& Movie = Context.Requests.Movie;
			Movie.Movie = static_cast<uint16>(ReadValue(Context, 1));
			Movie.StartSector = static_cast<uint16>(ReadValue(Context, 3));
			Movie.StartFrame = static_cast<uint16>(ReadValue(Context, 5));
			Movie.EndFrame = static_cast<uint16>(ReadValue(Context, 7));
			const uint16 Flags = static_cast<uint16>(ReadValue(Context, 9));
			Movie.bRewinds = Flags == RewindingFlags;
			const int16 X = static_cast<int16>(ReadValue(Context, 11));
			const int16 Y = static_cast<int16>(ReadValue(Context, 13));
			Movie.Buffers = {X, Y, X, Y};
			Movie.Width = static_cast<int16>(ReadValue(Context, 15));
			Movie.Height = static_cast<int16>(ReadValue(Context, 17));
			Movie.Flags = static_cast<uint8>(Flags & FFieldMovieRequest::OwnSoundOff);
			Movie.Cues = FFieldMovieRequest::NoCues;
			Context.MovieView = EFieldMovieView::InScene;
			Movie.bTrueColour = false;
			Movie.Kind = Flags & KindMask;
			Context.Requests.bMovie = true;
			Advance(Context, 19);
		}

		/**
		 * FE A0: a full-screen movie in 24-bit colour without its own sound, once no battle is on its way,
		 * from a start frame to an end frame, with a set of sound cues (the flags at operand 11 pick
		 * constants or variables). The kind keeps the last movie's.
		 *
		 * One whose operands run past the bytecode's end (map 489's, written for an older layout of the
		 * instruction) ends the running script there as 00 does, and asks for no movie: the original takes
		 * its flags from the section's stream past its end and the movie from memory past the variables.
		 */
		void PlayMovieWithCues(FEventContext& Context)
		{
			if (Self(Context).Pc + PlayMovieWithCuesLength > Context.Scripts->CodeSize())
			{
				EndThread(Context);
				Context.Run.bUnlimited = false;
				Context.Run.bYield = true;
				return;
			}
			Context.Run.bYield = true;
			if (Context.bBattleRequested)
			{
				Advance(Context, -1);
				return;
			}
			const uint8 Flags = CodeByte(Context, 11);
			FFieldMovieRequest& Movie = Context.Requests.Movie;
			Movie.Movie = static_cast<uint16>(ReadOperand(Context, 1, Flags, 0x80));
			Movie.StartSector = static_cast<uint16>(ReadOperand(Context, 3, Flags, 0x40));
			Movie.StartFrame = static_cast<uint16>(ReadOperand(Context, 5, Flags, 0x20));
			Movie.EndFrame = static_cast<uint16>(ReadOperand(Context, 7, Flags, 0x10));
			Movie.Cues = static_cast<uint16>(ReadOperand(Context, 9, Flags, 0x08));
			Movie.Width = ScreenWidth;
			Movie.Flags = FFieldMovieRequest::OwnSoundOff;
			Movie.bTrueColour = true;
			Movie.Height = PictureHeight;
			Movie.Buffers = {0, 0, 0, LowerBuffer};
			Movie.bRewinds = false;
			Movie.Kind &= KindMask;
			Context.MovieView = EFieldMovieView::FullScreen;
			Context.Requests.bMovie = true;
			Advance(Context, 12);
		}

		/** FE 61: waits until a movie has started playing, and takes the news. */
		void WaitMovieStart(FEventContext& Context)
		{
			if (Context.Movie.bStarted)
			{
				Context.Movie.bStarted = false;
				Advance(Context, 1);
			}
			else
			{
				Advance(Context, -1);
			}
			Context.Run.bYield = true;
		}

		/** FE CB: asks the movie shown in the scene to stop. */
		void StopMovie(FEventContext& Context)
		{
			Context.Run.bYield = true;
			++Context.Movie.StopRequests;
			Advance(Context, 1);
		}

		/** FE CC: waits until no movie plays with the field running beside it. */
		void WaitMovieEnd(FEventContext& Context)
		{
			Advance(Context, Context.MovieView == EFieldMovieView::FullScreen ? 1 : -1);
			Context.Run.bYield = true;
		}

		/** FE 85: the movie's frame into a variable. */
		void GetMovieFrame(FEventContext& Context)
		{
			SetVar(Context, ReadU16(Context, 1), Context.Movie.Frame);
			Advance(Context, 3);
		}

		/** FE 86: how the screen comes back after the menu or a movie. */
		void SetReturnFade(FEventContext& Context)
		{
			Context.ReturnFade = CodeByte(Context, 1);
			Advance(Context, 2);
		}

		/**
		 * FE 9B, FE 9C and FE 9D: a dissolve of the screen over the ticks of the operand, which the
		 * field runs once the tick is over: from its picture to the live field (1, 2), or from the live
		 * field to its picture, held until a script asks for 1 or 2 (3).
		 */
		template <uint8 Mode>
		void Dissolve(FEventContext& Context)
		{
			Context.Requests.DissolveTicks = ReadValue(Context, 1);
			Context.Requests.Dissolve = Mode;
			Advance(Context, 3);
		}
	}

	void RegisterMovies(FBaseTable& Base, FExtendedTable& Extended)
	{
		(void)Base;
		Extended[0x60] = &PlayMovie;
		Extended[0x61] = &WaitMovieStart;
		Extended[0x67] = &PlayMovieInScene;
		Extended[0x85] = &GetMovieFrame;
		Extended[0x86] = &SetReturnFade;
		Extended[0x9B] = &Dissolve<1>;
		Extended[0x9C] = &Dissolve<2>;
		Extended[0x9D] = &Dissolve<3>;
		Extended[0xA0] = &PlayMovieWithCues;
		Extended[0xCB] = &StopMovie;
		Extended[0xCC] = &WaitMovieEnd;
	}
}
