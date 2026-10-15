// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "GamebeastTypes.h"
#include "Internal/GamebeastApiClient.h"

/** Shared state handed to every service when the SDK starts. Game thread only. */
struct FGamebeastContext
{
	FGamebeastSetupOptions Options;

	/** "production", "development", or a custom alias. */
	FString EnvironmentAlias;

	TSharedPtr<FGamebeastApiClient> Api;

	/** The id configurations are evaluated for and markers are attributed to. */
	FString DistinctId;

	/** Generated once per device and persisted; used when no id was supplied. */
	FString AnonymousId;

	/** `unreal-<uuid>`, rotated after a long background. */
	FString SessionId;

	/** Bumped on Identify/ResetIdentity so responses for the previous user are discarded. */
	int32 IdentityGeneration = 0;

	/** Targeting properties: device/app facts overlaid with Options.UserProperties. */
	TSharedRef<FJsonObject> BuildEvaluationProperties() const;

	/** The project version (Project Settings > Description), stamped on caches. */
	static FString AppVersion();

	/** Load the persisted anonymous id, or create and save one. */
	static FString LoadOrCreateAnonymousId();

	static FString NewSessionId();
};
