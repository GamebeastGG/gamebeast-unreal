// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"

#include "GamebeastIsCohortMemberAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGamebeastIsCohortMemberResult, bool, bIsMember);

/** Blueprint "Is Cohort Member" node: asks whether the local user is in a cohort. */
UCLASS()
class GAMEBEAST_API UGamebeastIsCohortMemberAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gamebeast|Cohorts",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Is Cohort Member"))
	static UGamebeastIsCohortMemberAction* IsCohortMember(UObject* WorldContextObject, const FString& CohortName);

	/** Fires once with the answer (false when unknown). */
	UPROPERTY(BlueprintAssignable)
	FGamebeastIsCohortMemberResult OnResult;

	virtual void Activate() override;

private:
	UPROPERTY()
	TObjectPtr<UObject> WorldContext;

	FString CohortName;
};
