// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#include "GamebeastExperiments.h"

#include "Internal/GamebeastContext.h"
#include "Internal/GamebeastLog.h"

void UGamebeastExperiments::Start(const TSharedRef<FGamebeastContext>& InContext)
{
	Context = InContext;
	// Cached configurations may have reported assignments already; names arrive with /status.
	if (ReportedByConfiguration.Num() > 0)
	{
		Rebuild();
	}
}

void UGamebeastExperiments::Stop()
{
	Context.Reset();
	ReportedByConfiguration.Reset();
	Catalog.Reset();
	Assignments.Reset();
}

void UGamebeastExperiments::OnIdentityChanged()
{
	// These belong to the previous user; configurations re-evaluated for the new one refill them.
	if (ReportedByConfiguration.Num() == 0)
	{
		return;
	}
	ReportedByConfiguration.Reset();
	Rebuild();
}

void UGamebeastExperiments::ReportAssignments(
	const FString& ConfigurationAlias, const TArray<TSharedPtr<FJsonValue>>& Reported)
{
	TArray<FReported> Parsed;
	for (const TSharedPtr<FJsonValue>& Value : Reported)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object))
		{
			continue;
		}
		FReported Entry;
		double Number = 0.0;
		if ((*Object)->TryGetNumberField(TEXT("experimentId"), Number))
		{
			Entry.ExperimentId = static_cast<int64>(Number);
		}
		if ((*Object)->TryGetNumberField(TEXT("groupId"), Number))
		{
			Entry.GroupId = static_cast<int64>(Number);
		}
		if ((*Object)->TryGetNumberField(TEXT("assignmentVersion"), Number))
		{
			Entry.AssignmentVersion = static_cast<int64>(Number);
		}
		(*Object)->TryGetStringField(TEXT("source"), Entry.Source);
		Parsed.Add(Entry);
	}

	if (const TArray<FReported>* Existing = ReportedByConfiguration.Find(ConfigurationAlias))
	{
		if (*Existing == Parsed)
		{
			return;
		}
	}
	ReportedByConfiguration.Add(ConfigurationAlias, MoveTemp(Parsed));
	Rebuild();
}

void UGamebeastExperiments::ApplyCatalog(const TArray<TSharedPtr<FJsonValue>>& Descriptors)
{
	TMap<int64, FCatalogEntry> Next;
	for (const TSharedPtr<FJsonValue>& Value : Descriptors)
	{
		const TSharedPtr<FJsonObject>* Experiment = nullptr;
		double Id = 0.0;
		if (!Value.IsValid() || !Value->TryGetObject(Experiment) || !(*Experiment)->TryGetNumberField(TEXT("id"), Id))
		{
			continue;
		}
		FCatalogEntry& Entry = Next.Add(static_cast<int64>(Id));
		(*Experiment)->TryGetStringField(TEXT("name"), Entry.Name);

		const TArray<TSharedPtr<FJsonValue>>* Groups = nullptr;
		if ((*Experiment)->TryGetArrayField(TEXT("groups"), Groups))
		{
			for (const TSharedPtr<FJsonValue>& GroupValue : *Groups)
			{
				const TSharedPtr<FJsonObject>* Group = nullptr;
				double GroupId = 0.0;
				FString Label;
				if (GroupValue.IsValid() && GroupValue->TryGetObject(Group) && (*Group)->TryGetNumberField(TEXT("id"), GroupId)
					&& (*Group)->TryGetStringField(TEXT("label"), Label))
				{
					Entry.GroupLabels.Add(static_cast<int64>(GroupId), Label);
				}
			}
		}
	}

	Catalog = MoveTemp(Next);
	GB_INFO(TEXT("Experiment catalog updated (%d active)."), Catalog.Num());
	if (ReportedByConfiguration.Num() > 0)
	{
		Rebuild();
	}
}

void UGamebeastExperiments::Rebuild()
{
	TArray<FGamebeastExperimentAssignment> Next;
	for (const TPair<FString, TArray<FReported>>& Pair : ReportedByConfiguration)
	{
		for (const FReported& Entry : Pair.Value)
		{
			FGamebeastExperimentAssignment Assignment;
			Assignment.ExperimentId = Entry.ExperimentId;
			Assignment.GroupId = Entry.GroupId;
			Assignment.ConfigurationAlias = Pair.Key;
			Assignment.Source = Entry.Source;
			if (const FCatalogEntry* Experiment = Catalog.Find(Entry.ExperimentId))
			{
				Assignment.ExperimentName = Experiment->Name;
				if (const FString* Label = Experiment->GroupLabels.Find(Entry.GroupId))
				{
					Assignment.GroupLabel = *Label;
				}
			}
			Next.Add(MoveTemp(Assignment));
		}
	}
	Next.Sort([](const FGamebeastExperimentAssignment& A, const FGamebeastExperimentAssignment& B)
	{
		return A.ExperimentId != B.ExperimentId ? A.ExperimentId < B.ExperimentId
			: A.ConfigurationAlias < B.ConfigurationAlias;
	});

	Assignments = MoveTemp(Next);
	OnAssignmentsChangedNative.Broadcast(Assignments);
	OnAssignmentsChanged.Broadcast(Assignments);
}
