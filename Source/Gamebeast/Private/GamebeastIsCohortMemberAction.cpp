// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#include "GamebeastIsCohortMemberAction.h"

#include "GamebeastCohorts.h"
#include "GamebeastSubsystem.h"

UGamebeastIsCohortMemberAction* UGamebeastIsCohortMemberAction::IsCohortMember(
	UObject* WorldContextObject, const FString& CohortName)
{
	UGamebeastIsCohortMemberAction* Action = NewObject<UGamebeastIsCohortMemberAction>();
	Action->WorldContext = WorldContextObject;
	Action->CohortName = CohortName;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UGamebeastIsCohortMemberAction::Activate()
{
	UGamebeastSubsystem* Gamebeast = UGamebeastSubsystem::Get(WorldContext);
	if (Gamebeast == nullptr)
	{
		OnResult.Broadcast(false);
		SetReadyToDestroy();
		return;
	}

	TWeakObjectPtr<UGamebeastIsCohortMemberAction> WeakThis(this);
	Gamebeast->GetCohorts()->IsMember(CohortName, [WeakThis](bool bIsMember)
	{
		if (UGamebeastIsCohortMemberAction* This = WeakThis.Get())
		{
			This->OnResult.Broadcast(bIsMember);
			This->SetReadyToDestroy();
		}
	});
}
