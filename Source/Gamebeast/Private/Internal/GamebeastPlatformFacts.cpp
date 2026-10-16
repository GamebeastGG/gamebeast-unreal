// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#include "Internal/GamebeastPlatformFacts.h"

#include "GeneralProjectSettings.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/EngineVersion.h"

namespace
{
	const TCHAR* Unknown = TEXT("unknown");

	bool IsAsciiLetters(const FString& Value, int32 Length)
	{
		if (Value.Len() != Length)
		{
			return false;
		}
		for (const TCHAR Char : Value)
		{
			if (!((Char >= TEXT('a') && Char <= TEXT('z')) || (Char >= TEXT('A') && Char <= TEXT('Z'))))
			{
				return false;
			}
		}
		return true;
	}

	/** `en_US.UTF-8@euro` -> [en, US]. */
	TArray<FString> LocaleSegments(const FString& Locale)
	{
		FString Trimmed = Locale.TrimStartAndEnd();
		int32 Cut = INDEX_NONE;
		if (Trimmed.FindChar(TEXT('.'), Cut) || Trimmed.FindChar(TEXT('@'), Cut))
		{
			Trimmed.LeftInline(Cut);
		}
		Trimmed.ReplaceCharInline(TEXT('_'), TEXT('-'));

		TArray<FString> Segments;
		Trimmed.ParseIntoArray(Segments, TEXT("-"), /*InCullEmpty*/ true);
		return Segments;
	}
}

namespace Gamebeast::PlatformFacts
{
	FString NormalizePlatform(const FString& EnginePlatformName)
	{
		const FString Name = EnginePlatformName.TrimStartAndEnd();
		if (Name.Equals(TEXT("Windows"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("WinGDK"), ESearchCase::IgnoreCase))
		{
			return TEXT("windows");
		}
		if (Name.Equals(TEXT("Mac"), ESearchCase::IgnoreCase))
		{
			return TEXT("mac");
		}
		if (Name.StartsWith(TEXT("Linux"), ESearchCase::IgnoreCase))
		{
			return TEXT("linux");
		}
		if (Name.Equals(TEXT("IOS"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("TVOS"), ESearchCase::IgnoreCase)
			|| Name.Equals(TEXT("VisionOS"), ESearchCase::IgnoreCase))
		{
			return TEXT("ios");
		}
		if (Name.Equals(TEXT("Android"), ESearchCase::IgnoreCase))
		{
			return TEXT("android");
		}
		// PS4, PS5.
		if (Name.StartsWith(TEXT("PS"), ESearchCase::IgnoreCase) && Name.Len() <= 3)
		{
			return TEXT("playstation");
		}
		// XboxOne, XboxOneGDK, XSX.
		if (Name.StartsWith(TEXT("Xbox"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("XSX"), ESearchCase::IgnoreCase))
		{
			return TEXT("xbox");
		}
		if (Name.StartsWith(TEXT("Switch"), ESearchCase::IgnoreCase))
		{
			return TEXT("switch");
		}
		return Unknown;
	}

	FString DeviceForPlatform(const FString& PlatformSlug)
	{
		if (PlatformSlug == TEXT("windows") || PlatformSlug == TEXT("mac") || PlatformSlug == TEXT("linux"))
		{
			return TEXT("pc");
		}
		if (PlatformSlug == TEXT("ios") || PlatformSlug == TEXT("android"))
		{
			return TEXT("mobile");
		}
		if (PlatformSlug == TEXT("playstation") || PlatformSlug == TEXT("xbox") || PlatformSlug == TEXT("switch"))
		{
			return TEXT("console");
		}
		return Unknown;
	}

	FString LanguageFromLocale(const FString& Locale)
	{
		const TArray<FString> Segments = LocaleSegments(Locale);
		if (Segments.Num() == 0 || !IsAsciiLetters(Segments[0], 2))
		{
			return Unknown;
		}
		return Segments[0].ToLower();
	}

	FString CountryFromLocale(const FString& Locale)
	{
		const TArray<FString> Segments = LocaleSegments(Locale);
		// The region follows the language and an optional four-letter script: en-US, zh-Hans-CN.
		for (int32 Index = 1; Index < Segments.Num(); ++Index)
		{
			if (IsAsciiLetters(Segments[Index], 2))
			{
				return Segments[Index].ToUpper();
			}
		}
		return Unknown;
	}

	TSharedRef<FJsonObject> Collect()
	{
		const FString Platform = NormalizePlatform(UGameplayStatics::GetPlatformName());
		// The device's own settings, not the game's active culture, which the game may have changed.
		const FString Locale = FPlatformMisc::GetDefaultLocale();
		const UGeneralProjectSettings* ProjectSettings = GetDefault<UGeneralProjectSettings>();

		const TSharedRef<FJsonObject> Facts = MakeShared<FJsonObject>();
		Facts->SetStringField(TEXT("platform"), Platform);
		Facts->SetStringField(TEXT("device"), DeviceForPlatform(Platform));
		Facts->SetStringField(TEXT("country"), CountryFromLocale(Locale));
		Facts->SetStringField(TEXT("language"), LanguageFromLocale(FPlatformMisc::GetDefaultLanguage()));
		Facts->SetStringField(TEXT("appVersion"), ProjectSettings != nullptr ? ProjectSettings->ProjectVersion : FString());
		Facts->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString(EVersionComponent::Patch));
		return Facts;
	}
}
