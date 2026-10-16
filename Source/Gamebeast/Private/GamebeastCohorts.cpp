// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#include "GamebeastCohorts.h"

#include "HAL/PlatformTime.h"
#include "Internal/GamebeastContext.h"
#include "Internal/GamebeastLog.h"

namespace
{
	constexpr double CacheSeconds = 60.0;
}

void UGamebeastCohorts::Start(const TSharedRef<FGamebeastContext>& InContext)
{
	Context = InContext;
}

void UGamebeastCohorts::Stop()
{
	Context.Reset();
	Cache.Reset();
	// Anyone still waiting gets an answer rather than silence.
	TMap<FString, TArray<TFunction<void(bool)>>> Pending = MoveTemp(Waiting);
	Waiting.Reset();
	for (TPair<FString, TArray<TFunction<void(bool)>>>& Pair : Pending)
	{
		for (TFunction<void(bool)>& Callback : Pair.Value)
		{
			Callback(false);
		}
	}
	++Generation;
}

void UGamebeastCohorts::OnIdentityChanged()
{
	Cache.Reset();
	// Lookups in flight were for the previous user; their answers are discarded below.
	++Generation;
	TMap<FString, TArray<TFunction<void(bool)>>> Pending = MoveTemp(Waiting);
	Waiting.Reset();
	for (TPair<FString, TArray<TFunction<void(bool)>>>& Pair : Pending)
	{
		for (TFunction<void(bool)>& Callback : Pair.Value)
		{
			IsMember(Pair.Key, MoveTemp(Callback));
		}
	}
}

void UGamebeastCohorts::IsMember(const FString& CohortName, FGamebeastCohortResult Callback)
{
	IsMember(CohortName, [Callback = MoveTemp(Callback)](bool bIsMember) { Callback.ExecuteIfBound(bIsMember); });
}

void UGamebeastCohorts::IsMember(const FString& CohortName, TFunction<void(bool)> Callback)
{
	if (!Callback)
	{
		return;
	}
	if (!Context.IsValid() || !Context->Api.IsValid())
	{
		GB_WARN(TEXT("Cohort lookup '%s' skipped: call Setup first."), *CohortName);
		Callback(false);
		return;
	}
	if (CohortName.TrimStartAndEnd().IsEmpty())
	{
		GB_ERROR(TEXT("IsMember called without a cohort name."));
		Callback(false);
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (const FCachedResult* Cached = Cache.Find(CohortName))
	{
		if (Cached->ExpiresAt > Now)
		{
			Callback(Cached->bIsMember);
			return;
		}
		Cache.Remove(CohortName);
	}

	if (TArray<TFunction<void(bool)>>* InFlight = Waiting.Find(CohortName))
	{
		InFlight->Add(MoveTemp(Callback));
		return;
	}
	Waiting.Add(CohortName).Add(MoveTemp(Callback));

	TWeakObjectPtr<UGamebeastCohorts> WeakThis(this);
	const int32 RequestGeneration = Generation;
	Context->Api->CheckCohortMembership(CohortName, Context->DistinctId,
		[WeakThis, CohortName, RequestGeneration](const FGamebeastCohortLookup& Lookup)
		{
			UGamebeastCohorts* This = WeakThis.Get();
			if (This == nullptr || RequestGeneration != This->Generation)
			{
				return;
			}
			if (Lookup.bOk)
			{
				if (!Lookup.bCohortExists)
				{
					GB_WARN(TEXT("No cohort named '%s' exists; every player is reported as a non-member."), *CohortName);
				}
				FCachedResult& Cached = This->Cache.Add(CohortName);
				Cached.bIsMember = Lookup.bIsMember;
				Cached.ExpiresAt = FPlatformTime::Seconds() + CacheSeconds;
			}

			TArray<TFunction<void(bool)>> Callbacks;
			This->Waiting.RemoveAndCopyValue(CohortName, Callbacks);
			for (TFunction<void(bool)>& Callback : Callbacks)
			{
				Callback(Lookup.bOk && Lookup.bIsMember);
			}
		});
}
