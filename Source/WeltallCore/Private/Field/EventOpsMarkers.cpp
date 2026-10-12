#include "EventOps.h"

#include <algorithm>

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint8 NeutralColor = 0x80;

		/** A marker operand, or a fault when the markers are not made or it is past them. */
		bool IsUsableMarker(FEventContext& Context, int32 Marker)
		{
			if (!Context.Render.Markers.bMade || Marker < 0 || Marker >= FFieldScreenMarkers::Count)
			{
				Fail(Context, EEventFault::ScreenMarker, static_cast<uint32>(Marker));
				return false;
			}
			return true;
		}

		/**
		 * FE D4: the screen markers. Mode 0 makes them (neutral colours), 1 places one for the tick's frame
		 * (at x, y less (4, 12) for kind 0, (4, 4) for kind 1, as given otherwise), 2 gives them up and 3
		 * colours one; none ends the turn. Another mode leaves its second byte to run as an instruction.
		 */
		void ShowScreenMarker(FEventContext& Context)
		{
			FFieldScreenMarkers& Markers = Context.Render.Markers;
			switch (CodeByte(Context, 1))
			{
			case 0:
				Markers.bMade = true;
				for (std::array<uint8, 3>& Color : Markers.Colors)
				{
					Color.fill(NeutralColor);
				}
				Advance(Context, 2);
				return;
			case 1:
			{
				const int32 Marker = ReadValue(Context, 2);
				int16 X = static_cast<int16>(ReadValue(Context, 4));
				int16 Y = static_cast<int16>(ReadValue(Context, 6));
				const int32 Kind = ReadValue(Context, 8);
				if (!IsUsableMarker(Context, Marker))
				{
					return;
				}
				// A marker placed twice in a tick is linked twice into the frame's ordering table, which the
				// original's GPU then follows round forever.
				const auto Placed = [Marker](const FFieldScreenMarkers::FPlaced& Other) { return Other.Marker == Marker; };
				if (std::any_of(Markers.Placed.begin(), Markers.Placed.end(), Placed))
				{
					Fail(Context, EEventFault::ScreenMarker, static_cast<uint32>(Marker));
					return;
				}
				if (Kind == 0 || Kind == 1)
				{
					X = static_cast<int16>(X - 4);
					Y = static_cast<int16>(Y - (Kind == 0 ? 12 : 4));
				}
				Markers.Placed.push_back({static_cast<uint8>(Marker), X, Y});
				Advance(Context, 10);
				return;
			}
			case 2:
				Markers.bMade = false;
				Advance(Context, 2);
				return;
			case 3:
			{
				const int32 Marker = ReadValue(Context, 2);
				const std::array<uint8, 3> Color = {static_cast<uint8>(ReadValue(Context, 4)), static_cast<uint8>(ReadValue(Context, 6)),
					static_cast<uint8>(ReadValue(Context, 8))};
				if (!IsUsableMarker(Context, Marker))
				{
					return;
				}
				Markers.Colors[static_cast<std::size_t>(Marker)] = Color;
				Advance(Context, 10);
				return;
			}
			default:
				return;
			}
		}
	}

	void RegisterMarkers(FBaseTable&, FExtendedTable& Extended)
	{
		Extended[0xD4] = &ShowScreenMarker;
	}
}
