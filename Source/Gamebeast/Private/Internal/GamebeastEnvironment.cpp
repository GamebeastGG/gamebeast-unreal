// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

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

	FString Resolve(const FString& Environment, bool bIsEditor, bool& bOutUnsupported)
	{
		bOutUnsupported = false;
		const FString Automatic = bIsEditor ? Development : Production;
		if (Environment.TrimStartAndEnd().IsEmpty())
		{
			return Automatic;
		}
		const FString Alias = ToAliasSlug(Environment);
		if (Alias == Production)
		{
			return Production;
		}
		if (Alias == Development || Alias == TEXT("studio"))
		{
			return Development;
		}
		// TODO(post-release): custom environments. Pass other aliases through once they exist.
		bOutUnsupported = true;
		return Automatic;
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
