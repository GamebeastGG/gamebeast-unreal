// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#include "Internal/GamebeastStorage.h"

#include "HAL/FileManager.h"
#include "Internal/GamebeastLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace Gamebeast::Storage
{
	FString Directory()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Gamebeast"));
	}

	FString PathFor(const FString& FileName)
	{
		return FPaths::Combine(Directory(), FileName);
	}

	bool Load(const FString& FileName, FString& OutText)
	{
		const FString Path = PathFor(FileName);
		if (!IFileManager::Get().FileExists(*Path))
		{
			return false;
		}
		return FFileHelper::LoadFileToString(OutText, *Path);
	}

	bool Save(const FString& FileName, const FString& Text)
	{
		IFileManager& FileManager = IFileManager::Get();
		FileManager.MakeDirectory(*Directory(), /*Tree*/ true);

		const FString Path = PathFor(FileName);
		const FString TempPath = Path + TEXT(".tmp");
		if (!FFileHelper::SaveStringToFile(Text, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			GB_WARN(TEXT("Could not write %s."), *TempPath);
			return false;
		}
		if (!FileManager.Move(*Path, *TempPath, /*Replace*/ true))
		{
			GB_WARN(TEXT("Could not replace %s."), *Path);
			FileManager.Delete(*TempPath);
			return false;
		}
		return true;
	}

	void Delete(const FString& FileName)
	{
		const FString Path = PathFor(FileName);
		IFileManager& FileManager = IFileManager::Get();
		if (FileManager.FileExists(*Path))
		{
			FileManager.Delete(*Path);
		}
	}

	FString SafeFileComponent(const FString& Value)
	{
		FString Result;
		Result.Reserve(Value.Len());
		for (TCHAR Char : Value)
		{
			const bool bSafe = FChar::IsAlnum(Char) && Char < 128;
			Result.AppendChar(bSafe || Char == TEXT('-') || Char == TEXT('_') ? Char : TEXT('_'));
		}
		return Result.IsEmpty() ? TEXT("_") : Result;
	}
}
