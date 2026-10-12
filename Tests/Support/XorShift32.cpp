#include "Support/XorShift32.h"

namespace Weltall::Testing
{
	uint32 FXorShift32::Below(uint32 Limit)
	{
		return Next() % Limit;
	}
}
