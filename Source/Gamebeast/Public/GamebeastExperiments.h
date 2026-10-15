// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#pragma once

#include "CoreMinimal.h"
#include "GamebeastTypes.h"
#include "UObject/Object.h"

#include "GamebeastExperiments.generated.h"

struct FGamebeastContext;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FGamebeastOnAssignmentsChangedNative, const TArray<FGamebeastExperimentAssignment>& /* Assignments */);

/**
 * The local user's experiment assignments. Read-only.
 *
 * Enrollment is automatic: loading a configuration evaluates active experiments for this user
 * on the backend, and the configuration values already include the assigned group's changes.
 * Use this service for display, segmentation and debugging.
 */
UCLASS(BlueprintType)
class GAMEBEAST_API UGamebeastExperiments : public UObject
{
	GENERATED_BODY()

public:
	/** Assignments across every configuration loaded so far, sorted by experiment id. */
	UFUNCTION(BlueprintPure, Category = "Gamebeast|Experiments")
	TArray<FGamebeastExperimentAssignment> GetAssignments() const { return Assignments; }

	UPROPERTY(BlueprintAssignable, Category = "Gamebeast|Experiments")
	FGamebeastOnAssignmentsChanged OnAssignmentsChanged;

	FGamebeastOnAssignmentsChangedNative OnAssignmentsChangedNative;

	/** Fed by UGamebeastConfigs: the `experiments` array of an evaluated configuration. */
	void ReportAssignments(const FString& ConfigurationAlias, const TArray<TSharedPtr<FJsonValue>>& Reported);

	/** Fed by UGamebeastConfigs: `experiments.user` from `/status`, for names and group labels. */
	void ApplyCatalog(const TArray<TSharedPtr<FJsonValue>>& Descriptors);

private:
	friend class UGamebeastSubsystem;

	struct FReported
	{
		int64 ExperimentId = 0;
		int64 GroupId = 0;
		int64 AssignmentVersion = 0;
		FString Source;

		bool operator==(const FReported& Other) const
		{
			return ExperimentId == Other.ExperimentId && GroupId == Other.GroupId
				&& AssignmentVersion == Other.AssignmentVersion;
		}
	};

	struct FCatalogEntry
	{
		FString Name;
		TMap<int64, FString> GroupLabels;
	};

	void Start(const TSharedRef<FGamebeastContext>& InContext);
	void OnIdentityChanged();
	void Stop();

	void Rebuild();

	TSharedPtr<FGamebeastContext> Context;
	TMap<FString, TArray<FReported>> ReportedByConfiguration;
	TMap<int64, FCatalogEntry> Catalog;
	TArray<FGamebeastExperimentAssignment> Assignments;
};
