#include "Gears/GearEngine.h"

namespace Weltall
{
	FGearEngine::FGearEngine(const char* InName) : Name(InName)
	{
	}

	FGearEngine::~FGearEngine() = default;

	void FGearEngine::Fail(std::string Reason)
	{
		if (!Fault)
		{
			Fault = MakeError(std::string(Name) + ": " + Reason);
		}
	}
}
