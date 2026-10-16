// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * The device and build facts sent under the `unreal` namespace of every evaluation context.
 *
 * These are built-in targeting fields on the backend (`unreal.platform`, `unreal.country`, ...),
 * registered in `@gamebeast/experiments` with a closed vocabulary where one exists. Every value is
 * normalized here into that vocabulary, with `unknown` when the device can't say, so a condition
 * written on the dashboard matches the same way on every platform. Changing a value's format is a
 * breaking change for experiments already targeting it.
 */
namespace Gamebeast::PlatformFacts
{
	/** `UGameplayStatics::GetPlatformName()` -> windows, mac, linux, ios, android, playstation, xbox, switch, unknown. */
	FString NormalizePlatform(const FString& EnginePlatformName);

	/** Platform slug -> pc, mobile, console, unknown. */
	FString DeviceForPlatform(const FString& PlatformSlug);

	/** A locale like `en-US`, `zh-Hans-CN` or `en_US.UTF-8` -> ISO 639-1 (`en`), or `unknown`. */
	FString LanguageFromLocale(const FString& Locale);

	/** A locale like `en-US`, `zh-Hans-CN` or `en_US.UTF-8` -> ISO 3166-1 alpha-2 (`US`), or `unknown`. */
	FString CountryFromLocale(const FString& Locale);

	/** The `unreal` namespace for this device and build. */
	TSharedRef<FJsonObject> Collect();
}
