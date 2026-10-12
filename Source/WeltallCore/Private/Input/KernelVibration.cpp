#include "Input/KernelVibration.h"

namespace Weltall
{
	void FKernelVibration::Start(int32 Port, int32 Vblanks)
	{
		Left[static_cast<std::size_t>(Port)] = static_cast<uint16>(Vblanks);
	}

	void FKernelVibration::Vblank()
	{
		for (std::size_t Port = 0; Port < Left.size(); ++Port)
		{
			On[Port] = Left[Port] != 0;
			if (On[Port])
			{
				--Left[Port];
			}
		}
	}
}
