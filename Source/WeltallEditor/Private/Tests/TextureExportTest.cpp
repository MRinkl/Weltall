#include "Shell/TextureExport.h"

#include "GameLoopTestGame.h"
#include "Graphics/TextureRegion.h"
#include "Graphics/TextureSheet.h"
#include "HAL/FileManager.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Rendering/TextureModLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureExportTest, "Weltall.Assets.TextureExportSavesEachPictureOnceByItsKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTextureExportTest::RunTest(const FString&)
{
	Weltall::FGameLibrary Library;
	const Weltall::FIso9660* Disc = FindTestDisc(Library);
	if (Disc == nullptr)
	{
		AddInfo(TEXT("No first disc in Games; the texture export check is skipped."));
		return true;
	}
	const auto Executable = Weltall::ReadBootExecutable(*Disc);
	const auto Index = Executable ? Weltall::FGameFileIndex::Read(*Executable) : Weltall::TResult<Weltall::FGameFileIndex>(Weltall::MakeError("no executable"));
	if (!TestTrue(TEXT("The disc's file index is read"), Index.HasValue())) return false;
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/TextureExport"));
	const FString Fei = Folder / FTextureModLibrary::OriginalsFolder / TEXT("sprites/04-0005");
	IFileManager::Get().DeleteDirectory(*Folder, false, true);
	ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Folder, false, true); };
	auto Export = FTextureExport::Open(*Disc, *Index, Folder);
	if (!TestTrue(TEXT("The export opens"), Export.HasValue())) return false;
	TestTrue(TEXT("Every streamed sprite file, map and file of pictures is an item"), (*Export)->TotalItems() > 33u + 121u + 730u);
	const auto First = (*Export)->Step();
	TestTrue(TEXT("Fei's sprite file is exported"), First.HasValue() && !*First);
	TArray<FString> Pictures;
	IFileManager::Get().FindFiles(Pictures, *(Fei / TEXT("*.png")), true, false);
	TestEqual(TEXT("Fei's cells are on one sheet for his one palette set"), Pictures.Num(), 1);
	FString LayoutText;
	FFileHelper::LoadFileToString(LayoutText, *(Fei / TEXT("cells-0.tsv")));
	const auto Layout = Weltall::ReadTextureSheetLayout(TCHAR_TO_UTF8(*LayoutText));
	TestTrue(TEXT("The sheet's layout names more than a hundred cells"), Layout.HasValue() && Layout->Cells.size() > 100);
	TestFalse(TEXT("The index waits for the whole export"),
		IFileManager::Get().FileExists(*(Folder / FTextureModLibrary::OriginalsFolder / FTextureExport::IndexName)));
	if (Pictures.IsEmpty()) return false;

	// An edit saved over an original survives the export run again.
	const FString Edited = Fei / Pictures[0];
	if (!FFileHelper::SaveStringToFile(TEXT("edited"), *Edited)) return false;
	auto Again = FTextureExport::Open(*Disc, *Index, Folder);
	if (!TestTrue(TEXT("The export opens again"), Again.HasValue())) return false;
	TestTrue(TEXT("The export steps again"), (*Again)->Step().HasValue());
	FString After;
	FFileHelper::LoadFileToString(After, *Edited);
	TestEqual(TEXT("An existing file is kept"), After, FString(TEXT("edited")));
	return true;
}

#endif
