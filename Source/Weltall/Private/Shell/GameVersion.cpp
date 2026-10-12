#include "Shell/GameVersion.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FString GameVersion()
{
	FString Version;
	GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
	FString Revision;
	if (FFileHelper::LoadFileToString(Revision, *FPaths::Combine(FPaths::RootDir(), TEXT("Revision.txt"))))
	{
		Revision.TrimStartAndEndInline();
		if (!Revision.IsEmpty())
		{
			Version += FString::Printf(TEXT(" (%s)"), *Revision);
		}
	}
	return Version;
}
