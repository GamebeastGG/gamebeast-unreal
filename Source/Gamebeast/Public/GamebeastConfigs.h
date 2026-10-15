// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#pragma once

#include "CoreMinimal.h"
#include "GamebeastTypes.h"
#include "UObject/Object.h"

#include "GamebeastConfigs.generated.h"

class UGamebeastExperiments;
class FGamebeastStatusPoller;
struct FGamebeastContext;

/**
 * Remote configurations, evaluated for the local user.
 *
 * Values are addressed by dot-path. The first segment is the configuration alias, the rest walks
 * into the document: "MyConfig.PlayerSpeed", "MyConfig.Shop.Prices.0". A bare alias is the whole
 * document. If the user is in an experiment, values already include their group's changes.
 *
 * Configurations listed in Setup (or Project Settings) load at startup and gate IsReady/OnReady.
 * Any other configuration loads the first time it is read. Every configuration is cached on the
 * device, so values are there instantly on the next launch (and offline); a cache written by a
 * different build or for a different user is discarded.
 */
UCLASS(BlueprintType)
class GAMEBEAST_API UGamebeastConfigs : public UObject
{
	GENERATED_BODY()

public:
	/** True once every configuration listed in Setup has loaded (from the network or the cache). */
	UFUNCTION(BlueprintPure, Category = "Gamebeast|Configs")
	bool IsReady() const { return bReady; }

	/** Whether the path resolves to a value right now. */
	UFUNCTION(BlueprintCallable, Category = "Gamebeast|Configs")
	bool HasValue(const FString& Path);

	/**
	 * Typed reads. DefaultValue is both a fallback and a type check, as in the other SDKs: it is
	 * returned when the configuration hasn't loaded, the path is missing or null, or the value is a
	 * different JSON type (a string where a number was expected). Pass the value your game uses
	 * today, so it behaves the same offline. Each problem is logged once, not on every read.
	 */
	UFUNCTION(BlueprintCallable, Category = "Gamebeast|Configs")
	FString GetString(const FString& Path, const FString& DefaultValue);

	UFUNCTION(BlueprintCallable, Category = "Gamebeast|Configs")
	int32 GetInt(const FString& Path, int32 DefaultValue);

	UFUNCTION(BlueprintCallable, Category = "Gamebeast|Configs")
	float GetFloat(const FString& Path, float DefaultValue);

	UFUNCTION(BlueprintCallable, Category = "Gamebeast|Configs")
	bool GetBool(const FString& Path, bool DefaultValue);

	/** The value as JSON text (objects and arrays included), or DefaultValue. */
	UFUNCTION(BlueprintCallable, Category = "Gamebeast|Configs")
	FString GetJsonString(const FString& Path, const FString& DefaultValue);

	/** The raw value, or null when the configuration hasn't loaded or the path is missing. */
	TSharedPtr<FJsonValue> GetJson(const FString& Path);

	/** Start loading a configuration that wasn't listed in Setup, without reading from it yet. */
	UFUNCTION(BlueprintCallable, Category = "Gamebeast|Configs")
	void Preload(const FString& ConfigurationAlias);

	/**
	 * Check for changes now instead of waiting: polls `/status` and re-evaluates every known
	 * configuration for the local user (unchanged ones answer with a cheap "not modified").
	 */
	UFUNCTION(BlueprintCallable, Category = "Gamebeast|Configs")
	void Refresh();

	/** Runs Callback once IsReady is true (immediately if it already is). */
	void OnReady(TFunction<void()> Callback);

	/** Calls Callback with the new value whenever the value at Path changes. Returns a handle for Unsubscribe. */
	int64 OnChanged(const FString& Path, FGamebeastConfigValueCallback Callback);

	/** Like OnChanged, but also calls Callback with the current value as soon as there is one. */
	int64 Observe(const FString& Path, FGamebeastConfigValueCallback Callback);

	void Unsubscribe(int64 Handle);

	/** Fires once, when IsReady becomes true. Bind before Setup, or check IsReady first. */
	UPROPERTY(BlueprintAssignable, Category = "Gamebeast|Configs")
	FGamebeastOnReady OnReadyEvent;

	/** Fires each time a configuration loads with new values. Read the values you need again. */
	UPROPERTY(BlueprintAssignable, Category = "Gamebeast|Configs")
	FGamebeastOnConfigurationUpdated OnConfigurationUpdated;

private:
	friend class UGamebeastSubsystem;

	struct FConfigState
	{
		FString Alias;
		TSharedPtr<FJsonValue> Values;
		FString Hash;
		bool bLoaded = false;
		bool bInFlight = false;
		/** Another refresh was asked for while one was in flight (e.g. after Identify). */
		bool bRefreshQueued = false;
		bool bRequiredForReady = false;
	};

	struct FListener
	{
		int64 Id = 0;
		FString Alias;
		TArray<FString> Segments;
		FGamebeastConfigValueCallback Callback;
		bool bIncludeInitial = false;
	};

	void Start(const TSharedRef<FGamebeastContext>& InContext, UGamebeastExperiments* InExperiments);
	void Tick(float DeltaSeconds);
	void OnForeground();
	void OnIdentityChanged();
	/** Context->Rates changed (UGamebeastSubsystem::UpdateRates). */
	void OnRatesChanged();
	void Stop();

	FConfigState* EnsureConfig(const FString& Alias, bool bRequiredForReady);
	void RefreshConfig(const FString& Alias);
	void RefreshAll();
	void RefreshAssignments();
	void HandleStatus(const TSharedRef<FJsonObject>& Status);
	void ApplyResponse(FConfigState& State, const TSharedPtr<FJsonObject>& Response, bool bPersist);
	void CheckReady();
	TSharedPtr<FJsonValue> Lookup(const FString& Path, bool bLogMissing);
	/** Lookup, or null (warned once per path) when the value has a different JSON type. */
	TSharedPtr<FJsonValue> LookupTyped(const FString& Path, EJson Expected, const TCHAR* TypeName);
	int64 AddListener(const FString& Path, FGamebeastConfigValueCallback Callback, bool bIncludeInitial);

	FString CacheFileName(const FString& Alias) const;
	TSharedPtr<FJsonObject> LoadCache(const FString& Alias) const;
	void SaveCache(const FString& Alias, const TSharedPtr<FJsonObject>& Response) const;

	TSharedPtr<FGamebeastContext> Context;
	TWeakObjectPtr<UGamebeastExperiments> Experiments;
	/** Shared rather than unique so this public header needn't see the internal type. */
	TSharedPtr<FGamebeastStatusPoller> StatusPoller;

	/** FString keys compare case-insensitively, like configuration aliases. */
	TMap<FString, FConfigState> Configs;
	TArray<FListener> Listeners;
	TArray<TFunction<void()>> ReadyCallbacks;
	int64 NextListenerId = 1;
	bool bReady = false;
	float SecondsSinceAssignmentRefresh = 0.f;
	float SecondsSinceUnloadedRetry = 0.f;

	/** From the last `/status` snapshot, keyed by normalized alias. Empty until one loads. */
	TMap<FString, FString> SnapshotFingerprints;
	TSet<FString> SnapshotWithUserExperiments;
	bool bHaveSnapshot = false;

	/** Warnings already logged, so per-frame reads don't flood the log. Aliases / paths. */
	TSet<FString> WarnedNotLoaded;
	TSet<FString> WarnedTypeMismatch;
};
