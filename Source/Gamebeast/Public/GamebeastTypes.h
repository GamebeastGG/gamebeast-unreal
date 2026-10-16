// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "GamebeastTypes.generated.h"

/**
 * A JSON object of marker or targeting properties.
 *
 * C++ builds it fluently:
 *   FGamebeastProperties().Set(TEXT("Score"), 600).Set(TEXT("Map"), TEXT("Office"))
 * Blueprint builds it with the "Make Gamebeast Properties" and "Set ..." nodes.
 *
 * Copies are cheap and independent: the first write after a copy clones the underlying object.
 */
USTRUCT(BlueprintType)
struct GAMEBEAST_API FGamebeastProperties
{
	GENERATED_BODY()

	FGamebeastProperties() = default;
	explicit FGamebeastProperties(const TSharedPtr<FJsonObject>& InJson);

	FGamebeastProperties& Set(const FString& Key, int32 Value);
	FGamebeastProperties& Set(const FString& Key, int64 Value);
	FGamebeastProperties& Set(const FString& Key, float Value);
	FGamebeastProperties& Set(const FString& Key, double Value);
	FGamebeastProperties& Set(const FString& Key, bool Value);
	FGamebeastProperties& Set(const FString& Key, const TCHAR* Value);
	FGamebeastProperties& Set(const FString& Key, const FString& Value);
	FGamebeastProperties& Set(const FString& Key, const FName& Value);
	/** Serialized as `[x, y, z]`, the shape heatmap queries read positions in. */
	FGamebeastProperties& Set(const FString& Key, const FVector& Value);
	FGamebeastProperties& Set(const FString& Key, const FVector2D& Value);
	FGamebeastProperties& Set(const FString& Key, const FGamebeastProperties& Nested);
	FGamebeastProperties& Set(const FString& Key, const TArray<FString>& Values);
	FGamebeastProperties& Set(const FString& Key, const TArray<double>& Values);
	FGamebeastProperties& SetJson(const FString& Key, const TSharedPtr<FJsonValue>& Value);
	FGamebeastProperties& Remove(const FString& Key);

	bool IsEmpty() const;
	int32 Num() const;

	/** The underlying object (never null). Treat it as read-only. */
	TSharedRef<FJsonObject> ToJson() const;

private:
	FJsonObject& Mutable();

	TSharedPtr<FJsonObject> Json;
};

/** One experiment the local user is enrolled in, as reported on evaluated configurations. */
USTRUCT(BlueprintType)
struct GAMEBEAST_API FGamebeastExperimentAssignment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gamebeast|Experiments")
	int64 ExperimentId = 0;

	/** Empty until the first `/status` snapshot has loaded. */
	UPROPERTY(BlueprintReadOnly, Category = "Gamebeast|Experiments")
	FString ExperimentName;

	UPROPERTY(BlueprintReadOnly, Category = "Gamebeast|Experiments")
	int64 GroupId = 0;

	/** Empty until the first `/status` snapshot has loaded. */
	UPROPERTY(BlueprintReadOnly, Category = "Gamebeast|Experiments")
	FString GroupLabel;

	/** The configuration whose evaluation reported this assignment. */
	UPROPERTY(BlueprintReadOnly, Category = "Gamebeast|Experiments")
	FString ConfigurationAlias;

	/** `weightedHash`, `roundRobin` or `manual`. */
	UPROPERTY(BlueprintReadOnly, Category = "Gamebeast|Experiments")
	FString Source;
};

/**
 * How often the SDK talks to Gamebeast, in seconds. Values below a rate's minimum are raised to
 * it (with a warning); the same names and minimums as the Roblox SDK.
 */
USTRUCT(BlueprintType)
struct GAMEBEAST_API FGamebeastRates
{
	GENERATED_BODY()

	/** How often queued markers are sent. A full batch (10 markers) is sent immediately. At least 5. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gamebeast", meta = (ClampMin = "5", Units = "s"))
	float MarkerFlushRate = 30.f;

	/**
	 * How often the SDK checks for configuration and experiment changes (`GET /sdk/v2/status`, which
	 * costs almost nothing while nothing changed). During an incident Gamebeast can ask the SDK to
	 * check less often, never more often. At least 5.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gamebeast", meta = (ClampMin = "5", Units = "s"))
	float StatusPollRate = 30.f;

	/**
	 * How often configurations with a running experiment are re-evaluated for the local user. This
	 * is how a per-player change made on the dashboard (moving a player to another group, a staged
	 * rollout reaching them) arrives mid-session. At least 10.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gamebeast", meta = (ClampMin = "10", Units = "s"))
	float AssignmentRefreshRate = 60.f;
};

/** Options for UGamebeastSubsystem::SetupWithOptions. Project Settings fill the same fields. */
USTRUCT(BlueprintType)
struct GAMEBEAST_API FGamebeastSetupOptions
{
	GENERATED_BODY()

	/** Your project's SDK Key (Project Settings > API Keys in the dashboard). Required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gamebeast")
	FString ApiKey;

	/** The local user's id. Leave empty to use an anonymous id generated once per device. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gamebeast")
	FString DistinctId;

	/**
	 * The environment to report to, by its dashboard alias ("production", "development",
	 * "staging", ...). Leave empty to choose automatically: development in the editor (including
	 * Play In Editor), production in packaged builds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gamebeast")
	FString Environment;

	/** Configurations to preload at startup; IsReady/OnReady wait for these. Others load on first use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gamebeast")
	TArray<FString> Configurations;

	/** Custom targeting properties for experiments (strings, numbers, booleans). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gamebeast")
	FGamebeastProperties UserProperties;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gamebeast")
	FGamebeastRates Rates;

	/**
	 * In the editor (Play In Editor included), use each rate's minimum instead of Rates, so changes
	 * show up quickly while you test: markers every 5s, status every 5s, assignments every 10s.
	 * Turn it off to test your own Rates in the editor. Packaged builds always use Rates.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gamebeast")
	bool bFastRatesInEditor = true;

	/** Start a new session after this long in the background. 0 keeps one session per run. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gamebeast", meta = (ClampMin = "0"))
	float SessionTimeoutSeconds = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gamebeast")
	bool bDebugLogging = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gamebeast", AdvancedDisplay)
	FString ApiUrl = TEXT("https://api.gamebeast.gg");
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGamebeastOnReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGamebeastOnConfigurationUpdated, const FString&, ConfigurationAlias);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FGamebeastOnAssignmentsChanged, const TArray<FGamebeastExperimentAssignment>&, Assignments);

/** C++ callback for UGamebeastConfigs::OnChanged / Observe. The value is null when the path is missing. */
DECLARE_DELEGATE_OneParam(FGamebeastConfigValueCallback, const TSharedPtr<FJsonValue>& /* Value */);
/** C++ callback for UGamebeastCohorts::IsMember. */
DECLARE_DELEGATE_OneParam(FGamebeastCohortResult, bool /* bIsMember */);
