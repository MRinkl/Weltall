// The only Unreal code in the core: registering it as a module also routes its allocations through
// the engine's allocator, which objects passed to other modules rely on.
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, WeltallCore)
