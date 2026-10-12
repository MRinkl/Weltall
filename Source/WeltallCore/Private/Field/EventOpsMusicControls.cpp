#include "EventOps.h"

namespace Weltall::EventOps
{
	namespace
	{
		constexpr int32 NoSong = 0xFF;

		void SetVolume(FEventContext& Context)
		{
			const int32 Volume = ReadValue(Context, 1);
			Context.Songs->SetMusicVolume(Volume, ReadValue(Context, 3));
		}

		void SetPitch(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 5);
			const int32 Semitones = ReadOperand(Context, 1, Flags, 0x80);
			Context.Songs->SetMusicPitch(Semitones, ReadOperand(Context, 3, Flags, 0x40));
		}

		void SetTempo(FEventContext& Context)
		{
			const int32 Scale = ReadValue(Context, 1);
			Context.Songs->SetMusicTempo(Scale, ReadValue(Context, 3));
		}

		void SetPan(FEventContext& Context)
		{
			const uint8 Flags = CodeByte(Context, 5);
			const int32 Offset = ReadOperand(Context, 1, Flags, 0x80);
			Context.Songs->SetMusicPan(Offset, ReadOperand(Context, 3, Flags, 0x40));
		}

		void MuteTracks(FEventContext& Context)
		{
			Context.Songs->MuteMusicTracks(static_cast<uint32>(ReadValue(Context, 1)));
		}

		/**
		 * FE 0E-FE 12: a change to the playing song once it is loaded. Until then the instruction waits
		 * on its first byte, unless no song is asked for or the map's init runs, when it is passed over.
		 * The actor's turn ends either way.
		 */
		template <void (*Apply)(FEventContext&), int32 Length>
		void ChangeMusic(FEventContext& Context)
		{
			IFieldSongs* Songs = Context.Songs;
			if (Songs == nullptr)
			{
				Fail(Context, EEventFault::NoSongs, 0);
				return;
			}
			if (Songs->IsMusicLoaded())
			{
				Apply(Context);
				Advance(Context, Length - 1);
			}
			else
			{
				Advance(Context, Songs->ChosenSong() == NoSong || !Context.bInitDone ? Length - 1 : -1);
			}
			Context.Run.bYield = true;
		}

		/** FE A2: waits on its first byte while a song loads; the actor's turn ends either way. */
		void WaitMusicLoaded(FEventContext& Context)
		{
			if (Context.Songs == nullptr)
			{
				Fail(Context, EEventFault::NoSongs, 0);
				return;
			}
			Advance(Context, Context.Songs->IsLoading() ? -1 : 1);
			Context.Run.bYield = true;
		}
	}

	void RegisterMusicControls(FBaseTable&, FExtendedTable& Extended)
	{
		Extended[0x0E] = &ChangeMusic<&SetVolume, 6>;
		Extended[0x0F] = &ChangeMusic<&SetPitch, 7>;
		Extended[0x10] = &ChangeMusic<&SetTempo, 6>;
		Extended[0x11] = &ChangeMusic<&SetPan, 7>;
		Extended[0x12] = &ChangeMusic<&MuteTracks, 4>;
		Extended[0xA2] = &WaitMusicLoaded;
	}
}
