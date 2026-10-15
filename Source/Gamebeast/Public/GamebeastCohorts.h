// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#pragma once

#include "CoreMinimal.h"
#include "GamebeastTypes.h"
#include "UObject/Object.h"

#include "GamebeastCohorts.generated.h"

struct FGamebeastContext;

/**
 * Cohorts: player segments computed from analytics on the dashboard (e.g. "High Spenders").
 *
 * Results are cached for a minute, and concurrent lookups of the same cohort share one request.
 * Membership is recomputed by the backend, so treat it as eventually consistent.
 * In Blueprint, use the "Is Cohort Member" async node.
 */
UCLASS(BlueprintType)
class GAMEBEAST_API UGamebeastCohorts : public UObject
{
	GENERATED_BODY()

public:
	/** Whether the local user is in the cohort. Calls back with false when unknown (offline, no such cohort). */
	void IsMember(const FString& CohortName, FGamebeastCohortResult Callback);

	/** Same, with a lambda. */
	void IsMember(const FString& CohortName, TFunction<void(bool /* bIsMember */)> Callback);

private:
	friend class UGamebeastSubsystem;

	struct FCachedResult
	{
		bool bIsMember = false;
		double ExpiresAt = 0.0;
	};

	void Start(const TSharedRef<FGamebeastContext>& InContext);
	void OnIdentityChanged();
	void Stop();

	TSharedPtr<FGamebeastContext> Context;
	TMap<FString, FCachedResult> Cache;
	TMap<FString, TArray<TFunction<void(bool)>>> Waiting;
	int32 Generation = 0;
};
