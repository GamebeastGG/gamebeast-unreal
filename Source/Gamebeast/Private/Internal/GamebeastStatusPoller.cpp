// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#include "Internal/GamebeastStatusPoller.h"

#include "Internal/GamebeastApiClient.h"
#include "Internal/GamebeastEnvironment.h"
#include "Internal/GamebeastJson.h"
#include "Internal/GamebeastLog.h"

FGamebeastStatusPoller::FGamebeastStatusPoller(
	const TSharedRef<FGamebeastApiClient>& InApi, float InPollRate, FOnSnapshot InOnSnapshot)
	: Api(InApi)
	, OnSnapshot(MoveTemp(InOnSnapshot))
	, PollRate(InPollRate)
{
}

FGamebeastStatusPoller::~FGamebeastStatusPoller()
{
	*Alive = false;
}

void FGamebeastStatusPoller::Tick(float DeltaSeconds)
{
	if (bInFlight)
	{
		return;
	}
	SecondsUntilPoll -= DeltaSeconds;
	if (SecondsUntilPoll <= 0.f)
	{
		Poll();
	}
}

void FGamebeastStatusPoller::PollNow()
{
	if (bInFlight)
	{
		// The response in flight may predate the change the caller cares about: ask again after it.
		bPollQueued = true;
		return;
	}
	Poll();
}

Gamebeast::Rates::FPollingOverride FGamebeastStatusPoller::ParseOverride(const FJsonObject& Status)
{
	Gamebeast::Rates::FPollingOverride Parsed;
	const TSharedPtr<FJsonObject>* Polling = nullptr;
	double Interval = 0.0;
	double Jitter = 0.0;
	if (Status.TryGetObjectField(TEXT("polling"), Polling) && (*Polling)->TryGetNumberField(TEXT("intervalSeconds"), Interval)
		&& Interval > 0.0)
	{
		Parsed.bPresent = true;
		Parsed.IntervalSeconds = static_cast<float>(Interval);
		if ((*Polling)->TryGetNumberField(TEXT("jitterRatio"), Jitter) && Jitter >= 0.0 && Jitter <= 1.0)
		{
			Parsed.JitterRatio = static_cast<float>(Jitter);
		}
	}
	return Parsed;
}

FGamebeastStatusPoller::FSummary FGamebeastStatusPoller::Summarize(const FJsonObject& Status)
{
	// Base configuration id -> serialized descriptors of the user experiments built on it.
	TMap<int64, FString> ExperimentsByBase;
	const TSharedPtr<FJsonObject>* Experiments = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* UserExperiments = nullptr;
	if (Status.TryGetObjectField(TEXT("experiments"), Experiments)
		&& (*Experiments)->TryGetArrayField(TEXT("user"), UserExperiments))
	{
		for (const TSharedPtr<FJsonValue>& Value : *UserExperiments)
		{
			const TSharedPtr<FJsonObject>* Experiment = nullptr;
			double BaseId = 0.0;
			if (Value.IsValid() && Value->TryGetObject(Experiment)
				&& (*Experiment)->TryGetNumberField(TEXT("baseConfigurationId"), BaseId))
			{
				// Arrays arrive ordered by id, so concatenation is stable across polls.
				ExperimentsByBase.FindOrAdd(static_cast<int64>(BaseId)) +=
					Gamebeast::Json::Serialize((*Experiment).ToSharedRef());
			}
		}
	}

	FSummary Summary;
	const TArray<TSharedPtr<FJsonValue>>* Configurations = nullptr;
	if (!Status.TryGetArrayField(TEXT("configurations"), Configurations))
	{
		return Summary;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Configurations)
	{
		const TSharedPtr<FJsonObject>* Configuration = nullptr;
		FString Alias;
		FString Hash;
		double Id = 0.0;
		if (!Value.IsValid() || !Value->TryGetObject(Configuration)
			|| !(*Configuration)->TryGetStringField(TEXT("alias"), Alias)
			|| !(*Configuration)->TryGetStringField(TEXT("hash"), Hash)
			|| !(*Configuration)->TryGetNumberField(TEXT("id"), Id))
		{
			continue;
		}
		const FString Slug = Gamebeast::Environment::ToAliasSlug(Alias);
		if (Slug.IsEmpty())
		{
			continue;
		}
		FString Fingerprint = Hash;
		if (const FString* Attached = ExperimentsByBase.Find(static_cast<int64>(Id)))
		{
			Fingerprint += TEXT("|") + *Attached;
			Summary.WithUserExperiments.Add(Slug);
		}
		Summary.Fingerprints.Add(Slug, MoveTemp(Fingerprint));
	}
	return Summary;
}

void FGamebeastStatusPoller::Poll()
{
	const TSharedPtr<FGamebeastApiClient> Client = Api.Pin();
	if (!Client.IsValid())
	{
		return;
	}
	bInFlight = true;
	bPollQueued = false;

	TWeakPtr<bool> WeakAlive = Alive;
	Client->GetStatus(KnownHash,
		[this, WeakAlive](const FGamebeastFetchResult& Result)
		{
			const TSharedPtr<bool> StillAlive = WeakAlive.Pin();
			if (!StillAlive.IsValid() || !*StillAlive)
			{
				return;
			}
			bInFlight = false;

			switch (Result.Status)
			{
			case EGamebeastFetchStatus::Updated:
				ConsecutiveFailures = 0;
				KnownHash = Result.Hash;
				if (Result.Body.IsValid())
				{
					const Gamebeast::Rates::FPollingOverride Next = ParseOverride(*Result.Body);
					if (Next.bPresent && (!Override.bPresent || Next.IntervalSeconds != Override.IntervalSeconds))
					{
						GB_WARN(TEXT("Gamebeast asked the SDK to check for changes at most every %.0fs."), Next.IntervalSeconds);
					}
					Override = Next;
					if (OnSnapshot)
					{
						// A copy: the callback may destroy this poller (Shutdown from a listener).
						const FOnSnapshot Callback = OnSnapshot;
						Callback(Result.Body.ToSharedRef());
					}
				}
				break;
			case EGamebeastFetchStatus::NotModified:
				ConsecutiveFailures = 0;
				break;
			case EGamebeastFetchStatus::NotFound:
			case EGamebeastFetchStatus::Failed:
				++ConsecutiveFailures;
				GB_WARN(TEXT("Checking for configuration changes failed (%s); will retry."), *Result.Error);
				break;
			}

			// OnSnapshot may have destroyed us (e.g. a listener called Shutdown).
			if (!StillAlive.IsValid() || !*StillAlive)
			{
				return;
			}
			Schedule(Result.Status == EGamebeastFetchStatus::Failed ? Result.RetryAfterSeconds : -1.f);
			if (bPollQueued)
			{
				Poll();
			}
		});
}

void FGamebeastStatusPoller::Schedule(float RetryAfterSeconds)
{
	SecondsUntilPoll = Gamebeast::Rates::NextPollDelay(
		PollRate, Override, ConsecutiveFailures, RetryAfterSeconds, FMath::FRand());
}
