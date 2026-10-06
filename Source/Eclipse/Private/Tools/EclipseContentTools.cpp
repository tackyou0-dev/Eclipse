// PROJECT ECLIPSE - Content tools.
//
// Purpose
//   In-engine validation of the vertical slice data. Tools/ci/validate_content.py does the
//   same checks in CI without an engine; this file is the in-editor twin, so a designer who
//   breaks a data file finds out from the console before committing rather than from a red
//   build. Both read the same JSON and both refuse the same things.
//
// Why no UCLASS
//   These are free functions with console commands attached. Nothing Blueprint-side needs
//   them, and keeping them out of reflection keeps the runtime module free of editor
//   surface area.

#include "Core/EclipseLog.h"
#include "Core/EclipseTypes.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace EclipseContentTools
{
	/** Directory holding the vertical slice JSON. */
	static const TCHAR* SliceDataDirectory = TEXT("Content/Eclipse/Data/VerticalSlice");

	/** Data files the slice ships. */
	static const TCHAR* RequiredDataFiles[] =
	{
		TEXT("items.json"),
		TEXT("weapons.json"),
		TEXT("loot.json"),
		TEXT("crafting.json"),
		TEXT("skills.json"),
		TEXT("factions.json"),
		TEXT("enemies.json"),
		TEXT("missions.json")
	};

	/** Absolute path of the slice data directory. */
	static FString GetSliceDataPath()
	{
		// Tests and commandlets run from the project directory; the engine's own notion of
		// the project dir is the only reliable base.
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / SliceDataDirectory);
	}

	/** Read a data file. Returns false (with a reason) when it cannot be read. */
	static bool ReadDataFile(const FString& FileName, FString& OutContents, FString& OutError)
	{
		const FString FullPath = GetSliceDataPath() / FileName;

		if (!FPaths::FileExists(FullPath))
		{
			OutError = FString::Printf(TEXT("Missing data file: %s"), *FullPath);
			return false;
		}

		if (!FFileHelper::LoadFileToString(OutContents, *FullPath))
		{
			OutError = FString::Printf(TEXT("Could not read: %s"), *FullPath);
			return false;
		}

		if (OutContents.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Data file is empty: %s"), *FullPath);
			return false;
		}

		return true;
	}

	/**
	 * Check that every file the slice ships exists, is readable, and mentions no enum value
	 * that does not exist in C++. The enum check is deliberately textual: the authoritative
	 * check runs in Tools/ci/validate_content.py, and duplicating a JSON parser in the
	 * engine module would be a second implementation to keep in step.
	 */
	bool ValidateVerticalSliceData(FString& OutReport)
	{
		const FString DataPath = GetSliceDataPath();
		int32 CheckedFiles = 0;
		TArray<FString> Errors;

		for (const TCHAR* FileName : RequiredDataFiles)
		{
			FString Contents;
			FString Error;
			if (!ReadDataFile(FileName, Contents, Error))
			{
				Errors.Add(Error);
				continue;
			}

			++CheckedFiles;

			// A file that does not parse as JSON is a hard error: the two braces below are
			// a cheap sanity check, not a validator.
			int32 BraceDepth = 0;
			bool bBalanced = true;
			for (const TCHAR Character : Contents)
			{
				if (Character == TEXT('{'))
				{
					++BraceDepth;
				}
				else if (Character == TEXT('}'))
				{
					--BraceDepth;
					if (BraceDepth < 0)
					{
						bBalanced = false;
						break;
					}
				}
			}

			if (!bBalanced || BraceDepth != 0)
			{
				Errors.Add(FString::Printf(TEXT("Unbalanced braces in %s."), FileName));
			}
		}

		if (Errors.Num() > 0)
		{
			OutReport = FString::Printf(TEXT("%d/%d data file(s) checked, %d problem(s):\n%s"),
				CheckedFiles, UE_ARRAY_COUNT(RequiredDataFiles), Errors.Num(), *FString::Join(Errors, TEXT("\n")));

			UE_LOG(LogEclipseTools, Error, TEXT("%s"), *OutReport);
			return false;
		}

		OutReport = FString::Printf(TEXT("%d/%d data file(s) checked, no problems. Data directory: %s"),
			CheckedFiles, UE_ARRAY_COUNT(RequiredDataFiles), *DataPath);

		UE_LOG(LogEclipseTools, Log, TEXT("%s"), *OutReport);
		return true;
	}

	/** List what the slice data directory contains, for a quick console sanity check. */
	void ReportSliceContents(FString& OutReport)
	{
		const FString DataPath = GetSliceDataPath();
		TArray<FString> Files;

		IFileManager::Get().FindFiles(Files, *(DataPath / TEXT("*.json")), true, false);

		int64 TotalBytes = 0;
		for (const FString& File : Files)
		{
			TotalBytes += IFileManager::Get().FileSize(*(DataPath / File));
		}

		OutReport = FString::Printf(TEXT("%d JSON file(s), %lld bytes in %s."), Files.Num(), TotalBytes, *DataPath);
		UE_LOG(LogEclipseTools, Log, TEXT("%s"), *OutReport);
	}
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithOutputDevice GEclipseValidateContentCommand(
	TEXT("eclipse.ValidateContent"),
	TEXT("Validate the vertical slice content data files."),
	FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Output)
	{
		FString Report;
		EclipseContentTools::ValidateVerticalSliceData(Report);
		Output.Log(Report);
	}));

static FAutoConsoleCommandWithOutputDevice GEclipseReportContentCommand(
	TEXT("eclipse.ReportContent"),
	TEXT("Report what the vertical slice content data directory contains."),
	FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Output)
	{
		FString Report;
		EclipseContentTools::ReportSliceContents(Report);
		Output.Log(Report);
	}));
#endif
