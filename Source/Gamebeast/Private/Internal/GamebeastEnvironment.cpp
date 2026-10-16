// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#include "Internal/GamebeastEnvironment.h"

namespace Gamebeast::Environment
{
	FString ToAliasSlug(const FString& Value)
	{
		FString Result;
		Result.Reserve(Value.Len());

		bool bPendingDash = false;
		for (TCHAR Char : Value.ToLower())
		{
			if (FChar::IsWhitespace(Char) || Char == TEXT('-'))
			{
				bPendingDash = true;
				continue;
			}
			const bool bAllowed = (Char >= TEXT('a') && Char <= TEXT('z')) || (Char >= TEXT('0') && Char <= TEXT('9'));
			if (!bAllowed)
			{
				// Stripped characters don't separate words: "a!b" is "ab", like the backend.
				continue;
			}
			if (bPendingDash && !Result.IsEmpty())
			{
				Result.AppendChar(TEXT('-'));
			}
			bPendingDash = false;
			Result.AppendChar(Char);
		}
		return Result;
	}

	FString Resolve(EGamebeastEnvironment Environment, const FString& CustomEnvironment, bool bIsEditor)
	{
		const FString Custom = ToAliasSlug(CustomEnvironment);
		if (!Custom.IsEmpty())
		{
			return Custom == TEXT("studio") ? Development : Custom;
		}

		switch (Environment)
		{
		case EGamebeastEnvironment::Production:
			return Production;
		case EGamebeastEnvironment::Development:
			return Development;
		case EGamebeastEnvironment::Auto:
		default:
			return bIsEditor ? Development : Production;
		}
	}

	bool ToLegacyIsStudio(const FString& Alias, bool& bSupported)
	{
		if (Alias == Production)
		{
			bSupported = true;
			return false;
		}
		bSupported = Alias == Development;
		return true;
	}
}
