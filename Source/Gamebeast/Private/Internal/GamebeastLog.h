// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGamebeast, Log, All);

namespace Gamebeast::Private
{
	/** Set from FGamebeastSetupOptions::bDebugLogging. Info logs are silent unless it is on. */
	extern bool GDebugLogging;
}

#define GB_INFO(Format, ...) \
	do { if (::Gamebeast::Private::GDebugLogging) { UE_LOG(LogGamebeast, Log, Format, ##__VA_ARGS__); } } while (0)
#define GB_WARN(Format, ...) UE_LOG(LogGamebeast, Warning, Format, ##__VA_ARGS__)
#define GB_ERROR(Format, ...) UE_LOG(LogGamebeast, Error, Format, ##__VA_ARGS__)
