// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GamebeastTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "GamebeastSubsystem.generated.h"

class UGamebeastCohorts;
class UGamebeastConfigs;
class UGamebeastExperiments;
class UGamebeastMarkers;
struct FGamebeastContext;

/**
 * The Gamebeast SDK entry point. One per game instance (so one per Play In Editor session).
 *
 * C++:       UGamebeastSubsystem::Get(this)->GetMarkers()->SendMarker(...)
 * Blueprint: the "Gamebeast Subsystem" node, then Get Markers / Get Configs / ...
 *
 * With an API key in Project Settings > Plugins > Gamebeast the SDK starts itself on the first
 * frame. Calling Setup or SetupWithOptions before then (e.g. in your GameInstance's Init) takes
 * precedence, which is how you pass a DistinctId or UserProperties.
 */
UCLASS()
class GAMEBEAST_API UGamebeastSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem for WorldContextObject's game instance, or null outside a game. */
	static UGamebeastSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Start the SDK with an API key and the rest of Project Settings. False if it already started. */
	UFUNCTION(BlueprintCallable, Category = "Gamebeast")
	bool Setup(const FString& ApiKey);

	/** Start the SDK with explicit options. False if it already started or the options are invalid. */
	UFUNCTION(BlueprintCallable, Category = "Gamebeast")
	bool SetupWithOptions(const FGamebeastSetupOptions& Options);

	/** Save what's buffered and stop. Setup may be called again afterwards. */
	UFUNCTION(BlueprintCallable, Category = "Gamebeast")
	void Shutdown();

	UFUNCTION(BlueprintPure, Category = "Gamebeast")
	bool IsInitialized() const;

	/** "production", "development", or a custom alias. Empty before Setup. */
	UFUNCTION(BlueprintPure, Category = "Gamebeast")
	FString GetEnvironment() const;

	/** The id markers are attributed to and configurations evaluated for. Empty before Setup. */
	UFUNCTION(BlueprintPure, Category = "Gamebeast")
	FString GetDistinctId() const;

	UFUNCTION(BlueprintPure, Category = "Gamebeast")
	FString GetSessionId() const;

	/**
	 * Change the rates while running. Values below a minimum are raised to it with a warning.
	 * Overrides the editor minimums (bFastRatesInEditor) for the rest of the session.
	 */
	UFUNCTION(BlueprintCallable, Category = "Gamebeast")
	void UpdateRates(const FGamebeastRates& Rates);

	/** The rates in effect (after validation and editor minimums). Defaults before Setup. */
	UFUNCTION(BlueprintPure, Category = "Gamebeast")
	FGamebeastRates GetRates() const;

	/**
	 * Switch to a known user id (after login). Configurations re-evaluate for them right away.
	 * Not persisted: call it again on the next launch once your login state is restored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Gamebeast")
	void Identify(const FString& DistinctId);

	/** Go back to the device's anonymous id (after logout). */
	UFUNCTION(BlueprintCallable, Category = "Gamebeast")
	void ResetIdentity();

	UFUNCTION(BlueprintPure, Category = "Gamebeast")
	UGamebeastMarkers* GetMarkers() const { return Markers; }

	UFUNCTION(BlueprintPure, Category = "Gamebeast")
	UGamebeastConfigs* GetConfigs() const { return Configs; }

	UFUNCTION(BlueprintPure, Category = "Gamebeast")
	UGamebeastExperiments* GetExperiments() const { return Experiments; }

	UFUNCTION(BlueprintPure, Category = "Gamebeast")
	UGamebeastCohorts* GetCohorts() const { return Cohorts; }

private:
	bool HandleTick(float DeltaSeconds);
	void HandleWillEnterBackground();
	void HandleHasEnteredForeground();
	void SetIdentity(const FString& DistinctId);

	UPROPERTY()
	TObjectPtr<UGamebeastMarkers> Markers;

	UPROPERTY()
	TObjectPtr<UGamebeastConfigs> Configs;

	UPROPERTY()
	TObjectPtr<UGamebeastExperiments> Experiments;

	UPROPERTY()
	TObjectPtr<UGamebeastCohorts> Cohorts;

	TSharedPtr<FGamebeastContext> Context;
	FTSTicker::FDelegateHandle TickHandle;
	bool bAutoInitializePending = false;
	bool bInBackground = false;
	double BackgroundedAt = 0.0;
};
